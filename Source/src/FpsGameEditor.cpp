#pragma warning(disable : 4100) // unreferenced formal parameter

#include "FpsGameEditor.h"

#include "FpsGameMap.h"
#include "Mesh.h"
#include "MeshInstance.h"
#include "PathUtils.h"
#include "Renderer.h"
#include "Scene.h"

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
                                            bool & iReloadScene )
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

static bool EditorIsBuiltinMaterialName( const std::string & iName )
{
  return ( "floor" == iName )
      || ( "wall" == iName )
      || ( "pillar" == iName )
      || ( "crate" == iName )
      || ( "accent" == iName );
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

  const bool hasEditableSelection = ( FpsEditableKind::Box == _Selection._Kind )
                                 || ( FpsEditableKind::Collider == _Selection._Kind )
                                 || ( FpsEditableKind::Prop == _Selection._Kind )
                                 || ( FpsEditableKind::Light == _Selection._Kind );
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

  return 0;
}


}
