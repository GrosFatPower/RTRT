#include "Scene.h"
#include "Mesh.h"
#include "Texture.h"
#include "stb_image.h"
#include "stb_image_resize.h"
#include <iostream>
#include <memory>

namespace RTRT
{

Scene::Scene()
  : _Camera({0.f,0.f,-1.f}, {0.f,0.f,0.f}, 80.f)
{
}

Scene::~Scene()
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
}

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
      texID = _Textures.size();
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
    if ( newMesh -> Load(iFilename) )
    {
      meshID = _Meshes.size();
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

int Scene::AddMaterial( Material & ioMaterial, const std::string & iName )
{
  int matID = _Materials.size();
  ioMaterial._ID = (float)matID;
  _Materials.push_back(ioMaterial);

  _MaterialIDs.insert({iName, matID});

  return matID;
}

int Scene::AddMeshInstance( MeshInstance & iMeshInstance )
{
  int instanceID = _MeshInstances.size();
  _MeshInstances.push_back(iMeshInstance);
  return instanceID;
}

int Scene::FindMaterialID( const std::string & iMateralName ) const
{
  int matID = -1;

  auto search = _MaterialIDs.find(iMateralName);
  if ( search != _MaterialIDs.end() )
    matID = search -> second;

  return matID;
}

std::string Scene::FindMaterialName( int iMaterialID ) const
{
  for ( auto it = _MaterialIDs.begin(); it != _MaterialIDs.end(); ++it )
  {
    if ( it -> second == iMaterialID )
      return it -> first;
  }

  return "";
}

std::string Scene::FindPrimitiveName( int iPrimitiveInstanceID ) const
{
  for ( auto it = _PrimitiveNames.begin(); it != _PrimitiveNames.end(); ++it )
  {
    if ( it -> second == iPrimitiveInstanceID )
      return it -> first;
  }

  return "";
}

int Scene::AddPrimitive( const Primitive & iPrimitive )
{
  int PrimitiveID = _Primitives.size();

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

int Scene::AddPrimitiveInstance( PrimitiveInstance & iPrimitiveInstance )
{
  int instanceID = _PrimitiveInstances.size();
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

int Scene::AddPrimitiveInstance( int iPrimitiveID, int iMaterialID, const Mat4x4 & iTransform )
{
  std::string PrimitiveName;

  if ( iPrimitiveID >= 0 && iPrimitiveID < _Primitives.size() )
  {
    Primitive * curPrimitive = _Primitives[iPrimitiveID];

    PrimitiveInstance instance(iPrimitiveID, iMaterialID, iTransform);
    return AddPrimitiveInstance(instance);
  }

  return -1;
}

void Scene::CompileMeshData( Vec2i iTextureArraySize, bool iBuildTextureArray, bool iBuildBVH )
{
  _NbFaces = 0;
  _Vertices.clear();
  _Normals.clear();
  _UVMatID.clear();
  _Indices.clear();

  // Geometry
  int vtxIndexOffset  = 0;
  int normIndexOffset = 0;
  int uvIndexOffset   = 0;
  for ( auto meshInst : _MeshInstances )
  {
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

    Vec3 low, high;
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
    _MeshIdxRange.push_back(_Indices.size());
    _MeshIdxRange.push_back(_Indices.size() + curIndices.size() - 1);

    _Vertices.insert(std::end(_Vertices), std::begin(transformedVertices), std::end(transformedVertices));
    _Normals.insert(std::end(_Normals), std::begin(transformedNormals), std::end(transformedNormals));
    _UVMatID.insert(std::end(_UVMatID), std::begin(uvMatIDs), std::end(uvMatIDs));
    _Indices.insert(std::end(_Indices), std::begin(offsetIdx), std::end(offsetIdx));

    vtxIndexOffset  += transformedVertices.size();
    normIndexOffset += transformedNormals.size();
    uvIndexOffset   += uvMatIDs.size();
  }

  _NbFaces = _Indices.size() / 3;

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
      _TextureArrayIDs = std::vector<int>(_Textures.size(), -1);

      int textureUCSize = iTextureArraySize.x * iTextureArraySize.y * 4;
      _TextureArray.resize(textureUCSize * MatTextures.size());

      _NbCompiledTex = 0;
      for ( int i = 0; i < MatTextures.size(); ++i )
      {
        Texture * curTexture = MatTextures[i];

        // 4 component-uchar only
        if ( 4 != curTexture -> GetNbComponents() )
          continue;
        unsigned char * texUCData = curTexture -> GetUCData();
        if ( !texUCData )
          continue;
      
        // Resize if necessary
        if ( ( curTexture -> GetWidth() != iTextureArraySize.x ) || ( curTexture -> GetHeight() != iTextureArraySize.y ) )
        {
          unsigned char * resizedTex = new unsigned char[textureUCSize];
          stbir_resize_uint8(texUCData, curTexture -> GetWidth(), curTexture -> GetHeight(), 0, resizedTex, iTextureArraySize.x, iTextureArraySize.y, 0, 4);
          std::copy(resizedTex, resizedTex + textureUCSize, &_TextureArray[_NbCompiledTex * textureUCSize]);
          delete[] resizedTex;
        }
        else
            std::copy(texUCData, texUCData + textureUCSize, &_TextureArray[_NbCompiledTex * textureUCSize]);

        int texID = curTexture -> GetTexID();
        _TextureArrayIDs[texID] = _NbCompiledTex;
        _NbCompiledTex++;
      }
    }
  }

  // BVH
  if ( iBuildBVH )
  {
    _TLAS.Build(_Meshes, _MeshInstances);

    for ( auto meshInst : _TLAS.GetPackedMeshInstances() )
    {
      _TLASPackedTransforms.emplace_back(meshInst._Transform);
      _TLASPackedMeshMatID.emplace_back(meshInst._MeshID, meshInst._MaterialID);
    }

    for (auto & mesh : _Meshes)
    {
      mesh -> BuildBvh();

      std::shared_ptr<GpuBLAS> curBLAS = mesh -> GetBvh();

      int nbNodes = curBLAS -> GetNodes().size();
      int startIdx = _BLASNodes.size();
      _BLASNodes.insert(_BLASNodes.end(), curBLAS -> GetNodes().begin(), curBLAS -> GetNodes().end());
      _BLASNodesRange.emplace_back(startIdx, nbNodes);

      const std::vector<Vec3>  & curVertices = mesh -> GetVertices();
      const std::vector<Vec3>  & curNormals  = mesh -> GetNormals();
      const std::vector<Vec2>  & curUVs      = mesh -> GetUVs();
  
      const Vec3i offset(_BLASPackedVertices.size(), _BLASPackedNormals.size(), _BLASPackedUVs.size());
      _BLASPackedVertices.insert(_BLASPackedVertices.end(), curVertices.begin(), curVertices.end());
      _BLASPackedNormals. insert(_BLASPackedNormals.end(),  curNormals.begin(),  curNormals.end());
      _BLASPackedUVs.     insert(_BLASPackedUVs.end(),      curUVs.begin(),      curUVs.end());

      int nbIndices = curBLAS -> GetPackedTriangleIdx().size();
      startIdx = _BLASPackedIndices.size();
      for (auto & indices : curBLAS -> GetPackedTriangleIdx())
        _BLASPackedIndices.emplace_back(indices + offset);
      _BLASPackedIndicesRange.emplace_back(startIdx, nbIndices);
    }
  }

}

}
