#pragma warning(disable : 4100) // unreferenced formal parameter

#include "FpsGameEditor.h"

#include "DeferredRenderer.h"
#include "DroppedFileUtils.h"
#include "FpsGameMap.h"
#include "Mesh.h"
#include "MeshInstance.h"
#include "PathUtils.h"
#include "PathTracer.h"
#include "Renderer.h"
#include "RenderStatsUI.h"
#include "Scene.h"
#include "SoftwareRasterizer.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuizmo.h"

#include "glm/gtc/type_ptr.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <thread>

#include <GLFW/glfw3.h>

namespace RTRT
{


// ----------------------------------------------------------------------------
// FpsGameEditorContext
// ----------------------------------------------------------------------------
FpsGameEditorContext::FpsGameEditorContext( GLFWwindow * iWindow,
                                            Scene * iScene,
                                            Renderer * iRenderer,
                                            RenderSettings & iSettings,
                                            const KeyInput & iKeyInput,
                                            FpsGameSettings & iGameSettings,
                                            FpsGameWorld & iGameWorld,
                                            FpsGameSceneBinding & iSceneBinding,
                                            FpsGameMap & iMap,
                                            std::string & iMapPath,
                                            bool & iMapLoaded,
                                            bool & iReloadScene,
                                            double iFrameRate,
                                            double iFrameTime,
                                            double iDeltaTime,
                                            unsigned int iNbRenderedFrames,
                                            int & iDebugMode,
                                            int & iDeferredDebugView,
                                            bool & iDeferredShowWires,
                                            int & iSoftwareDebugView,
                                            bool & iSoftwareShowWires,
                                            const std::vector<FpsCpuTiming> & iCpuTimings )
: _Window(iWindow)
, _Scene(iScene)
, _Renderer(iRenderer)
, _Settings(iSettings)
, _KeyInput(iKeyInput)
, _GameSettings(iGameSettings)
, _GameWorld(iGameWorld)
, _SceneBinding(iSceneBinding)
, _Map(iMap)
, _MapPath(iMapPath)
, _MapLoaded(iMapLoaded)
, _ReloadScene(iReloadScene)
, _FrameRate(iFrameRate)
, _FrameTime(iFrameTime)
, _DeltaTime(iDeltaTime)
, _NbRenderedFrames(iNbRenderedFrames)
, _DebugMode(iDebugMode)
, _DeferredDebugView(iDeferredDebugView)
, _DeferredShowWires(iDeferredShowWires)
, _SoftwareDebugView(iSoftwareDebugView)
, _SoftwareShowWires(iSoftwareShowWires)
, _CpuTimings(iCpuTimings)
{
}

// ----------------------------------------------------------------------------
// HandleMousePick
// ----------------------------------------------------------------------------
void FpsGameEditor::HandleMousePick( const FpsGameEditorContext & iContext, double iMouseX, double iMouseY )
{
  FpsEditorSelection selection;
  if ( PickSelection(iContext, iMouseX, iMouseY, selection) )
    _Selection = selection;
  else
    _Selection = FpsEditorSelection();
}

// ----------------------------------------------------------------------------
// DrawPanels
// ----------------------------------------------------------------------------
void FpsGameEditor::DrawPanels( FpsGameEditorContext & ioContext )
{
  DrawScenePanel(ioContext);
  DrawInspectorPanel(ioContext);
  DrawMaterialsPanel(ioContext);
  DrawSettingsPanel(ioContext);
  DrawRenderSettingsPanel(ioContext);
  DrawPerformancePanel(ioContext);
}

// ----------------------------------------------------------------------------
// CopyPathToBuffer
// ----------------------------------------------------------------------------
static void CopyPathToBuffer( char * oBuffer, size_t iBufferSize, const std::string & iPath )
{
  if ( !oBuffer || !iBufferSize )
    return;

  std::snprintf(oBuffer, iBufferSize, "%s", iPath.c_str());
}

// ----------------------------------------------------------------------------
// EditorPropAssetPath
// ----------------------------------------------------------------------------
static bool EditorIsPropAssetPath( const std::filesystem::path & iPath )
{
  std::string ext = iPath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), []( unsigned char c ) { return static_cast<char>(std::tolower(c)); });
  return ( ".obj" == ext ) || ( ".gltf" == ext ) || ( ".glb" == ext );
}

static bool EditorPropAssetCombo( const char * iLabel, const std::vector<std::string> & iAssets, std::string & ioPath )
{
  const char * preview = ioPath.empty() ? "<select prop>" : ioPath.c_str();
  bool changed = false;

  if ( ImGui::BeginCombo(iLabel, preview) )
  {
    for ( int i = 0; i < static_cast<int>(iAssets.size()); ++i )
    {
      const bool selected = ( ioPath == iAssets[i] );
      if ( ImGui::Selectable(iAssets[i].c_str(), selected) )
      {
        ioPath = iAssets[i];
        changed = true;
      }
      if ( selected )
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  return changed;
}

static std::string EditorUniqueName( const std::string & iBaseName, const std::vector<std::string> & iUsedNames )
{
  std::string baseName = iBaseName.empty() ? "Item" : iBaseName;
  std::string name = baseName + " Copy";
  int suffix = 2;
  bool found = true;
  while ( found )
  {
    found = false;
    for ( const std::string & usedName : iUsedNames )
    {
      if ( usedName == name )
      {
        found = true;
        name = baseName + " Copy " + std::to_string(suffix++);
        break;
      }
    }
  }
  return name;
}

static bool EditorPathIsUnderAssets( const std::filesystem::path & iPath )
{
  std::error_code ec;
  const std::filesystem::path assetsDir = std::filesystem::weakly_canonical(PathUtils::GetAssetPath(""), ec);
  if ( ec || assetsDir.empty() )
    return false;

  ec.clear();
  const std::filesystem::path filePath = std::filesystem::weakly_canonical(iPath, ec);
  if ( ec || filePath.empty() )
    return false;

  const std::filesystem::path relativePath = std::filesystem::relative(filePath, assetsDir, ec);
  if ( ec || relativePath.empty() )
    return false;

  for ( const std::filesystem::path & part : relativePath )
  {
    if ( ".." == part )
      return false;
  }

  return true;
}

static bool EditorIsBuiltinMaterialName( const std::string & iName )
{
  return ( "floor" == iName )
      || ( "wall" == iName )
      || ( "pillar" == iName )
      || ( "crate" == iName )
      || ( "accent" == iName );
}

// ----------------------------------------------------------------------------
// EditorSyncFramebufferResolution
// ----------------------------------------------------------------------------
static void EditorSyncFramebufferResolution( FpsGameEditorContext & ioContext, bool iNotifyRenderer )
{
  if ( !ioContext._Window )
    return;

  int frameBufferWidth = 0;
  int frameBufferHeight = 0;
  glfwGetFramebufferSize(ioContext._Window, &frameBufferWidth, &frameBufferHeight);
  if ( !frameBufferWidth || !frameBufferHeight )
    return;

  const int renderWidth = std::max(1, frameBufferWidth * ioContext._Settings._RenderScale / 100);
  const int renderHeight = std::max(1, frameBufferHeight * ioContext._Settings._RenderScale / 100);
  if ( ( ioContext._Settings._WindowResolution.x == frameBufferWidth )
    && ( ioContext._Settings._WindowResolution.y == frameBufferHeight )
    && ( ioContext._Settings._RenderResolution.x == renderWidth )
    && ( ioContext._Settings._RenderResolution.y == renderHeight ) )
    return;

  ioContext._Settings._WindowResolution.x = frameBufferWidth;
  ioContext._Settings._WindowResolution.y = frameBufferHeight;
  ioContext._Settings._RenderResolution.x = renderWidth;
  ioContext._Settings._RenderResolution.y = renderHeight;

  if ( iNotifyRenderer && ioContext._Renderer )
    ioContext._Renderer -> Notify(DirtyState::RenderSettings);
}

// ----------------------------------------------------------------------------
// EditorMaterialSlotFromName
// ----------------------------------------------------------------------------
static bool EditorMaterialSlotFromName( const std::string & iName, FpsMaterialSlot & oMaterial )
{
  if ( "floor" == iName )
    oMaterial = FpsMaterialSlot::Floor;
  else if ( "wall" == iName )
    oMaterial = FpsMaterialSlot::Wall;
  else if ( "pillar" == iName )
    oMaterial = FpsMaterialSlot::Pillar;
  else if ( "crate" == iName )
    oMaterial = FpsMaterialSlot::Crate;
  else if ( "accent" == iName )
    oMaterial = FpsMaterialSlot::Accent;
  else
    return false;

  return true;
}

// ----------------------------------------------------------------------------
// EditorObjectTransform
// ----------------------------------------------------------------------------
static glm::quat EditorQuatFromVec4( const Vec4 & iQuat )
{
  glm::quat quat(iQuat.w, iQuat.x, iQuat.y, iQuat.z);
  const float len = glm::length(quat);
  if ( len <= EPSILON )
    return glm::quat(1.f, 0.f, 0.f, 0.f);
  return glm::normalize(quat);
}

static Vec4 EditorVec4FromQuat( const glm::quat & iQuat )
{
  glm::quat quat = glm::normalize(iQuat);
  return Vec4(quat.x, quat.y, quat.z, quat.w);
}

static Mat4x4 EditorEulerTransform( const Vec3 & iRotation )
{
  const Vec3 rotRad(MathUtil::ToRadians(iRotation.x),
                    MathUtil::ToRadians(iRotation.y),
                    MathUtil::ToRadians(iRotation.z));

  Mat4x4 transform(1.f);
  transform = transform * glm::rotate(rotRad.x, Vec3(1.f, 0.f, 0.f));
  transform = transform * glm::rotate(rotRad.y, Vec3(0.f, 1.f, 0.f));
  transform = transform * glm::rotate(rotRad.z, Vec3(0.f, 0.f, 1.f));
  return transform;
}

static Vec4 EditorVec4FromEuler( const Vec3 & iRotation )
{
  const glm::quat quat = glm::quat_cast(glm::mat3(EditorEulerTransform(iRotation)));
  return EditorVec4FromQuat(quat);
}

static Vec3 EditorEulerFromMatrix( const Mat4x4 & iTransform )
{
  float translation[3] = { 0.f, 0.f, 0.f };
  float rotation[3] = { 0.f, 0.f, 0.f };
  float scale[3] = { 1.f, 1.f, 1.f };
  ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(iTransform), translation, rotation, scale);
  return Vec3(rotation[0], rotation[1], rotation[2]);
}

static Mat4x4 EditorObjectRotationTransform( const FpsSceneObject & iObject )
{
  Vec4 orientation = iObject._Orientation;
  if ( glm::length(orientation) <= EPSILON )
    orientation = EditorVec4FromEuler(iObject._Rotation);
  return glm::toMat4(EditorQuatFromVec4(orientation));
}

static Mat4x4 EditorObjectTransform( const FpsSceneObject & iObject )
{
  Mat4x4 transform = glm::translate(iObject._Center);
  transform = transform * EditorObjectRotationTransform(iObject);
  transform = transform * glm::scale(iObject._HalfExtents * 2.f);
  return transform;
}

// ----------------------------------------------------------------------------
// EditorObjectGizmoTransform
// ----------------------------------------------------------------------------
static Mat4x4 EditorObjectGizmoTransform( const FpsSceneObject & iObject )
{
  Mat4x4 transform = glm::translate(iObject._Center);
  transform = transform * EditorObjectRotationTransform(iObject);
  return transform;
}

static Mat4x4 EditorPropTransform( const FpsMapProp & iProp )
{
  Mat4x4 transform = glm::translate(iProp._Position);
  transform = transform * EditorEulerTransform(iProp._Rotation);
  transform = transform * glm::scale(iProp._Scale);
  return transform;
}

static Mat4x4 EditorPropGizmoTransform( const FpsMapProp & iProp )
{
  Mat4x4 transform = glm::translate(iProp._Position);
  transform = transform * EditorEulerTransform(iProp._Rotation);
  return transform;
}

static Mat4x4 EditorBoidsTransform( const FpsMapBoids & iBoids )
{
  const BoidSettings & settings = iBoids._Settings;
  Mat4x4 transform = glm::translate(settings._BoundsCenter);
  transform = transform * glm::scale(Vec3(settings._BoundsRadius * 2.f,
                                          settings._BoundsHeight,
                                          settings._BoundsRadius * 2.f));
  return transform;
}

static Mat4x4 EditorBoidsGizmoTransform( const FpsMapBoids & iBoids )
{
  Mat4x4 transform(1.f);
  transform[3] = Vec4(iBoids._Settings._BoundsCenter, 1.f);
  return transform;
}

// ----------------------------------------------------------------------------
// EditorUnitBox
// ----------------------------------------------------------------------------
static AABB<Vec3> EditorUnitBox()
{
  AABB<Vec3> box;
  box.Insert(Vec3(-0.5f));
  box.Insert(Vec3( 0.5f));
  return box;
}

// ----------------------------------------------------------------------------
// EditorIntersectRayObject
// ----------------------------------------------------------------------------
static bool EditorIntersectRayObject( const Vec3 & iRayOrigin, const Vec3 & iRayDir, const FpsSceneObject & iObject, float & oHitT )
{
  Mat4x4 invTransform = glm::inverse(EditorObjectTransform(iObject));
  Vec3 localOrigin = Vec3(invTransform * Vec4(iRayOrigin, 1.f));
  Vec3 localDir = glm::normalize(Vec3(invTransform * Vec4(iRayDir, 0.f)));

  float localHitT = 0.f;
  if ( !MathUtil::IntersectRayAABB(localOrigin, localDir, EditorUnitBox(), localHitT) )
    return false;

  const Vec3 localHit = localOrigin + localDir * localHitT;
  const Vec3 worldHit = MathUtil::TransformPoint(localHit, EditorObjectTransform(iObject));
  oHitT = glm::length(worldHit - iRayOrigin);
  return true;
}

// ----------------------------------------------------------------------------
// EditorIntersectRayMeshInstance
// ----------------------------------------------------------------------------
static bool EditorIntersectRayMeshInstance( const Vec3 & iRayOrigin, const Vec3 & iRayDir, const MeshInstance & iInstance, const Mesh & iMesh, float & oHitT )
{
  Mat4x4 invTransform = glm::inverse(iInstance._Transform);
  Vec3 localOrigin = Vec3(invTransform * Vec4(iRayOrigin, 1.f));
  Vec3 localDir = glm::normalize(Vec3(invTransform * Vec4(iRayDir, 0.f)));

  float boxHitT = 0.f;
  if ( !MathUtil::IntersectRayAABB(localOrigin, localDir, iMesh.GetBoundingBox(), boxHitT) )
    return false;

  bool hit = false;
  float nearestDist = MAX_FLOAT;
  const std::vector<Vec3> & vertices = iMesh.GetVertices();
  const std::vector<Vec3i> & indices = iMesh.GetIndices();
  for ( int i = 0; i + 2 < static_cast<int>(indices.size()); i += 3 )
  {
    const Vec3i & i0 = indices[i + 0];
    const Vec3i & i1 = indices[i + 1];
    const Vec3i & i2 = indices[i + 2];

    float triHitT = 0.f;
    if ( MathUtil::IntersectRayTriangle(localOrigin, localDir, vertices[i0.x], vertices[i1.x], vertices[i2.x], triHitT) )
    {
      const Vec3 localHit = localOrigin + localDir * triHitT;
      const Vec3 worldHit = MathUtil::TransformPoint(localHit, iInstance._Transform);
      const float worldDist = glm::length(worldHit - iRayOrigin);
      if ( worldDist < nearestDist )
      {
        nearestDist = worldDist;
        hit = true;
      }
    }
  }

  if ( !hit )
    return false;

  oHitT = nearestDist;
  return true;
}

// ----------------------------------------------------------------------------
// EditorIntersectRaySphere
// ----------------------------------------------------------------------------
static bool EditorIntersectRaySphere( const Vec3 & iRayOrigin, const Vec3 & iRayDir, const Vec3 & iCenter, float iRadius, float & oHitT )
{
  const Vec3 oc = iRayOrigin - iCenter;
  const float b = glm::dot(oc, iRayDir);
  const float c = glm::dot(oc, oc) - iRadius * iRadius;
  const float discriminant = b * b - c;
  if ( discriminant < 0.f )
    return false;

  const float sqrtDiscriminant = std::sqrt(discriminant);
  float hitT = -b - sqrtDiscriminant;
  if ( hitT < 0.f )
    hitT = -b + sqrtDiscriminant;
  if ( hitT < 0.f )
    return false;

  oHitT = hitT;
  return true;
}

static bool EditorIntersectRayBoids( const Vec3 & iRayOrigin, const Vec3 & iRayDir, const FpsMapBoids & iBoids, float & oHitT )
{
  Mat4x4 transform = EditorBoidsTransform(iBoids);
  Mat4x4 invTransform = glm::inverse(transform);
  Vec3 localOrigin = Vec3(invTransform * Vec4(iRayOrigin, 1.f));
  Vec3 localDir = glm::normalize(Vec3(invTransform * Vec4(iRayDir, 0.f)));

  float localHitT = 0.f;
  if ( !MathUtil::IntersectRayAABB(localOrigin, localDir, EditorUnitBox(), localHitT) )
    return false;

  const Vec3 localHit = localOrigin + localDir * localHitT;
  const Vec3 worldHit = MathUtil::TransformPoint(localHit, transform);
  oHitT = glm::length(worldHit - iRayOrigin);
  return true;
}

// ----------------------------------------------------------------------------
// EditorProjectPoint
// ----------------------------------------------------------------------------
static bool EditorProjectPoint( const Vec3 & iPoint, const Mat4x4 & iView, const Mat4x4 & iProj, const ImVec2 & iDisplaySize, ImVec2 & oPoint )
{
  Vec4 clip = iProj * iView * Vec4(iPoint, 1.f);
  if ( clip.w <= 0.00001f )
    return false;

  Vec3 ndc = Vec3(clip) / clip.w;
  oPoint.x = ( ndc.x * 0.5f + 0.5f ) * iDisplaySize.x;
  oPoint.y = ( 0.5f - ndc.y * 0.5f ) * iDisplaySize.y;
  return true;
}

// ----------------------------------------------------------------------------
// EditorDrawTransformedBox
// ----------------------------------------------------------------------------
static void EditorDrawTransformedBox( const Mat4x4 & iTransform, const Mat4x4 & iView, const Mat4x4 & iProj, ImDrawList * ioDrawList, ImU32 iColor, float iLineWidth )
{
  if ( !ioDrawList )
    return;

  Vec3 corners[8];
  EditorUnitBox().Corners(corners);

  ImGuiIO & io = ImGui::GetIO();
  ImVec2 screenCorners[8];
  bool visibleCorners[8] = { false, false, false, false, false, false, false, false };
  for ( int i = 0; i < 8; ++i )
    visibleCorners[i] = EditorProjectPoint(MathUtil::TransformPoint(corners[i], iTransform), iView, iProj, io.DisplaySize, screenCorners[i]);

  static const int Edges[12][2] =
  {
    { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
    { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
    { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
  };

  for ( int i = 0; i < 12; ++i )
  {
    const int c0 = Edges[i][0];
    const int c1 = Edges[i][1];
    if ( visibleCorners[c0] && visibleCorners[c1] )
      ioDrawList -> AddLine(screenCorners[c0], screenCorners[c1], iColor, iLineWidth);
  }
}

static void EditorDrawTransformedAABB( const AABB<Vec3> & iBox, const Mat4x4 & iTransform, const Mat4x4 & iView, const Mat4x4 & iProj, ImDrawList * ioDrawList, ImU32 iColor, float iLineWidth )
{
  if ( !ioDrawList )
    return;

  Vec3 corners[8];
  iBox.Corners(corners);

  ImGuiIO & io = ImGui::GetIO();
  ImVec2 screenCorners[8];
  bool visibleCorners[8] = { false, false, false, false, false, false, false, false };
  for ( int i = 0; i < 8; ++i )
    visibleCorners[i] = EditorProjectPoint(MathUtil::TransformPoint(corners[i], iTransform), iView, iProj, io.DisplaySize, screenCorners[i]);

  static const int Edges[12][2] =
  {
    { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
    { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
    { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
  };

  for ( int i = 0; i < 12; ++i )
  {
    const int c0 = Edges[i][0];
    const int c1 = Edges[i][1];
    if ( visibleCorners[c0] && visibleCorners[c1] )
      ioDrawList -> AddLine(screenCorners[c0], screenCorners[c1], iColor, iLineWidth);
  }
}

// ----------------------------------------------------------------------------
// SetEditorPathBuffers
// ----------------------------------------------------------------------------
void FpsGameEditor::SetPathBuffers( const std::string & iMapPath )
{
  CopyPathToBuffer(_SavePath, sizeof(_SavePath), iMapPath);
  CopyPathToBuffer(_LoadPath, sizeof(_LoadPath), iMapPath);
}

// ----------------------------------------------------------------------------
// SetEditorStatus
// ----------------------------------------------------------------------------
void FpsGameEditor::SetStatus( const std::string & iMessage )
{
  _StatusMessage = iMessage;
}

// ----------------------------------------------------------------------------
// RefreshEditorPropAssets
// ----------------------------------------------------------------------------
void FpsGameEditor::RefreshPropAssets()
{
  _PropAssetPaths.clear();

  const std::filesystem::path assetsDir(PathUtils::GetAssetPath(""));
  std::error_code ec;
  if ( !std::filesystem::exists(assetsDir, ec) )
  {
    _NewPropAssetIndex = -1;
    return;
  }

  std::filesystem::directory_iterator it(assetsDir, std::filesystem::directory_options::skip_permission_denied, ec);
  std::filesystem::directory_iterator end;
  while ( !ec && it != end )
  {
    const std::filesystem::directory_entry entry = *it;
    if ( entry.is_regular_file(ec) && EditorIsPropAssetPath(entry.path()) )
    {
      const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), assetsDir, ec);
      if ( !ec )
        _PropAssetPaths.push_back(relativePath.generic_string());
    }
    it.increment(ec);
  }

  std::sort(_PropAssetPaths.begin(), _PropAssetPaths.end());
  if ( _PropAssetPaths.empty() )
    _NewPropAssetIndex = -1;
  else if ( ( _NewPropAssetIndex < 0 ) || ( _NewPropAssetIndex >= static_cast<int>(_PropAssetPaths.size()) ) )
    _NewPropAssetIndex = 0;
}

// ----------------------------------------------------------------------------
// AddDroppedEditorProp
// ----------------------------------------------------------------------------
bool FpsGameEditor::AddDroppedProp( FpsGameEditorContext & ioContext, const std::filesystem::path & iPath, const Vec3 & iPosition )
{
  if ( !ioContext._Scene || !DroppedFileUtils::IsDroppedPropPath(iPath) )
    return false;

  std::error_code ec;
  std::filesystem::path filepath = DroppedFileUtils::NormalizeDroppedPath(iPath);

  if ( !std::filesystem::exists(filepath, ec) || ec )
  {
    SetStatus("Dropped prop file does not exist: " + filepath.generic_string());
    return false;
  }

  std::vector<std::string> usedNames;
  for ( const FpsMapProp & prop : ioContext._Map._Props )
    usedNames.push_back(prop._Name);

  std::string propName = filepath.stem().string();
  if ( propName.empty() )
    propName = "Dropped Prop";
  if ( std::find(usedNames.begin(), usedNames.end(), propName) != usedNames.end() )
    propName = EditorUniqueName(propName, usedNames);

  FpsMapProp prop;
  prop._Name = propName;
  prop._Path = filepath.generic_string();
  prop._Position = iPosition;
  prop._Rotation = Vec3(0.f);
  prop._Scale = Vec3(1.f);
  prop._Visible = true;

  ioContext._Map._Props.push_back(prop);
  const int propIndex = static_cast<int>(ioContext._Map._Props.size()) - 1;

  if ( 0 != ioContext._SceneBinding.LoadProp(*ioContext._Scene, ioContext._Map, propIndex) )
  {
    ioContext._Map._Props.pop_back();
    SetStatus("Failed to load dropped prop: " + filepath.generic_string());
    return false;
  }

  _Selection._Kind = FpsEditableKind::Prop;
  _Selection._Index = propIndex;
  _Selection._SceneInstanceID = -1;
  MarkDirty();

  if ( ioContext._Renderer )
  {
    ioContext._Renderer -> Notify(DirtyState::SceneInstances);
    ioContext._Renderer -> Notify(DirtyState::SceneMaterials);
    ioContext._Renderer -> Notify(DirtyState::Textures);
  }

  if ( EditorPathIsUnderAssets(filepath) )
    RefreshPropAssets();

  SetStatus("Dropped prop loaded: " + prop._Name);
  return true;
}

// ----------------------------------------------------------------------------
// SyncMapFromRuntimeSettings
// ----------------------------------------------------------------------------
void FpsGameEditor::SyncMapFromRuntimeSettings( FpsGameEditorContext & ioContext )
{
  ioContext._Map._MaxProjectiles = ioContext._GameSettings._MaxProjectiles;
  ioContext._Map._MaxProjectileAmmo = ioContext._GameSettings._MaxProjectileAmmo;
  ioContext._Map._ProjectileAmmoRefillTime = ioContext._GameSettings._ProjectileAmmoRefillTime;

  ioContext._Map._Weapon._Visible = _Enabled ? _PreviousShowViewWeapon : ioContext._GameSettings._ShowViewWeapon;
  ioContext._Map._Weapon._Offset = ioContext._GameSettings._ViewWeaponOffset;
  ioContext._Map._Weapon._Rotation = ioContext._GameSettings._ViewWeaponRotation;
  ioContext._Map._Weapon._Scale = ioContext._GameSettings._ViewWeaponScale;
}

// ----------------------------------------------------------------------------
// SetEditorMode
// ----------------------------------------------------------------------------
bool FpsGameEditor::SetMode( FpsGameEditorContext & ioContext, bool iEnabled )
{
  if ( _Enabled == iEnabled )
    return false;

  _Enabled = iEnabled;

  if ( _Enabled )
  {
    _PreviousShowViewWeapon = ioContext._GameSettings._ShowViewWeapon;
    _PreviousFreeLook = ioContext._GameSettings._FreeLook;
    ioContext._GameSettings._ShowViewWeapon = false;
    ioContext._GameSettings._FreeLook = true;
    ioContext._GameWorld.ClearProjectiles();
  }
  else
  {
    ioContext._GameSettings._ShowViewWeapon = _PreviousShowViewWeapon;
    ioContext._GameSettings._FreeLook = _PreviousFreeLook;
  }

  if ( ioContext._Scene )
  {
    ioContext._SceneBinding.SyncCamera(*ioContext._Scene, ioContext._GameWorld, ioContext._GameSettings);
    ioContext._SceneBinding.SyncTransforms(*ioContext._Scene, ioContext._GameWorld, ioContext._GameSettings);
  }

  if ( ioContext._Renderer )
  {
    ioContext._Renderer -> Notify(DirtyState::SceneCamera);
    ioContext._Renderer -> Notify(DirtyState::SceneInstances);
  }

  return true;
}

// ----------------------------------------------------------------------------
// SyncEditorObject
// ----------------------------------------------------------------------------
void FpsGameEditor::SyncObject( FpsGameEditorContext & ioContext, int iObjectIndex )
{
  if ( ( iObjectIndex < 0 ) || ( iObjectIndex >= static_cast<int>(ioContext._Map._Objects.size()) ) )
    return;

  std::vector<FpsSceneObject> & objects = ioContext._GameWorld.GetObjects();
  if ( iObjectIndex >= static_cast<int>(objects.size()) )
    return;

  objects[iObjectIndex] = ioContext._Map._Objects[iObjectIndex];

  if ( ioContext._Scene )
    ioContext._SceneBinding.SyncTransforms(*ioContext._Scene, ioContext._GameWorld, ioContext._GameSettings);
  if ( ioContext._Renderer )
    ioContext._Renderer -> Notify(DirtyState::SceneInstances);
}

// ----------------------------------------------------------------------------
// SyncEditorProp
// ----------------------------------------------------------------------------
void FpsGameEditor::SyncProp( FpsGameEditorContext & ioContext, int iPropIndex )
{
  if ( !ioContext._Scene )
    return;
  if ( ( iPropIndex < 0 ) || ( iPropIndex >= static_cast<int>(ioContext._Map._Props.size()) ) )
    return;

  if ( 0 != ioContext._SceneBinding.SyncProp(*ioContext._Scene, ioContext._Map, iPropIndex) )
    return;

  if ( ioContext._Renderer )
    ioContext._Renderer -> Notify(DirtyState::SceneInstances);
}

// ----------------------------------------------------------------------------
// SyncEditorLight
// ----------------------------------------------------------------------------
void FpsGameEditor::SyncLight( FpsGameEditorContext & ioContext, int iLightIndex )
{
  if ( !ioContext._Scene )
    return;
  if ( ( iLightIndex < 0 ) || ( iLightIndex >= static_cast<int>(ioContext._Map._Lights.size()) ) )
    return;

  Light * light = ioContext._Scene -> GetLight(iLightIndex);
  if ( !light )
    return;

  *light = ioContext._Map._Lights[iLightIndex];
  if ( ioContext._Renderer )
    ioContext._Renderer -> Notify(DirtyState::SceneLights);
}

// ----------------------------------------------------------------------------
// DeleteSelectedEditorItem
// ----------------------------------------------------------------------------
bool FpsGameEditor::DeleteSelectedItem( FpsGameEditorContext & ioContext )
{
  const FpsEditableKind kind = _Selection._Kind;
  const int index = _Selection._Index;

  if ( ( FpsEditableKind::Box == kind ) || ( FpsEditableKind::Collider == kind ) )
  {
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Objects.size()) ) )
      return false;

    const std::string name = ioContext._Map._Objects[index]._Name;
    ioContext._Map._Objects.erase(ioContext._Map._Objects.begin() + index);
    if ( index < static_cast<int>(_ObjectInstanceVisible.size()) )
      _ObjectInstanceVisible.erase(_ObjectInstanceVisible.begin() + index);
    _Selection = FpsEditorSelection();
    MarkDirty();
    ioContext._ReloadScene = true;
    SetStatus("Deleted " + name);
    return true;
  }

  if ( FpsEditableKind::Prop == kind )
  {
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Props.size()) ) )
      return false;

    const std::string name = ioContext._Map._Props[index]._Name;
    ioContext._Map._Props.erase(ioContext._Map._Props.begin() + index);
    _Selection = FpsEditorSelection();
    MarkDirty();
    ioContext._ReloadScene = true;
    SetStatus("Deleted " + name);
    return true;
  }

  if ( FpsEditableKind::Light == kind )
  {
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Lights.size()) ) )
      return false;

    ioContext._Map._Lights.erase(ioContext._Map._Lights.begin() + index);
    _Selection = FpsEditorSelection();
    MarkDirty();
    ioContext._ReloadScene = true;
    SetStatus("Deleted light");
    return true;
  }

  if ( FpsEditableKind::Boids == kind )
  {
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Boids.size()) ) )
      return false;

    const std::string name = ioContext._Map._Boids[index]._Name;
    ioContext._Map._Boids.erase(ioContext._Map._Boids.begin() + index);
    _Selection = FpsEditorSelection();
    MarkDirty();
    ioContext._ReloadScene = true;
    SetStatus("Deleted " + name);
    return true;
  }

  return false;
}

// ----------------------------------------------------------------------------
// DuplicateSelectedEditorItem
// ----------------------------------------------------------------------------
bool FpsGameEditor::DuplicateSelectedItem( FpsGameEditorContext & ioContext )
{
  const FpsEditableKind kind = _Selection._Kind;
  const int index = _Selection._Index;

  if ( ( FpsEditableKind::Box == kind ) || ( FpsEditableKind::Collider == kind ) )
  {
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Objects.size()) ) )
      return false;

    std::vector<std::string> usedNames;
    for ( const FpsSceneObject & object : ioContext._Map._Objects )
      usedNames.push_back(object._Name);

    FpsSceneObject object = ioContext._Map._Objects[index];
    object._Name = EditorUniqueName(object._Name, usedNames);
    object._Center += Vec3(1.f, 0.f, 1.f);
    ioContext._Map._Objects.push_back(object);
    EnsureObjectVisibility(ioContext);
    if ( index < static_cast<int>(_ObjectInstanceVisible.size()) )
      _ObjectInstanceVisible.back() = _ObjectInstanceVisible[index];
    _Selection._Kind = object._Visible ? FpsEditableKind::Box : FpsEditableKind::Collider;
    _Selection._Index = static_cast<int>(ioContext._Map._Objects.size()) - 1;
    MarkDirty();
    ioContext._ReloadScene = true;
    SetStatus("Duplicated " + object._Name);
    return true;
  }

  if ( FpsEditableKind::Prop == kind )
  {
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Props.size()) ) )
      return false;

    std::vector<std::string> usedNames;
    for ( const FpsMapProp & prop : ioContext._Map._Props )
      usedNames.push_back(prop._Name);

    FpsMapProp prop = ioContext._Map._Props[index];
    prop._Name = EditorUniqueName(prop._Name, usedNames);
    prop._Position += Vec3(1.f, 0.f, 1.f);
    ioContext._Map._Props.push_back(prop);
    _Selection._Kind = FpsEditableKind::Prop;
    _Selection._Index = static_cast<int>(ioContext._Map._Props.size()) - 1;
    MarkDirty();
    ioContext._ReloadScene = true;
    SetStatus("Duplicated " + prop._Name);
    return true;
  }

  if ( FpsEditableKind::Light == kind )
  {
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Lights.size()) ) )
      return false;

    Light light = ioContext._Map._Lights[index];
    if ( LightType::DistantLight != (LightType)(int)light._Type )
      light._Pos += Vec3(1.f, 0.f, 1.f);
    ioContext._Map._Lights.push_back(light);
    _Selection._Kind = FpsEditableKind::Light;
    _Selection._Index = static_cast<int>(ioContext._Map._Lights.size()) - 1;
    MarkDirty();
    ioContext._ReloadScene = true;
    SetStatus("Duplicated light");
    return true;
  }

  if ( FpsEditableKind::Boids == kind )
  {
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Boids.size()) ) )
      return false;

    std::vector<std::string> usedNames;
    for ( const FpsMapBoids & boids : ioContext._Map._Boids )
      usedNames.push_back(boids._Name);

    FpsMapBoids boids = ioContext._Map._Boids[index];
    boids._Name = EditorUniqueName(boids._Name, usedNames);
    boids._Settings._BoundsCenter += Vec3(1.f, 0.f, 1.f);
    ioContext._Map._Boids.push_back(boids);
    _Selection._Kind = FpsEditableKind::Boids;
    _Selection._Index = static_cast<int>(ioContext._Map._Boids.size()) - 1;
    MarkDirty();
    ioContext._ReloadScene = true;
    SetStatus("Duplicated " + boids._Name);
    return true;
  }

  return false;
}

// ----------------------------------------------------------------------------
// DeleteSelectedMaterial
// ----------------------------------------------------------------------------
bool FpsGameEditor::DeleteSelectedMaterial( FpsGameEditorContext & ioContext )
{
  if ( ( _SelectedMaterial < 0 ) || ( _SelectedMaterial >= static_cast<int>(ioContext._Map._Materials.size()) ) )
    return false;

  const std::string name = ioContext._Map._Materials[_SelectedMaterial]._Name;
  if ( EditorIsBuiltinMaterialName(name) )
  {
    SetStatus("Built-in material cannot be deleted");
    return false;
  }

  for ( const FpsSceneObject & object : ioContext._Map._Objects )
  {
    if ( object._Visible && ( object._MaterialName == name ) )
    {
      SetStatus("Material is still used by an object");
      return false;
    }
  }

  ioContext._Map._Materials.erase(ioContext._Map._Materials.begin() + _SelectedMaterial);
  if ( _SelectedMaterial >= static_cast<int>(ioContext._Map._Materials.size()) )
    _SelectedMaterial = static_cast<int>(ioContext._Map._Materials.size()) - 1;
  if ( _SelectedMaterial < 0 )
    _SelectedMaterial = 0;

  MarkDirty();
  ioContext._ReloadScene = true;
  SetStatus("Deleted material " + name);
  return true;
}

// ----------------------------------------------------------------------------
// DuplicateSelectedMaterial
// ----------------------------------------------------------------------------
bool FpsGameEditor::DuplicateSelectedMaterial( FpsGameEditorContext & ioContext )
{
  if ( ( _SelectedMaterial < 0 ) || ( _SelectedMaterial >= static_cast<int>(ioContext._Map._Materials.size()) ) )
    return false;

  std::vector<std::string> usedNames;
  for ( const FpsMapMaterial & material : ioContext._Map._Materials )
    usedNames.push_back(material._Name);

  FpsMapMaterial material = ioContext._Map._Materials[_SelectedMaterial];
  material._Name = EditorUniqueName(material._Name, usedNames);
  ioContext._Map._Materials.push_back(material);
  _SelectedMaterial = static_cast<int>(ioContext._Map._Materials.size()) - 1;
  MarkDirty();
  ioContext._ReloadScene = true;
  SetStatus("Duplicated material " + material._Name);
  return true;
}

// ----------------------------------------------------------------------------
// EnsureEditorObjectVisibility
// ----------------------------------------------------------------------------
void FpsGameEditor::EnsureObjectVisibility( FpsGameEditorContext & ioContext )
{
  const int objectCount = static_cast<int>(ioContext._Map._Objects.size());
  const int oldCount = static_cast<int>(_ObjectInstanceVisible.size());
  if ( oldCount < objectCount )
  {
    _ObjectInstanceVisible.resize(objectCount, true);
    for ( int i = oldCount; i < objectCount; ++i )
      _ObjectInstanceVisible[i] = ioContext._Map._Objects[i]._Visible;
  }
  else if ( oldCount > objectCount )
    _ObjectInstanceVisible.resize(objectCount);
}

// ----------------------------------------------------------------------------
// ApplyEditorObjectVisibility
// ----------------------------------------------------------------------------
void FpsGameEditor::ApplyObjectVisibility( FpsGameEditorContext & ioContext )
{
  if ( !ioContext._Scene )
    return;

  EnsureObjectVisibility(ioContext);
  for ( int i = 0; i < static_cast<int>(_ObjectInstanceVisible.size()); ++i )
    ioContext._SceneBinding.SetObjectInstanceVisible(*ioContext._Scene, i, _ObjectInstanceVisible[i]);
}

// ----------------------------------------------------------------------------
// BuildPickingRay
// ----------------------------------------------------------------------------
bool FpsGameEditor::BuildPickingRay( const FpsGameEditorContext & iContext, double iMouseX, double iMouseY, Vec3 & oRayOrigin, Vec3 & oRayDir ) const
{
  if ( !iContext._Window || !iContext._Scene )
    return false;

  int windowWidth = 0;
  int windowHeight = 0;
  glfwGetWindowSize(iContext._Window, &windowWidth, &windowHeight);
  if ( !windowWidth || !windowHeight || !iContext._Settings._WindowResolution.x || !iContext._Settings._WindowResolution.y )
    return false;

  const float mouseX = static_cast<float>(iMouseX) * static_cast<float>(iContext._Settings._WindowResolution.x) / static_cast<float>(windowWidth);
  const float mouseY = static_cast<float>(iMouseY) * static_cast<float>(iContext._Settings._WindowResolution.y) / static_cast<float>(windowHeight);

  const float ndcX = 2.f * mouseX / static_cast<float>(iContext._Settings._WindowResolution.x) - 1.f;
  const float ndcY = 1.f - 2.f * mouseY / static_cast<float>(iContext._Settings._WindowResolution.y);

  Mat4x4 view(1.f);
  Mat4x4 proj(1.f);
  Camera & camera = iContext._Scene -> GetCamera();
  camera.ComputeLookAtMatrix(view);
  camera.ComputePerspectiveProjMatrix(static_cast<float>(iContext._Settings._WindowResolution.x) / static_cast<float>(iContext._Settings._WindowResolution.y), proj);

  Mat4x4 invViewProj = glm::inverse(proj * view);
  Vec4 nearPoint = invViewProj * Vec4(ndcX, ndcY, -1.f, 1.f);
  Vec4 farPoint  = invViewProj * Vec4(ndcX, ndcY,  1.f, 1.f);
  if ( nearPoint.w != 0.f )
    nearPoint /= nearPoint.w;
  if ( farPoint.w != 0.f )
    farPoint /= farPoint.w;

  oRayOrigin = camera.GetPos();
  oRayDir = glm::normalize(Vec3(farPoint) - oRayOrigin);

  return glm::length(oRayDir) > 0.f;
}

// ----------------------------------------------------------------------------
// PickEditorSelection
// ----------------------------------------------------------------------------
bool FpsGameEditor::PickSelection( const FpsGameEditorContext & iContext, double iMouseX, double iMouseY, FpsEditorSelection & oSelection ) const
{
  oSelection = FpsEditorSelection();
  if ( !iContext._Scene )
    return false;

  Vec3 rayOrigin(0.f);
  Vec3 rayDir(0.f);
  if ( !BuildPickingRay(iContext, iMouseX, iMouseY, rayOrigin, rayDir) )
    return false;

  float nearestDist = MAX_FLOAT;

  for ( int i = 0; i < static_cast<int>(iContext._Map._Objects.size()); ++i )
  {
    const FpsSceneObject & object = iContext._Map._Objects[i];
    if ( object._Visible )
    {
      if ( i < static_cast<int>(_ObjectInstanceVisible.size()) && !_ObjectInstanceVisible[i] )
        continue;
    }
    else if ( !_ShowColliderHelpers )
      continue;

    float hitT = 0.f;
    if ( EditorIntersectRayObject(rayOrigin, rayDir, object, hitT) && hitT < nearestDist )
    {
      nearestDist = hitT;
      oSelection._Kind = object._Visible ? FpsEditableKind::Box : FpsEditableKind::Collider;
      oSelection._Index = i;
      oSelection._SceneInstanceID = -1;
    }
  }

  const std::vector<MeshInstance> & meshInstances = iContext._Scene -> GetMeshInstances();
  const std::vector<Mesh*> & meshes = iContext._Scene -> GetMeshes();
  for ( int propIndex = 0; propIndex < static_cast<int>(iContext._Map._Props.size()); ++propIndex )
  {
    const FpsMapProp & prop = iContext._Map._Props[propIndex];
    if ( !prop._Visible )
      continue;

    const std::vector<int> * instanceIDs = iContext._SceneBinding.GetPropInstanceIDs(propIndex);
    if ( !instanceIDs )
      continue;

    for ( int instanceID : *instanceIDs )
    {
      if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(meshInstances.size()) ) )
        continue;

      const MeshInstance & instance = meshInstances[instanceID];
      if ( !instance._Visible )
        continue;
      if ( ( instance._MeshID < 0 ) || ( instance._MeshID >= static_cast<int>(meshes.size()) ) )
        continue;

      Mesh * mesh = meshes[instance._MeshID];
      if ( !mesh )
        continue;

      float hitT = 0.f;
      if ( EditorIntersectRayMeshInstance(rayOrigin, rayDir, instance, *mesh, hitT) && hitT < nearestDist )
      {
        nearestDist = hitT;
        oSelection._Kind = FpsEditableKind::Prop;
        oSelection._Index = propIndex;
        oSelection._SceneInstanceID = instanceID;
      }
    }
  }

  if ( _ShowLightHelpers )
  {
    for ( int i = 0; i < static_cast<int>(iContext._Map._Lights.size()); ++i )
    {
      const Light & light = iContext._Map._Lights[i];
      const LightType type = (LightType)(int)light._Type;
      if ( LightType::DistantLight == type )
        continue;

      const float radius = std::max(0.25f, light._Radius);
      float hitT = 0.f;
      if ( EditorIntersectRaySphere(rayOrigin, rayDir, light._Pos, radius, hitT) && hitT < nearestDist )
      {
        nearestDist = hitT;
        oSelection._Kind = FpsEditableKind::Light;
        oSelection._Index = i;
        oSelection._SceneInstanceID = -1;
      }
    }
  }

  if ( _ShowBoidsHelpers )
  {
    for ( int i = 0; i < static_cast<int>(iContext._Map._Boids.size()); ++i )
    {
      const FpsMapBoids & boids = iContext._Map._Boids[i];
      if ( !boids._Visible )
        continue;

      float hitT = 0.f;
      if ( EditorIntersectRayBoids(rayOrigin, rayDir, boids, hitT) && hitT < nearestDist )
      {
        nearestDist = hitT;
        oSelection._Kind = FpsEditableKind::Boids;
        oSelection._Index = i;
        oSelection._SceneInstanceID = -1;
      }
    }
  }

  return FpsEditableKind::None != oSelection._Kind;
}

// ----------------------------------------------------------------------------
// DrawEditorDockspace
// ----------------------------------------------------------------------------
void FpsGameEditor::DrawDockspace()
{
  const ImGuiViewport * viewport = ImGui::GetMainViewport();

  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar
                               | ImGuiWindowFlags_NoDocking
                               | ImGuiWindowFlags_NoTitleBar
                               | ImGuiWindowFlags_NoCollapse
                               | ImGuiWindowFlags_NoResize
                               | ImGuiWindowFlags_NoMove
                               | ImGuiWindowFlags_NoBringToFrontOnFocus
                               | ImGuiWindowFlags_NoNavFocus
                               | ImGuiWindowFlags_NoBackground;

  ImGui::SetNextWindowPos(viewport -> WorkPos);
  ImGui::SetNextWindowSize(viewport -> WorkSize);
  ImGui::SetNextWindowViewport(viewport -> ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
  ImGui::Begin("Test6 Editor Dockspace", nullptr, windowFlags);
  ImGui::PopStyleVar(3);

  const ImGuiID dockspaceID = ImGui::GetID("Test6EditorDockspace");
  const ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;

  if ( _ResetDockLayout || !ImGui::DockBuilderGetNode(dockspaceID) )
  {
    _ResetDockLayout = false;
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace | dockspaceFlags);
    ImGui::DockBuilderSetNodeSize(dockspaceID, viewport -> WorkSize);

    ImGuiID leftID = 0;
    ImGuiID rightID = 0;
    ImGuiID rightBottomID = 0;
    ImGuiID centerID = dockspaceID;
    ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Left, 0.24f, &leftID, &centerID);
    ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Right, 0.30f, &rightID, &centerID);
    ImGui::DockBuilderSplitNode(rightID, ImGuiDir_Down, 0.45f, &rightBottomID, &rightID);

    ImGui::DockBuilderDockWindow("Editor Scene", leftID);
    ImGui::DockBuilderDockWindow("Editor Inspector", rightID);
    ImGui::DockBuilderDockWindow("Editor Materials", rightBottomID);
    ImGui::DockBuilderDockWindow("Editor Settings", rightBottomID);
    ImGui::DockBuilderDockWindow("Editor Render Settings", rightBottomID);
    ImGui::DockBuilderDockWindow("Editor Performance", rightBottomID);
    ImGui::DockBuilderFinish(dockspaceID);
  }

  if ( ImGui::BeginMenuBar() )
  {
    if ( ImGui::BeginMenu("Panels") )
    {
      ImGui::MenuItem("Scene", nullptr, &_ShowScenePanel);
      ImGui::MenuItem("Inspector", nullptr, &_ShowInspectorPanel);
      ImGui::MenuItem("Materials", nullptr, &_ShowMaterialsPanel);
      ImGui::MenuItem("Settings", nullptr, &_ShowSettingsPanel);
      ImGui::MenuItem("Render Settings", nullptr, &_ShowRenderSettingsPanel);
      ImGui::MenuItem("Performance", nullptr, &_ShowPerformancePanel);
      ImGui::EndMenu();
    }
    if ( ImGui::MenuItem("Reset editor layout") )
      _ResetDockLayout = true;
    ImGui::EndMenuBar();
  }

  ImGui::DockSpace(dockspaceID, ImVec2(0.f, 0.f), dockspaceFlags);
  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawEditorScenePanel
// ----------------------------------------------------------------------------
void FpsGameEditor::DrawScenePanel( FpsGameEditorContext & ioContext )
{
  if ( !_ShowScenePanel )
    return;

  if ( !ImGui::Begin("Editor Scene", &_ShowScenePanel) )
  {
    ImGui::End();
    return;
  }

  ImGui::Text("Map: %s%s", ioContext._MapPath.c_str(), _Dirty ? " *" : "");
  ImGui::PushItemWidth(-FLT_MIN);
  ImGui::InputText("Save path", _SavePath, sizeof(_SavePath));
  if ( ImGui::Button("Save") )
  {
    SyncMapFromRuntimeSettings(ioContext);
    if ( FpsGameMapLoader::Save(_SavePath, ioContext._Map) )
    {
      ioContext._MapPath = _SavePath;
      SetPathBuffers(ioContext._MapPath);
      _Dirty = false;
      SetStatus("Saved " + ioContext._MapPath);
    }
    else
      SetStatus("Save failed");
  }

  ImGui::InputText("Load path", _LoadPath, sizeof(_LoadPath));
  if ( ImGui::Button("Load") )
  {
    FpsGameMap loadedMap;
    if ( FpsGameMapLoader::Load(_LoadPath, loadedMap) )
    {
      ioContext._Map = loadedMap;
      ioContext._MapPath = _LoadPath;
      ioContext._MapLoaded = true;
      _ObjectInstanceVisible.clear();
      _Selection = FpsEditorSelection();
      _Dirty = false;
      SetPathBuffers(ioContext._MapPath);
      ioContext._ReloadScene = true;
      SetStatus("Loaded " + ioContext._MapPath);
    }
    else
      SetStatus("Load failed");
  }
  ImGui::PopItemWidth();

  if ( ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen) )
  {
    if ( ImGui::Button("Add box") )
    {
      FpsSceneObject object;
      object._Name = "Box " + std::to_string(static_cast<int>(ioContext._Map._Objects.size()));
      object._Center = ioContext._GameWorld.GetPlayer().EyePosition(ioContext._GameSettings) + Vec3(0.f, 0.f, 3.f);
      object._HalfExtents = Vec3(0.5f);
      object._MaterialName = "wall";
      object._Material = FpsMaterialSlot::Wall;
      object._Collidable = true;
      object._Visible = true;
      ioContext._Map._Objects.push_back(object);
      EnsureObjectVisibility(ioContext);
      ioContext._MapLoaded = true;
      _Selection._Kind = FpsEditableKind::Box;
      _Selection._Index = static_cast<int>(ioContext._Map._Objects.size()) - 1;
      MarkDirty();
      ioContext._ReloadScene = true;
    }
    ImGui::SameLine();
    if ( ImGui::Button("Add collider") )
    {
      FpsSceneObject object;
      object._Name = "Collider " + std::to_string(static_cast<int>(ioContext._Map._Objects.size()));
      object._Center = ioContext._GameWorld.GetPlayer().EyePosition(ioContext._GameSettings) + Vec3(0.f, 0.f, 3.f);
      object._HalfExtents = Vec3(0.5f);
      object._MaterialName = "wall";
      object._Material = FpsMaterialSlot::Wall;
      object._Collidable = true;
      object._Visible = false;
      ioContext._Map._Objects.push_back(object);
      EnsureObjectVisibility(ioContext);
      ioContext._MapLoaded = true;
      _Selection._Kind = FpsEditableKind::Collider;
      _Selection._Index = static_cast<int>(ioContext._Map._Objects.size()) - 1;
      MarkDirty();
      ioContext._ReloadScene = true;
    }

    if ( ImGui::BeginListBox("Instances", ImVec2(-FLT_MIN, 180.f)) )
    {
      EnsureObjectVisibility(ioContext);
      for ( int i = 0; i < static_cast<int>(ioContext._Map._Objects.size()); ++i )
      {
        const FpsSceneObject & object = ioContext._Map._Objects[i];
        const bool selected = ( ( FpsEditableKind::Box == _Selection._Kind )
                             || ( FpsEditableKind::Collider == _Selection._Kind ) )
                           && ( _Selection._Index == i );
        std::string label = std::string(object._Visible ? "Box: " : "Collider: ") + object._Name;
        if ( object._Visible && !_ObjectInstanceVisible[i] )
          label += " (hidden)";
        label += "##object" + std::to_string(i);
        if ( ImGui::Selectable(label.c_str(), selected) )
        {
          _Selection._Kind = object._Visible ? FpsEditableKind::Box : FpsEditableKind::Collider;
          _Selection._Index = i;
        }
        if ( selected )
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndListBox();
    }

  }

  if ( ImGui::CollapsingHeader("Props") )
  {
    ImGui::InputText("New prop name", _NewPropName, sizeof(_NewPropName));
    if ( ImGui::Button("Refresh prop list") )
    {
      RefreshPropAssets();
      SetStatus("Found " + std::to_string(_PropAssetPaths.size()) + " root asset props");
    }

    const bool hasPropAssets = !_PropAssetPaths.empty();
    const bool hasSelectedPropAsset = hasPropAssets
                                   && ( _NewPropAssetIndex >= 0 )
                                   && ( _NewPropAssetIndex < static_cast<int>(_PropAssetPaths.size()) );
    if ( hasPropAssets )
    {
      const char * preview = hasSelectedPropAsset ? _PropAssetPaths[_NewPropAssetIndex].c_str()
                                                  : "<select prop>";
      if ( ImGui::BeginCombo("New prop asset", preview) )
      {
        for ( int i = 0; i < static_cast<int>(_PropAssetPaths.size()); ++i )
        {
          const bool selected = ( _NewPropAssetIndex == i );
          if ( ImGui::Selectable(_PropAssetPaths[i].c_str(), selected) )
            _NewPropAssetIndex = i;
          if ( selected )
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
    }
    else
      ImGui::TextUnformatted("No loadable props found in Assets");

    ImGui::BeginDisabled(!hasSelectedPropAsset);
    if ( ImGui::Button("Add prop") )
    {
      FpsMapProp prop;
      prop._Name = std::strlen(_NewPropName) > 0
                 ? _NewPropName
                 : ( "Prop " + std::to_string(static_cast<int>(ioContext._Map._Props.size())) );
      prop._Path = _PropAssetPaths[_NewPropAssetIndex];
      prop._Position = ioContext._GameWorld.GetPlayer().EyePosition(ioContext._GameSettings) + Vec3(0.f, 0.f, 3.f);
      prop._Rotation = Vec3(0.f);
      prop._Scale = Vec3(1.f);
      prop._Visible = true;
      ioContext._Map._Props.push_back(prop);
      ioContext._MapLoaded = true;
      _Selection._Kind = FpsEditableKind::Prop;
      _Selection._Index = static_cast<int>(ioContext._Map._Props.size()) - 1;
      MarkDirty();
      ioContext._ReloadScene = true;
    }
    ImGui::EndDisabled();

    if ( ImGui::BeginListBox("Props", ImVec2(-FLT_MIN, 150.f)) )
    {
      for ( int i = 0; i < static_cast<int>(ioContext._Map._Props.size()); ++i )
      {
        const FpsMapProp & prop = ioContext._Map._Props[i];
        const bool selected = ( FpsEditableKind::Prop == _Selection._Kind ) && ( _Selection._Index == i );
        std::string label = "Prop: " + prop._Name;
        if ( !prop._Visible )
          label += " (hidden)";
        label += "##prop" + std::to_string(i);
        if ( ImGui::Selectable(label.c_str(), selected) )
        {
          _Selection._Kind = FpsEditableKind::Prop;
          _Selection._Index = i;
        }
        if ( selected )
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndListBox();
    }

  }

  if ( ImGui::CollapsingHeader("Boids") )
  {
    if ( ImGui::Button("Add boids") )
    {
      FpsMapBoids boids;
      boids._Name = "Boids " + std::to_string(static_cast<int>(ioContext._Map._Boids.size()));
      boids._Settings._BoundsCenter = ioContext._GameWorld.GetPlayer().EyePosition(ioContext._GameSettings) + Vec3(0.f, 1.f, 3.f);
      boids._Settings._BoundsRadius = 4.f;
      boids._Settings._BoundsHeight = 3.f;
      boids._Visible = true;
      ioContext._Map._Boids.push_back(boids);
      ioContext._MapLoaded = true;
      _Selection._Kind = FpsEditableKind::Boids;
      _Selection._Index = static_cast<int>(ioContext._Map._Boids.size()) - 1;
      MarkDirty();
      ioContext._ReloadScene = true;
    }

    if ( ImGui::BeginListBox("Boids", ImVec2(-FLT_MIN, 120.f)) )
    {
      for ( int i = 0; i < static_cast<int>(ioContext._Map._Boids.size()); ++i )
      {
        const FpsMapBoids & boids = ioContext._Map._Boids[i];
        const bool selected = ( FpsEditableKind::Boids == _Selection._Kind ) && ( _Selection._Index == i );
        std::string label = "Boids: " + boids._Name;
        if ( !boids._Visible )
          label += " (hidden)";
        label += "##boids" + std::to_string(i);
        if ( ImGui::Selectable(label.c_str(), selected) )
        {
          _Selection._Kind = FpsEditableKind::Boids;
          _Selection._Index = i;
        }
        if ( selected )
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndListBox();
    }
  }

  if ( ImGui::CollapsingHeader("Lights") )
  {
    if ( ImGui::Button("Add sphere light") )
    {
      Light light;
      light._Type = (float)LightType::SphereLight;
      light._Pos = ioContext._GameWorld.GetPlayer().EyePosition(ioContext._GameSettings) + Vec3(0.f, 0.f, 2.f);
      light._Emission = Vec3(1.f, 0.85f, 0.65f);
      light._Intensity = 20.f;
      light._Radius = 0.25f;
      light._Area = 4.0f * static_cast<float>(M_PI) * light._Radius * light._Radius;
      ioContext._Map._Lights.push_back(light);
      _Selection._Kind = FpsEditableKind::Light;
      _Selection._Index = static_cast<int>(ioContext._Map._Lights.size()) - 1;
      MarkDirty();
      ioContext._ReloadScene = true;
    }
    ImGui::SameLine();
    if ( ImGui::Button("Add distant light") )
    {
      Light light;
      light._Type = (float)LightType::DistantLight;
      light._Pos = Vec3(-0.4f, 0.8f, -0.3f);
      light._Emission = Vec3(1.f);
      light._Intensity = 2.f;
      light._CastShadow = true;
      ioContext._Map._Lights.push_back(light);
      _Selection._Kind = FpsEditableKind::Light;
      _Selection._Index = static_cast<int>(ioContext._Map._Lights.size()) - 1;
      MarkDirty();
      ioContext._ReloadScene = true;
    }

    if ( ImGui::BeginListBox("Lights", ImVec2(-FLT_MIN, 150.f)) )
    {
      for ( int i = 0; i < static_cast<int>(ioContext._Map._Lights.size()); ++i )
      {
        const LightType type = (LightType)(int)ioContext._Map._Lights[i]._Type;
        const char * typeName = ( LightType::DistantLight == type ) ? "Distant" : ( LightType::RectLight == type ? "Rect" : "Sphere" );
        const bool selected = ( FpsEditableKind::Light == _Selection._Kind ) && ( _Selection._Index == i );
        const std::string label = std::string(typeName) + " light##light" + std::to_string(i);
        if ( ImGui::Selectable(label.c_str(), selected) )
        {
          _Selection._Kind = FpsEditableKind::Light;
          _Selection._Index = i;
        }
        if ( selected )
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndListBox();
    }

  }

  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawEditorInspectorPanel
// ----------------------------------------------------------------------------
void FpsGameEditor::DrawInspectorPanel( FpsGameEditorContext & ioContext )
{
  if ( !_ShowInspectorPanel )
    return;

  if ( !ImGui::Begin("Editor Inspector", &_ShowInspectorPanel) )
  {
    ImGui::End();
    return;
  }

  if ( ( FpsEditableKind::Box == _Selection._Kind )
    || ( FpsEditableKind::Collider == _Selection._Kind ) )
  {
    const int index = _Selection._Index;
    if ( ( index >= 0 ) && ( index < static_cast<int>(ioContext._Map._Objects.size()) ) )
    {
      FpsSceneObject & object = ioContext._Map._Objects[index];
      EnsureObjectVisibility(ioContext);
      bool objectDirty = false;
      char nameBuffer[128];
      std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", object._Name.c_str());
      if ( ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)) )
      {
        object._Name = nameBuffer;
        objectDirty = true;
      }

      objectDirty |= ImGui::DragFloat3("Center", &object._Center.x, 0.05f, -100.f, 100.f, "%.3f");
      if ( ImGui::DragFloat3("Rotation", &object._Rotation.x, 0.5f, -360.f, 360.f, "%.2f") )
      {
        object._Orientation = EditorVec4FromEuler(object._Rotation);
        objectDirty = true;
      }
      objectDirty |= ImGui::DragFloat3("Half extents", &object._HalfExtents.x, 0.05f, 0.01f, 100.f, "%.3f");
      objectDirty |= ImGui::Checkbox("Collidable", &object._Collidable);

      if ( object._Visible )
      {
        bool instanceVisible = _ObjectInstanceVisible[index];
        if ( ImGui::Checkbox("Show instance", &instanceVisible) )
        {
          _ObjectInstanceVisible[index] = instanceVisible;
          if ( ioContext._Scene )
            ioContext._SceneBinding.SetObjectInstanceVisible(*ioContext._Scene, index, instanceVisible);
          if ( ioContext._Renderer )
            ioContext._Renderer -> Notify(DirtyState::SceneInstances);
        }

        FpsGameMapLoader::SeedDefaultMaterials(ioContext._Map);
        int currentMaterial = 0;
        for ( int i = 0; i < static_cast<int>(ioContext._Map._Materials.size()); ++i )
        {
          if ( ioContext._Map._Materials[i]._Name == object._MaterialName )
            currentMaterial = i;
        }

        const char * currentName = ioContext._Map._Materials.empty() ? "" : ioContext._Map._Materials[currentMaterial]._Name.c_str();
        if ( ImGui::BeginCombo("Material", currentName) )
        {
          for ( int i = 0; i < static_cast<int>(ioContext._Map._Materials.size()); ++i )
          {
            const bool selected = ( i == currentMaterial );
            if ( ImGui::Selectable(ioContext._Map._Materials[i]._Name.c_str(), selected) )
            {
              object._MaterialName = ioContext._Map._Materials[i]._Name;
              FpsMaterialSlot materialSlot;
              if ( EditorMaterialSlotFromName(object._MaterialName, materialSlot) )
                object._Material = materialSlot;
              MarkDirty();
              ioContext._ReloadScene = true;
            }
            if ( selected )
              ImGui::SetItemDefaultFocus();
          }
          ImGui::EndCombo();
        }
      }

      if ( objectDirty )
      {
        object._HalfExtents = MathUtil::Max(object._HalfExtents, Vec3(0.01f));
        SyncObject(ioContext, index);
        MarkDirty();
      }
    }
  }
  else if ( FpsEditableKind::Prop == _Selection._Kind )
  {
    const int index = _Selection._Index;
    if ( ( index >= 0 ) && ( index < static_cast<int>(ioContext._Map._Props.size()) ) )
    {
      FpsMapProp & prop = ioContext._Map._Props[index];
      bool propDirty = false;
      bool reloadProp = false;

      char nameBuffer[128];
      std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", prop._Name.c_str());
      if ( ImGui::InputText("Prop name", nameBuffer, sizeof(nameBuffer)) )
      {
        prop._Name = nameBuffer;
        propDirty = true;
      }

      if ( !_PropAssetPaths.empty() )
      {
        if ( EditorPropAssetCombo("Prop asset", _PropAssetPaths, prop._Path) )
        {
          propDirty = true;
          reloadProp = true;
        }
      }
      else
        ImGui::TextUnformatted("No loadable props found in Assets");

      propDirty |= ImGui::DragFloat3("Prop position", &prop._Position.x, 0.05f, -100.f, 100.f, "%.3f");
      propDirty |= ImGui::DragFloat3("Prop rotation", &prop._Rotation.x, 0.5f, -360.f, 360.f, "%.2f");
      propDirty |= ImGui::DragFloat3("Prop scale", &prop._Scale.x, 0.05f, 0.01f, 100.f, "%.3f");
      propDirty |= ImGui::Checkbox("Prop visible", &prop._Visible);

      if ( propDirty )
      {
        prop._Scale = MathUtil::Max(prop._Scale, Vec3(0.01f));
        MarkDirty();
        if ( reloadProp )
          ioContext._ReloadScene = true;
        else
          SyncProp(ioContext, index);
      }
    }
  }
  else if ( FpsEditableKind::Light == _Selection._Kind )
  {
    const int index = _Selection._Index;
    if ( ( index >= 0 ) && ( index < static_cast<int>(ioContext._Map._Lights.size()) ) )
    {
      Light & light = ioContext._Map._Lights[index];
      bool lightDirty = false;
      LightType type = (LightType)(int)light._Type;
      int lightType = ( LightType::DistantLight == type ) ? 2 : ( LightType::RectLight == type ? 1 : 0 );
      static const char * lightTypes[] = { "Sphere", "Rect", "Distant" };
      if ( ImGui::Combo("Type", &lightType, lightTypes, 3) )
      {
        if ( 0 == lightType )
          light._Type = (float)LightType::SphereLight;
        else if ( 1 == lightType )
          light._Type = (float)LightType::RectLight;
        else
          light._Type = (float)LightType::DistantLight;
        lightDirty = true;
      }

      type = (LightType)(int)light._Type;
      if ( LightType::DistantLight == type )
        lightDirty |= ImGui::DragFloat3("Direction", &light._Pos.x, 0.01f, -1.f, 1.f, "%.3f");
      else
        lightDirty |= ImGui::DragFloat3("Position", &light._Pos.x, 0.05f, -100.f, 100.f, "%.3f");

      lightDirty |= ImGui::ColorEdit3("Emission", &light._Emission.x);
      lightDirty |= ImGui::DragFloat("Intensity", &light._Intensity, 0.1f, 0.f, 500.f, "%.2f");
      if ( LightType::SphereLight == type )
        lightDirty |= ImGui::DragFloat("Radius", &light._Radius, 0.01f, 0.01f, 20.f, "%.3f");
      if ( LightType::RectLight == type )
      {
        lightDirty |= ImGui::DragFloat3("Dir U", &light._DirU.x, 0.05f, -20.f, 20.f, "%.3f");
        lightDirty |= ImGui::DragFloat3("Dir V", &light._DirV.x, 0.05f, -20.f, 20.f, "%.3f");
        lightDirty |= ImGui::DragFloat("Area", &light._Area, 0.01f, 0.01f, 400.f, "%.3f");
      }
      lightDirty |= ImGui::Checkbox("Cast shadow", &light._CastShadow);
      lightDirty |= ImGui::DragFloat("Shadow radius", &light._ShadowRadius, 0.05f, 0.f, 200.f, "%.2f");

      if ( lightDirty )
      {
        if ( LightType::SphereLight == (LightType)(int)light._Type )
          light._Area = 4.0f * static_cast<float>(M_PI) * light._Radius * light._Radius;
        SyncLight(ioContext, index);
        MarkDirty();
      }
    }
  }
  else if ( FpsEditableKind::Boids == _Selection._Kind )
  {
    const int index = _Selection._Index;
    if ( ( index >= 0 ) && ( index < static_cast<int>(ioContext._Map._Boids.size()) ) )
    {
      FpsMapBoids & boids = ioContext._Map._Boids[index];
      BoidSettings & settings = boids._Settings;
      bool boidsDirty = false;

      char nameBuffer[128];
      std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", boids._Name.c_str());
      if ( ImGui::InputText("Boids name", nameBuffer, sizeof(nameBuffer)) )
      {
        boids._Name = nameBuffer;
        boidsDirty = true;
      }

      boidsDirty |= ImGui::Checkbox("Visible", &boids._Visible);
      boidsDirty |= ImGui::DragFloat3("Bounds center", &settings._BoundsCenter.x, 0.05f, -100.f, 100.f, "%.3f");
      boidsDirty |= ImGui::DragFloat("Bounds radius", &settings._BoundsRadius, 0.05f, 0.1f, 50.f, "%.3f");
      boidsDirty |= ImGui::DragFloat("Bounds height", &settings._BoundsHeight, 0.05f, 0.1f, 50.f, "%.3f");
      boidsDirty |= ImGui::ColorEdit3("Color", &settings._Color.x);

      int count = settings._Count;
      if ( ImGui::SliderInt("Count", &count, 0, 512) )
      {
        settings._Count = count;
        boidsDirty = true;
      }

      int seed = static_cast<int>(settings._Seed);
      if ( ImGui::InputInt("Seed", &seed) )
      {
        settings._Seed = static_cast<unsigned int>(std::max(seed, 1));
        boidsDirty = true;
      }

      boidsDirty |= ImGui::DragFloat("Scale", &settings._Scale, 0.01f, 0.01f, 1.f, "%.3f");
      if ( ImGui::DragFloat("Min speed", &settings._MinSpeed, 0.01f, 0.01f, 10.f, "%.3f") )
      {
        settings._MinSpeed = std::min(settings._MinSpeed, settings._MaxSpeed);
        boidsDirty = true;
      }
      if ( ImGui::DragFloat("Max speed", &settings._MaxSpeed, 0.01f, 0.01f, 12.f, "%.3f") )
      {
        settings._MaxSpeed = std::max(settings._MaxSpeed, settings._MinSpeed);
        boidsDirty = true;
      }
      boidsDirty |= ImGui::DragFloat("Max force", &settings._MaxForce, 0.01f, 0.01f, 20.f, "%.3f");
      if ( ImGui::DragFloat("Neighbor radius", &settings._NeighborRadius, 0.01f, 0.01f, 10.f, "%.3f") )
      {
        settings._NeighborRadius = std::max(settings._NeighborRadius, settings._SeparationRadius);
        boidsDirty = true;
      }
      if ( ImGui::DragFloat("Separation radius", &settings._SeparationRadius, 0.01f, 0.01f, 10.f, "%.3f") )
      {
        settings._SeparationRadius = std::min(settings._SeparationRadius, settings._NeighborRadius);
        boidsDirty = true;
      }
      boidsDirty |= ImGui::DragFloat("Separation", &settings._SeparationWeight, 0.01f, 0.f, 10.f, "%.3f");
      boidsDirty |= ImGui::DragFloat("Alignment", &settings._AlignmentWeight, 0.01f, 0.f, 10.f, "%.3f");
      boidsDirty |= ImGui::DragFloat("Cohesion", &settings._CohesionWeight, 0.01f, 0.f, 10.f, "%.3f");
      boidsDirty |= ImGui::DragFloat("Bounds weight", &settings._BoundsWeight, 0.01f, 0.f, 10.f, "%.3f");

      if ( boidsDirty )
      {
        settings._Count = std::max(0, settings._Count);
        settings._BoundsRadius = std::max(0.1f, settings._BoundsRadius);
        settings._BoundsHeight = std::max(0.1f, settings._BoundsHeight);
        settings._Scale = std::max(0.01f, settings._Scale);
        settings._MinSpeed = std::max(0.01f, settings._MinSpeed);
        settings._MaxSpeed = std::max(settings._MaxSpeed, settings._MinSpeed);
        settings._MaxForce = std::max(0.01f, settings._MaxForce);
        settings._NeighborRadius = std::max(0.01f, settings._NeighborRadius);
        settings._SeparationRadius = MathUtil::Clamp(settings._SeparationRadius, 0.01f, settings._NeighborRadius);
        MarkDirty();
        ioContext._ReloadScene = true;
      }
    }
  }
  else
    ImGui::TextUnformatted("No editable item selected.");

  if ( ImGui::CollapsingHeader("Player spawn") )
  {
    bool spawnDirty = false;
    spawnDirty |= ImGui::DragFloat3("Spawn position", &ioContext._Map._Player._Position.x, 0.05f, -100.f, 100.f, "%.3f");
    spawnDirty |= ImGui::DragFloat("Spawn yaw", &ioContext._Map._Player._Yaw, 0.5f, -360.f, 360.f, "%.2f");
    spawnDirty |= ImGui::DragFloat("Spawn pitch", &ioContext._Map._Player._Pitch, 0.5f, -89.f, 89.f, "%.2f");
    if ( ImGui::Button("Use current camera") )
    {
      const FpsPlayer & player = ioContext._GameWorld.GetPlayer();
      ioContext._Map._Player._Position = player._Position;
      ioContext._Map._Player._Yaw = player._Yaw;
      ioContext._Map._Player._Pitch = player._Pitch;
      spawnDirty = true;
    }
    if ( spawnDirty )
      MarkDirty();
  }

  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawEditorMaterialsPanel
// ----------------------------------------------------------------------------
void FpsGameEditor::DrawMaterialsPanel( FpsGameEditorContext & ioContext )
{
  if ( !_ShowMaterialsPanel )
    return;

  if ( !ImGui::Begin("Editor Materials", &_ShowMaterialsPanel) )
  {
    ImGui::End();
    return;
  }

  FpsGameMapLoader::SeedDefaultMaterials(ioContext._Map);
  if ( !ioContext._Map._Materials.empty() )
    _SelectedMaterial = MathUtil::Clamp(_SelectedMaterial, 0, static_cast<int>(ioContext._Map._Materials.size()) - 1);

  ImGui::InputText("New material", _NewMaterialName, sizeof(_NewMaterialName));
  ImGui::SameLine();
  if ( ImGui::Button("Add material") && std::strlen(_NewMaterialName) > 0 )
  {
    bool exists = false;
    for ( int i = 0; i < static_cast<int>(ioContext._Map._Materials.size()); ++i )
    {
      if ( ioContext._Map._Materials[i]._Name == _NewMaterialName )
      {
        _SelectedMaterial = i;
        exists = true;
        break;
      }
    }

    if ( !exists )
    {
      FpsMapMaterial material;
      material._Name = _NewMaterialName;
      material._Material._Albedo = Vec3(0.8f);
      material._Material._Roughness = 0.5f;
      ioContext._Map._Materials.push_back(material);
      _SelectedMaterial = static_cast<int>(ioContext._Map._Materials.size()) - 1;
      MarkDirty();
      ioContext._ReloadScene = true;
      SetStatus("Added material " + material._Name);
    }
  }

  if ( !ioContext._Map._Materials.empty() )
  {
    if ( ImGui::BeginListBox("Materials", ImVec2(-FLT_MIN, 150.f)) )
    {
      for ( int i = 0; i < static_cast<int>(ioContext._Map._Materials.size()); ++i )
      {
        const bool selected = ( i == _SelectedMaterial );
        if ( ImGui::Selectable(ioContext._Map._Materials[i]._Name.c_str(), selected) )
          _SelectedMaterial = i;
        if ( selected )
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndListBox();
    }

    if ( ImGui::Button("Duplicate material") )
      DuplicateSelectedMaterial(ioContext);
    ImGui::SameLine();
    if ( ImGui::Button("Delete material") )
      DeleteSelectedMaterial(ioContext);

    if ( ( _SelectedMaterial >= 0 ) && ( _SelectedMaterial < static_cast<int>(ioContext._Map._Materials.size()) ) )
    {
      FpsMapMaterial & mapMaterial = ioContext._Map._Materials[_SelectedMaterial];
      Material & material = mapMaterial._Material;
      bool materialDirty = false;
      materialDirty |= ImGui::ColorEdit3("Albedo", &material._Albedo.x);
      materialDirty |= ImGui::DragFloat("Roughness", &material._Roughness, 0.01f, 0.f, 1.f, "%.3f");
      materialDirty |= ImGui::DragFloat("Metallic", &material._Metallic, 0.01f, 0.f, 1.f, "%.3f");
      materialDirty |= ImGui::DragFloat("Reflectance", &material._Reflectance, 0.01f, 0.f, 1.f, "%.3f");
      materialDirty |= ImGui::ColorEdit3("Emission", &material._Emission.x);
      materialDirty |= ImGui::DragFloat("Opacity", &material._Opacity, 0.01f, 0.f, 1.f, "%.3f");
      int alphaMode = static_cast<int>(material._AlphaMode);
      static const char * alphaModes[] = { "Opaque", "Blend", "Mask" };
      if ( ImGui::Combo("Alpha mode", &alphaMode, alphaModes, 3) )
      {
        material._AlphaMode = static_cast<float>(alphaMode);
        materialDirty = true;
      }

      if ( materialDirty )
      {
        material._Roughness = MathUtil::Clamp(material._Roughness, 0.f, 1.f);
        material._Metallic = MathUtil::Clamp(material._Metallic, 0.f, 1.f);
        material._Reflectance = MathUtil::Clamp(material._Reflectance, 0.f, 1.f);
        material._Opacity = MathUtil::Clamp(material._Opacity, 0.f, 1.f);
        if ( ioContext._Scene )
        {
          const int materialID = ioContext._Scene -> FindMaterialID("__FpsMap_" + mapMaterial._Name);
          if ( ( materialID >= 0 ) && ( materialID < static_cast<int>(ioContext._Scene -> GetMaterials().size()) ) )
          {
            const float id = ioContext._Scene -> GetMaterials()[materialID]._ID;
            ioContext._Scene -> GetMaterials()[materialID] = material;
            ioContext._Scene -> GetMaterials()[materialID]._ID = id;
          }
        }
        if ( ioContext._Renderer )
          ioContext._Renderer -> Notify(DirtyState::SceneMaterials);
        MarkDirty();
      }
    }
  }

  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawEditorSettingsPanel
// ----------------------------------------------------------------------------
void FpsGameEditor::DrawSettingsPanel( FpsGameEditorContext & ioContext )
{
  if ( !_ShowSettingsPanel )
    return;

  if ( !ImGui::Begin("Editor Settings", &_ShowSettingsPanel) )
  {
    ImGui::End();
    return;
  }

  ImGui::Text("Mode: editor");
  ImGui::Text("Renderer: %s", FpsRendererMode::Deferred == ioContext._GameSettings._RendererMode ? "Deferred" :
                              FpsRendererMode::Software == ioContext._GameSettings._RendererMode ? "Software" : "Photo Path Tracer");
  if ( !_StatusMessage.empty() )
    ImGui::TextWrapped("Status: %s", _StatusMessage.c_str());

  ImGui::Separator();
  ImGui::Checkbox("Collider helpers", &_ShowColliderHelpers);
  ImGui::Checkbox("Light helpers", &_ShowLightHelpers);
  ImGui::Checkbox("Boids helpers", &_ShowBoidsHelpers);

  const bool hasEditableSelection = ( FpsEditableKind::Box == _Selection._Kind )
                                 || ( FpsEditableKind::Collider == _Selection._Kind )
                                 || ( FpsEditableKind::Prop == _Selection._Kind )
                                 || ( FpsEditableKind::Light == _Selection._Kind )
                                 || ( FpsEditableKind::Boids == _Selection._Kind );
  ImGui::BeginDisabled(!hasEditableSelection);
  if ( ImGui::Button("Duplicate selected") )
  {
    if ( !DuplicateSelectedItem(ioContext) )
      SetStatus("Nothing selected to duplicate");
  }
  ImGui::SameLine();
  if ( ImGui::Button("Delete selected") )
  {
    if ( !DeleteSelectedItem(ioContext) )
      SetStatus("Nothing selected to delete");
  }
  ImGui::EndDisabled();

  if ( ImGui::Button("Refresh prop list") )
  {
    RefreshPropAssets();
    SetStatus("Found " + std::to_string(_PropAssetPaths.size()) + " root asset props");
  }
  ImGui::SameLine();
  if ( ImGui::Button("Reset editor layout") )
    _ResetDockLayout = true;

  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawEditorRenderSettingsPanel
// ----------------------------------------------------------------------------
void FpsGameEditor::DrawRenderSettingsPanel( FpsGameEditorContext & ioContext )
{
  if ( !_ShowRenderSettingsPanel )
    return;

  if ( !ImGui::Begin("Editor Render Settings", &_ShowRenderSettingsPanel) )
  {
    ImGui::End();
    return;
  }

  DrawRenderSettingsUI(ioContext);

  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawEditorPerformancePanel
// ----------------------------------------------------------------------------
void FpsGameEditor::DrawPerformancePanel( FpsGameEditorContext & ioContext )
{
  if ( !_ShowPerformancePanel )
    return;

  if ( !ImGui::Begin("Editor Performance", &_ShowPerformancePanel) )
  {
    ImGui::End();
    return;
  }

  DrawPerformanceUI(ioContext);

  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawRenderSettingsUI
// ----------------------------------------------------------------------------
int FpsGameEditor::DrawRenderSettingsUI( FpsGameEditorContext & ioContext )
{
  if ( !ioContext._Renderer )
  {
    ImGui::Text("No renderer");
    return 0;
  }

  int renderScale = ioContext._Settings._RenderScale;
  if ( ImGui::SliderInt("Render scale", &renderScale, 25, 150) )
  {
    ioContext._Settings._RenderScale = renderScale;
    EditorSyncFramebufferResolution(ioContext, true);
  }

  if ( ImGui::Checkbox("Show lights", &ioContext._Settings._ShowLights) )
    ioContext._Renderer -> Notify(DirtyState::SceneLights);

  if ( ImGui::Checkbox("Tone mapping", &ioContext._Settings._ToneMapping) )
    ioContext._Renderer -> Notify(DirtyState::RenderSettings);

  if ( ioContext._Settings._ToneMapping )
  {
    if ( ImGui::SliderFloat("Gamma", &ioContext._Settings._Gamma, 0.5f, 3.f) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("Exposure", &ioContext._Settings._Exposure, 0.1f, 5.f) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
  }

  ImGui::Separator();

  if ( FpsRendererMode::Deferred == ioContext._GameSettings._RendererMode )
  {
    if ( ImGui::Checkbox("Shadow mapping", &ioContext._Settings._ShadowMapping) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    int shadowMapResolution = ioContext._Settings._ShadowMapResolution;
    if ( ImGui::SliderInt( "Shadow map resolution", &shadowMapResolution, 256, 4096 ) )
    {
      shadowMapResolution = std::max(256, ( shadowMapResolution / 64 ) * 64);
      ioContext._Settings._ShadowMapResolution = shadowMapResolution;
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    }
    if ( ImGui::SliderFloat( "Shadow bias", &ioContext._Settings._ShadowBias, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic ) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);

    if ( ImGui::Checkbox("SSAO", &ioContext._Settings._SSAO) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::Checkbox("SSAO blur", &ioContext._Settings._SSAOBlur) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("SSAO radius", &ioContext._Settings._SSAORadius, 0.05f, 5.f, "%.3f", ImGuiSliderFlags_Logarithmic) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("SSAO bias", &ioContext._Settings._SSAOBias, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("SSAO intensity", &ioContext._Settings._SSAOIntensity, 0.f, 3.f) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    int ssaoKernelSize = ioContext._Settings._SSAOKernelSize;
    if ( ImGui::SliderInt("SSAO kernel size", &ssaoKernelSize, 4, 32) )
    {
      ioContext._Settings._SSAOKernelSize = std::max(4, std::min(32, ssaoKernelSize));
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    }
    if ( ImGui::SliderInt("Max shadow casting lights", &ioContext._Settings._MaxShadowCastingLights, 1, 8) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);

    if ( ImGui::Checkbox("SSR", &ioContext._Settings._SSR) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("SSR intensity", &ioContext._Settings._SSRIntensity, 0.f, 2.f) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("SSR max roughness", &ioContext._Settings._SSRMaxRoughness, 0.05f, 1.f) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    int ssrMaxSteps = ioContext._Settings._SSRMaxSteps;
    if ( ImGui::SliderInt("SSR max steps", &ssrMaxSteps, 4, 128) )
    {
      ioContext._Settings._SSRMaxSteps = std::max(4, std::min(128, ssrMaxSteps));
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    }
    if ( ImGui::SliderFloat("SSR step size", &ioContext._Settings._SSRStepSize, 0.01f, 1.f, "%.3f", ImGuiSliderFlags_Logarithmic) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("SSR max distance", &ioContext._Settings._SSRMaxDistance, 1.f, 100.f) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("SSR thickness", &ioContext._Settings._SSRThickness, 0.01f, 2.f, "%.3f", ImGuiSliderFlags_Logarithmic) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("SSR edge fade", &ioContext._Settings._SSRFade, 0.01f, 0.5f) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);

    if ( ImGui::Checkbox("PBR direct lighting", &ioContext._Settings._PBRDirectLighting) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("Direct light intensity", &ioContext._Settings._DirectLightIntensity, 0.f, 8.f) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderFloat("IBL max roughness", &ioContext._Settings._SpecularIBLMaxRoughness, 0.05f, 1.f) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);

    static const char * DEBUG_VIEWS[] = { "Color", "Depth", "Normals", "Shadows", "SSAO", "Specular IBL", "Material Params", "SSR", "Direct diffuse", "Direct specular" };
    if ( ImGui::Combo("Debug view", &ioContext._DeferredDebugView, DEBUG_VIEWS, 10) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::Checkbox("Show wires", &ioContext._DeferredShowWires) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);

    ioContext._DebugMode = 0;
    ioContext._Settings._ShowShadowMap = ( 3 == ioContext._DeferredDebugView );
    if ( 1 == ioContext._DeferredDebugView )
      ioContext._DebugMode |= (int)DeferredDebugModes::DepthBuffer;
    else if ( 2 == ioContext._DeferredDebugView )
      ioContext._DebugMode |= (int)DeferredDebugModes::Normals;
    else if ( 3 == ioContext._DeferredDebugView )
      ioContext._DebugMode |= (int)DeferredDebugModes::Shadows;
    else if ( 4 == ioContext._DeferredDebugView )
      ioContext._DebugMode |= (int)DeferredDebugModes::SSAO;
    else if ( 5 == ioContext._DeferredDebugView )
      ioContext._DebugMode |= (int)DeferredDebugModes::SpecularIBL;
    else if ( 6 == ioContext._DeferredDebugView )
      ioContext._DebugMode |= (int)DeferredDebugModes::MaterialParams;
    else if ( 7 == ioContext._DeferredDebugView )
      ioContext._DebugMode |= (int)DeferredDebugModes::SSR;
    else if ( 8 == ioContext._DeferredDebugView )
      ioContext._DebugMode |= (int)DeferredDebugModes::DirectDiffuse;
    else if ( 9 == ioContext._DeferredDebugView )
      ioContext._DebugMode |= (int)DeferredDebugModes::DirectSpecular;

    if ( ioContext._DeferredShowWires )
      ioContext._DebugMode |= (int)DeferredDebugModes::Wires;
    ioContext._Renderer -> SetDebugMode(ioContext._DebugMode);
  }
  else if ( FpsRendererMode::Software == ioContext._GameSettings._RendererMode )
  {
    const unsigned int nbThreadsMax = std::max(1u, std::thread::hardware_concurrency());
    int numThreads = (int)ioContext._Settings._NbThreads;
    if ( ImGui::SliderInt("Nb threads", &numThreads, 1, (int)nbThreadsMax) )
    {
      ioContext._Settings._NbThreads = std::max(1, numThreads);
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    }

    static const char * DEBUG_VIEWS[] = { "Color", "Depth", "Normals" };
    if ( ImGui::Combo("Debug view", &ioContext._SoftwareDebugView, DEBUG_VIEWS, 3) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::Checkbox("Show wires", &ioContext._SoftwareShowWires) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);

    ioContext._DebugMode = 0;
    if ( 1 == ioContext._SoftwareDebugView )
      ioContext._DebugMode |= (int)RasterDebugModes::DepthBuffer;
    else if ( 2 == ioContext._SoftwareDebugView )
      ioContext._DebugMode |= (int)RasterDebugModes::Normals;
    if ( ioContext._SoftwareShowWires )
      ioContext._DebugMode |= (int)RasterDebugModes::Wires;
    ioContext._Renderer -> SetDebugMode(ioContext._DebugMode);
  }
  else if ( FpsRendererMode::PhotoPathTracer == ioContext._GameSettings._RendererMode )
  {
    if ( ImGui::SliderInt("Bounces", &ioContext._Settings._Bounces, 1, 8) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::SliderInt("SPP", &ioContext._Settings._NbSamplesPerPixel, 1, 8) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    if ( ImGui::Checkbox("Denoise", &ioContext._Settings._Denoise) )
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);

    static const char * DEBUG_VIEWS[] = { "Off", "Tiles", "Albedo", "Metalness", "Roughness", "Normals", "UV", "BLAS" };
    if ( ImGui::Combo("Debug view", &ioContext._DebugMode, DEBUG_VIEWS, 8) )
    {
      ioContext._Renderer -> SetDebugMode(ioContext._DebugMode);
      ioContext._Renderer -> Notify(DirtyState::RenderSettings);
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawPerformanceUI
// ----------------------------------------------------------------------------
int FpsGameEditor::DrawPerformanceUI( FpsGameEditorContext & ioContext )
{
  RenderStatsUI::DrawFrameRateGraph(_RenderStatsState, ioContext._FrameRate, ioContext._DeltaTime, ioContext._NbRenderedFrames);
  RenderStatsUI::DrawRenderOverview(ioContext._Settings, ioContext._FrameTime, ioContext._NbRenderedFrames);

  if ( !ioContext._CpuTimings.empty() )
  {
    double cpuFrameTotal = 0.;
    ImGui::Separator();
    ImGui::Text("CPU frame");
    for ( const FpsCpuTiming & timing : ioContext._CpuTimings )
    {
      if ( timing._Enabled )
      {
        ImGui::Text("%-24s : %.3f ms [CPU]", timing._Name, timing._Seconds * 1000.);
        cpuFrameTotal += timing._Seconds;
      }
      else
        ImGui::Text("%-24s : -- [CPU]", timing._Name);
    }
    ImGui::Text("CPU frame total       : %.3f ms", cpuFrameTotal * 1000.);
  }

  RenderStatsUI::DrawRenderPassTimings(ioContext._Renderer);
  RenderStatsUI::DrawPathTracerStats(ioContext._Renderer);
  RenderStatsUI::DrawSceneStats(ioContext._Scene);

  return 0;
}

// ----------------------------------------------------------------------------
// DrawEditorOverlays
// ----------------------------------------------------------------------------
int FpsGameEditor::DrawOverlays( FpsGameEditorContext & ioContext )
{
  if ( !_Enabled || !ioContext._Scene )
    return 0;

  Mat4x4 view(1.f);
  Mat4x4 proj(1.f);

  Camera & camera = ioContext._Scene -> GetCamera();
  camera.ComputeLookAtMatrix(view);

  const float width  = static_cast<float>(std::max(1, ioContext._Settings._WindowResolution.x));
  const float height = static_cast<float>(std::max(1, ioContext._Settings._WindowResolution.y));
  camera.ComputePerspectiveProjMatrix(width / height, proj);

  ImGuiIO & io = ImGui::GetIO();
  ImDrawList * drawList = ImGui::GetForegroundDrawList();

  const ImU32 selectedColor = IM_COL32(255, 184, 48, 245);
  const ImU32 colliderColor = IM_COL32(74, 202, 255, 190);
  const ImU32 hiddenColor = IM_COL32(180, 180, 180, 170);
  const ImU32 lightColor = IM_COL32(255, 232, 96, 230);
  const ImU32 boidsColor = IM_COL32(98, 224, 140, 220);

  for ( int i = 0; i < static_cast<int>(ioContext._Map._Objects.size()); ++i )
  {
    const FpsSceneObject & object = ioContext._Map._Objects[i];
    const bool selected = ( ( FpsEditableKind::Box == _Selection._Kind )
                         || ( FpsEditableKind::Collider == _Selection._Kind ) )
                       && ( _Selection._Index == i );
    const bool collider = !object._Visible;
    const bool runtimeHidden = object._Visible
                            && ( i < static_cast<int>(_ObjectInstanceVisible.size()) )
                            && !_ObjectInstanceVisible[i];

    if ( !selected && !collider )
      continue;
    if ( collider && !_ShowColliderHelpers && !selected )
      continue;

    ImU32 color = selected ? selectedColor : colliderColor;
    if ( runtimeHidden && !selected )
      color = hiddenColor;

    EditorDrawTransformedBox(EditorObjectTransform(object), view, proj, drawList, color, selected ? 2.5f : 1.5f);
  }

  if ( FpsEditableKind::Prop == _Selection._Kind )
  {
    const int propIndex = _Selection._Index;
    const std::vector<int> * instanceIDs = ioContext._SceneBinding.GetPropInstanceIDs(propIndex);
    if ( instanceIDs )
    {
      const std::vector<MeshInstance> & meshInstances = ioContext._Scene -> GetMeshInstances();
      const std::vector<Mesh*> & meshes = ioContext._Scene -> GetMeshes();
      for ( int instanceID : *instanceIDs )
      {
        if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(meshInstances.size()) ) )
          continue;

        const MeshInstance & instance = meshInstances[instanceID];
        if ( !instance._Visible )
          continue;
        if ( ( instance._MeshID < 0 ) || ( instance._MeshID >= static_cast<int>(meshes.size()) ) )
          continue;

        Mesh * mesh = meshes[instance._MeshID];
        if ( mesh )
          EditorDrawTransformedAABB(mesh -> GetBoundingBox(), instance._Transform, view, proj, drawList, selectedColor, 2.5f);
      }
    }
  }

  for ( int i = 0; i < static_cast<int>(ioContext._Map._Lights.size()); ++i )
  {
    const Light & light = ioContext._Map._Lights[i];
    const bool selected = ( FpsEditableKind::Light == _Selection._Kind ) && ( _Selection._Index == i );
    const LightType type = (LightType)(int)light._Type;
    if ( LightType::DistantLight == type )
      continue;
    if ( !_ShowLightHelpers && !selected )
      continue;

    ImVec2 screenPos;
    if ( !EditorProjectPoint(light._Pos, view, proj, io.DisplaySize, screenPos) )
      continue;

    const float radius = selected ? 8.f : 6.f;
    const ImU32 color = selected ? selectedColor : lightColor;
    drawList -> AddCircle(screenPos, radius, color, 20, selected ? 2.5f : 1.8f);
    drawList -> AddLine(ImVec2(screenPos.x - radius - 3.f, screenPos.y), ImVec2(screenPos.x - 2.f, screenPos.y), color, 1.5f);
    drawList -> AddLine(ImVec2(screenPos.x + 2.f, screenPos.y), ImVec2(screenPos.x + radius + 3.f, screenPos.y), color, 1.5f);
    drawList -> AddLine(ImVec2(screenPos.x, screenPos.y - radius - 3.f), ImVec2(screenPos.x, screenPos.y - 2.f), color, 1.5f);
    drawList -> AddLine(ImVec2(screenPos.x, screenPos.y + 2.f), ImVec2(screenPos.x, screenPos.y + radius + 3.f), color, 1.5f);
  }

  for ( int i = 0; i < static_cast<int>(ioContext._Map._Boids.size()); ++i )
  {
    const FpsMapBoids & boids = ioContext._Map._Boids[i];
    const bool selected = ( FpsEditableKind::Boids == _Selection._Kind ) && ( _Selection._Index == i );
    if ( !_ShowBoidsHelpers && !selected )
      continue;
    if ( !boids._Visible && !selected )
      continue;

    const ImU32 color = selected ? selectedColor : boidsColor;
    EditorDrawTransformedBox(EditorBoidsTransform(boids), view, proj, drawList, color, selected ? 2.5f : 1.5f);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawEditorGizmo
// ----------------------------------------------------------------------------
int FpsGameEditor::DrawGizmo( FpsGameEditorContext & ioContext )
{
  if ( !_Enabled || !ioContext._Scene || !ioContext._Renderer )
    return 0;

  Mat4x4 view(1.f);
  Mat4x4 proj(1.f);

  Camera & camera = ioContext._Scene -> GetCamera();
  camera.ComputeLookAtMatrix(view);

  const float width  = static_cast<float>(std::max(1, ioContext._Settings._WindowResolution.x));
  const float height = static_cast<float>(std::max(1, ioContext._Settings._WindowResolution.y));
  camera.ComputePerspectiveProjMatrix(width / height, proj);

  ImGuiIO & io = ImGui::GetIO();
  ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
  ImGuizmo::SetRect(0.f, 0.f, io.DisplaySize.x, io.DisplaySize.y);
  ImGuizmo::SetOrthographic(false);
  const bool scaleMode = ioContext._KeyInput.IsKeyDown(GLFW_KEY_LEFT_ALT) || ioContext._KeyInput.IsKeyDown(GLFW_KEY_RIGHT_ALT);
  const bool rotateMode = ioContext._KeyInput.IsKeyDown(GLFW_KEY_LEFT_SHIFT) || ioContext._KeyInput.IsKeyDown(GLFW_KEY_RIGHT_SHIFT);
  const ImGuizmo::OPERATION operation = scaleMode ? ImGuizmo::SCALE : ( rotateMode ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE );

  if ( ( FpsEditableKind::Box == _Selection._Kind )
    || ( FpsEditableKind::Collider == _Selection._Kind ) )
  {
    const int index = _Selection._Index;
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Objects.size()) ) )
      return 0;

    FpsSceneObject & object = ioContext._Map._Objects[index];
    Mat4x4 transform = ( ImGuizmo::ROTATE == operation ) ? EditorObjectGizmoTransform(object) : EditorObjectTransform(object);
    if ( ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                              operation, ImGuizmo::WORLD,
                              glm::value_ptr(transform)) )
    {
      if ( ImGuizmo::ROTATE == operation )
      {
        object._Center = Vec3(transform[3]);
        object._Orientation = EditorVec4FromQuat(glm::quat_cast(glm::mat3(transform)));
        object._Rotation = EditorEulerFromMatrix(transform);
      }
      else if ( ImGuizmo::SCALE == operation )
      {
        object._Center = Vec3(transform[3]);
        object._HalfExtents.x = std::max(0.01f, glm::length(Vec3(transform[0])) * 0.5f);
        object._HalfExtents.y = std::max(0.01f, glm::length(Vec3(transform[1])) * 0.5f);
        object._HalfExtents.z = std::max(0.01f, glm::length(Vec3(transform[2])) * 0.5f);
      }
      else
        object._Center = Vec3(transform[3]);
      SyncObject(ioContext, index);
      MarkDirty();
    }
  }
  else if ( FpsEditableKind::Prop == _Selection._Kind )
  {
    const int index = _Selection._Index;
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Props.size()) ) )
      return 0;

    FpsMapProp & prop = ioContext._Map._Props[index];
    Mat4x4 transform = ( ImGuizmo::ROTATE == operation ) ? EditorPropGizmoTransform(prop) : EditorPropTransform(prop);
    if ( ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                              operation, ImGuizmo::WORLD,
                              glm::value_ptr(transform)) )
    {
      if ( ImGuizmo::ROTATE == operation )
      {
        prop._Position = Vec3(transform[3]);
        prop._Rotation = EditorEulerFromMatrix(transform);
      }
      else if ( ImGuizmo::SCALE == operation )
      {
        prop._Position = Vec3(transform[3]);
        prop._Scale.x = std::max(0.01f, glm::length(Vec3(transform[0])));
        prop._Scale.y = std::max(0.01f, glm::length(Vec3(transform[1])));
        prop._Scale.z = std::max(0.01f, glm::length(Vec3(transform[2])));
      }
      else
        prop._Position = Vec3(transform[3]);

      SyncProp(ioContext, index);
      MarkDirty();
    }
  }
  else if ( FpsEditableKind::Light == _Selection._Kind )
  {
    const int index = _Selection._Index;
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Lights.size()) ) )
      return 0;

    Light & light = ioContext._Map._Lights[index];
    if ( LightType::DistantLight == (LightType)(int)light._Type )
      return 0;
    if ( scaleMode )
      return 0;
    if ( rotateMode )
      return 0;

    Mat4x4 transform(1.f);
    transform[3] = Vec4(light._Pos, 1.f);
    if ( ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                              ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                              glm::value_ptr(transform)) )
    {
      light._Pos = Vec3(transform[3]);
      SyncLight(ioContext, index);
      MarkDirty();
    }
  }
  else if ( FpsEditableKind::Boids == _Selection._Kind )
  {
    const int index = _Selection._Index;
    if ( ( index < 0 ) || ( index >= static_cast<int>(ioContext._Map._Boids.size()) ) )
      return 0;
    if ( rotateMode )
      return 0;

    FpsMapBoids & boids = ioContext._Map._Boids[index];
    BoidSettings & settings = boids._Settings;
    Mat4x4 transform = scaleMode ? EditorBoidsTransform(boids) : EditorBoidsGizmoTransform(boids);
    if ( ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                              scaleMode ? ImGuizmo::SCALE : ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                              glm::value_ptr(transform)) )
    {
      settings._BoundsCenter = Vec3(transform[3]);
      if ( scaleMode )
      {
        const float radiusX = glm::length(Vec3(transform[0])) * 0.5f;
        const float radiusZ = glm::length(Vec3(transform[2])) * 0.5f;
        settings._BoundsRadius = std::max(0.1f, ( radiusX + radiusZ ) * 0.5f);
        settings._BoundsHeight = std::max(0.1f, glm::length(Vec3(transform[1])));
      }
      MarkDirty();
      ioContext._ReloadScene = true;
    }
  }

  return 0;
}


}
