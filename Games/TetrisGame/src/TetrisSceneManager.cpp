#include "TetrisSceneManager.h"

#include "Camera.h"
#include "Light.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshInstance.h"
#include "PathUtils.h"
#include "ProceduralMesh.h"
#include "Scene.h"

#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Local Mesh Builders
// ----------------------------------------------------------------------------
struct MeshBuilder
{
  std::vector<Vec3>  _Vertices;
  std::vector<Vec3>  _Normals;
  std::vector<Vec2>  _UVs;
  std::vector<Vec3i> _Indices;

  int AddNormal( const Vec3 & iNorm )
  {
    _Normals.push_back(glm::normalize(iNorm));
    return static_cast<int>(_Normals.size()) - 1;
  }

  int AddUV( const Vec2 & iUV )
  {
    _UVs.push_back(iUV);
    return static_cast<int>(_UVs.size()) - 1;
  }

  int AddVertex( const Vec3 & iPos )
  {
    _Vertices.push_back(iPos);
    return static_cast<int>(_Vertices.size()) - 1;
  }

  void AddQuad( const Vec3 & iA, const Vec3 & iB, const Vec3 & iC, const Vec3 & iD, const Vec3 & iNorm )
  {
    const int normIdx = AddNormal(iNorm);
    const int uv0 = AddUV(Vec2(0.f, 0.f));
    const int uv1 = AddUV(Vec2(1.f, 0.f));
    const int uv2 = AddUV(Vec2(1.f, 1.f));
    const int uv3 = AddUV(Vec2(0.f, 1.f));

    const int v0 = AddVertex(iA);
    const int v1 = AddVertex(iB);
    const int v2 = AddVertex(iC);
    const int v3 = AddVertex(iD);

    // CCW triangle 1
    _Indices.push_back(Vec3i(v0, normIdx, uv0));
    _Indices.push_back(Vec3i(v1, normIdx, uv1));
    _Indices.push_back(Vec3i(v2, normIdx, uv2));

    // CCW triangle 2
    _Indices.push_back(Vec3i(v0, normIdx, uv0));
    _Indices.push_back(Vec3i(v2, normIdx, uv2));
    _Indices.push_back(Vec3i(v3, normIdx, uv3));
  }

  void AddBox( const Vec3 & iMin, const Vec3 & iMax )
  {
    // +X (Right)
    AddQuad(Vec3(iMax.x, iMin.y, iMin.z), Vec3(iMax.x, iMax.y, iMin.z), Vec3(iMax.x, iMax.y, iMax.z), Vec3(iMax.x, iMin.y, iMax.z), Vec3(1.f, 0.f, 0.f));
    // -X (Left)
    AddQuad(Vec3(iMin.x, iMin.y, iMax.z), Vec3(iMin.x, iMax.y, iMax.z), Vec3(iMin.x, iMax.y, iMin.z), Vec3(iMin.x, iMin.y, iMin.z), Vec3(-1.f, 0.f, 0.f));
    // +Y (Top)
    AddQuad(Vec3(iMin.x, iMax.y, iMin.z), Vec3(iMin.x, iMax.y, iMax.z), Vec3(iMax.x, iMax.y, iMax.z), Vec3(iMax.x, iMax.y, iMin.z), Vec3(0.f, 1.f, 0.f));
    // -Y (Bottom)
    AddQuad(Vec3(iMin.x, iMin.y, iMax.z), Vec3(iMin.x, iMin.y, iMin.z), Vec3(iMax.x, iMin.y, iMin.z), Vec3(iMax.x, iMin.y, iMax.z), Vec3(0.f, -1.f, 0.f));
    // +Z (Front)
    AddQuad(Vec3(iMin.x, iMin.y, iMax.z), Vec3(iMax.x, iMin.y, iMax.z), Vec3(iMax.x, iMax.y, iMax.z), Vec3(iMin.x, iMax.y, iMax.z), Vec3(0.f, 0.f, 1.f));
    // -Z (Back)
    AddQuad(Vec3(iMax.x, iMin.y, iMin.z), Vec3(iMin.x, iMin.y, iMin.z), Vec3(iMin.x, iMax.y, iMin.z), Vec3(iMax.x, iMax.y, iMin.z), Vec3(0.f, 0.f, -1.f));
  }
};

// ----------------------------------------------------------------------------
// CellToWorld
// ----------------------------------------------------------------------------
Vec3 TetrisSceneManager::CellToWorld( int iX, int iY )
{
  // 10x20 visible board centered horizontally around X = 0: x in [0..9] -> (x - 4.5f)
  // Rows y in [0..19] -> (y + 0.5f) so row 0 base rests at Y = 0.f
  return Vec3(static_cast<float>(iX) - 4.5f, static_cast<float>(iY) + 0.5f, 0.0f);
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
TetrisSceneManager::TetrisSceneManager()
{
  for ( int i = 0; i < (int)TetrisPieceType::Count; ++i )
    _PieceMaterialIDs[i] = -1;
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
TetrisSceneManager::~TetrisSceneManager()
{
}

// ----------------------------------------------------------------------------
// CreateGhostCageMesh
// ----------------------------------------------------------------------------
Mesh * TetrisSceneManager::CreateGhostCageMesh( const std::string & iName, float iSize, float iThickness )
{
  MeshBuilder builder;
  const float h = iSize * 0.5f;
  const float t = std::max(0.01f, iThickness);

  // 12 rectangular border prisms forming a hollow cube cage
  // 4 edges parallel to X
  builder.AddBox(Vec3(-h,  h - t,  h - t), Vec3( h,  h,  h));
  builder.AddBox(Vec3(-h, -h,      h - t), Vec3( h, -h + t,  h));
  builder.AddBox(Vec3(-h,  h - t, -h),     Vec3( h,  h, -h + t));
  builder.AddBox(Vec3(-h, -h,     -h),     Vec3( h, -h + t, -h + t));

  // 4 edges parallel to Y
  builder.AddBox(Vec3(-h,     -h,  h - t), Vec3(-h + t,  h,  h));
  builder.AddBox(Vec3( h - t, -h,  h - t), Vec3( h,      h,  h));
  builder.AddBox(Vec3(-h,     -h, -h),     Vec3(-h + t,  h, -h + t));
  builder.AddBox(Vec3( h - t, -h, -h),     Vec3( h,      h, -h + t));

  // 4 edges parallel to Z
  builder.AddBox(Vec3(-h,      h - t, -h), Vec3(-h + t,  h,  h));
  builder.AddBox(Vec3( h - t,  h - t, -h), Vec3( h,      h,  h));
  builder.AddBox(Vec3(-h,     -h,     -h), Vec3(-h + t, -h + t,  h));
  builder.AddBox(Vec3( h - t, -h,     -h), Vec3( h,     -h + t,  h));

  return new Mesh(iName, builder._Vertices, builder._Normals, builder._UVs, builder._Indices);
}

// ----------------------------------------------------------------------------
// CreateEnclosureMesh
// ----------------------------------------------------------------------------
Mesh * TetrisSceneManager::CreateEnclosureMesh( const std::string & iName )
{
  MeshBuilder builder;

  // Left Pillar
  builder.AddBox(Vec3(-5.65f, -0.6f, -0.8f), Vec3(-5.0f, 21.6f, 0.7f));
  // Right Pillar
  builder.AddBox(Vec3( 5.0f,  -0.6f, -0.8f), Vec3( 5.65f, 21.6f, 0.7f));
  // Bottom Base
  builder.AddBox(Vec3(-5.65f, -1.2f, -0.8f), Vec3( 5.65f,  0.0f, 0.7f));
  // Top Cap
  builder.AddBox(Vec3(-5.65f, 21.4f, -0.8f), Vec3( 5.65f, 22.0f, 0.7f));

  // Hold queue lower shelf trim
  builder.AddBox(Vec3(-10.2f, 13.3f, -0.7f), Vec3(-5.8f, 13.6f, 0.3f));

  // Next queue lower shelf trim
  builder.AddBox(Vec3( 5.8f,  4.2f, -0.7f),  Vec3(10.2f,  4.5f, 0.3f));

  return new Mesh(iName, builder._Vertices, builder._Normals, builder._UVs, builder._Indices);
}

// ----------------------------------------------------------------------------
// CreateBackplateMesh
// ----------------------------------------------------------------------------
Mesh * TetrisSceneManager::CreateBackplateMesh( const std::string & iName )
{
  MeshBuilder builder;

  // Deep dark backplate behind the matrix (extending up to buffer zone)
  builder.AddBox(Vec3(-5.0f, 0.0f, -0.65f), Vec3(5.0f, 21.4f, -0.55f));

  // Hold queue backing shelf
  builder.AddBox(Vec3(-10.1f, 13.5f, -0.65f), Vec3(-5.9f, 19.5f, -0.55f));

  // Next queue backing shelf
  builder.AddBox(Vec3( 5.9f,  4.4f, -0.65f),  Vec3(10.1f, 19.5f, -0.55f));

  return new Mesh(iName, builder._Vertices, builder._Normals, builder._UVs, builder._Indices);
}

// ----------------------------------------------------------------------------
// CreateSkylineMesh
// ----------------------------------------------------------------------------
Mesh * TetrisSceneManager::CreateSkylineMesh( const std::string & iName )
{
  MeshBuilder builder;

  // Thin glowing Skyline indicator bar at row 20
  builder.AddBox(Vec3(-5.0f, 19.95f, -0.2f), Vec3(5.0f, 20.05f, 0.45f));

  return new Mesh(iName, builder._Vertices, builder._Normals, builder._UVs, builder._Indices);
}

// ----------------------------------------------------------------------------
// SetupMaterials
// ----------------------------------------------------------------------------
int TetrisSceneManager::SetupMaterials( Scene & ioScene )
{
  for ( int i = 1; i < (int)TetrisPieceType::Count; ++i )
  {
    const TetrisPieceType pieceType = static_cast<TetrisPieceType>(i);
    const Vec3 col = TetrisPiece::GetColor(pieceType);

    Material mat;
    mat._Albedo = col;
    mat._Roughness = 0.15f;
    mat._Clearcoat = 1.0f;
    mat._ClearcoatGloss = 0.95f;
    mat._Emission = col * 0.18f;

    _PieceMaterialIDs[i] = ioScene.AddMaterial(mat, std::string("__TetrisPiece_") + TetrisPiece::GetName(pieceType));
    if ( _PieceMaterialIDs[i] < 0 )
      return 1;
  }

  // Ghost Material: crisp tinted emissive frame
  {
    Material ghostMat;
    ghostMat._Albedo = Vec3(0.85f, 0.92f, 1.0f);
    ghostMat._Roughness = 0.10f;
    ghostMat._Clearcoat = 1.0f;
    ghostMat._Emission = Vec3(0.55f, 0.75f, 1.0f) * 0.6f;
    _GhostMaterialID = ioScene.AddMaterial(ghostMat, "__Tetris_Ghost");
  }

  // Chassis Material
  {
    Material chassis;
    chassis._Albedo = Vec3(0.12f, 0.14f, 0.18f);
    chassis._Metallic = 0.85f;
    chassis._Roughness = 0.28f;
    chassis._Reflectance = 0.75f;
    _ChassisMaterialID = ioScene.AddMaterial(chassis, "__Tetris_Chassis");
  }

  // Backplate Material
  {
    Material backplate;
    backplate._Albedo = Vec3(0.025f, 0.028f, 0.035f);
    backplate._Roughness = 0.18f;
    backplate._Metallic = 0.35f;
    _BackplateMaterialID = ioScene.AddMaterial(backplate, "__Tetris_Backplate");
  }

  // Skyline indicator
  {
    Material skyline;
    skyline._Albedo = Vec3(0.95f, 0.95f, 1.0f);
    skyline._Emission = Vec3(1.2f, 1.4f, 2.0f);
    _SkylineMaterialID = ioScene.AddMaterial(skyline, "__Tetris_Skyline");
  }

  return 0;
}

// ----------------------------------------------------------------------------
// SetMaterialPreset
// ----------------------------------------------------------------------------
void TetrisSceneManager::SetMaterialPreset( Scene & ioScene, TetrisMaterialPreset iPreset )
{
  _MaterialPreset = iPreset;
  std::vector<Material> & materials = ioScene.GetMaterials();

  for ( int i = 1; i < (int)TetrisPieceType::Count; ++i )
  {
    const int matID = _PieceMaterialIDs[i];
    if ( ( matID < 0 ) || ( matID >= static_cast<int>(materials.size()) ) )
      continue;

    const Vec3 col = TetrisPiece::GetColor(static_cast<TetrisPieceType>(i));
    Material & mat = materials[matID];

    switch ( _MaterialPreset )
    {
      case TetrisMaterialPreset::GlossyPlastic:
        mat._Albedo = col;
        mat._Roughness = 0.15f;
        mat._Metallic = 0.0f;
        mat._Clearcoat = 1.0f;
        mat._ClearcoatGloss = 0.95f;
        mat._SpecTrans = 0.0f;
        mat._Emission = col * 0.15f;
        break;

      case TetrisMaterialPreset::CrystalGlass:
        mat._Albedo = col * 0.1f;
        mat._Roughness = 0.04f;
        mat._Metallic = 0.0f;
        mat._Clearcoat = 1.0f;
        mat._ClearcoatGloss = 0.98f;
        mat._SpecTrans = 0.82f;
        mat._IOR = 1.52f;
        mat._MediumType = static_cast<float>(MediumType::Absorb);
        mat._MediumColor = col;
        mat._MediumDensity = 2.0f;
        mat._Emission = col * 0.05f;
        break;

      case TetrisMaterialPreset::CyberpunkNeon:
        mat._Albedo = col * 0.4f;
        mat._Roughness = 0.22f;
        mat._Metallic = 0.65f;
        mat._Clearcoat = 0.6f;
        mat._ClearcoatGloss = 0.8f;
        mat._SpecTrans = 0.0f;
        mat._Emission = col * 2.2f;
        break;

      case TetrisMaterialPreset::Metallic:
        mat._Albedo = col * 0.9f;
        mat._Roughness = 0.22f;
        mat._Metallic = 0.92f;
        mat._Clearcoat = 0.3f;
        mat._ClearcoatGloss = 0.7f;
        mat._Reflectance = 0.85f;
        mat._SpecTrans = 0.0f;
        mat._Emission = Vec3(0.0f);
        break;
    }
  }
}

// ----------------------------------------------------------------------------
// SetupLights
// ----------------------------------------------------------------------------
int TetrisSceneManager::SetupLights( Scene & ioScene )
{
  // 1. Key Directional Light
  Light sun;
  sun._Type = static_cast<float>(LightType::DistantLight);
  sun._Pos = Vec3(-0.35f, 0.85f, 0.40f);
  sun._Emission = Vec3(1.0f, 0.96f, 0.90f);
  sun._Intensity = 2.2f;
  sun._CastShadow = true;
  ioScene.AddLight(sun);

  // 2. Active Piece Dynamic Light (SphereLight parented in front of the active tetromino)
  Light pieceLight;
  pieceLight._Type = static_cast<float>(LightType::SphereLight);
  pieceLight._Pos = Vec3(0.0f, 10.0f, 1.2f);
  pieceLight._Emission = Vec3(0.0f, 0.94f, 0.94f);
  pieceLight._Intensity = 18.0f;
  pieceLight._Radius = 0.35f;
  pieceLight._Area = 4.0f * static_cast<float>(M_PI) * pieceLight._Radius * pieceLight._Radius;
  pieceLight._CastShadow = true;
  pieceLight._ShadowRadius = 8.0f;
  _ActivePieceLightIdx = ioScene.GetNbLights();
  ioScene.AddLight(pieceLight);

  // 3. Impact Flash Light (Hard drop & line clear flare)
  Light flashLight;
  flashLight._Type = static_cast<float>(LightType::SphereLight);
  flashLight._Pos = Vec3(0.0f, 10.0f, 2.5f);
  flashLight._Emission = Vec3(1.0f, 1.0f, 1.0f);
  flashLight._Intensity = 0.0f;
  flashLight._Radius = 0.5f;
  flashLight._Area = 4.0f * static_cast<float>(M_PI) * flashLight._Radius * flashLight._Radius;
  flashLight._CastShadow = false;
  _LineClearLightIdx = ioScene.GetNbLights();
  ioScene.AddLight(flashLight);

  return 0;
}

// ----------------------------------------------------------------------------
// SetupInstances
// ----------------------------------------------------------------------------
int TetrisSceneManager::SetupInstances( Scene & ioScene )
{
  const Mat4x4 hiddenTransform = glm::translate(Vec3(0.f, -100.f, 0.f)) * glm::scale(Vec3(0.001f));
  const int defaultMatID = _PieceMaterialIDs[(int)TetrisPieceType::I];

  // 1. Matrix Pool: 10 x 22 = 220 instances (rows 0 to 21)
  _MatrixInstanceStart = ioScene.GetNbMeshInstances();
  for ( int i = 0; i < 220; ++i )
  {
    MeshInstance inst("MatrixCell_" + std::to_string(i), _BeveledCubeMeshID, defaultMatID, hiddenTransform);
    inst._Visible = false;
    ioScene.AddMeshInstance(inst);
  }

  // 2. Active Piece: 4 instances
  _ActiveInstanceStart = ioScene.GetNbMeshInstances();
  for ( int i = 0; i < 4; ++i )
  {
    MeshInstance inst("ActiveMino_" + std::to_string(i), _BeveledCubeMeshID, defaultMatID, hiddenTransform);
    inst._Visible = false;
    ioScene.AddMeshInstance(inst);
  }

  // 3. Ghost Piece: 4 instances (using ghost cage mesh)
  _GhostInstanceStart = ioScene.GetNbMeshInstances();
  for ( int i = 0; i < 4; ++i )
  {
    MeshInstance inst("GhostMino_" + std::to_string(i), _GhostCageMeshID, _GhostMaterialID, hiddenTransform);
    inst._Visible = false;
    ioScene.AddMeshInstance(inst);
  }

  // 4. Hold Piece: 4 instances
  _HoldInstanceStart = ioScene.GetNbMeshInstances();
  for ( int i = 0; i < 4; ++i )
  {
    MeshInstance inst("HoldMino_" + std::to_string(i), _BeveledCubeMeshID, defaultMatID, hiddenTransform);
    inst._Visible = false;
    ioScene.AddMeshInstance(inst);
  }

  // 5. Next Queue: 5 pieces x 4 minos = 20 instances
  _NextInstanceStart = ioScene.GetNbMeshInstances();
  for ( int i = 0; i < 20; ++i )
  {
    MeshInstance inst("NextMino_" + std::to_string(i), _BeveledCubeMeshID, defaultMatID, hiddenTransform);
    inst._Visible = false;
    ioScene.AddMeshInstance(inst);
  }

  // 6. Board Enclosure Chassis
  MeshInstance enclosureInst("BoardEnclosure", _EnclosureMeshID, _ChassisMaterialID, Mat4x4(1.0f));
  enclosureInst._Visible = true;
  _EnclosureInstanceID = ioScene.AddMeshInstance(enclosureInst);

  // 7. Backplate
  MeshInstance backplateInst("BoardBackplate", _BackplateMeshID, _BackplateMaterialID, Mat4x4(1.0f));
  backplateInst._Visible = true;
  _BackplateInstanceID = ioScene.AddMeshInstance(backplateInst);

  // 8. Skyline Indicator
  MeshInstance skylineInst("SkylineIndicator", _SkylineMeshID, _SkylineMaterialID, Mat4x4(1.0f));
  skylineInst._Visible = true;
  _SkylineInstanceID = ioScene.AddMeshInstance(skylineInst);

  return 0;
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int TetrisSceneManager::Initialize( Scene & ioScene )
{
  ioScene.Clear();

  // Create Procedural Meshes
  Mesh * beveledCube = ProceduralMesh::CreateCube("__TetrisBlockCube");
  _BeveledCubeMeshID = ioScene.AddMesh(beveledCube);
  if ( _BeveledCubeMeshID < 0 )
  {
    delete beveledCube;
    return 1;
  }

  Mesh * ghostCage = CreateGhostCageMesh("__TetrisGhostCage", 0.94f, 0.07f);
  _GhostCageMeshID = ioScene.AddMesh(ghostCage);
  if ( _GhostCageMeshID < 0 )
  {
    delete ghostCage;
    return 1;
  }

  Mesh * enclosure = CreateEnclosureMesh("__TetrisEnclosure");
  _EnclosureMeshID = ioScene.AddMesh(enclosure);
  if ( _EnclosureMeshID < 0 )
  {
    delete enclosure;
    return 1;
  }

  Mesh * backplate = CreateBackplateMesh("__TetrisBackplate");
  _BackplateMeshID = ioScene.AddMesh(backplate);
  if ( _BackplateMeshID < 0 )
  {
    delete backplate;
    return 1;
  }

  Mesh * skyline = CreateSkylineMesh("__TetrisSkyline");
  _SkylineMeshID = ioScene.AddMesh(skyline);
  if ( _SkylineMeshID < 0 )
  {
    delete skyline;
    return 1;
  }

  // Setup Materials & Lights
  if ( 0 != SetupMaterials(ioScene) )
    return 1;
  if ( 0 != SetupLights(ioScene) )
    return 1;
  if ( 0 != SetupInstances(ioScene) )
    return 1;

  // Load environment map for reflection and ambient lighting
  const std::string envMapPath = PathUtils::GetEnvMapPath("syferfontein_18d_clear_1k.hdr");
  ioScene.LoadEnvMap(envMapPath);

  // Compile TLAS / BVH
  ioScene.CompileMeshData(Vec2i(0), false, true, false);

  return 0;
}

// ----------------------------------------------------------------------------
// MaterialIDForPiece
// ----------------------------------------------------------------------------
int TetrisSceneManager::MaterialIDForPiece( TetrisPieceType iType ) const
{
  const int idx = (int)iType;
  if ( ( idx <= 0 ) || ( idx >= (int)TetrisPieceType::Count ) )
    return _ChassisMaterialID;
  return _PieceMaterialIDs[idx];
}

// ----------------------------------------------------------------------------
// SyncScene
// ----------------------------------------------------------------------------
int TetrisSceneManager::SyncScene( Scene & ioScene, const TetrisGameLogic & iLogic, float iDeltaTime )
{
  std::vector<MeshInstance> & instances = ioScene.GetMeshInstances();
  const TetrisMatrix & matrix = iLogic.GetMatrix();
  const Mat4x4 hiddenTransform = glm::translate(Vec3(0.f, -100.f, 0.f)) * glm::scale(Vec3(0.001f));

  // 1. Synchronize locked matrix blocks (rows 0 to 21)
  int cellInstanceIdx = 0;
  for ( int y = 0; y < 22; ++y )
  {
    for ( int x = 0; x < 10; ++x )
    {
      const int instID = _MatrixInstanceStart + cellInstanceIdx++;
      if ( ( instID < 0 ) || ( instID >= static_cast<int>(instances.size()) ) )
        continue;

      const TetrisPieceType cellType = matrix.GetCell(x, y);
      if ( TetrisPieceType::None != cellType )
      {
        instances[instID]._Visible = true;
        instances[instID]._MaterialID = MaterialIDForPiece(cellType);
        instances[instID]._Transform = glm::translate(CellToWorld(x, y)) * glm::scale(Vec3(0.92f));
      }
      else
      {
        instances[instID]._Visible = false;
        instances[instID]._Transform = hiddenTransform;
      }
    }
  }

  // 2. Synchronize Active Piece
  const TetrisPieceType activePiece = iLogic.GetActivePiece();
  const TetrisRotation activeRot = iLogic.GetActiveRotation();
  const Vec2i & activeOrigin = iLogic.GetActiveOrigin();
  const bool hasActive = ( TetrisPieceType::None != activePiece ) && !iLogic.IsGameOver();

  if ( hasActive )
  {
    const std::array<Vec2i, 4> & minos = TetrisPiece::GetMinos(activePiece, activeRot);
    const int activeMatID = MaterialIDForPiece(activePiece);

    for ( int i = 0; i < 4; ++i )
    {
      const int instID = _ActiveInstanceStart + i;
      if ( ( instID < 0 ) || ( instID >= static_cast<int>(instances.size()) ) )
        continue;

      instances[instID]._Visible = true;
      instances[instID]._MaterialID = activeMatID;
      instances[instID]._Transform = glm::translate(CellToWorld(activeOrigin.x + minos[i].x, activeOrigin.y + minos[i].y)) * glm::scale(Vec3(0.92f));
    }
  }
  else
  {
    for ( int i = 0; i < 4; ++i )
    {
      const int instID = _ActiveInstanceStart + i;
      if ( ( instID >= 0 ) && ( instID < static_cast<int>(instances.size()) ) )
      {
        instances[instID]._Visible = false;
        instances[instID]._Transform = hiddenTransform;
      }
    }
  }

  // 3. Synchronize Ghost Piece
  const int ghostY = iLogic.GetGhostY();
  const bool showGhost = _ShowGhost && hasActive && ( ghostY < activeOrigin.y );

  for ( int i = 0; i < 4; ++i )
  {
    const int instID = _GhostInstanceStart + i;
    if ( ( instID < 0 ) || ( instID >= static_cast<int>(instances.size()) ) )
      continue;

    if ( showGhost )
    {
      const std::array<Vec2i, 4> & minos = TetrisPiece::GetMinos(activePiece, activeRot);
      instances[instID]._Visible = true;
      instances[instID]._MaterialID = _GhostMaterialID;
      instances[instID]._Transform = glm::translate(CellToWorld(activeOrigin.x + minos[i].x, ghostY + minos[i].y));
    }
    else
    {
      instances[instID]._Visible = false;
      instances[instID]._Transform = hiddenTransform;
    }
  }

  // 4. Synchronize Hold Piece
  const TetrisPieceType holdPiece = iLogic.GetHoldPiece();
  if ( TetrisPieceType::None != holdPiece )
  {
    const std::array<Vec2i, 4> & minos = TetrisPiece::GetMinos(holdPiece, TetrisRotation::Spawn);
    const int holdMatID = MaterialIDForPiece(holdPiece);
    const Vec3 holdCenter(-7.9f, 16.5f, 0.0f);
    const float xCenterOffset = (TetrisPieceType::I == holdPiece) ? 1.5f : ((TetrisPieceType::O == holdPiece) ? 0.5f : 1.0f);
    const float yCenterOffset = (TetrisPieceType::I == holdPiece) ? 2.5f : ((TetrisPieceType::O == holdPiece) ? 1.5f : 1.0f);

    for ( int i = 0; i < 4; ++i )
    {
      const int instID = _HoldInstanceStart + i;
      if ( ( instID < 0 ) || ( instID >= static_cast<int>(instances.size()) ) )
        continue;

      instances[instID]._Visible = true;
      instances[instID]._MaterialID = holdMatID;
      instances[instID]._Transform = glm::translate(holdCenter + Vec3(static_cast<float>(minos[i].x) - xCenterOffset, static_cast<float>(minos[i].y) - yCenterOffset, 0.f))
                                   * glm::scale(Vec3(0.85f * 0.92f));
    }
  }
  else
  {
    for ( int i = 0; i < 4; ++i )
    {
      const int instID = _HoldInstanceStart + i;
      if ( ( instID >= 0 ) && ( instID < static_cast<int>(instances.size()) ) )
      {
        instances[instID]._Visible = false;
        instances[instID]._Transform = hiddenTransform;
      }
    }
  }

  // 5. Synchronize Next Queue (5 previews)
  const float nextBaseY[5] = { 17.0f, 14.2f, 11.4f, 8.6f, 5.8f };
  for ( int queueIdx = 0; queueIdx < 5; ++queueIdx )
  {
    const TetrisPieceType nextPiece = iLogic.PeekNextPiece(queueIdx);
    if ( TetrisPieceType::None != nextPiece )
    {
      const std::array<Vec2i, 4> & minos = TetrisPiece::GetMinos(nextPiece, TetrisRotation::Spawn);
      const int nextMatID = MaterialIDForPiece(nextPiece);
      const Vec3 nextCenter(8.0f, nextBaseY[queueIdx], 0.0f);
      const float xCenterOffset = (TetrisPieceType::I == nextPiece) ? 1.5f : ((TetrisPieceType::O == nextPiece) ? 0.5f : 1.0f);
      const float yCenterOffset = (TetrisPieceType::I == nextPiece) ? 2.5f : ((TetrisPieceType::O == nextPiece) ? 1.5f : 1.0f);

      for ( int m = 0; m < 4; ++m )
      {
        const int instID = _NextInstanceStart + queueIdx * 4 + m;
        if ( ( instID < 0 ) || ( instID >= static_cast<int>(instances.size()) ) )
          continue;

        instances[instID]._Visible = true;
        instances[instID]._MaterialID = nextMatID;
        instances[instID]._Transform = glm::translate(nextCenter + Vec3(static_cast<float>(minos[m].x) - xCenterOffset, static_cast<float>(minos[m].y) - yCenterOffset, 0.f))
                                     * glm::scale(Vec3(0.85f * 0.92f));
      }
    }
    else
    {
      for ( int m = 0; m < 4; ++m )
      {
        const int instID = _NextInstanceStart + queueIdx * 4 + m;
        if ( ( instID >= 0 ) && ( instID < static_cast<int>(instances.size()) ) )
        {
          instances[instID]._Visible = false;
          instances[instID]._Transform = hiddenTransform;
        }
      }
    }
  }

  // 6. Update Dynamic Lights
  Light * pieceLight = ioScene.GetLight(_ActivePieceLightIdx);
  if ( pieceLight )
  {
    if ( hasActive && _DynamicPieceLighting )
    {
      const Vec3 pieceWorldPos = CellToWorld(activeOrigin.x + 1, activeOrigin.y + 1);
      pieceLight->_Pos = pieceWorldPos + Vec3(0.f, 0.f, 1.2f);
      pieceLight->_Emission = TetrisPiece::GetColor(activePiece);
      pieceLight->_Intensity = 18.0f;
    }
    else
    {
      pieceLight->_Intensity = 0.0f;
    }
  }

  // Update line clear / drop flash light
  Light * flashLight = ioScene.GetLight(_LineClearLightIdx);
  if ( flashLight )
  {
    const float shake = iLogic.GetScreenShakeImpulse();
    if ( shake > 0.01f )
    {
      _FlashLightIntensity = shake * 45.0f;
      flashLight->_Intensity = _FlashLightIntensity;
      flashLight->_Pos = Vec3(0.0f, 10.0f, 2.0f);
    }
    else
    {
      _FlashLightIntensity = std::max(0.0f, _FlashLightIntensity - iDeltaTime * 60.0f);
      flashLight->_Intensity = _FlashLightIntensity;
    }
  }

  // Rebuild TLAS for acceleration structure
  ioScene.RebuildTLASData();

  return 0;
}

// ----------------------------------------------------------------------------
// Orbit Manipulation
// ----------------------------------------------------------------------------
void TetrisSceneManager::OrbitYawPitch( float iDeltaYaw, float iDeltaPitch )
{
  _OrbitYaw += iDeltaYaw;
  _OrbitPitch = MathUtil::Clamp(_OrbitPitch + iDeltaPitch, -75.f, 75.f);
}

void TetrisSceneManager::OrbitZoom( float iDeltaRadius )
{
  _OrbitRadius = MathUtil::Clamp(_OrbitRadius - iDeltaRadius, 10.f, 50.f);
}

// ----------------------------------------------------------------------------
// SyncCamera
// ----------------------------------------------------------------------------
int TetrisSceneManager::SyncCamera( Scene & ioScene, const TetrisGameLogic & iLogic )
{
  Camera & camera = ioScene.GetCamera();
  const Vec3 lookAtTarget(0.0f, 10.0f, 0.0f);

  // Apply subtle screen shake offset
  const float shake = iLogic.GetScreenShakeImpulse();
  const Vec3 shakeOffset(std::sin(glfwGetTime() * 45.0) * shake * 0.15f,
                         std::cos(glfwGetTime() * 50.0) * shake * 0.22f,
                         0.0f);

  if ( TetrisCameraPreset::Isometric == _CameraPreset )
  {
    camera.Initialize(Vec3(11.5f, 13.5f, 26.0f) + shakeOffset, lookAtTarget, 74.0f);
  }
  else if ( TetrisCameraPreset::FreeOrbit == _CameraPreset )
  {
    const float yawRad = MathUtil::ToRadians(_OrbitYaw);
    const float pitchRad = MathUtil::ToRadians(_OrbitPitch);
    const Vec3 eyePos = lookAtTarget + Vec3(
      _OrbitRadius * std::sin(yawRad) * std::cos(pitchRad),
      _OrbitRadius * std::sin(pitchRad),
      _OrbitRadius * std::cos(yawRad) * std::cos(pitchRad)
    );
    camera.Initialize(eyePos + shakeOffset, lookAtTarget, 76.0f);
  }
  else // Frontal3D
  {
    camera.Initialize(Vec3(0.0f, 10.0f, 27.0f) + shakeOffset, lookAtTarget, 76.0f);
  }

  camera.SetZNearFar(0.1f, 150.0f);
  return 0;
}

}
