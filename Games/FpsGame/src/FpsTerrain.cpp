#include "FpsTerrain.h"

#include "FpsGameMap.h"
#include "Material.h"
#include "PathUtils.h"
#include "Scene.h"
#include "Texture.h"

#include "stb_image.h"
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace FPS
{
using namespace RTRT;

// ----------------------------------------------------------------------------
// Local helpers
// ----------------------------------------------------------------------------
static float SmoothStep( float iEdge0, float iEdge1, float iX )
{
  const float span = iEdge1 - iEdge0;
  if ( std::abs(span) <= 1e-6f )
    return ( iX >= iEdge1 ) ? 1.f : 0.f;

  const float t = MathUtil::Clamp(( iX - iEdge0 ) / span, 0.f, 1.f);
  return t * t * ( 3.f - 2.f * t );
}

static std::filesystem::path ResolveTerrainHeightmapPath( const std::string & iPath )
{
  if ( iPath.empty() )
    return std::filesystem::path();

  const std::filesystem::path rawPath(iPath);
  std::error_code ec;
  if ( rawPath.is_absolute() && std::filesystem::exists(rawPath, ec) )
    return rawPath.lexically_normal();

  if ( std::filesystem::exists(rawPath, ec) )
    return std::filesystem::absolute(rawPath, ec).lexically_normal();

  const std::string assetResolved = PathUtils::GetAssetPath(iPath);
  if ( !assetResolved.empty() && std::filesystem::exists(assetResolved, ec) )
    return std::filesystem::path(assetResolved).lexically_normal();

  return rawPath;
}

static float SampleBilinearGrid( const std::vector<float> & iGrid, int iWidth, int iHeight, float iU, float iV )
{
  if ( iGrid.empty() || ( iWidth <= 0 ) || ( iHeight <= 0 ) )
    return 0.f;

  const float u = MathUtil::Clamp(iU, 0.f, 1.f);
  const float v = MathUtil::Clamp(iV, 0.f, 1.f);

  const float fx = u * static_cast<float>(std::max(0, iWidth - 1));
  const float fy = v * static_cast<float>(std::max(0, iHeight - 1));

  const int x0 = MathUtil::Clamp(static_cast<int>(std::floor(fx)), 0, iWidth - 1);
  const int y0 = MathUtil::Clamp(static_cast<int>(std::floor(fy)), 0, iHeight - 1);
  const int x1 = std::min(x0 + 1, iWidth - 1);
  const int y1 = std::min(y0 + 1, iHeight - 1);

  const float tx = fx - static_cast<float>(x0);
  const float ty = fy - static_cast<float>(y0);

  const float h00 = iGrid[static_cast<size_t>(y0) * iWidth + x0];
  const float h10 = iGrid[static_cast<size_t>(y0) * iWidth + x1];
  const float h01 = iGrid[static_cast<size_t>(y1) * iWidth + x0];
  const float h11 = iGrid[static_cast<size_t>(y1) * iWidth + x1];

  const float hx0 = MathUtil::Lerp(h00, h10, tx);
  const float hx1 = MathUtil::Lerp(h01, h11, tx);
  return MathUtil::Lerp(hx0, hx1, ty);
}

static const FpsMapMaterial * FindTerrainMapMaterial( const std::vector<FpsMapMaterial> & iMaterials, const std::string & iName )
{
  for ( const FpsMapMaterial & mat : iMaterials )
  {
    if ( mat._Name == iName )
      return &mat;
  }
  return nullptr;
}

static Material DefaultFallbackLayerMaterial( const std::string & iName )
{
  Material mat;
  if ( "seabed" == iName )
  {
    mat._Albedo = Vec3(0.16f, 0.19f, 0.18f);
    mat._Roughness = 0.90f;
  }
  else if ( "sand" == iName )
  {
    mat._Albedo = Vec3(0.76f, 0.68f, 0.48f);
    mat._Roughness = 0.82f;
  }
  else if ( "grass" == iName )
  {
    mat._Albedo = Vec3(0.24f, 0.44f, 0.18f);
    mat._Roughness = 0.78f;
  }
  else if ( "rock" == iName )
  {
    mat._Albedo = Vec3(0.42f, 0.40f, 0.38f);
    mat._Roughness = 0.72f;
  }
  else if ( "snow" == iName )
  {
    mat._Albedo = Vec3(0.92f, 0.94f, 0.97f);
    mat._Roughness = 0.35f;
    mat._Reflectance = 0.65f;
  }
  else
  {
    mat._Albedo = Vec3(0.5f, 0.5f, 0.5f);
    mat._Roughness = 0.75f;
  }
  return mat;
}

// ----------------------------------------------------------------------------
// SeedDefaultLayers
// ----------------------------------------------------------------------------
void FpsTerrain::SeedDefaultLayers( FpsMapTerrain & ioTerrain )
{
  if ( !ioTerrain._Layers.empty() )
    return;

  FpsTerrainLayer seabed;
  seabed._MaterialName = "seabed";
  seabed._MinElevation = -10.0f;
  seabed._MaxElevation = -0.15f;
  seabed._Feather      = 0.8f;
  seabed._SlopeWeight  = 0.0f;
  seabed._UVScale      = 16.0f;
  ioTerrain._Layers.push_back(seabed);

  FpsTerrainLayer sand;
  sand._MaterialName = "sand";
  sand._MinElevation = -0.35f;
  sand._MaxElevation = 1.6f;
  sand._Feather      = 0.9f;
  sand._SlopeWeight  = -0.35f;
  sand._UVScale      = 20.0f;
  ioTerrain._Layers.push_back(sand);

  FpsTerrainLayer grass;
  grass._MaterialName = "grass";
  grass._MinElevation = 1.0f;
  grass._MaxElevation = 9.5f;
  grass._Feather      = 1.8f;
  grass._SlopeWeight  = -0.45f;
  grass._UVScale      = 18.0f;
  ioTerrain._Layers.push_back(grass);

  FpsTerrainLayer rock;
  rock._MaterialName = "rock";
  rock._MinElevation = 7.0f;
  rock._MaxElevation = 17.5f;
  rock._Feather      = 2.2f;
  rock._SlopeWeight  = 0.75f;
  rock._UVScale      = 14.0f;
  ioTerrain._Layers.push_back(rock);

  FpsTerrainLayer snow;
  snow._MaterialName = "snow";
  snow._MinElevation = 14.5f;
  snow._MaxElevation = 40.0f;
  snow._Feather      = 2.5f;
  snow._SlopeWeight  = -0.30f;
  snow._UVScale      = 16.0f;
  ioTerrain._Layers.push_back(snow);
}

// ----------------------------------------------------------------------------
// LoadOrGenerateRuntime
// ----------------------------------------------------------------------------
bool FpsTerrain::LoadOrGenerateRuntime( const FpsMapTerrain & iTerrain,
                                        FpsTerrainRuntime & ioRuntime,
                                        bool iForceReloadSource )
{
  const bool needSourceLoad = iForceReloadSource
                           || ioRuntime._SourceHeights.empty()
                           || ( ioRuntime._LoadedPath != iTerrain._HeightmapPath );

  if ( needSourceLoad )
  {
    ioRuntime._LoadedPath = iTerrain._HeightmapPath;
    ioRuntime._SourceWidth = 0;
    ioRuntime._SourceHeight = 0;
    ioRuntime._SourceHeights.clear();

    bool loadedFromDisk = false;
    if ( !iTerrain._HeightmapPath.empty() )
    {
      const std::filesystem::path resolved = ResolveTerrainHeightmapPath(iTerrain._HeightmapPath);
      std::error_code ec;
      if ( !resolved.empty() && std::filesystem::exists(resolved, ec) )
      {
        const std::string filepathStr = resolved.string();
        int width = 0;
        int height = 0;
        int channels = 0;

        if ( stbi_is_16_bit(filepathStr.c_str()) )
        {
          stbi_us * pixels16 = stbi_load_16(filepathStr.c_str(), &width, &height, &channels, 0);
          if ( pixels16 && ( width > 0 ) && ( height > 0 ) && ( channels > 0 ) )
          {
            ioRuntime._SourceWidth = width;
            ioRuntime._SourceHeight = height;
            ioRuntime._SourceHeights.resize(static_cast<size_t>(width) * static_cast<size_t>(height), 0.f);
            const float inv65535 = 1.0f / 65535.0f;
            for ( int y = 0; y < height; ++y )
            {
              for ( int x = 0; x < width; ++x )
              {
                const size_t idx = ( static_cast<size_t>(y) * width + x ) * channels;
                float lum = 0.f;
                if ( channels >= 3 )
                {
                  lum = ( 0.299f * static_cast<float>(pixels16[idx + 0])
                        + 0.587f * static_cast<float>(pixels16[idx + 1])
                        + 0.114f * static_cast<float>(pixels16[idx + 2]) ) * inv65535;
                }
                else
                  lum = static_cast<float>(pixels16[idx]) * inv65535;
                ioRuntime._SourceHeights[static_cast<size_t>(y) * width + x] = MathUtil::Clamp(lum, 0.f, 1.f);
              }
            }
            stbi_image_free(pixels16);
            loadedFromDisk = true;
          }
        }

        if ( !loadedFromDisk )
        {
          stbi_uc * pixels8 = stbi_load(filepathStr.c_str(), &width, &height, &channels, 0);
          if ( pixels8 && ( width > 0 ) && ( height > 0 ) && ( channels > 0 ) )
          {
            ioRuntime._SourceWidth = width;
            ioRuntime._SourceHeight = height;
            ioRuntime._SourceHeights.resize(static_cast<size_t>(width) * static_cast<size_t>(height), 0.f);
            const float inv255 = 1.0f / 255.0f;
            for ( int y = 0; y < height; ++y )
            {
              for ( int x = 0; x < width; ++x )
              {
                const size_t idx = ( static_cast<size_t>(y) * width + x ) * channels;
                float lum = 0.f;
                if ( channels >= 3 )
                {
                  lum = ( 0.299f * static_cast<float>(pixels8[idx + 0])
                        + 0.587f * static_cast<float>(pixels8[idx + 1])
                        + 0.114f * static_cast<float>(pixels8[idx + 2]) ) * inv255;
                }
                else
                  lum = static_cast<float>(pixels8[idx]) * inv255;
                ioRuntime._SourceHeights[static_cast<size_t>(y) * width + x] = MathUtil::Clamp(lum, 0.f, 1.f);
              }
            }
            stbi_image_free(pixels8);
            loadedFromDisk = true;
          }
        }
      }
    }

    if ( !loadedFromDisk )
    {
      const int fallbackRes = 128;
      ioRuntime._SourceWidth = fallbackRes;
      ioRuntime._SourceHeight = fallbackRes;
      ioRuntime._SourceHeights.resize(static_cast<size_t>(fallbackRes) * static_cast<size_t>(fallbackRes), 0.15f);
      for ( int z = 0; z < fallbackRes; ++z )
      {
        const float v = static_cast<float>(z) / static_cast<float>(fallbackRes - 1);
        for ( int x = 0; x < fallbackRes; ++x )
        {
          const float u = static_cast<float>(x) / static_cast<float>(fallbackRes - 1);
          const float cx = ( u - 0.5f ) * 2.f;
          const float cz = ( v - 0.5f ) * 2.f;
          const float radial = std::max(0.f, 1.f - 0.65f * ( cx * cx + cz * cz ));
          const float wave = 0.5f + 0.28f * std::sin(cx * 3.8f) * std::cos(cz * 3.4f)
                           + 0.14f * std::cos(( cx + cz ) * 6.5f)
                           + 0.08f * std::sin(cx * 11.f - cz * 9.f);
          ioRuntime._SourceHeights[static_cast<size_t>(z) * fallbackRes + x] = MathUtil::Clamp(wave * radial, 0.f, 1.f);
        }
      }
    }
  }

  RebuildGridFromSource(iTerrain, ioRuntime);
  return !ioRuntime._GridHeights.empty();
}

// ----------------------------------------------------------------------------
// RebuildGridFromSource
// ----------------------------------------------------------------------------
void FpsTerrain::RebuildGridFromSource( const FpsMapTerrain & iTerrain,
                                        FpsTerrainRuntime & ioRuntime )
{
  const int nx = MathUtil::Clamp(iTerrain._GridResolution.x, 4, 512);
  const int nz = MathUtil::Clamp(iTerrain._GridResolution.y, 4, 512);
  ioRuntime._GridResolution = Vec2i(nx, nz);
  ioRuntime._GridHeights.resize(static_cast<size_t>(nx) * static_cast<size_t>(nz), 0.f);

  for ( int z = 0; z < nz; ++z )
  {
    const float v = static_cast<float>(z) / static_cast<float>(nz - 1);
    for ( int x = 0; x < nx; ++x )
    {
      const float u = static_cast<float>(x) / static_cast<float>(nx - 1);
      float h = SampleBilinearGrid(ioRuntime._SourceHeights, ioRuntime._SourceWidth, ioRuntime._SourceHeight, u, v);
      if ( iTerrain._IslandFalloff )
      {
        const float dx = ( u - 0.5f ) * 2.f;
        const float dz = ( v - 0.5f ) * 2.f;
        const float dist = std::sqrt(dx * dx + dz * dz);
        const float falloff = 1.f - SmoothStep(0.55f, 1.0f, dist);
        h *= falloff;
      }
      ioRuntime._GridHeights[static_cast<size_t>(z) * nx + x] = MathUtil::Clamp(h, 0.f, 1.f);
    }
  }

  const int smoothPasses = MathUtil::Clamp(iTerrain._SmoothPasses, 0, 16);
  if ( smoothPasses > 0 )
  {
    std::vector<float> temp(ioRuntime._GridHeights.size(), 0.f);
    for ( int pass = 0; pass < smoothPasses; ++pass )
    {
      // Horizontal [0.25, 0.5, 0.25] pass
      for ( int z = 0; z < nz; ++z )
      {
        const size_t rowOffset = static_cast<size_t>(z) * nx;
        for ( int x = 0; x < nx; ++x )
        {
          const int xPrev = std::max(0, x - 1);
          const int xNext = std::min(nx - 1, x + 1);
          temp[rowOffset + x] = 0.25f * ioRuntime._GridHeights[rowOffset + xPrev]
                              + 0.50f * ioRuntime._GridHeights[rowOffset + x]
                              + 0.25f * ioRuntime._GridHeights[rowOffset + xNext];
        }
      }

      // Vertical [0.25, 0.5, 0.25] pass
      for ( int z = 0; z < nz; ++z )
      {
        const int zPrev = std::max(0, z - 1);
        const int zNext = std::min(nz - 1, z + 1);
        const size_t rowPrev = static_cast<size_t>(zPrev) * nx;
        const size_t rowCurr = static_cast<size_t>(z) * nx;
        const size_t rowNext = static_cast<size_t>(zNext) * nx;
        for ( int x = 0; x < nx; ++x )
        {
          ioRuntime._GridHeights[rowCurr + x] = 0.25f * temp[rowPrev + x]
                                              + 0.50f * temp[rowCurr + x]
                                              + 0.25f * temp[rowNext + x];
        }
      }
    }
  }

  float minH = 1.f;
  float maxH = 0.f;
  for ( float h : ioRuntime._GridHeights )
  {
    minH = std::min(minH, h);
    maxH = std::max(maxH, h);
  }
  ioRuntime._MinNormHeight = ( minH <= maxH ) ? minH : 0.f;
  ioRuntime._MaxNormHeight = ( minH <= maxH ) ? maxH : 1.f;

  RecomputeGridNormals(iTerrain, ioRuntime);
}

// ----------------------------------------------------------------------------
// RecomputeGridNormals
// ----------------------------------------------------------------------------
void FpsTerrain::RecomputeGridNormals( const FpsMapTerrain & iTerrain,
                                       FpsTerrainRuntime & ioRuntime )
{
  const int nx = ioRuntime._GridResolution.x;
  const int nz = ioRuntime._GridResolution.y;
  if ( ( nx < 2 ) || ( nz < 2 ) || ( static_cast<int>(ioRuntime._GridHeights.size()) != nx * nz ) )
    return;

  ioRuntime._GridNormals.resize(static_cast<size_t>(nx) * static_cast<size_t>(nz), Vec3(0.f, 1.f, 0.f));

  const float sizeX = std::max(0.1f, iTerrain._Size.x);
  const float sizeZ = std::max(0.1f, iTerrain._Size.y);
  const float dx = sizeX / static_cast<float>(nx - 1);
  const float dz = sizeZ / static_cast<float>(nz - 1);
  const float elevSpan = iTerrain._MaxElevation - iTerrain._MinElevation;

  for ( int z = 0; z < nz; ++z )
  {
    const int zPrev = std::max(0, z - 1);
    const int zNext = std::min(nz - 1, z + 1);
    const float spanZ = static_cast<float>(zNext - zPrev) * dz;

    for ( int x = 0; x < nx; ++x )
    {
      const int xPrev = std::max(0, x - 1);
      const int xNext = std::min(nx - 1, x + 1);
      const float spanX = static_cast<float>(xNext - xPrev) * dx;

      const float hL = ioRuntime._GridHeights[static_cast<size_t>(z) * nx + xPrev] * elevSpan;
      const float hR = ioRuntime._GridHeights[static_cast<size_t>(z) * nx + xNext] * elevSpan;
      const float hD = ioRuntime._GridHeights[static_cast<size_t>(zPrev) * nx + x] * elevSpan;
      const float hU = ioRuntime._GridHeights[static_cast<size_t>(zNext) * nx + x] * elevSpan;

      const Vec3 tangentX(spanX, hR - hL, 0.f);
      const Vec3 tangentZ(0.f, hU - hD, spanZ);
      Vec3 normal = glm::cross(tangentZ, tangentX);
      const float len = glm::length(normal);
      ioRuntime._GridNormals[static_cast<size_t>(z) * nx + x] = ( len > 1e-6f ) ? ( normal / len ) : Vec3(0.f, 1.f, 0.f);
    }
  }
}

// ----------------------------------------------------------------------------
// BuildMeshGeometry
// ----------------------------------------------------------------------------
void FpsTerrain::BuildMeshGeometry( const FpsMapTerrain & iTerrain,
                                    const FpsTerrainRuntime & iRuntime,
                                    std::vector<Vec3> & oVertices,
                                    std::vector<Vec3> & oNormals,
                                    std::vector<Vec2> & oUVs,
                                    std::vector<Vec3i> & oIndices )
{
  const int nx = iRuntime._GridResolution.x;
  const int nz = iRuntime._GridResolution.y;
  if ( ( nx < 2 ) || ( nz < 2 ) || ( static_cast<int>(iRuntime._GridHeights.size()) != nx * nz ) )
  {
    oVertices.clear();
    oNormals.clear();
    oUVs.clear();
    oIndices.clear();
    return;
  }

  const size_t vertexCount = static_cast<size_t>(nx) * static_cast<size_t>(nz);
  oVertices.resize(vertexCount);
  oNormals.resize(vertexCount);
  oUVs.resize(vertexCount);

  const float sizeX = std::max(0.1f, iTerrain._Size.x);
  const float sizeZ = std::max(0.1f, iTerrain._Size.y);
  const float halfX = sizeX * 0.5f;
  const float halfZ = sizeZ * 0.5f;

  for ( int z = 0; z < nz; ++z )
  {
    const float v = static_cast<float>(z) / static_cast<float>(nz - 1);
    const float localZ = -halfZ + v * sizeZ;
    for ( int x = 0; x < nx; ++x )
    {
      const float u = static_cast<float>(x) / static_cast<float>(nx - 1);
      const float localX = -halfX + u * sizeX;
      const size_t idx = static_cast<size_t>(z) * nx + x;
      const float normH = iRuntime._GridHeights[idx];
      const float localY = MathUtil::Lerp(iTerrain._MinElevation, iTerrain._MaxElevation, normH);

      oVertices[idx] = Vec3(localX, localY, localZ);
      oNormals[idx]  = ( idx < iRuntime._GridNormals.size() ) ? iRuntime._GridNormals[idx] : Vec3(0.f, 1.f, 0.f);
      oUVs[idx]      = Vec2(u, v);
    }
  }

  const size_t quadCount = static_cast<size_t>(nx - 1) * static_cast<size_t>(nz - 1);
  oIndices.clear();
  oIndices.reserve(quadCount * 6);

  for ( int z = 0; z < nz - 1; ++z )
  {
    for ( int x = 0; x < nx - 1; ++x )
    {
      const int i00 = z * nx + x;
      const int i10 = z * nx + ( x + 1 );
      const int i01 = ( z + 1 ) * nx + x;
      const int i11 = ( z + 1 ) * nx + ( x + 1 );

      // First triangle (counter-clockwise looking from +Y)
      oIndices.push_back(Vec3i(i00, i00, i00));
      oIndices.push_back(Vec3i(i01, i01, i01));
      oIndices.push_back(Vec3i(i10, i10, i10));

      // Second triangle
      oIndices.push_back(Vec3i(i10, i10, i10));
      oIndices.push_back(Vec3i(i01, i01, i01));
      oIndices.push_back(Vec3i(i11, i11, i11));
    }
  }
}

// ----------------------------------------------------------------------------
// EvaluateLayerWeights
// ----------------------------------------------------------------------------
void FpsTerrain::EvaluateLayerWeights( const FpsMapTerrain & iTerrain,
                                       float iWorldY,
                                       const Vec3 & iWorldNormal,
                                       std::vector<float> & oWeights )
{
  const size_t nbLayers = iTerrain._Layers.size();
  oWeights.assign(nbLayers, 0.f);
  if ( 0 == nbLayers )
    return;

  if ( 1 == nbLayers )
  {
    oWeights[0] = 1.f;
    return;
  }

  const float slope = MathUtil::Clamp(1.f - std::max(0.f, iWorldNormal.y), 0.f, 1.f);
  const float smoothSlope = SmoothStep(0.12f, 0.55f, slope);

  float weightSum = 0.f;
  for ( size_t i = 0; i < nbLayers; ++i )
  {
    const FpsTerrainLayer & layer = iTerrain._Layers[i];
    const float feather = std::max(0.05f, layer._Feather);

    const float fadeIn = ( 0 == i )
                       ? 1.f
                       : SmoothStep(layer._MinElevation - feather, layer._MinElevation + feather, iWorldY);
    const float fadeOut = ( i + 1 == nbLayers )
                        ? 1.f
                        : ( 1.f - SmoothStep(layer._MaxElevation - feather, layer._MaxElevation + feather, iWorldY) );

    float weight = std::max(0.f, fadeIn * fadeOut);

    if ( layer._SlopeWeight > 0.f )
    {
      const float slopeBoost = smoothSlope * MathUtil::Clamp(layer._SlopeWeight, 0.f, 1.f);
      weight = std::max(weight, slopeBoost * fadeIn);
    }
    else if ( layer._SlopeWeight < 0.f )
    {
      const float slopePenalty = smoothSlope * MathUtil::Clamp(-layer._SlopeWeight, 0.f, 1.f);
      weight *= std::max(0.05f, 1.f - slopePenalty);
    }

    oWeights[i] = weight;
    weightSum += weight;
  }

  if ( weightSum > 1e-6f )
  {
    const float invSum = 1.f / weightSum;
    for ( float & w : oWeights )
      w *= invSum;
  }
  else
  {
    oWeights[0] = 1.f;
  }
}

// ----------------------------------------------------------------------------
// BuildBlendedTextures
// ----------------------------------------------------------------------------
void FpsTerrain::BuildBlendedTextures( const FpsMapTerrain & iTerrain,
                                       const FpsTerrainRuntime & iRuntime,
                                       const std::vector<FpsMapMaterial> & iMaterials,
                                       Scene * iScene,
                                       int iTexResolution,
                                       std::vector<unsigned char> & oAlbedoRGBA,
                                       std::vector<unsigned char> & oMetallicRoughnessRGBA )
{
  const int res = MathUtil::Clamp(iTexResolution, 64, 2048);
  const size_t pixelCount = static_cast<size_t>(res) * static_cast<size_t>(res);
  oAlbedoRGBA.resize(pixelCount * 4, 255);
  oMetallicRoughnessRGBA.resize(pixelCount * 4, 255);

  FpsMapTerrain evalTerrain = iTerrain;
  if ( evalTerrain._Layers.empty() )
    SeedDefaultLayers(evalTerrain);

  const size_t nbLayers = evalTerrain._Layers.size();
  std::vector<Material> layerMaterials(nbLayers);
  std::vector<const Texture *> layerTextures(nbLayers, nullptr);

  for ( size_t i = 0; i < nbLayers; ++i )
  {
    const FpsTerrainLayer & layer = evalTerrain._Layers[i];
    const FpsMapMaterial * foundMat = FindTerrainMapMaterial(iMaterials, layer._MaterialName);
    layerMaterials[i] = foundMat ? foundMat -> _Material : DefaultFallbackLayerMaterial(layer._MaterialName);

    if ( iScene && ( layerMaterials[i]._BaseColorTexId >= 0.f ) )
    {
      const int texID = static_cast<int>(layerMaterials[i]._BaseColorTexId + 0.5f);
      if ( ( texID >= 0 ) && ( texID < static_cast<int>(iScene -> GetTextures().size()) ) )
        layerTextures[i] = iScene -> GetTextures()[texID];
    }
  }

  const int nx = std::max(2, iRuntime._GridResolution.x);
  const int nz = std::max(2, iRuntime._GridResolution.y);
  std::vector<float> weights(nbLayers, 0.f);

  for ( int y = 0; y < res; ++y )
  {
    // Note:Scene texture sampling flips V (1 - iUV.y), so row y corresponds to v = 1 - y/(res-1)
    const float v = 1.f - static_cast<float>(y) / static_cast<float>(res - 1);
    const float fz = v * static_cast<float>(nz - 1);
    const int z0 = MathUtil::Clamp(static_cast<int>(std::floor(fz)), 0, nz - 1);
    const int z1 = std::min(z0 + 1, nz - 1);
    const float tz = fz - static_cast<float>(z0);

    for ( int x = 0; x < res; ++x )
    {
      const float u = static_cast<float>(x) / static_cast<float>(res - 1);
      const float fx = u * static_cast<float>(nx - 1);
      const int x0 = MathUtil::Clamp(static_cast<int>(std::floor(fx)), 0, nx - 1);
      const int x1 = std::min(x0 + 1, nx - 1);
      const float tx = fx - static_cast<float>(x0);

      float normH = 0.f;
      Vec3 normal(0.f, 1.f, 0.f);
      if ( static_cast<int>(iRuntime._GridHeights.size()) == nx * nz )
      {
        const float h00 = iRuntime._GridHeights[static_cast<size_t>(z0) * nx + x0];
        const float h10 = iRuntime._GridHeights[static_cast<size_t>(z0) * nx + x1];
        const float h01 = iRuntime._GridHeights[static_cast<size_t>(z1) * nx + x0];
        const float h11 = iRuntime._GridHeights[static_cast<size_t>(z1) * nx + x1];
        normH = MathUtil::Lerp(MathUtil::Lerp(h00, h10, tx), MathUtil::Lerp(h01, h11, tx), tz);
      }
      if ( static_cast<int>(iRuntime._GridNormals.size()) == nx * nz )
      {
        const Vec3 & n00 = iRuntime._GridNormals[static_cast<size_t>(z0) * nx + x0];
        const Vec3 & n10 = iRuntime._GridNormals[static_cast<size_t>(z0) * nx + x1];
        const Vec3 & n01 = iRuntime._GridNormals[static_cast<size_t>(z1) * nx + x0];
        const Vec3 & n11 = iRuntime._GridNormals[static_cast<size_t>(z1) * nx + x1];
        const Vec3 nx0 = MathUtil::Lerp(n00, n10, tx);
        const Vec3 nx1 = MathUtil::Lerp(n01, n11, tx);
        normal = glm::normalize(MathUtil::Lerp(nx0, nx1, tz));
      }

      const float localY = MathUtil::Lerp(iTerrain._MinElevation, iTerrain._MaxElevation, normH);
      EvaluateLayerWeights(evalTerrain, localY, normal, weights);

      Vec3 blendedAlbedo(0.f);
      float blendedRoughness = 0.f;
      float blendedMetallic = 0.f;

      for ( size_t i = 0; i < nbLayers; ++i )
      {
        const float w = weights[i];
        if ( w <= 1e-5f )
          continue;

        Vec3 layerColor = layerMaterials[i]._Albedo;
        if ( layerTextures[i] )
        {
          const float uvScale = std::max(0.1f, evalTerrain._Layers[i]._UVScale);
          const Vec2 tiledUV(u * uvScale - std::floor(u * uvScale), v * uvScale - std::floor(v * uvScale));
          const Vec4 texSample = layerTextures[i] -> BiLinearSample(tiledUV);
          layerColor *= Vec3(texSample);
        }

        blendedAlbedo    += layerColor * w;
        blendedRoughness += layerMaterials[i]._Roughness * w;
        blendedMetallic  += layerMaterials[i]._Metallic * w;
      }

      // Subtle high-frequency micro-detail grain so untextured procedural layers show surface contour
      const float grain = 0.96f + 0.04f * std::sin(u * 211.f + v * 173.f) * std::cos(u * 137.f - v * 251.f);
      blendedAlbedo = glm::clamp(blendedAlbedo * grain, Vec3(0.f), Vec3(1.f));
      blendedRoughness = MathUtil::Clamp(blendedRoughness, 0.04f, 1.f);
      blendedMetallic  = MathUtil::Clamp(blendedMetallic, 0.f, 1.f);

      const size_t dstIdx = ( static_cast<size_t>(y) * res + x ) * 4;
      oAlbedoRGBA[dstIdx + 0] = static_cast<unsigned char>(std::round(blendedAlbedo.x * 255.f));
      oAlbedoRGBA[dstIdx + 1] = static_cast<unsigned char>(std::round(blendedAlbedo.y * 255.f));
      oAlbedoRGBA[dstIdx + 2] = static_cast<unsigned char>(std::round(blendedAlbedo.z * 255.f));
      oAlbedoRGBA[dstIdx + 3] = 255;

      // RTRT MetallicRoughness convention: G = roughness, B = metallic
      oMetallicRoughnessRGBA[dstIdx + 0] = 255;
      oMetallicRoughnessRGBA[dstIdx + 1] = static_cast<unsigned char>(std::round(blendedRoughness * 255.f));
      oMetallicRoughnessRGBA[dstIdx + 2] = static_cast<unsigned char>(std::round(blendedMetallic * 255.f));
      oMetallicRoughnessRGBA[dstIdx + 3] = 255;
    }
  }
}

// ----------------------------------------------------------------------------
// SampleHeightAndNormal
// ----------------------------------------------------------------------------
bool FpsTerrain::SampleHeightAndNormal( const FpsMapTerrain & iTerrain,
                                        const FpsTerrainRuntime & iRuntime,
                                        float iWorldX,
                                        float iWorldZ,
                                        float & oWorldY,
                                        Vec3 & oWorldNormal )
{
  const int nx = iRuntime._GridResolution.x;
  const int nz = iRuntime._GridResolution.y;
  if ( ( nx < 2 ) || ( nz < 2 ) || ( static_cast<int>(iRuntime._GridHeights.size()) != nx * nz ) )
    return false;

  const float sizeX = std::max(0.1f, iTerrain._Size.x);
  const float sizeZ = std::max(0.1f, iTerrain._Size.y);
  const float halfX = sizeX * 0.5f;
  const float halfZ = sizeZ * 0.5f;

  const float localX = iWorldX - iTerrain._Center.x;
  const float localZ = iWorldZ - iTerrain._Center.z;
  if ( ( localX < -halfX ) || ( localX > halfX ) || ( localZ < -halfZ ) || ( localZ > halfZ ) )
    return false;

  const float u = MathUtil::Clamp(( localX + halfX ) / sizeX, 0.f, 1.f);
  const float v = MathUtil::Clamp(( localZ + halfZ ) / sizeZ, 0.f, 1.f);

  const float fx = u * static_cast<float>(nx - 1);
  const float fz = v * static_cast<float>(nz - 1);

  const int x0 = MathUtil::Clamp(static_cast<int>(std::floor(fx)), 0, nx - 2);
  const int z0 = MathUtil::Clamp(static_cast<int>(std::floor(fz)), 0, nz - 2);
  const int x1 = x0 + 1;
  const int z1 = z0 + 1;

  const float tx = fx - static_cast<float>(x0);
  const float tz = fz - static_cast<float>(z0);

  const float h00 = iRuntime._GridHeights[static_cast<size_t>(z0) * nx + x0];
  const float h10 = iRuntime._GridHeights[static_cast<size_t>(z0) * nx + x1];
  const float h01 = iRuntime._GridHeights[static_cast<size_t>(z1) * nx + x0];
  const float h11 = iRuntime._GridHeights[static_cast<size_t>(z1) * nx + x1];

  // Match the exact diagonal split used in BuildMeshGeometry:
  // Tri 0: (x0,z0) -> (x0,z1) -> (x1,z0)  (tx + tz <= 1)
  // Tri 1: (x1,z0) -> (x0,z1) -> (x1,z1)  (tx + tz >  1)
  float normH = 0.f;
  if ( tx + tz <= 1.f )
    normH = h00 + tx * ( h10 - h00 ) + tz * ( h01 - h00 );
  else
    normH = h11 + ( 1.f - tx ) * ( h01 - h11 ) + ( 1.f - tz ) * ( h10 - h11 );

  oWorldY = iTerrain._Center.y + MathUtil::Lerp(iTerrain._MinElevation, iTerrain._MaxElevation, normH);

  if ( static_cast<int>(iRuntime._GridNormals.size()) == nx * nz )
  {
    const Vec3 & n00 = iRuntime._GridNormals[static_cast<size_t>(z0) * nx + x0];
    const Vec3 & n10 = iRuntime._GridNormals[static_cast<size_t>(z0) * nx + x1];
    const Vec3 & n01 = iRuntime._GridNormals[static_cast<size_t>(z1) * nx + x0];
    const Vec3 & n11 = iRuntime._GridNormals[static_cast<size_t>(z1) * nx + x1];
    const Vec3 nx0 = MathUtil::Lerp(n00, n10, tx);
    const Vec3 nx1 = MathUtil::Lerp(n01, n11, tx);
    const Vec3 blended = MathUtil::Lerp(nx0, nx1, tz);
    const float len = glm::length(blended);
    oWorldNormal = ( len > 1e-6f ) ? ( blended / len ) : Vec3(0.f, 1.f, 0.f);
  }
  else
    oWorldNormal = Vec3(0.f, 1.f, 0.f);

  return true;
}

// ----------------------------------------------------------------------------
// Raycast
// ----------------------------------------------------------------------------
bool FpsTerrain::Raycast( const FpsMapTerrain & iTerrain,
                          const FpsTerrainRuntime & iRuntime,
                          const Vec3 & iRayOrigin,
                          const Vec3 & iRayDir,
                          float iMaxDistance,
                          FpsCollisionRayResult & oResult )
{
  oResult = FpsCollisionRayResult();
  if ( iMaxDistance <= 0.f )
    return false;

  const int nx = iRuntime._GridResolution.x;
  const int nz = iRuntime._GridResolution.y;
  if ( ( nx < 2 ) || ( nz < 2 ) || ( static_cast<int>(iRuntime._GridHeights.size()) != nx * nz ) )
    return false;

  const float rayLen = glm::length(iRayDir);
  if ( rayLen <= EPSILON )
    return false;
  const Vec3 dir = iRayDir / rayLen;

  const float halfX = std::max(0.1f, iTerrain._Size.x) * 0.5f;
  const float halfZ = std::max(0.1f, iTerrain._Size.y) * 0.5f;
  const float minY  = iTerrain._Center.y + std::min(iTerrain._MinElevation, iTerrain._MaxElevation) - 0.5f;
  const float maxY  = iTerrain._Center.y + std::max(iTerrain._MinElevation, iTerrain._MaxElevation) + 0.5f;

  AABB<Vec3> terrainBounds;
  terrainBounds._Low  = Vec3(iTerrain._Center.x - halfX, minY, iTerrain._Center.z - halfZ);
  terrainBounds._High = Vec3(iTerrain._Center.x + halfX, maxY, iTerrain._Center.z + halfZ);

  const bool originInside = ( iRayOrigin.x >= terrainBounds._Low.x ) && ( iRayOrigin.x <= terrainBounds._High.x )
                         && ( iRayOrigin.y >= terrainBounds._Low.y ) && ( iRayOrigin.y <= terrainBounds._High.y )
                         && ( iRayOrigin.z >= terrainBounds._Low.z ) && ( iRayOrigin.z <= terrainBounds._High.z );

  float tEnter = 0.f;
  if ( !originInside )
  {
    if ( !MathUtil::IntersectRayAABB(iRayOrigin, dir, terrainBounds, tEnter) )
      return false;
    if ( tEnter > iMaxDistance )
      return false;
  }

  // Compute exit distance along AABB
  float tExit = iMaxDistance;
  for ( int axis = 0; axis < 3; ++axis )
  {
    if ( std::abs(dir[axis]) > 1e-6f )
    {
      const float t1 = ( terrainBounds._Low[axis]  - iRayOrigin[axis] ) / dir[axis];
      const float t2 = ( terrainBounds._High[axis] - iRayOrigin[axis] ) / dir[axis];
      tExit = std::min(tExit, std::max(t1, t2));
    }
  }
  tEnter = std::max(0.f, tEnter);
  if ( tExit < tEnter )
    return false;

  const float cellSizeX = ( halfX * 2.f ) / static_cast<float>(nx - 1);
  const float cellSizeZ = ( halfZ * 2.f ) / static_cast<float>(nz - 1);
  const float stepSize = std::max(0.05f, std::min(cellSizeX, cellSizeZ) * 0.5f);

  float prevT = tEnter;
  Vec3 prevPt = iRayOrigin + dir * prevT;
  float prevTerrainY = 0.f;
  Vec3 prevNormal(0.f, 1.f, 0.f);
  bool prevValid = SampleHeightAndNormal(iTerrain, iRuntime, prevPt.x, prevPt.z, prevTerrainY, prevNormal);
  float prevDiff = prevPt.y - prevTerrainY;

  for ( float t = tEnter; t <= tExit + stepSize; t += stepSize )
  {
    const float curT = std::min(t, tExit);
    const Vec3 curPt = iRayOrigin + dir * curT;
    float curTerrainY = 0.f;
    Vec3 curNormal(0.f, 1.f, 0.f);
    const bool curValid = SampleHeightAndNormal(iTerrain, iRuntime, curPt.x, curPt.z, curTerrainY, curNormal);
    if ( curValid )
    {
      const float curDiff = curPt.y - curTerrainY;
      if ( ( prevValid && ( prevDiff >= 0.f ) && ( curDiff <= 0.f ) ) || ( std::abs(curDiff) <= 1e-4f ) )
      {
        // Binary search refinement between prevT and curT
        float lo = prevT;
        float hi = curT;
        for ( int iter = 0; iter < 8; ++iter )
        {
          const float mid = 0.5f * ( lo + hi );
          const Vec3 midPt = iRayOrigin + dir * mid;
          float midY = 0.f;
          Vec3 midN(0.f, 1.f, 0.f);
          if ( SampleHeightAndNormal(iTerrain, iRuntime, midPt.x, midPt.z, midY, midN) )
          {
            if ( midPt.y >= midY )
              lo = mid;
            else
              hi = mid;
          }
        }

        const float hitT = 0.5f * ( lo + hi );
        if ( ( hitT >= 0.f ) && ( hitT <= iMaxDistance ) )
        {
          const Vec3 hitPt = iRayOrigin + dir * hitT;
          float hitTerrainY = hitPt.y;
          Vec3 hitNormal(0.f, 1.f, 0.f);
          SampleHeightAndNormal(iTerrain, iRuntime, hitPt.x, hitPt.z, hitTerrainY, hitNormal);

          oResult._Hit = true;
          oResult._Distance = hitT;
          oResult._Point = Vec3(hitPt.x, hitTerrainY, hitPt.z);
          oResult._Normal = hitNormal;
          return true;
        }
      }
      prevDiff = curDiff;
    }
    prevT = curT;
    prevValid = curValid;
    if ( curT >= tExit )
      break;
  }

  return false;
}

// ----------------------------------------------------------------------------
// ApplyBrush
// ----------------------------------------------------------------------------
bool FpsTerrain::ApplyBrush( const FpsMapTerrain & iTerrain,
                             FpsTerrainRuntime & ioRuntime,
                             FpsTerrainBrushMode iMode,
                             const Vec3 & iWorldHitPoint,
                             float iBrushRadius,
                             float iBrushStrength,
                             float iDeltaTime,
                             float iFlattenTargetWorldY )
{
  if ( FpsTerrainBrushMode::Off == iMode )
    return false;

  const int nx = ioRuntime._GridResolution.x;
  const int nz = ioRuntime._GridResolution.y;
  if ( ( nx < 2 ) || ( nz < 2 ) || ( static_cast<int>(ioRuntime._GridHeights.size()) != nx * nz ) )
    return false;

  const float radius = std::max(0.1f, iBrushRadius);
  const float sizeX = std::max(0.1f, iTerrain._Size.x);
  const float sizeZ = std::max(0.1f, iTerrain._Size.y);
  const float halfX = sizeX * 0.5f;
  const float halfZ = sizeZ * 0.5f;
  const float elevSpan = std::max(0.01f, iTerrain._MaxElevation - iTerrain._MinElevation);

  const float hitLocalX = iWorldHitPoint.x - iTerrain._Center.x;
  const float hitLocalZ = iWorldHitPoint.z - iTerrain._Center.z;

  // Ensure _SourceHeights matches _GridResolution when sculpting so edits persist across RebuildGridFromSource
  if ( ( ioRuntime._SourceWidth != nx ) || ( ioRuntime._SourceHeight != nz ) || ( ioRuntime._SourceHeights.size() != ioRuntime._GridHeights.size() ) )
  {
    ioRuntime._SourceWidth = nx;
    ioRuntime._SourceHeight = nz;
    ioRuntime._SourceHeights = ioRuntime._GridHeights;
  }

  const std::vector<float> prevHeights = ioRuntime._GridHeights;
  const float flattenNormTarget = MathUtil::Clamp(( iFlattenTargetWorldY - iTerrain._Center.y - iTerrain._MinElevation ) / elevSpan, 0.f, 1.f);
  const float deltaRate = MathUtil::Clamp(iBrushStrength * std::max(0.005f, iDeltaTime), 0.001f, 0.5f);

  bool modified = false;
  for ( int z = 0; z < nz; ++z )
  {
    const float v = static_cast<float>(z) / static_cast<float>(nz - 1);
    const float localZ = -halfZ + v * sizeZ;
    const float dz = localZ - hitLocalZ;
    if ( std::abs(dz) > radius )
      continue;

    for ( int x = 0; x < nx; ++x )
    {
      const float u = static_cast<float>(x) / static_cast<float>(nx - 1);
      const float localX = -halfX + u * sizeX;
      const float dx = localX - hitLocalX;
      const float distSq = dx * dx + dz * dz;
      if ( distSq > radius * radius )
        continue;

      const float dist = std::sqrt(distSq);
      const float falloff = 1.f - SmoothStep(0.f, radius, dist);
      if ( falloff <= 0.f )
        continue;

      const size_t idx = static_cast<size_t>(z) * nx + x;
      float h = prevHeights[idx];

      switch ( iMode )
      {
      case FpsTerrainBrushMode::Raise:
        h = MathUtil::Clamp(h + deltaRate * falloff, 0.f, 1.f);
        break;
      case FpsTerrainBrushMode::Lower:
        h = MathUtil::Clamp(h - deltaRate * falloff, 0.f, 1.f);
        break;
      case FpsTerrainBrushMode::Smooth:
      {
        const int xPrev = std::max(0, x - 1);
        const int xNext = std::min(nx - 1, x + 1);
        const int zPrev = std::max(0, z - 1);
        const int zNext = std::min(nz - 1, z + 1);
        const float avg = ( prevHeights[static_cast<size_t>(z) * nx + xPrev]
                          + prevHeights[static_cast<size_t>(z) * nx + xNext]
                          + prevHeights[static_cast<size_t>(zPrev) * nx + x]
                          + prevHeights[static_cast<size_t>(zNext) * nx + x]
                          + prevHeights[idx] ) * 0.2f;
        h = MathUtil::Lerp(h, avg, MathUtil::Clamp(deltaRate * 4.f * falloff, 0.f, 1.f));
        break;
      }
      case FpsTerrainBrushMode::Flatten:
        h = MathUtil::Lerp(h, flattenNormTarget, MathUtil::Clamp(deltaRate * 3.f * falloff, 0.f, 1.f));
        break;
      case FpsTerrainBrushMode::Off:
      default:
        break;
      }

      ioRuntime._GridHeights[idx] = h;
      ioRuntime._SourceHeights[idx] = h;
      modified = true;
    }
  }

  if ( modified )
    RecomputeGridNormals(iTerrain, ioRuntime);

  return modified;
}

// ----------------------------------------------------------------------------
// SaveHeightmapPNG
// ----------------------------------------------------------------------------
bool FpsTerrain::SaveHeightmapPNG( const FpsTerrainRuntime & iRuntime,
                                   const std::string & iFilePath )
{
  const int width  = ( !iRuntime._GridHeights.empty() ) ? iRuntime._GridResolution.x : iRuntime._SourceWidth;
  const int height = ( !iRuntime._GridHeights.empty() ) ? iRuntime._GridResolution.y : iRuntime._SourceHeight;
  const std::vector<float> & heights = ( !iRuntime._GridHeights.empty() ) ? iRuntime._GridHeights : iRuntime._SourceHeights;

  if ( iFilePath.empty() || ( width <= 0 ) || ( height <= 0 ) || ( static_cast<int>(heights.size()) != width * height ) )
    return false;

  std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 255);
  for ( size_t i = 0; i < heights.size(); ++i )
  {
    const unsigned char val = static_cast<unsigned char>(std::round(MathUtil::Clamp(heights[i], 0.f, 1.f) * 255.f));
    pixels[i * 4 + 0] = val;
    pixels[i * 4 + 1] = val;
    pixels[i * 4 + 2] = val;
    pixels[i * 4 + 3] = 255;
  }

  std::error_code ec;
  const std::filesystem::path outPath(iFilePath);
  if ( outPath.has_parent_path() )
    std::filesystem::create_directories(outPath.parent_path(), ec);

  stbi_flip_vertically_on_write(false);
  return 0 != stbi_write_png(outPath.string().c_str(), width, height, 4, pixels.data(), width * 4);
}

}
