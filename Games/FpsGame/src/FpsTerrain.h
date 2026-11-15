#ifndef _FpsTerrain_
#define _FpsTerrain_

#include "FpsCollision.h"
#include "MathUtil.h"

#include <string>
#include <vector>

namespace RTRT
{
class Scene;
}

namespace FPS
{

struct FpsMapMaterial;

enum class FpsTerrainBrushMode
{
  Off = 0,
  Raise,
  Lower,
  Smooth,
  Flatten
};

struct FpsTerrainLayer
{
  std::string   _MaterialName = "grass";
  float         _MinElevation = 0.f;
  float         _MaxElevation = 10.f;
  float         _Feather      = 1.5f;
  float         _SlopeWeight  = 0.f;
  float         _UVScale      = 16.f;
};

struct FpsMapTerrain
{
  std::string                  _Name              = "Terrain";
  std::string                  _HeightmapPath     = "Heightmap/heightmap_20261115110046_JL3L.png";
  Vec3                         _Center            = Vec3(0.f, 0.f, 0.f);
  Vec2                         _Size              = Vec2(64.f, 64.f);
  float                        _MinElevation      = -3.0f;
  float                        _MaxElevation      = 22.0f;
  Vec2i                        _GridResolution    = Vec2i(192, 192);
  int                          _SmoothPasses      = 2;
  bool                         _IslandFalloff     = false;
  bool                         _WaterEnabled      = true;
  float                        _WaterLevel        = 0.0f;
  std::string                  _WaterMaterialName = "water";
  bool                         _Collidable        = true;
  bool                         _Visible           = true;
  std::vector<FpsTerrainLayer> _Layers;
};

struct FpsTerrainRuntime
{
  std::string        _LoadedPath;
  int                _SourceWidth    = 0;
  int                _SourceHeight   = 0;
  std::vector<float> _SourceHeights; // Normalized [0..1], size _SourceWidth * _SourceHeight
  Vec2i              _GridResolution = Vec2i(0, 0);
  std::vector<float> _GridHeights;   // Normalized [0..1] after smoothing/falloff, size Nx * Nz
  std::vector<Vec3>  _GridNormals;   // World-space vertex normals, size Nx * Nz
  float              _MinNormHeight  = 0.f;
  float              _MaxNormHeight  = 1.f;
};

class FpsTerrain
{
public:
  static void SeedDefaultLayers( FpsMapTerrain & ioTerrain );

  static bool LoadOrGenerateRuntime( const FpsMapTerrain & iTerrain,
                                     FpsTerrainRuntime & ioRuntime,
                                     bool iForceReloadSource = false );

  static void RebuildGridFromSource( const FpsMapTerrain & iTerrain,
                                     FpsTerrainRuntime & ioRuntime );

  static void RecomputeGridNormals( const FpsMapTerrain & iTerrain,
                                    FpsTerrainRuntime & ioRuntime );

  static void BuildMeshGeometry( const FpsMapTerrain & iTerrain,
                                 const FpsTerrainRuntime & iRuntime,
                                 std::vector<Vec3> & oVertices,
                                 std::vector<Vec3> & oNormals,
                                 std::vector<Vec2> & oUVs,
                                 std::vector<Vec3i> & oIndices );

  static void EvaluateLayerWeights( const FpsMapTerrain & iTerrain,
                                    float iWorldY,
                                    const Vec3 & iWorldNormal,
                                    std::vector<float> & oWeights );

  static void BuildBlendedTextures( const FpsMapTerrain & iTerrain,
                                    const FpsTerrainRuntime & iRuntime,
                                    const std::vector<FpsMapMaterial> & iMaterials,
                                    RTRT::Scene * iScene,
                                    int iTexResolution,
                                    std::vector<unsigned char> & oAlbedoRGBA,
                                    std::vector<unsigned char> & oMetallicRoughnessRGBA );

  static bool SampleHeightAndNormal( const FpsMapTerrain & iTerrain,
                                     const FpsTerrainRuntime & iRuntime,
                                     float iWorldX,
                                     float iWorldZ,
                                     float & oWorldY,
                                     Vec3 & oWorldNormal );

  static bool Raycast( const FpsMapTerrain & iTerrain,
                       const FpsTerrainRuntime & iRuntime,
                       const Vec3 & iRayOrigin,
                       const Vec3 & iRayDir,
                       float iMaxDistance,
                       FpsCollisionRayResult & oResult );

  static bool ApplyBrush( const FpsMapTerrain & iTerrain,
                          FpsTerrainRuntime & ioRuntime,
                          FpsTerrainBrushMode iMode,
                          const Vec3 & iWorldHitPoint,
                          float iBrushRadius,
                          float iBrushStrength,
                          float iDeltaTime,
                          float iFlattenTargetWorldY );

  static bool SaveHeightmapPNG( const FpsTerrainRuntime & iRuntime,
                                const std::string & iFilePath );
};

}

#endif /* _FpsTerrain_ */
