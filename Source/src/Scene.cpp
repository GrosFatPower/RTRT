#include "Scene.h"
#include "Mesh.h"
#include "Texture.h"
#include "stb_image.h"
#include "stb_image_resize.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <array>
#include <algorithm>

namespace RTRT
{

static constexpr std::array<int, S_TextureBucketCount> S_TextureBucketSizes = { 256, 512, 1024, 2048, 4096 };

// ----------------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Texture buckets : GetTextureBucketID
// ----------------------------------------------------------------------------
static int GetTextureBucketID( int iSize )
{
  for ( int i = 0; i < S_TextureBucketCount; ++i )
  {
    if ( iSize <= S_TextureBucketSizes[i] )
      return i;
  }
  return S_TextureBucketCount - 1;
}

// ----------------------------------------------------------------------------
// Texture buckets : GetTextureBucketLimit
// ----------------------------------------------------------------------------
static int GetTextureBucketLimit( Vec2i iTextureArraySize )
{
  int requestedSize = std::max(iTextureArraySize.x, iTextureArraySize.y);
  if ( requestedSize <= 0 )
    return S_TextureBucketSizes[S_TextureBucketCount - 1];

  int bucketID = 0;
  for ( int i = 0; i < S_TextureBucketCount; ++i )
  {
    if ( S_TextureBucketSizes[i] <= requestedSize )
      bucketID = i;
  }
  return S_TextureBucketSizes[bucketID];
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
Scene::Scene()
  : _Camera({0.f,0.f,-1.f}, {0.f,0.f,0.f}, 80.f)
{
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
Scene::~Scene()
{
  Clear();
}

// ----------------------------------------------------------------------------
// Clear
// ----------------------------------------------------------------------------
void Scene::Clear()
{
  for (auto & texture : _Textures)
    delete texture;
  _Textures.clear();

  for (auto & mesh : _Meshes)
    delete mesh;
  _Meshes.clear();

  for (auto & Primitive : _Primitives)
    delete Primitive;
  _Primitives.clear();

  _Camera = Camera();
  _EnvMap.Reset();

  _Lights.clear();
  _Materials.clear();
  _MaterialIDs.clear();
  _MeshInstances.clear();
  _PrimitiveNames.clear();
  _PrimitiveInstances.clear();

  _NbFaces = 0;
  _Vertices.clear();
  _Normals.clear();
  _UVMatID.clear();
  _Indices.clear();
  _NbCompiledTex = 0;
  _CompiledTextureBuckets = {};
  _TextureArrayMappings.clear();
  _MeshBBoxes.clear();
  _MeshIdxRange.clear();

  _TLAS = GpuTLAS();
  _TLASPackedTransforms.clear();
  _TLASPackedMeshMatID.clear();
  _BLASNodes.clear();
  _BLASNodesRange.clear();
  _BLASPackedIndices.clear();
  _BLASPackedIndicesRange.clear();
  _BLASPackedVertices.clear();
  _BLASPackedNormals.clear();
  _BLASPackedUVs.clear();
}

// ----------------------------------------------------------------------------
// AddTexture
// ----------------------------------------------------------------------------
int Scene::AddTexture( const std::string & iFilename, int iNbComponents, TexFormat iFormat )
{
  int texID = -1;

  for ( auto & tex : _Textures )
  {
    if ( tex && ( tex -> Filename() == iFilename ) )
    {
      texID = tex -> GetTexID();
      break;
    }
  }
  
  if ( texID < 0 )
  {
    Texture * texture = new Texture;

    std::cout << "Scene : Loading texture " << iFilename << std::endl;
    if ( texture -> Load(iFilename, iNbComponents, iFormat) )
    {
      texID = static_cast<int>(_Textures.size());
      texture -> SetTexID(texID);
      _Textures.push_back(texture);
    }
    else
    {
      delete texture;
      texture =  nullptr;
    }
  }

  if ( texID < 0 )
    std::cout << "Scene : ERROR. Unable to load texture " << iFilename << std::endl;

  return texID;
}

// ----------------------------------------------------------------------------
// AddTexture
// ----------------------------------------------------------------------------
int Scene::AddTexture( const std::string & iTexName, unsigned char * iTexData, int iWidth, int iHeight, int iNbComponents )
{
  int texID = this -> FindTextureID(iTexName);
  
  if ( texID < 0 )
  {
    std::cout << "Scene : Loading texture " << iTexName << std::endl;
    Texture * texture = new Texture(iTexName, iTexData, iWidth, iHeight, iNbComponents);

    texID = static_cast<int>(_Textures.size());
    texture -> SetTexID(texID);
    _Textures.push_back(texture);
  }

  if ( texID < 0 )
    std::cout << "Scene : ERROR. Unable to load texture " << iTexName << std::endl;

  return texID;
}

// ----------------------------------------------------------------------------
// AddMesh
// ----------------------------------------------------------------------------
int Scene::AddMesh( Mesh * iMesh )
{
  int meshID = -1;

  for ( auto& mesh : _Meshes )
  {
    if ( mesh && ( mesh -> Filename() == iMesh -> Filename() ) )
    {
      std::cout << "Scene : ERROR. Trying to load 2 meshes with same name " << iMesh -> Filename() << std::endl;
      return meshID;
    }
  }

  if ( meshID < 0 )
  {
    meshID = static_cast<int>(_Meshes.size());
    iMesh -> SetMeshID( meshID );
    _Meshes.push_back( iMesh );
  }

  return meshID;
}

// ----------------------------------------------------------------------------
// AddMesh
// ----------------------------------------------------------------------------
int Scene::AddMesh( const std::string & iFilename )
{
  int meshID = -1;

  for ( auto & mesh : _Meshes )
  {
    if ( mesh && ( mesh -> Filename() == iFilename ) )
    {
      meshID = mesh -> GetMeshID();
      break;
    }
  }

  if ( meshID < 0 )
  {
    Mesh * newMesh = new Mesh;

    std::cout << "Scene : Loading mesh " << iFilename << std::endl;
    if ( newMesh -> LoadOBJ(iFilename) )
    {
      meshID = static_cast<int>(_Meshes.size());
      newMesh -> SetMeshID(meshID);
      _Meshes.push_back(newMesh);
    }
    else
    {
      delete newMesh;
      newMesh =  nullptr;
    }
  }

  if ( meshID < 0 )
    std::cout << "Scene : ERROR. Unable to load mesh " << iFilename << std::endl;

  return meshID;
}

// ----------------------------------------------------------------------------
// AddMaterial
// ----------------------------------------------------------------------------
int Scene::AddMaterial( Material & ioMaterial, const std::string & iName )
{
  int matID = static_cast<int>(_Materials.size());
  ioMaterial._ID = (float)matID;
  _Materials.push_back(ioMaterial);

  _MaterialIDs.insert({iName, matID});

  return matID;
}

// ----------------------------------------------------------------------------
// AddMeshInstance
// ----------------------------------------------------------------------------
int Scene::AddMeshInstance( MeshInstance & iMeshInstance )
{
  int instanceID = static_cast<int>(_MeshInstances.size());
  _MeshInstances.push_back(iMeshInstance);
  return instanceID;
}

// ----------------------------------------------------------------------------
// FindMaterialID
// ----------------------------------------------------------------------------
int Scene::FindMaterialID( const std::string & iMateralName ) const
{
  int matID = -1;

  auto search = _MaterialIDs.find(iMateralName);
  if ( search != _MaterialIDs.end() )
    matID = search -> second;

  return matID;
}

// ----------------------------------------------------------------------------
// FindMaterialName
// ----------------------------------------------------------------------------
std::string Scene::FindMaterialName( int iMaterialID ) const
{
  for ( auto it = _MaterialIDs.begin(); it != _MaterialIDs.end(); ++it )
  {
    if ( it -> second == iMaterialID )
      return it -> first;
  }

  return "";
}

// ----------------------------------------------------------------------------
// FindTextureID
// ----------------------------------------------------------------------------
int Scene::FindTextureID( const std::string & iTextureName ) const
{
  int texID = -1;

  for ( auto & tex : _Textures )
  {
    if ( tex && ( tex -> Filename() == iTextureName ) )
    {
      texID = tex -> GetTexID();
      break;
    }
  }

  return texID;
}

// ----------------------------------------------------------------------------
// FindMeshID
// ----------------------------------------------------------------------------
int Scene::FindMeshID( const std::string& iMeshName ) const
{
  int meshID = -1;

  for ( auto & mesh : _Meshes )
  {
    if ( mesh && ( mesh -> Filename() == iMeshName ) )
    {
      meshID = mesh -> GetMeshID();
      break;
    }
  }

  return meshID;
}

// ----------------------------------------------------------------------------
// FindPrimitiveName
// ----------------------------------------------------------------------------
std::string Scene::FindPrimitiveName( int iPrimitiveInstanceID ) const
{
  for ( auto it = _PrimitiveNames.begin(); it != _PrimitiveNames.end(); ++it )
  {
    if ( it -> second == iPrimitiveInstanceID )
      return it -> first;
  }

  return "";
}

// ----------------------------------------------------------------------------
// AddPrimitive
// ----------------------------------------------------------------------------
int Scene::AddPrimitive( const Primitive & iPrimitive )
{
  int PrimitiveID = static_cast<int>(_Primitives.size());

  Primitive * newPrimitive = nullptr;
  if ( iPrimitive._Type == PrimitiveType::Sphere )
    newPrimitive = new Sphere(*((Sphere*)&iPrimitive));
  else if ( iPrimitive._Type == PrimitiveType::Plane )
    newPrimitive = new Plane(*((Plane*)&iPrimitive));
  else if ( iPrimitive._Type == PrimitiveType::Box )
    newPrimitive = new Box(*((Box*)&iPrimitive));

  if ( newPrimitive )
  {
    newPrimitive -> _PrimID = PrimitiveID;
    _Primitives.push_back(newPrimitive);
    return PrimitiveID;
  }
  return -1;
}

// ----------------------------------------------------------------------------
// AddPrimitiveInstance
// ----------------------------------------------------------------------------
int Scene::AddPrimitiveInstance( PrimitiveInstance & iPrimitiveInstance )
{
  int instanceID = static_cast<int>(_PrimitiveInstances.size());
  _PrimitiveInstances.push_back(iPrimitiveInstance);

  {
    Primitive * curPrimitive = _Primitives[iPrimitiveInstance._PrimID];

    std::string PrimitiveName;
    if ( curPrimitive -> _Type == PrimitiveType::Sphere )
      PrimitiveName = std::string("Sphere[").append(std::to_string(instanceID).append("]"));
    else if ( curPrimitive -> _Type == PrimitiveType::Plane )
      PrimitiveName = std::string("Plane[").append(std::to_string(instanceID).append("]"));
    else if ( curPrimitive -> _Type == PrimitiveType::Box )
      PrimitiveName = std::string("Box[").append(std::to_string(instanceID).append("]"));
    else
      PrimitiveName = std::string("Unknown[").append(std::to_string(instanceID).append("]"));

    _PrimitiveNames.insert({PrimitiveName, instanceID});
  }

  return instanceID;
}

// ----------------------------------------------------------------------------
// AddPrimitiveInstance
// ----------------------------------------------------------------------------
int Scene::AddPrimitiveInstance( int iPrimitiveID, int iMaterialID, const Mat4x4 & iTransform )
{
  std::string PrimitiveName;

  if ( iPrimitiveID >= 0 && iPrimitiveID < _Primitives.size() )
  {
    PrimitiveInstance instance(iPrimitiveID, iMaterialID, iTransform);
    return AddPrimitiveInstance(instance);
  }

  return -1;
}

// ----------------------------------------------------------------------------
// LoadEnvMap
// ----------------------------------------------------------------------------
bool Scene::LoadEnvMap( const std::string & iFilename )
{
  return _EnvMap.Load(iFilename);
}

// ----------------------------------------------------------------------------
// ResetEnvMap
// ----------------------------------------------------------------------------
void Scene::ResetEnvMap()
{
  return _EnvMap.Reset();
}

// ----------------------------------------------------------------------------
// RebuildTLASData
// ----------------------------------------------------------------------------
int Scene::RebuildTLASData()
{
  _TLAS.Clear();
  _TLASPackedTransforms.clear();
  _TLASPackedMeshMatID.clear();

  if ( 0 != _TLAS.Build(_Meshes, _MeshInstances) )
    return 1;

  for ( auto meshInst : _TLAS.GetPackedMeshInstances() )
  {
    _TLASPackedTransforms.emplace_back(meshInst._Transform);
    _TLASPackedMeshMatID.emplace_back(meshInst._MeshID, meshInst._MaterialID);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// CompileMeshData
// ----------------------------------------------------------------------------
void Scene::CompileMeshData( Vec2i iTextureArraySize, bool iBuildTextureArray, bool iBuildBVH )
{
  auto startTime = std::chrono::system_clock::now();

  _NbFaces = 0;
  _Vertices.clear();
  _Normals.clear();
  _UVMatID.clear();
  _Indices.clear();
  _NbCompiledTex = 0;
  _CompiledTextureBuckets = {};
  _TextureArrayMappings.clear();
  _MeshBBoxes.clear();
  _MeshIdxRange.clear();
  _TLAS.Clear();
  _TLASPackedTransforms.clear();
  _TLASPackedMeshMatID.clear();
  _BLASNodes.clear();
  _BLASNodesRange.clear();
  _BLASPackedIndices.clear();
  _BLASPackedIndicesRange.clear();
  _BLASPackedVertices.clear();
  _BLASPackedNormals.clear();
  _BLASPackedUVs.clear();

  // Geometry
  int vtxIndexOffset  = 0;
  int normIndexOffset = 0;
  int uvIndexOffset   = 0;
  for ( auto meshInst : _MeshInstances )
  {
    if ( !meshInst._Visible )
      continue;
    if ( ( meshInst._MeshID < 0 ) || ( meshInst._MeshID >= static_cast<int>(_Meshes.size()) ) )
      continue;

    Mesh * curMesh = _Meshes[meshInst._MeshID];
    if ( !curMesh || !curMesh -> GetNbFaces() )
      continue;

    const std::vector<Vec3>  & curVertices = curMesh -> GetVertices();
    const std::vector<Vec3>  & curNormals  = curMesh -> GetNormals();
    const std::vector<Vec2>  & curUVs      = curMesh -> GetUVs();
    const std::vector<Vec3i> & curIndices  = curMesh -> GetIndices();

    std::vector<Vec3> transformedVertices;
    std::vector<Vec3> transformedNormals;
    std::vector<Vec3> uvMatIDs;
    std::vector<Vec3i> offsetIdx;
    transformedVertices.resize(curVertices.size());
    transformedNormals.resize(curNormals.size());
    uvMatIDs.resize(curUVs.size());
    offsetIdx.resize(curIndices.size());

    Vec3 low(0.f), high(0.f);
    for ( int i = 0; i < curVertices.size(); ++i )
    {
      Vec4 transformedVtx = meshInst._Transform * Vec4(curVertices[i], 1.f);
      transformedVertices[i] = { transformedVtx[0], transformedVtx[1], transformedVtx[2] };

      if ( i )
      {
        if ( transformedVtx.x < low.x )
          low.x = transformedVtx.x;
        if ( transformedVtx.y < low.y )
          low.y = transformedVtx.y;
        if ( transformedVtx.z < low.z )
          low.z = transformedVtx.z;
        if ( transformedVtx.x > high.x )
          high.x = transformedVtx.x;
        if ( transformedVtx.y > high.y )
          high.y = transformedVtx.y;
        if ( transformedVtx.z > high.z )
          high.z = transformedVtx.z;
      }
      else
        low = high = Vec3(transformedVtx);
    }

    Mat4x4 trInvTransfo = glm::transpose(glm::inverse(meshInst._Transform));
    for ( int i = 0; i < curNormals.size(); ++i )
    {
      Vec4 transformedNormal = trInvTransfo * Vec4(curNormals[i], 1.f);
      if ( transformedNormal.w != 0.f )
        transformedNormal /= transformedNormal.w;
      transformedNormals[i] = transformedNormal;
      transformedNormals[i] = glm::normalize(transformedNormals[i]);
    }

    if ( curUVs.size() )
    {
      for ( int i = 0; i < curUVs.size(); ++i )
      {
        uvMatIDs[i] = { curUVs[i].x, curUVs[i].y, (float)meshInst._MaterialID };
      }
    }
    else
    {
      uvMatIDs.push_back({ 0.f, 0.f, (float)meshInst._MaterialID });
    }

    for ( int i = 0; i < curIndices.size(); ++i )
    {
      if ( curUVs.size() )
        offsetIdx[i] = { curIndices[i].x + vtxIndexOffset, curIndices[i].y + normIndexOffset, curIndices[i].z + uvIndexOffset };
      else
        offsetIdx[i] = { curIndices[i].x + vtxIndexOffset, curIndices[i].y + normIndexOffset, (uvMatIDs.size()-1)  + uvIndexOffset };
    }

    _MeshBBoxes.push_back(low);
    _MeshBBoxes.push_back(high);
    _MeshIdxRange.push_back(static_cast<int>(_Indices.size()));
    _MeshIdxRange.push_back(static_cast<int>(_Indices.size() + curIndices.size() - 1));

    _Vertices.insert(std::end(_Vertices), std::begin(transformedVertices), std::end(transformedVertices));
    _Normals.insert(std::end(_Normals), std::begin(transformedNormals), std::end(transformedNormals));
    _UVMatID.insert(std::end(_UVMatID), std::begin(uvMatIDs), std::end(uvMatIDs));
    _Indices.insert(std::end(_Indices), std::begin(offsetIdx), std::end(offsetIdx));

    vtxIndexOffset  += static_cast<int>(transformedVertices.size());
    normIndexOffset += static_cast<int>(transformedNormals.size());
    uvIndexOffset   += static_cast<int>(uvMatIDs.size());
  }

  _NbFaces = static_cast<int>(_Indices.size() / 3);

  // Textures
  if ( iBuildTextureArray )
  {
    std::vector<Texture*> MatTextures;
    for ( int i = 0; i < _Materials.size(); ++i )
    {
      int baseColorTexId         = (int) _Materials[i]._BaseColorTexId;
      int metallicRoughnessTexID = (int) _Materials[i]._MetallicRoughnessTexID;
      int normalMapTexID         = (int) _Materials[i]._NormalMapTexID;
      int emissionMapTexID       = (int) _Materials[i]._EmissionMapTexID;

      if ( ( baseColorTexId >= 0 ) && _Textures[baseColorTexId] && ( _Textures[baseColorTexId] -> GetTexID() == baseColorTexId ) )
      {
        if ( std::find(MatTextures.begin(), MatTextures.end(), _Textures[baseColorTexId]) == MatTextures.end() )
          MatTextures.push_back(_Textures[baseColorTexId]);
      }
      if ( ( metallicRoughnessTexID >= 0 ) && _Textures[metallicRoughnessTexID] && ( _Textures[metallicRoughnessTexID] -> GetTexID() == metallicRoughnessTexID ) )
      {
        if ( std::find(MatTextures.begin(), MatTextures.end(), _Textures[metallicRoughnessTexID]) == MatTextures.end() )
          MatTextures.push_back(_Textures[metallicRoughnessTexID]);
      }
      if ( ( normalMapTexID >= 0 ) && _Textures[normalMapTexID] && ( _Textures[normalMapTexID] -> GetTexID() == normalMapTexID ) )
      {
        if ( std::find(MatTextures.begin(), MatTextures.end(), _Textures[normalMapTexID]) == MatTextures.end() )
          MatTextures.push_back(_Textures[normalMapTexID]);
      }
      if ( ( emissionMapTexID >= 0 ) && _Textures[emissionMapTexID] && ( _Textures[emissionMapTexID] -> GetTexID() == emissionMapTexID ) )
      {
        if ( std::find(MatTextures.begin(), MatTextures.end(), _Textures[emissionMapTexID]) == MatTextures.end() )
          MatTextures.push_back(_Textures[emissionMapTexID]);
      }
    }

    if ( MatTextures.size() )
    {
      const int maxBucketSize = GetTextureBucketLimit(iTextureArraySize);
      _TextureArrayMappings.resize(_Textures.size());
      for ( int i = 0; i < S_TextureBucketCount; ++i )
        _CompiledTextureBuckets[i]._Size = S_TextureBucketSizes[i];

      for ( Texture * curTexture : MatTextures )
      {
        // 4 component-uchar only
        if ( 4 != curTexture -> GetNbComponents() )
          continue;
        unsigned char * texUCData = curTexture -> GetUCData();
        if ( !texUCData )
          continue;

        const int sourceWidth = curTexture -> GetWidth();
        const int sourceHeight = curTexture -> GetHeight();
        const int sourceSize = std::max(sourceWidth, sourceHeight);
        if ( ( sourceWidth <= 0 ) || ( sourceHeight <= 0 ) )
          continue;

        const int targetSize = std::min(sourceSize, maxBucketSize);
        const int bucketID = GetTextureBucketID(targetSize);
        CompiledTextureBucket & bucket = _CompiledTextureBuckets[bucketID];
        const int bucketSize = bucket._Size;
        const float resizeScale = std::min(1.f, float(bucketSize) / float(sourceSize));
        const int contentWidth = std::max(1, int(std::round(float(sourceWidth) * resizeScale)));
        const int contentHeight = std::max(1, int(std::round(float(sourceHeight) * resizeScale)));

        std::vector<unsigned char> resizedPixels;
        const unsigned char * sourcePixels = texUCData;
        if ( ( contentWidth != sourceWidth ) || ( contentHeight != sourceHeight ) )
        {
          resizedPixels.resize(contentWidth * contentHeight * 4);
          stbir_resize_uint8(texUCData, sourceWidth, sourceHeight, 0, resizedPixels.data(), contentWidth, contentHeight, 0, 4);
          sourcePixels = resizedPixels.data();
        }

        int texID = curTexture -> GetTexID();
        if ( ( texID < 0 ) || ( texID >= static_cast<int>(_TextureArrayMappings.size()) ) )
          continue;

        const int layerSize = bucketSize * bucketSize * 4;
        const int layerID = bucket._LayerCount++;
        const size_t layerOffset = static_cast<size_t>(layerID) * layerSize;
        bucket._Pixels.resize(layerOffset + layerSize, 0);
        unsigned char * layerPixels = bucket._Pixels.data() + layerOffset;
        for ( int y = 0; y < contentHeight; ++y )
        {
          unsigned char * dstRow = layerPixels + ( y * bucketSize * 4 );
          const unsigned char * srcRow = sourcePixels + ( y * contentWidth * 4 );
          std::copy(srcRow, srcRow + contentWidth * 4, dstRow);
          for ( int x = contentWidth; x < bucketSize; ++x )
            std::copy(dstRow + ( contentWidth - 1 ) * 4, dstRow + contentWidth * 4, dstRow + x * 4);
        }
        for ( int y = contentHeight; y < bucketSize; ++y )
        {
          unsigned char * dstRow = layerPixels + ( y * bucketSize * 4 );
          const unsigned char * srcRow = layerPixels + ( contentHeight - 1 ) * bucketSize * 4;
          std::copy(srcRow, srcRow + bucketSize * 4, dstRow);
        }

        _TextureArrayMappings[texID] = { bucketID, layerID, contentWidth, contentHeight };
        _NbCompiledTex++;
      }

      size_t baseMemory = 0;
      std::cout << "Scene : Texture buckets";
      for ( const CompiledTextureBucket & bucket : _CompiledTextureBuckets )
      {
        if ( bucket._LayerCount <= 0 )
          continue;
        std::cout << " " << bucket._Size << "=" << bucket._LayerCount;
        baseMemory += bucket._Pixels.size();
      }
      std::cout << " (" << _NbCompiledTex << " textures, " << ( baseMemory / ( 1024 * 1024 ) ) << " MiB base, " << ( baseMemory * 4 / 3 / ( 1024 * 1024 ) ) << " MiB with mips)" << std::endl;
    }
  }

  // BVH
  if ( iBuildBVH )
  {
    if ( 0 != RebuildTLASData() )
      std::cout << "Scene : ERROR. Unable to rebuild TLAS data" << std::endl;

    for (auto & mesh : _Meshes)
    {
      mesh -> BuildBvh();

      std::shared_ptr<GpuBLAS> curBLAS = mesh -> GetBvh();

      int nbNodes = static_cast<int>(curBLAS -> _Nodes.size());
      int startIdx = static_cast<int>(_BLASNodes.size());
      _BLASNodes.insert(_BLASNodes.end(), curBLAS -> _Nodes.begin(), curBLAS -> _Nodes.end());
      _BLASNodesRange.emplace_back(startIdx, nbNodes);

      const std::vector<Vec3>  & curVertices = mesh -> GetVertices();
      const std::vector<Vec3>  & curNormals  = mesh -> GetNormals();
      const std::vector<Vec2>  & curUVs      = mesh -> GetUVs();
  
      const Vec3i offset(_BLASPackedVertices.size(), _BLASPackedNormals.size(), _BLASPackedUVs.size());
      _BLASPackedVertices.insert(_BLASPackedVertices.end(), curVertices.begin(), curVertices.end());
      _BLASPackedNormals. insert(_BLASPackedNormals.end(),  curNormals.begin(),  curNormals.end());
      _BLASPackedUVs.     insert(_BLASPackedUVs.end(),      curUVs.begin(),      curUVs.end());

      int nbIndices = static_cast<int>(curBLAS -> GetPackedTriangleIdx().size());
      startIdx = static_cast<int>(_BLASPackedIndices.size());
      for (auto & indices : curBLAS -> GetPackedTriangleIdx())
        _BLASPackedIndices.emplace_back(indices + offset);
      _BLASPackedIndicesRange.emplace_back(startIdx, nbIndices);
    }
  }

  auto endTime = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count();
  std::cout << "Scene compiled in " << elapsed << "ms\n";
}

}
