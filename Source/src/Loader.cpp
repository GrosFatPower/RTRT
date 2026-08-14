/*
  This is a modified version of the original code : https://github.com/knightcrawler25/GLSL-PathTracer
  already derived from: https://github.com/mmacklin/tinsel
*/

#pragma warning(disable : 4100) // unreferenced formal parameter

#include "Loader.h"
#include "Scene.h"
#include "Mesh.h"
#include "Primitive.h"
#include "Light.h"
#include "Camera.h"
#include "RenderSettings.h"
#include <map>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem> // C++17
#include <sstream>
#include <fstream>
#include <set>

#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

namespace fs = std::filesystem;

namespace RTRT
{

enum class State
{
  ExpectNewBlock,
  ExpectOpenBracket,
  ExpectClosingBracket,
};

// ----------------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------------
void Tokenize( std::string iStr, std::vector<std::string> & oTokens )
{
  oTokens.clear();

  //iStr.erase(std::remove(iStr.begin(), iStr.end(), '\t'), iStr.end());
  std::replace(iStr.begin(), iStr.end(), '\t', ' ');

  std::istringstream iss(iStr);
  {
    std::string token;
    while ( std::getline(iss, token, ' ') )
    {
      if ( token.size() )
        oTokens.push_back(token);
    }
  }
}

bool CompareChar( char iC1, char iC2 )
{
  if ( iC1 == iC2 )
    return true;
  else if ( std::toupper(iC1) == std::toupper(iC2) )
    return true;
  return false;
}

 // Case insensitive string comparison
bool IsEqual( const std::string & iStr1, const std::string & iStr2 )
{
  return ( ( iStr1.size() == iStr2.size() )
        && std::equal(iStr1.begin(), iStr1.end(), iStr2.begin(), &CompareChar) );
}
// ----------------------------------------------------------------------------
// GLTF loader : GltfDiagnostics
// ----------------------------------------------------------------------------
class GltfDiagnostics
{
public:
  void Warn( const std::string & iMessage )
  {
    if ( _Warnings.insert(iMessage).second )
      std::cout << "GLTF warning: " << iMessage << std::endl;
  }

private:
  std::set<std::string> _Warnings;
};

// ----------------------------------------------------------------------------
// GLTF loader : IsValidIndex
// ----------------------------------------------------------------------------
bool IsValidIndex( int iIndex, size_t iCount )
{
  return ( iIndex >= 0 ) && ( iIndex < static_cast<int>(iCount) );
}

// ----------------------------------------------------------------------------
// GLTF loader : GetBufferData
// ----------------------------------------------------------------------------
bool GetBufferData( const tinygltf::Model & iModel, int iBufferViewIndex, size_t iOffset, size_t iSize, const uint8_t * & oData )
{
  if ( !IsValidIndex(iBufferViewIndex, iModel.bufferViews.size()) )
    return false;

  const tinygltf::BufferView & bufferView = iModel.bufferViews[iBufferViewIndex];
  if ( !IsValidIndex(bufferView.buffer, iModel.buffers.size()) )
    return false;

  const std::vector<unsigned char> & buffer = iModel.buffers[bufferView.buffer].data;
  if ( ( iOffset > bufferView.byteLength ) || ( iSize > ( bufferView.byteLength - iOffset ) ) )
    return false;
  const size_t begin = bufferView.byteOffset + iOffset;
  if ( ( begin > buffer.size() ) || ( iSize > ( buffer.size() - begin ) ) )
    return false;

  oData = buffer.data() + begin;
  return true;
}

// ----------------------------------------------------------------------------
// GLTF loader : ReadComponent
// ----------------------------------------------------------------------------
bool ReadComponent( const uint8_t * iData, int iComponentType, bool iNormalized, double & oValue )
{
  switch ( iComponentType )
  {
  case TINYGLTF_COMPONENT_TYPE_BYTE:
    { int8_t value; memcpy(&value, iData, sizeof(value)); oValue = iNormalized ? std::max(-1.0, value / 127.0) : value; return true; }
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
    { uint8_t value; memcpy(&value, iData, sizeof(value)); oValue = iNormalized ? value / 255.0 : value; return true; }
  case TINYGLTF_COMPONENT_TYPE_SHORT:
    { int16_t value; memcpy(&value, iData, sizeof(value)); oValue = iNormalized ? std::max(-1.0, value / 32767.0) : value; return true; }
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
    { uint16_t value; memcpy(&value, iData, sizeof(value)); oValue = iNormalized ? value / 65535.0 : value; return true; }
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    { uint32_t value; memcpy(&value, iData, sizeof(value)); oValue = iNormalized ? value / 4294967295.0 : value; return true; }
  case TINYGLTF_COMPONENT_TYPE_FLOAT:
    { float value; memcpy(&value, iData, sizeof(value)); oValue = value; return true; }
  default:
    return false;
  }
}

// ----------------------------------------------------------------------------
// GLTF loader : ReadAccessor
// ----------------------------------------------------------------------------
bool ReadAccessor( const tinygltf::Model & iModel, const tinygltf::Accessor & iAccessor, std::vector<double> & oValues )
{
  const int componentSize = tinygltf::GetComponentSizeInBytes(iAccessor.componentType);
  const int componentCount = tinygltf::GetNumComponentsInType(iAccessor.type);
  if ( ( componentSize <= 0 ) || ( componentCount <= 0 ) )
    return false;

  oValues.assign( iAccessor.count * componentCount, 0.0 );
  const size_t elementSize = static_cast<size_t>(componentSize * componentCount);
  if ( iAccessor.bufferView >= 0 )
  {
    if ( !IsValidIndex(iAccessor.bufferView, iModel.bufferViews.size()) )
      return false;
    const tinygltf::BufferView & bufferView = iModel.bufferViews[iAccessor.bufferView];
    const int stride = iAccessor.ByteStride(bufferView);
    if ( ( stride < static_cast<int>(elementSize) ) || ( 0 == iAccessor.count ) )
      return ( 0 == iAccessor.count );

    const uint8_t * data = nullptr;
    const size_t dataSize = iAccessor.byteOffset + static_cast<size_t>(stride) * ( iAccessor.count - 1 ) + elementSize;
    if ( !GetBufferData(iModel, iAccessor.bufferView, 0, dataSize, data) )
      return false;

    for ( size_t element = 0; element < iAccessor.count; ++element )
    {
      const uint8_t * source = data + iAccessor.byteOffset + static_cast<size_t>(stride) * element;
      for ( int component = 0; component < componentCount; ++component )
      {
        if ( !ReadComponent(source + component * componentSize, iAccessor.componentType, iAccessor.normalized, oValues[element * componentCount + component]) )
          return false;
      }
    }
  }
  else if ( !iAccessor.sparse.isSparse )
    return false;

  if ( !iAccessor.sparse.isSparse )
    return true;

  const tinygltf::Accessor::Sparse & sparse = iAccessor.sparse;
  if ( ( sparse.count < 0 ) || ( static_cast<size_t>(sparse.count) > iAccessor.count ) )
    return false;
  const int sparseIndexSize = tinygltf::GetComponentSizeInBytes(sparse.indices.componentType);
  if ( ( sparseIndexSize <= 0 )
    || ( ( sparse.indices.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE )
      && ( sparse.indices.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT )
      && ( sparse.indices.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ) ) )
    return false;

  const uint8_t * sparseIndices = nullptr;
  const uint8_t * sparseValues = nullptr;
  if ( !GetBufferData(iModel, sparse.indices.bufferView, sparse.indices.byteOffset, static_cast<size_t>(sparse.count) * sparseIndexSize, sparseIndices)
    || !GetBufferData(iModel, sparse.values.bufferView, sparse.values.byteOffset, static_cast<size_t>(sparse.count) * elementSize, sparseValues) )
    return false;

  for ( int sparseElement = 0; sparseElement < sparse.count; ++sparseElement )
  {
    double sparseIndexValue = 0.0;
    if ( !ReadComponent(sparseIndices + sparseElement * sparseIndexSize, sparse.indices.componentType, false, sparseIndexValue) )
      return false;
    const size_t element = static_cast<size_t>(sparseIndexValue);
    if ( element >= iAccessor.count )
      return false;

    const uint8_t * source = sparseValues + static_cast<size_t>(sparseElement) * elementSize;
    for ( int component = 0; component < componentCount; ++component )
    {
      if ( !ReadComponent(source + component * componentSize, iAccessor.componentType, iAccessor.normalized, oValues[element * componentCount + component]) )
        return false;
    }
  }

  return true;
}

// ----------------------------------------------------------------------------
// GLTF loader : ReadVec3Accessor
// ----------------------------------------------------------------------------
bool ReadVec3Accessor( const tinygltf::Model & iModel, int iAccessorIndex, std::vector<Vec3> & oValues )
{
  if ( !IsValidIndex(iAccessorIndex, iModel.accessors.size()) )
    return false;
  const tinygltf::Accessor & accessor = iModel.accessors[iAccessorIndex];
  if ( TINYGLTF_TYPE_VEC3 != accessor.type )
    return false;

  std::vector<double> values;
  if ( !ReadAccessor(iModel, accessor, values) )
    return false;
  oValues.resize(accessor.count);
  for ( size_t i = 0; i < accessor.count; ++i )
    oValues[i] = Vec3( static_cast<float>(values[3 * i]), static_cast<float>(values[3 * i + 1]), static_cast<float>(values[3 * i + 2]) );
  return true;
}

// ----------------------------------------------------------------------------
// GLTF loader : ReadVec2Accessor
// ----------------------------------------------------------------------------
bool ReadVec2Accessor( const tinygltf::Model & iModel, int iAccessorIndex, std::vector<Vec2> & oValues )
{
  if ( !IsValidIndex(iAccessorIndex, iModel.accessors.size()) )
    return false;
  const tinygltf::Accessor & accessor = iModel.accessors[iAccessorIndex];
  if ( TINYGLTF_TYPE_VEC2 != accessor.type )
    return false;

  std::vector<double> values;
  if ( !ReadAccessor(iModel, accessor, values) )
    return false;
  oValues.resize(accessor.count);
  for ( size_t i = 0; i < accessor.count; ++i )
    oValues[i] = Vec2( static_cast<float>(values[2 * i]), static_cast<float>(values[2 * i + 1]) );
  return true;
}

// ----------------------------------------------------------------------------
// GLTF loader : ReadIndices
// ----------------------------------------------------------------------------
bool ReadIndices( const tinygltf::Model & iModel, int iAccessorIndex, size_t iVertexCount, std::vector<int> & oIndices )
{
  if ( !IsValidIndex(iAccessorIndex, iModel.accessors.size()) )
    return false;
  const tinygltf::Accessor & accessor = iModel.accessors[iAccessorIndex];
  if ( ( TINYGLTF_TYPE_SCALAR != accessor.type )
    || ( ( accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE )
      && ( accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT )
      && ( accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ) ) )
    return false;

  std::vector<double> values;
  if ( !ReadAccessor(iModel, accessor, values) )
    return false;
  oIndices.resize(accessor.count);
  for ( size_t i = 0; i < accessor.count; ++i )
  {
    if ( ( values[i] < 0.0 ) || ( values[i] >= static_cast<double>(iVertexCount) ) )
      return false;
    oIndices[i] = static_cast<int>(values[i]);
  }
  return true;
}

// ----------------------------------------------------------------------------
// GLTF loader : WarnTextureInfoLimitations
// ----------------------------------------------------------------------------
void WarnTextureInfoLimitations( const tinygltf::TextureInfo & iTextureInfo, GltfDiagnostics & ioDiagnostics )
{
  if ( iTextureInfo.texCoord > 0 )
    ioDiagnostics.Warn("non-zero texture coordinate sets are ignored");
  if ( iTextureInfo.extensions.find("KHR_texture_transform") != iTextureInfo.extensions.end() )
    ioDiagnostics.Warn("KHR_texture_transform is ignored");
}

// ----------------------------------------------------------------------------
// GLTF loader : GenerateNormals
// ----------------------------------------------------------------------------
void GenerateNormals( const std::vector<Vec3> & iVertices, const std::vector<int> & iIndices, std::vector<Vec3> & oNormals )
{
  oNormals.assign(iVertices.size(), Vec3(0.f));
  for ( size_t i = 0; i + 2 < iIndices.size(); i += 3 )
  {
    const int i0 = iIndices[i], i1 = iIndices[i + 1], i2 = iIndices[i + 2];
    const Vec3 normal = glm::cross(iVertices[i1] - iVertices[i0], iVertices[i2] - iVertices[i0]);
    oNormals[i0] += normal; oNormals[i1] += normal; oNormals[i2] += normal;
  }
  for ( Vec3 & normal : oNormals )
    normal = ( glm::dot(normal, normal) > 0.f ) ? glm::normalize(normal) : Vec3(0.f, 1.f, 0.f);
}

// ----------------------------------------------------------------------------
// GLTF loader : GetTextureName
// ----------------------------------------------------------------------------
void GetTextureName( const tinygltf::Model & iGltfModel, const tinygltf::Texture & iGltfTex, std::string & oTexName )
{
  oTexName = iGltfTex.name;

  if ( iGltfTex.name.empty() )
  {
    const tinygltf::Image & image = iGltfModel.images[iGltfTex.source];
    if ( !image.name.empty() )
      oTexName = image.name;
    else
      oTexName = image.uri;
  }
}

// ----------------------------------------------------------------------------
// GLTF loader : GetMaterialName
// ----------------------------------------------------------------------------
void GetMaterialName( const tinygltf::Model & iGltfModel, const tinygltf::Material & iGltfMat, int iIndMat, std::string & oMatName )
{
  oMatName = iGltfMat.name;

  if ( iGltfMat.name.empty() )
    oMatName = "Material_" + std::to_string( iIndMat );
}

// ----------------------------------------------------------------------------
// GLTF loader : GetMeshName
// ----------------------------------------------------------------------------
void GetMeshName( const tinygltf::Mesh & iGltfMesh, int iIndMesh, int iIndPrim, std::string& oMeshName )
{
  oMeshName = iGltfMesh.name;

  if ( oMeshName.empty() )
    oMeshName = "Mesh_" + std::to_string( iIndMesh );
  else
    oMeshName += "_Mesh" + std::to_string(iIndMesh);

  if ( iGltfMesh.primitives.size() > 1 )
  {
    oMeshName += "_Prim";
    oMeshName += std::to_string(iIndPrim);
  }
}

// ----------------------------------------------------------------------------
// GLTF loader : GetLocalTransfo
// ----------------------------------------------------------------------------
void GetLocalTransfo( const tinygltf::Node & iGltfNode, Mat4x4 & oLocalTransfoMat )
{
  if ( iGltfNode.matrix.size() > 0 )
  {
    oLocalTransfoMat = glm::make_mat4(iGltfNode.matrix.data());
  }
  else
  {
    Mat4x4 translate( 1.f ), rot( 1.f ), scale( 1.f );

    if ( iGltfNode.translation.size() > 0 )
    {
      translate = glm::translate(Mat4x4(1.f), Vec3(iGltfNode.translation[0], iGltfNode.translation[1], iGltfNode.translation[2]));
    }

    if ( iGltfNode.rotation.size() > 0 )
    {
      rot = MathUtil::QuatToMatrix( iGltfNode.rotation[0], iGltfNode.rotation[1], iGltfNode.rotation[2], iGltfNode.rotation[3] );
    }

    if ( iGltfNode.scale.size() > 0 )
    {
      scale = glm::scale(Mat4x4(1.f), Vec3(iGltfNode.scale[0], iGltfNode.scale[1], iGltfNode.scale[2]));
    }

    oLocalTransfoMat = translate * rot * scale;
  }
}

// ----------------------------------------------------------------------------
// GLTF loader : LoadTextures
// ----------------------------------------------------------------------------
bool LoadTextures( Scene & ioScene, tinygltf::Model & iGltfModel, GltfDiagnostics & ioDiagnostics )
{
  for ( const tinygltf::Texture & gltfTex : iGltfModel.textures )
  {
    if ( !IsValidIndex(gltfTex.source, iGltfModel.images.size()) )
      return false;
    tinygltf::Image & image = iGltfModel.images[gltfTex.source];

    std::string texName;
    GetTextureName(iGltfModel, gltfTex, texName);

    if ( image.image.data() && image.width && image.height && image.component )
      ioScene.AddTexture(texName, image.image.data(), image.width, image.height, image.component);
    else
      return false;

    if ( gltfTex.sampler >= 0 )
      ioDiagnostics.Warn("texture sampler settings are ignored");
  }

  return true;
}

// ----------------------------------------------------------------------------
// GLTF loader : LoadMaterials
// ----------------------------------------------------------------------------
bool LoadMaterials( Scene & ioScene, tinygltf::Model & iGltfModel, GltfDiagnostics & ioDiagnostics )
{
  bool ret = true;

  for ( size_t indMat = 0; indMat < iGltfModel.materials.size(); ++indMat )
  {
    const tinygltf::Material & gltfMaterial = iGltfModel.materials[indMat];

    const tinygltf::PbrMetallicRoughness & pbr = gltfMaterial.pbrMetallicRoughness;

    // Convert glTF material
    Material material;

    // Albedo
    material._Albedo = Vec3((float)pbr.baseColorFactor[0], (float)pbr.baseColorFactor[1], (float)pbr.baseColorFactor[2]);
    if ( pbr.baseColorTexture.index > -1 )
    {
      if ( !IsValidIndex(pbr.baseColorTexture.index, iGltfModel.textures.size()) )
        return false;
      const tinygltf::Texture & gltfTex = iGltfModel.textures[pbr.baseColorTexture.index];

      std::string texName;
      GetTextureName(iGltfModel, gltfTex, texName);

      material._BaseColorTexId = static_cast<float>(ioScene.FindTextureID(texName));
      if ( material._BaseColorTexId < 0 )
      {
        ret = false;
        break;
      }
    }

    // Opacity
    material._Opacity = (float)pbr.baseColorFactor[3];

    WarnTextureInfoLimitations(pbr.baseColorTexture, ioDiagnostics);

    // Alpha
    material._AlphaCutoff = (float)gltfMaterial.alphaCutoff;
    if ( !strcmp(gltfMaterial.alphaMode.c_str(), "OPAQUE") )
      material._AlphaMode = (float)AlphaMode::Opaque;
    else if (strcmp(gltfMaterial.alphaMode.c_str(), "BLEND") == 0)
      material._AlphaMode = (float)AlphaMode::Blend;
    else if (strcmp(gltfMaterial.alphaMode.c_str(), "MASK") == 0)
      material._AlphaMode = (float)AlphaMode::Mask;

    // Roughness and Metallic
    material._Roughness = sqrtf((float)pbr.roughnessFactor); // Repo's disney material doesn't use squared roughness
    material._Metallic = (float)pbr.metallicFactor;
    if ( pbr.metallicRoughnessTexture.index > -1 )
    {
      if ( !IsValidIndex(pbr.metallicRoughnessTexture.index, iGltfModel.textures.size()) )
        return false;
      const tinygltf::Texture & gltfTex = iGltfModel.textures[pbr.metallicRoughnessTexture.index];

      std::string texName;
      GetTextureName(iGltfModel, gltfTex, texName);

      material._MetallicRoughnessTexID = static_cast<float>(ioScene.FindTextureID(texName));
      if ( material._MetallicRoughnessTexID < 0 )
      {
        ret = false;
        break;
      }
    }
    WarnTextureInfoLimitations(pbr.metallicRoughnessTexture, ioDiagnostics);

    // Normal Map
    if ( gltfMaterial.normalTexture.index > -1 )
    {
      if ( !IsValidIndex(gltfMaterial.normalTexture.index, iGltfModel.textures.size()) )
        return false;
      const tinygltf::Texture & gltfTex = iGltfModel.textures[gltfMaterial.normalTexture.index];

      std::string texName;
      GetTextureName(iGltfModel, gltfTex, texName);

      material._NormalMapTexID = static_cast<float>(ioScene.FindTextureID(texName));
      if ( material._NormalMapTexID < 0 )
      {
        ret = false;
        break;
      }
    }
    if ( gltfMaterial.normalTexture.texCoord > 0 )
      ioDiagnostics.Warn("non-zero texture coordinate sets are ignored");
    if ( gltfMaterial.normalTexture.extensions.find("KHR_texture_transform") != gltfMaterial.normalTexture.extensions.end() )
      ioDiagnostics.Warn("KHR_texture_transform is ignored");
    if ( ( gltfMaterial.normalTexture.index >= 0 ) && ( std::abs(gltfMaterial.normalTexture.scale - 1.0) > 0.0001 ) )
      ioDiagnostics.Warn("normal texture scale is ignored");

    // Emission
    material._Emission = Vec3((float)gltfMaterial.emissiveFactor[0], (float)gltfMaterial.emissiveFactor[1], (float)gltfMaterial.emissiveFactor[2]);
    if ( gltfMaterial.emissiveTexture.index > -1 )
    {
      if ( !IsValidIndex(gltfMaterial.emissiveTexture.index, iGltfModel.textures.size()) )
        return false;
      const tinygltf::Texture & gltfTex = iGltfModel.textures[gltfMaterial.emissiveTexture.index];

      std::string texName;
      GetTextureName(iGltfModel, gltfTex, texName);

      material._EmissionMapTexID = static_cast<float>(ioScene.FindTextureID(texName));
      if ( material._EmissionMapTexID < 0 )
      {
        ret = false;
        break;
      }
    }
    WarnTextureInfoLimitations(gltfMaterial.emissiveTexture, ioDiagnostics);

    // KHR_materials_transmission
    if ( gltfMaterial.extensions.find("KHR_materials_transmission") != gltfMaterial.extensions.end() )
    {
      const auto & ext = gltfMaterial.extensions.find("KHR_materials_transmission") -> second;
      if ( ext.Has("transmissionFactor") )
        material._SpecTrans = (float)(ext.Get("transmissionFactor").Get<double>());
    }

    const auto emissiveStrength = gltfMaterial.extensions.find("KHR_materials_emissive_strength");
    if ( emissiveStrength != gltfMaterial.extensions.end() && emissiveStrength -> second.Has("emissiveStrength") )
      material._Emission *= static_cast<float>(emissiveStrength -> second.Get("emissiveStrength").Get<double>());

    const auto ior = gltfMaterial.extensions.find("KHR_materials_ior");
    if ( ior != gltfMaterial.extensions.end() && ior -> second.Has("ior") )
      material._IOR = static_cast<float>(ior -> second.Get("ior").Get<double>());

    const auto clearcoat = gltfMaterial.extensions.find("KHR_materials_clearcoat");
    if ( clearcoat != gltfMaterial.extensions.end() )
    {
      if ( clearcoat -> second.Has("clearcoatFactor") )
        material._Clearcoat = static_cast<float>(clearcoat -> second.Get("clearcoatFactor").Get<double>());
      if ( clearcoat -> second.Has("clearcoatRoughnessFactor") )
        material._ClearcoatGloss = 1.f - static_cast<float>(clearcoat -> second.Get("clearcoatRoughnessFactor").Get<double>());
    }

    if ( gltfMaterial.occlusionTexture.index >= 0 )
      ioDiagnostics.Warn("occlusion textures are ignored");
    if ( gltfMaterial.doubleSided )
      ioDiagnostics.Warn("double-sided material semantics are ignored");
    for ( const auto & extension : gltfMaterial.extensions )
    {
      if ( ( "KHR_materials_transmission" != extension.first )
        && ( "KHR_materials_emissive_strength" != extension.first )
        && ( "KHR_materials_ior" != extension.first )
        && ( "KHR_materials_clearcoat" != extension.first ) )
        ioDiagnostics.Warn("material extension '" + extension.first + "' is ignored");
    }
    if ( ( gltfMaterial.extensions.find("KHR_materials_transmission") != gltfMaterial.extensions.end() )
      && gltfMaterial.extensions.find("KHR_materials_transmission") -> second.Has("transmissionTexture") )
      ioDiagnostics.Warn("transmission textures are ignored");
    if ( ( clearcoat != gltfMaterial.extensions.end() )
      && ( clearcoat -> second.Has("clearcoatTexture") || clearcoat -> second.Has("clearcoatRoughnessTexture") || clearcoat -> second.Has("clearcoatNormalTexture") ) )
      ioDiagnostics.Warn("clearcoat textures are ignored");

    std::string matName;
    GetMaterialName(iGltfModel, gltfMaterial, static_cast<int>(indMat), matName);

    ioScene.AddMaterial(material, matName);
  }

  if ( ret && ( ioScene.FindMaterialID("Default Material") < 0 ) )
  {
    Material defaultMat;
    ioScene.AddMaterial(defaultMat, "Default Material");
  }

  return ret;
}

// ----------------------------------------------------------------------------
// GLTF loader : LoadMeshes
// ----------------------------------------------------------------------------
bool LoadMeshes( Scene & ioScene, tinygltf::Model & iGltfModel, GltfDiagnostics & ioDiagnostics )
{
  for ( size_t indMesh = 0; indMesh < iGltfModel.meshes.size(); ++indMesh )
  {
    tinygltf::Mesh & gltfMesh = iGltfModel.meshes[indMesh];
    if ( !gltfMesh.primitives.empty() && !gltfMesh.weights.empty() )
      ioDiagnostics.Warn("morph target weights are ignored");

    for ( size_t indPrim = 0; indPrim < gltfMesh.primitives.size(); ++indPrim )
    {
      tinygltf::Primitive & prim = gltfMesh.primitives[indPrim];
      const int mode = ( prim.mode >= 0 ) ? prim.mode : TINYGLTF_MODE_TRIANGLES;
      if ( ( TINYGLTF_MODE_POINTS == mode ) || ( TINYGLTF_MODE_LINE == mode ) || ( TINYGLTF_MODE_LINE_LOOP == mode ) || ( TINYGLTF_MODE_LINE_STRIP == mode ) )
      {
        ioDiagnostics.Warn("point and line primitives are ignored");
        continue;
      }
      if ( ( TINYGLTF_MODE_TRIANGLES != mode ) && ( TINYGLTF_MODE_TRIANGLE_STRIP != mode ) && ( TINYGLTF_MODE_TRIANGLE_FAN != mode ) )
        return false;

      const auto positionIt = prim.attributes.find("POSITION");
      if ( positionIt == prim.attributes.end() ) return false;
      std::vector<Vec3> vertices;
      if ( !ReadVec3Accessor(iGltfModel, positionIt -> second, vertices) || vertices.empty() )
        return false;
      std::vector<int> sourceIndices;
      if ( prim.indices >= 0 )
      {
        if ( !ReadIndices(iGltfModel, prim.indices, vertices.size(), sourceIndices) )
          return false;
      }
      else
      {
        sourceIndices.resize(vertices.size());
        for ( size_t i = 0; i < vertices.size(); ++i ) sourceIndices[i] = static_cast<int>(i);
      }

      std::vector<int> triangleIndices;
      if ( TINYGLTF_MODE_TRIANGLES == mode )
      {
        if ( 0 != ( sourceIndices.size() % 3 ) ) return false;
        triangleIndices = sourceIndices;
      }
      else if ( sourceIndices.size() >= 3 )
      {
        for ( size_t i = 2; i < sourceIndices.size(); ++i )
        {
          if ( TINYGLTF_MODE_TRIANGLE_FAN == mode )
          {
            triangleIndices.push_back(sourceIndices[0]); triangleIndices.push_back(sourceIndices[i - 1]); triangleIndices.push_back(sourceIndices[i]);
          }
          else if ( 0 == ( i % 2 ) )
          {
            triangleIndices.push_back(sourceIndices[i - 2]); triangleIndices.push_back(sourceIndices[i - 1]); triangleIndices.push_back(sourceIndices[i]);
          }
          else
          {
            triangleIndices.push_back(sourceIndices[i - 1]); triangleIndices.push_back(sourceIndices[i - 2]); triangleIndices.push_back(sourceIndices[i]);
          }
        }
      }
      if ( triangleIndices.empty() ) return false;

      std::vector<Vec3> normals;
      const auto normalIt = prim.attributes.find("NORMAL");
      if ( normalIt != prim.attributes.end() )
      {
        if ( !ReadVec3Accessor(iGltfModel, normalIt -> second, normals) || ( normals.size() != vertices.size() ) ) return false;
      }
      else
      {
        ioDiagnostics.Warn("missing normals are generated");
        GenerateNormals(vertices, triangleIndices, normals);
      }

      std::vector<Vec2> uvs;
      const auto uvIt = prim.attributes.find("TEXCOORD_0");
      if ( ( uvIt != prim.attributes.end() ) && ( !ReadVec2Accessor(iGltfModel, uvIt -> second, uvs) || ( uvs.size() != vertices.size() ) ) ) return false;
      if ( prim.attributes.find("TANGENT") != prim.attributes.end() ) ioDiagnostics.Warn("tangent attributes are ignored");
      if ( ( prim.attributes.find("JOINTS_0") != prim.attributes.end() ) || ( prim.attributes.find("WEIGHTS_0") != prim.attributes.end() ) ) ioDiagnostics.Warn("skinning attributes are ignored");
      if ( !prim.targets.empty() ) ioDiagnostics.Warn("morph targets are ignored");

      std::vector<Vec3i> indices;
      indices.reserve(triangleIndices.size());
      for ( int index : triangleIndices ) indices.push_back(Vec3i(index));

      std::string meshName;
      GetMeshName( gltfMesh, static_cast<int>(indMesh), static_cast<int>(indPrim), meshName );
      Mesh * newMesh = new Mesh( meshName, vertices, normals, uvs, indices );
      if ( -1 == ioScene.AddMesh(newMesh) ) return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------------
// GLTF loader : TraverseNodes
// ----------------------------------------------------------------------------
bool TraverseNodes( Scene & ioScene, tinygltf::Model & iGltfModel, int iNodeIdx, const Mat4x4 & iParentTransfoMat, const RenderSettings & iRenderSettings, GltfDiagnostics & ioDiagnostics, std::set<int> & ioAncestors )
{
  if ( !IsValidIndex(iNodeIdx, iGltfModel.nodes.size()) || !ioAncestors.insert(iNodeIdx).second )
    return false;
  tinygltf::Node gltfNode = iGltfModel.nodes[iNodeIdx];

  if ( ( !gltfNode.matrix.empty() && ( 16 != gltfNode.matrix.size() ) )
    || ( !gltfNode.translation.empty() && ( 3 != gltfNode.translation.size() ) )
    || ( !gltfNode.rotation.empty() && ( 4 != gltfNode.rotation.size() ) )
    || ( !gltfNode.scale.empty() && ( 3 != gltfNode.scale.size() ) ) )
    return false;

  Mat4x4 localTransfoMat;
  GetLocalTransfo( gltfNode , localTransfoMat );

  Mat4x4 transfoMat = iParentTransfoMat * localTransfoMat;

  if ( gltfNode.mesh >= 0 )
  {
    if ( !IsValidIndex(gltfNode.mesh, iGltfModel.meshes.size()) ) return false;
    tinygltf::Mesh & gltfMesh = iGltfModel.meshes[gltfNode.mesh];

    for ( size_t indPrim = 0; indPrim < gltfMesh.primitives.size(); ++indPrim )
    {
      tinygltf::Primitive & prim = gltfMesh.primitives[indPrim];
      const int mode = ( prim.mode >= 0 ) ? prim.mode : TINYGLTF_MODE_TRIANGLES;
      if ( ( TINYGLTF_MODE_TRIANGLES != mode ) && ( TINYGLTF_MODE_TRIANGLE_STRIP != mode ) && ( TINYGLTF_MODE_TRIANGLE_FAN != mode ) )
        continue;

      std::string meshName;
      GetMeshName( gltfMesh, gltfNode.mesh, static_cast<int>(indPrim), meshName );

      int meshID = ioScene.FindMeshID( meshName );
      if ( meshID < 0 )
        continue;

      int matID = 0;
      if ( prim.material >= 0 )
      {
        if ( !IsValidIndex(prim.material, iGltfModel.materials.size()) ) return false;
        const tinygltf::Material & gltfMaterial = iGltfModel.materials[prim.material];

        std::string matName;
        GetMaterialName(iGltfModel, gltfMaterial, prim.material, matName);
        matID = ioScene.FindMaterialID( matName );
      }
      else
        matID = ioScene.FindMaterialID( "Default Material" );
      if ( matID < 0 )
        return false;

      std::string instanceName( gltfNode.name );
      instanceName += "_inst";
      instanceName += std::to_string(indPrim);

      MeshInstance instance( instanceName, meshID, matID, transfoMat );
      ioScene.AddMeshInstance(instance);
    }
  }

  if ( gltfNode.light >= 0 )
  {
    if ( !IsValidIndex(gltfNode.light, iGltfModel.lights.size()) ) return false;
    const tinygltf::Light & gltfLight = iGltfModel.lights[gltfNode.light];

    Light newLight;
    if ( gltfLight.color.size() >= 3 )
    {
      newLight._Emission = Vec3( static_cast<float>(gltfLight.color[0]),
                                 static_cast<float>(gltfLight.color[1]),
                                 static_cast<float>(gltfLight.color[2]) );
    }

    if ( "directional" == gltfLight.type )
    {
      // glTF lights emit along their local -Z axis. This renderer stores the
      // opposite, surface-to-light direction in _Pos for distant lights.
      newLight._Pos = glm::normalize( Vec3( transfoMat[2][0], transfoMat[2][1], transfoMat[2][2] ) );
      newLight._Type = (float)LightType::DistantLight;
      newLight._Area = 0.f;
      newLight._Radius = 0.f;
      newLight._Intensity = static_cast<float>(gltfLight.intensity);
    }
    else if ( ( "point" == gltfLight.type ) || ( "spot" == gltfLight.type ) )
    {
      // Point and spot lights are represented as small spherical emitters.
      // Spot cone and range are not supported by the renderer's Light type.
      const float radius = 0.1f;
      newLight._Pos = Vec3( transfoMat[3][0], transfoMat[3][1], transfoMat[3][2] );
      newLight._Radius = radius;
      newLight._Area = 4.f * static_cast<float>(M_PI) * radius * radius;
      newLight._Type = (float)LightType::SphereLight;
      newLight._Intensity = static_cast<float>(gltfLight.intensity) / ( static_cast<float>(M_PI) * radius * radius );
      if ( "spot" == gltfLight.type )
        ioDiagnostics.Warn("spot light cone and range are approximated as point lights");
    }
    else
      return false;

    ioScene.AddLight( newLight );
  }

  if ( gltfNode.camera >= 0 )
  {
    if ( !IsValidIndex(gltfNode.camera, iGltfModel.cameras.size()) ) return false;
    const tinygltf::Camera & curCam = iGltfModel.cameras[gltfNode.camera];
    if ( "perspective" == curCam.type )
    {
      float aspectRatio = static_cast<float>(curCam.perspective.aspectRatio);
      if ( aspectRatio <= 0.f )
      {
        if ( ( iRenderSettings._RenderResolution.x > 0 ) && ( iRenderSettings._RenderResolution.y > 0 ) )
          aspectRatio = static_cast<float>(iRenderSettings._RenderResolution.x) / iRenderSettings._RenderResolution.y;
        else
        {
          aspectRatio = 1.f;
          ioDiagnostics.Warn("camera aspect ratio is unavailable; using 1:1 to convert vertical FOV");
        }
      }
      const float fov = MathUtil::ToDegrees(2.f * atan(tan(static_cast<float>(curCam.perspective.yfov) * .5f) * aspectRatio));
      float focalDist = -1.f;
      float aperture = -1.f;
      float nearPlane = (float)curCam.perspective.znear;
      float farPlane = (float)curCam.perspective.zfar;

      Vec3 forward = { -transfoMat[2][0], -transfoMat[2][1], -transfoMat[2][2] };
      Vec3 pos = { transfoMat[3][0], transfoMat[3][1], transfoMat[3][2] };
      Vec3 lookAt = pos + forward;

      Camera newCamera( pos, lookAt, fov );
      if ( aperture >= 0 )
        newCamera.SetAperture( aperture );
      if ( focalDist >= 0 )
        newCamera.SetFocalDist( focalDist );

      if ( ( nearPlane > 0.f ) || ( farPlane > 0.f ) )
      {
        float nNear, nFar;
        newCamera.GetZNearFar( nNear, nFar );

        if ( nearPlane > 0.f )
          nNear = nearPlane;
        if ( farPlane > 0.f )
          nFar = farPlane;

        newCamera.SetZNearFar( nNear, nFar );
      }

      ioScene.SetCamera( newCamera );
    }
    else if ( "orthographic" == curCam.type )
    {
      ioDiagnostics.Warn("orthographic cameras are ignored");
    }
  }

  for ( size_t i = 0; i < gltfNode.children.size(); ++i )
  {
    if ( !TraverseNodes(ioScene, iGltfModel, gltfNode.children[i], transfoMat, iRenderSettings, ioDiagnostics, ioAncestors) ) return false;
  }

  ioAncestors.erase(iNodeIdx);
  return true;
}

// ----------------------------------------------------------------------------
// GLTF loader : LoadInstances
// ----------------------------------------------------------------------------
bool LoadInstances( Scene & ioScene, tinygltf::Model & iGltfModel, const Mat4x4 & iTransfoMat, const RenderSettings & iRenderSettings, GltfDiagnostics & ioDiagnostics )
{
  if ( iGltfModel.scenes.empty() ) return false;
  const int sceneIndex = ( iGltfModel.defaultScene >= 0 ) ? iGltfModel.defaultScene : 0;
  if ( !IsValidIndex(sceneIndex, iGltfModel.scenes.size()) ) return false;

  const tinygltf::Scene gltfScene = iGltfModel.scenes[sceneIndex];

  for ( int nodeIdx : gltfScene.nodes )
  {
    std::set<int> ancestors;
    if ( !TraverseNodes(ioScene, iGltfModel, nodeIdx, iTransfoMat, iRenderSettings, ioDiagnostics, ancestors) ) return false;
  }
  return true;
}

// ----------------------------------------------------------------------------
// LoadScene
// ----------------------------------------------------------------------------
bool Loader::LoadScene(const std::string & iFilename, Scene & oScene, RenderSettings & oRenderSettings)
{
  fs::path filepath = iFilename;

  if ( ".scene" == filepath.extension() )
    return Loader::LoadFromSceneFile(iFilename, oScene, oRenderSettings);
  else if ( ".gltf" == filepath.extension() )
    return Loader::LoadFromGLTF(iFilename, Mat4x4{1.f}, oScene, oRenderSettings);
  else if ( ".glb" == filepath.extension() )
    return Loader::LoadFromGLTF(iFilename, Mat4x4{ 1.f }, oScene, oRenderSettings, true);

  return false;
}

// ----------------------------------------------------------------------------
// LoadFromSceneFile
// ----------------------------------------------------------------------------
bool Loader::LoadFromSceneFile(const std::string & iFilename, Scene & oScene, RenderSettings & oRenderSettings )
{
  fs::path filepath = iFilename;
  filepath.remove_filename();
  std::string path = filepath.string();

  std::ifstream file(iFilename);

  if ( !file.is_open() )
  {
    printf("Loader : Couldn't open %s for reading\n", iFilename.c_str());
    return false;
  }

  printf("Loading Scene...\n");

  int parsingError = 0;
  State curState = State::ExpectNewBlock;

  std::string line;
  while( std::getline( file, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = static_cast<int>(tokens.size());
    if ( !nbTokens || ( '#' == tokens[0][0] ) )
      continue;
    if ( State::ExpectNewBlock != curState )
    {
      parsingError++;
      continue;
    }
    if ( ( '}' == tokens[0][0] ) || ( '}' == tokens[0][0] ) )
    {
      parsingError++;
      continue;
    }

    //--------------------------------------------
    // Material - START
    if ( ( 2 == nbTokens ) && ( IsEqual("material", tokens[0]) ) )
    {
      std::cout << "New material : " << tokens[1] << std::endl;

      std::string materialName = tokens[1];

      parsingError += Loader::ParseMaterial(file, path, materialName, oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Material - END
    //--------------------------------------------


    //--------------------------------------------
    // Mesh - START
    if ( IsEqual("mesh", tokens[0]) )
    {
      std::cout << "New mesh" << std::endl;

      parsingError += Loader::ParseMeshData(file, path, oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Mesh - END
    //--------------------------------------------


    //--------------------------------------------
    // Sphere - START
    if ( IsEqual("sphere", tokens[0]) )
    {
      std::cout << "New sphere" << std::endl;

      parsingError += Loader::ParseSphere(file, oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Sphere - END
    //--------------------------------------------


    //--------------------------------------------
    // Box - START
    if ( IsEqual("box", tokens[0]) )
    {
      std::cout << "New box" << std::endl;

      parsingError += Loader::ParseBox(file, oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Box - END
    //--------------------------------------------


    //--------------------------------------------
    // Plane - START
    if ( IsEqual("plane", tokens[0]) )
    {
      std::cout << "New plane" << std::endl;

      parsingError += Loader::ParsePlane(file, oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Plane - END
    //--------------------------------------------


    //--------------------------------------------
    // Light - START
    if ( IsEqual("light", tokens[0]) )
    {
      std::cout << "New light" << std::endl;

      parsingError += Loader::ParseLight(file, oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Light - END
    //--------------------------------------------


    //--------------------------------------------
    // Camera - START
    if ( IsEqual("camera", tokens[0]) )
    {
      std::cout << "New camera" << std::endl;

      parsingError += Loader::ParseCamera(file, oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Camera - END
    //--------------------------------------------


    //--------------------------------------------
    // Renderer - START
    if ( IsEqual("renderer", tokens[0]) )
    {
      std::cout << "New renderer" << std::endl;

      RenderSettings settings;
      parsingError += Loader::ParseRenderSettings(file, path, settings, oScene);

      if ( !parsingError )
      {
        oRenderSettings = settings;

        curState = State::ExpectNewBlock;
      }
    }
    // Renderer - END
    //--------------------------------------------


    //--------------------------------------------
    // GLTF - START
    if ( IsEqual("gltf", tokens[0]) )
    {
      std::cout << "New gltf model" << std::endl;

      parsingError += Loader::ParseGLTF(file, path, oScene, oRenderSettings);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // GLTF - END
    //--------------------------------------------
   

  }

  if ( parsingError )
  {
    printf("ERROR\n");
    oScene.Clear();
    return false;
  }
  else
    printf("DONE\n");

  return true;
}

// ----------------------------------------------------------------------------
// LoadFromGLTF
// Adapted from accompanying code for Ray Tracing Gems II, Chapter 14: The Reference Path Tracer
// https://github.com/boksajak/referencePT
// ----------------------------------------------------------------------------
bool Loader::LoadFromGLTF(const std::string & iGltfFilename, const Mat4x4 & iTransfoMat, Scene & ioScene, RenderSettings & ioRenderSettings, bool isBinary)
{
  bool ret = false;

  do
  {
    printf("Loading GLTF %s\n", iGltfFilename.c_str());

    tinygltf::Model gltfModel;
    {
      tinygltf::TinyGLTF loader;
      std::string err;
      std::string warn;
  if ( isBinary )
        ret = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, iGltfFilename);
      else
        ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, iGltfFilename);

      if ( !ret )
      {
        printf("Unable to load file %s. Error: %s\n", iGltfFilename.c_str(), err.c_str());
        break;
      }
    }

    printf("Loading Scene from gltf...\n");

    GltfDiagnostics diagnostics;
    if ( !gltfModel.animations.empty() )
      diagnostics.Warn("animations are ignored");
    if ( !gltfModel.skins.empty() )
      diagnostics.Warn("skins are ignored");

    ret = LoadTextures(ioScene, gltfModel, diagnostics);
    if ( ret )
      ret = LoadMaterials(ioScene, gltfModel, diagnostics);
    if ( ret )
      ret = LoadMeshes(ioScene, gltfModel, diagnostics);
    if ( ret )
      ret = LoadInstances(ioScene, gltfModel, iTransfoMat, ioRenderSettings, diagnostics);

    if ( !ret )
    {
      printf("Error while to loading scene from gltf file %s\n", iGltfFilename.c_str());
      break;
    }

  } while ( 0 );

  return ret;
}

// ----------------------------------------------------------------------------
// ParseMaterial
// ----------------------------------------------------------------------------
int Loader::ParseMaterial( std::ifstream & iStr, const std::string & iPath, const std::string & iMaterialName, Scene & ioScene )
{
  int parsingError = 0;

  Material newMaterial;

  std::string albedoTexName;
  std::string metallicRoughnessTexName;
  std::string normalTexName;
  std::string emissionTexName;
  std::string alphaMode;
  std::string mediumType;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = static_cast<int>(tokens.size());
    if ( !nbTokens || ( '#' == tokens[0][0] ) )
      continue;

    if ( State::ExpectOpenBracket == curState )
    {
      if ( '{' == tokens[0][0] )
        curState = State::ExpectClosingBracket;
      else if ( '}' == tokens[0][0] )
        parsingError++;
      continue;
    }

    if ( State::ExpectClosingBracket == curState )
    {
      if ( '}' == tokens[0][0] )
      {
        curState = State::ExpectNewBlock;
        break;
      }
      else if ( '{' == tokens[0][0] )
      {
        parsingError++;
        continue;
      }
    }

    if ( IsEqual("color", tokens[0]) )
    {
      if ( 4 == nbTokens )
        newMaterial._Albedo = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( IsEqual("emission", tokens[0]) )
    {
      if ( 4 == nbTokens )
        newMaterial._Emission = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( IsEqual("mediumcolor", tokens[0]) )
    {
      if ( 4 == nbTokens )
        newMaterial._MediumColor = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( IsEqual("opacity", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._Opacity = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("alphaCutoff", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._AlphaCutoff = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("metallic", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._Metallic = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("roughness", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._Roughness = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("subsurface", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._Subsurface = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("speculartint", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._SpecTint = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("anisotropic", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._Anisotropic = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("sheen", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._Sheen = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("sheenTint", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._SheenTint = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("clearcoat", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._Clearcoat = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("clearcoatgloss", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._ClearcoatGloss = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("spectrans", tokens[0]) || IsEqual("transmission", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._SpecTrans = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("ior", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._IOR = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("mediumdensity", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._MediumDensity = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("mediumanisotropy", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newMaterial._MediumAnisotropy = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("albedotexture", tokens[0]) )
    {
      if ( 2 == nbTokens )
        albedoTexName = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("metallicroughnesstexture", tokens[0]) )
    {
      if ( 2 == nbTokens )
        metallicRoughnessTexName = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("normaltexture", tokens[0]) )
    {
      if ( 2 == nbTokens )
        normalTexName = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("emissiontexture", tokens[0]) )
    {
      if ( 2 == nbTokens )
        emissionTexName = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("alphamode", tokens[0]) )
    {
      if ( 2 == nbTokens )
        alphaMode = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("mediumtype", tokens[0]) )
    {
      if ( 2 == nbTokens )
        mediumType = tokens[1];
      else
        parsingError++;
    }
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError )
  {
    if ( "opaque" == alphaMode )
      newMaterial._AlphaMode = (float)AlphaMode::Opaque;
    else if ( "blend" == alphaMode )
      newMaterial._AlphaMode = (float)AlphaMode::Blend;
    else if ( "mask" == alphaMode )
      newMaterial._AlphaMode = (float)AlphaMode::Mask;

    if ( "absorb" == mediumType )
      newMaterial._MediumType = (float)MediumType::Absorb;
    else if ( "scatter" == mediumType )
      newMaterial._MediumType = (float)MediumType::Scatter;
    else if ( "emissive" == mediumType )
      newMaterial._MediumType = (float)MediumType::Emissive;

    if ( !albedoTexName.empty() )
      newMaterial._BaseColorTexId = static_cast<float>(ioScene.AddTexture(iPath + albedoTexName));
    if ( !metallicRoughnessTexName.empty() )
      newMaterial._MetallicRoughnessTexID = static_cast<float>(ioScene.AddTexture(iPath + metallicRoughnessTexName));
    if ( !normalTexName.empty() )
      newMaterial._NormalMapTexID = static_cast<float>(ioScene.AddTexture(iPath + normalTexName));
    if ( !emissionTexName.empty() )
      newMaterial._EmissionMapTexID = static_cast<float>(ioScene.AddTexture(iPath + emissionTexName));

     ioScene.AddMaterial(newMaterial, iMaterialName);
  }

  return parsingError;
}

// ----------------------------------------------------------------------------
// ParseLight
// ----------------------------------------------------------------------------
int Loader::ParseLight( std::ifstream & iStr, Scene & ioScene )
{
  int parsingError = 0;

  Light newLight;
  Vec3 v1, v2;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = static_cast<int>(tokens.size());
    if ( !nbTokens || ( '#' == tokens[0][0] ) )
      continue;

    if ( State::ExpectOpenBracket == curState )
    {
      if ( '{' == tokens[0][0] )
        curState = State::ExpectClosingBracket;
      else if ( '}' == tokens[0][0] )
        parsingError++;
      continue;
    }

    if ( State::ExpectClosingBracket == curState )
    {
      if ( '}' == tokens[0][0] )
      {
        curState = State::ExpectNewBlock;
        break;
      }
      else if ( '{' == tokens[0][0] )
      {
        parsingError++;
        continue;
      }
    }

    if ( IsEqual("position", tokens[0]) )
    {
      if ( 4 == nbTokens )
        newLight._Pos = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( IsEqual("emission", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        Vec3 emission( std::stof( tokens[1] ), std::stof( tokens[2] ), std::stof( tokens[3] ) );
        newLight._Intensity = std::max(std::max(std::max( emission.r, emission.g), emission.b ), 1.f );
        newLight._Emission = glm::min( emission, Vec3(1.f) );
      }
      else
        parsingError++;
    }
    else if ( IsEqual("intensity", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newLight._Intensity = std::max(0.f, std::stof(tokens[1]));
      else
        parsingError++;
    }
    else if ( IsEqual("v1", tokens[0]) )
    {
      if ( 4 == nbTokens )
        v1 = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( IsEqual("v2", tokens[0]) )
    {
      if ( 4 == nbTokens )
        v2 = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( IsEqual("radius", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newLight._Radius = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("castshadow", tokens[0]) )
    {
      if ( 2 == nbTokens )
      {
        if ( IsEqual("true", tokens[1]) || IsEqual("1", tokens[1]) )
          newLight._CastShadow = true;
        else if ( IsEqual("false", tokens[1]) || IsEqual("0", tokens[1]) )
          newLight._CastShadow = false;
        else
          parsingError++;
      }
      else
        parsingError++;
    }
    else if ( IsEqual("shadowradius", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newLight._ShadowRadius = std::max(0.f, std::stof(tokens[1]));
      else
        parsingError++;
    }
    else if ( IsEqual("type", tokens[0]) )
    {
      if ( 2 == nbTokens )
      {
        if ( IsEqual("quad", tokens[1]) )
          newLight._Type = (float) LightType::RectLight;
        else if ( IsEqual("sphere", tokens[1]) )
          newLight._Type = (float) LightType::SphereLight;
        else if ( IsEqual("distant", tokens[1]) )
          newLight._Type = (float) LightType::DistantLight;
        else
          parsingError++;
      }
      else
        parsingError++;
    }
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError )
  {
    if ( LightType::RectLight == (LightType)newLight._Type )
    {
      newLight._DirU = v1 - newLight._Pos;
      newLight._DirV = v2 - newLight._Pos;
      newLight._Area = glm::length(glm::cross(newLight._DirU, newLight._DirV));
    }
    else if ( LightType::SphereLight == (LightType)newLight._Type )
    {
      newLight._Area = 4.0f * static_cast<float>(M_PI) * newLight._Radius * newLight._Radius;
    }
    else if ( LightType::DistantLight == (LightType)newLight._Type )
    {
      newLight._Area = 0.f;
    }

    ioScene.AddLight(newLight);
  }

  return parsingError;
}

// ----------------------------------------------------------------------------
// ParseCamera
// ----------------------------------------------------------------------------
int Loader::ParseCamera( std::ifstream & iStr, Scene & ioScene )
{
  int parsingError = 0;

  Vec3 pos({0.f, 0.f, -1.f});
  Vec3 lookAt({0.f, 0.f, 0.f});
  float fov = 80.f;
  float focalDist = -1.f;
  float aperture  = -1.f;
  float nearPlane = -1.f;
  float farPlane  = -1.f;

  Mat4x4 xform;
  bool hasMatrix = false;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = static_cast<int>(tokens.size());
    if ( !nbTokens || ( '#' == tokens[0][0] ) )
      continue;

    if ( State::ExpectOpenBracket == curState )
    {
      if ( '{' == tokens[0][0] )
        curState = State::ExpectClosingBracket;
      else if ( '}' == tokens[0][0] )
        parsingError++;
      continue;
    }

    if ( State::ExpectClosingBracket == curState )
    {
      if ( '}' == tokens[0][0] )
      {
        curState = State::ExpectNewBlock;
        break;
      }
      else if ( '{' == tokens[0][0] )
      {
        parsingError++;
        continue;
      }
    }

    if ( IsEqual("position", tokens[0]) )
    {
      if ( 4 == nbTokens )
        pos = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( IsEqual("lookat", tokens[0]) )
    {
      if ( 4 == nbTokens )
        lookAt = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( IsEqual("aperture", tokens[0]) )
    {
      if ( 2 == nbTokens )
        aperture = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("focaldist", tokens[0]) )
    {
      if ( 2 == nbTokens )
        focalDist = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("fov", tokens[0]) )
    {
      if ( 2 == nbTokens )
        fov = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("near", tokens[0]) )
    {
      if ( 2 == nbTokens )
        nearPlane = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("far", tokens[0]) )
    {
      if ( 2 == nbTokens )
        farPlane = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("matrix", tokens[0]) )
    {
      if ( 17 == nbTokens )
      {
        xform[0][0] = std::stof(tokens[1]);
        xform[1][0] = std::stof(tokens[2]);
        xform[2][0] = std::stof(tokens[3]);
        xform[3][0] = std::stof(tokens[4]);
        xform[0][1] = std::stof(tokens[5]);
        xform[1][1] = std::stof(tokens[6]);
        xform[2][1] = std::stof(tokens[7]);
        xform[3][1] = std::stof(tokens[8]);
        xform[0][2] = std::stof(tokens[9]);
        xform[1][2] = std::stof(tokens[10]);
        xform[2][2] = std::stof(tokens[11]);
        xform[3][2] = std::stof(tokens[12]);
        xform[0][3] = std::stof(tokens[13]);
        xform[1][3] = std::stof(tokens[14]);
        xform[2][3] = std::stof(tokens[15]);
        xform[3][3] = std::stof(tokens[16]);
        hasMatrix = true;
      }
      else
        parsingError++;
    }

  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError )
  {
    if ( hasMatrix )
    {
      Vec3 forward = Vec3(xform[2][0], xform[2][1], xform[2][2]);
      pos = Vec3(xform[3][0], xform[3][1], xform[3][2]);
      lookAt = pos + forward;
    }

    Camera newCamera(pos, lookAt, fov);
    if ( aperture >= 0 )
      newCamera.SetAperture(aperture);
    if ( focalDist >= 0 )
      newCamera.SetFocalDist(focalDist);

    if ( ( nearPlane > 0.f ) || ( farPlane > 0.f ) )
    {
      float nNear, nFar;
      newCamera.GetZNearFar(nNear, nFar);

      if ( nearPlane > 0.f )
        nNear = nearPlane;
      if ( farPlane > 0.f )
        nFar = farPlane;

      newCamera.SetZNearFar(nNear, nFar);
    }

    ioScene.SetCamera(newCamera);
  }

  return parsingError;
}

// ----------------------------------------------------------------------------
// ParseRenderSettings
// ----------------------------------------------------------------------------
int Loader::ParseRenderSettings( std::ifstream & iStr, const std::string & iPath, RenderSettings & oSettings, Scene & ioScene )
{
  int parsingError = 0;

  bool hasRenderResolution = false;
  bool hasWindowResolution = false;
  Vec2i tileResolution = { -1, -1 };
  std::string envMapFile = "none";

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = static_cast<int>(tokens.size());
    if ( !nbTokens || ( '#' == tokens[0][0] ) )
      continue;

    if ( State::ExpectOpenBracket == curState )
    {
      if ( '{' == tokens[0][0] )
        curState = State::ExpectClosingBracket;
      else if ( '}' == tokens[0][0] )
        parsingError++;
      continue;
    }

    if ( State::ExpectClosingBracket == curState )
    {
      if ( '}' == tokens[0][0] )
      {
        curState = State::ExpectNewBlock;
        break;
      }
      else if ( '{' == tokens[0][0] )
      {
        parsingError++;
        continue;
      }
    }

    if ( IsEqual("resolution", tokens[0]) )
    {
      if ( 3 == nbTokens )
      {
        hasRenderResolution = true;
        oSettings._RenderResolution = Vec2i(std::stoi(tokens[1]), std::stoi(tokens[2]));
      }
      else
        parsingError++;
    }
    else if ( IsEqual("windowresolution", tokens[0]) )
    {
      if ( 3 == nbTokens )
      {
        hasWindowResolution = true;
        oSettings._WindowResolution = Vec2i(std::stoi(tokens[1]), std::stoi(tokens[2]));
      }
      else
        parsingError++;
    }
    else if ( IsEqual("backgroundcolor", tokens[0]) )
    {
      if ( 4 == nbTokens )
        oSettings._BackgroundColor = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( IsEqual("uniformlightcolor", tokens[0]) )
    {
      if ( 4 == nbTokens )
        oSettings._UniformLightCol = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( IsEqual("enablebackground", tokens[0]) )
    {
      if ( 2 == nbTokens )
      {
        if ( IsEqual("true", tokens[1]) )
          oSettings._EnableBackGround = true;
        else if ( IsEqual("false", tokens[1]) )
          oSettings._EnableBackGround = false;
        else
          parsingError++;
      }
      else
        parsingError++;
    }
    else if ( IsEqual("enableskybox", tokens[0]) )
    {
      if ( 2 == nbTokens )
      {
        if ( IsEqual("true", tokens[1]) )
          oSettings._EnableSkybox = true;
        else if ( IsEqual("false", tokens[1]) )
          oSettings._EnableSkybox = false;
        else
          parsingError++;
      }
      else
        parsingError++;
    }
    else if ( IsEqual("envmap", tokens[0]) || IsEqual( "envmapfile", tokens[0] ) )
    {
      if ( 2 == nbTokens )
      {
        envMapFile = tokens[1];
        oSettings._EnableSkybox = true;
      }
      else
        parsingError++;
    }
    else if ( IsEqual("enableuniformlight", tokens[0]) )
    {
      if ( 2 == nbTokens )
      {
        if ( IsEqual("true", tokens[1]) )
          oSettings._EnableUniformLight = true;
        else if ( IsEqual("false", tokens[1]) )
          oSettings._EnableUniformLight = false;
        else
          parsingError++;
      }
      else
        parsingError++;
    }
    else if ( IsEqual("bounces", tokens[0]) )
    {
      if ( 2 == nbTokens )
        oSettings._Bounces =  std::stoi(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("maxdepth", tokens[0]) )
    {
      if ( 2 == nbTokens )
        oSettings._Bounces =  std::stoi(tokens[1]);
      else
        parsingError++;
    }
    //else if ( IsEqual("background", tokens[0]) )
    //{
    //  if ( 2 == nbTokens )
    //  {
    //  }
    //  else
    //    parsingError++;
    //}
    else if ( IsEqual("tilewidth", tokens[0]) )
    {
      if ( 2 == nbTokens )
        tileResolution.x =  std::stoi(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("tileheight", tokens[0]) )
    {
      if ( 2 == nbTokens )
        tileResolution.y =  std::stoi(tokens[1]);
      else
        parsingError++;
    }
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError )
  {
    if ( hasRenderResolution && !hasWindowResolution )
      oSettings._WindowResolution = oSettings._RenderResolution;
    else if ( !hasRenderResolution && hasWindowResolution )
      oSettings._RenderResolution = oSettings._WindowResolution;
    else if ( !hasRenderResolution && !hasWindowResolution )
      oSettings._RenderResolution = oSettings._WindowResolution = { 1920, 1080 };

    if ( ( tileResolution.x > 0 ) || ( tileResolution.y > 0 ) )
    {
      if ( tileResolution.y <= 0 )
        tileResolution.y = tileResolution.x;
      else if ( tileResolution.x <= 0 )
        tileResolution.x = tileResolution.y;

      oSettings._TiledRendering = true;
      oSettings._TileResolution = tileResolution;
    }

    if ( envMapFile != "none" )
      ioScene.LoadEnvMap(iPath + envMapFile);
  }

  return parsingError;
}

// ----------------------------------------------------------------------------
// ParseSphere
// ----------------------------------------------------------------------------
int Loader::ParseSphere( std::ifstream & iStr, Scene & ioScene )
{
  int parsingError = 0;

  std::string materialName;
  Mat4x4 transMat(1.f), rotMat(1.f), scaleMat(1.f);
  Mat4x4 xform(1.f);
  bool hasMatrix = false;

  Sphere newSphere;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = static_cast<int>(tokens.size());
    if ( !nbTokens || ( '#' == tokens[0][0] ) )
      continue;

    if ( State::ExpectOpenBracket == curState )
    {
      if ( '{' == tokens[0][0] )
        curState = State::ExpectClosingBracket;
      else if ( '}' == tokens[0][0] )
        parsingError++;
      continue;
    }

    if ( State::ExpectClosingBracket == curState )
    {
      if ( '}' == tokens[0][0] )
      {
        curState = State::ExpectNewBlock;
        break;
      }
      else if ( '{' == tokens[0][0] )
      {
        parsingError++;
        continue;
      }
    }

    if ( IsEqual("radius", tokens[0]) )
    {
      if ( 2 == nbTokens )
        newSphere._Radius = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( IsEqual("material", tokens[0]) )
    {
      if ( 2 == nbTokens )
        materialName = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("position", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        transMat[3][0] = std::stof(tokens[1]);
        transMat[3][1] = std::stof(tokens[2]);
        transMat[3][2] = std::stof(tokens[3]);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("scale", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        scaleMat[0][0] = std::stof(tokens[1]);
        scaleMat[1][1] = std::stof(tokens[2]);
        scaleMat[2][2] = std::stof(tokens[3]);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("rotation", tokens[0]) )
    {
      if ( 5 == nbTokens )
      {
        Vec4 quaternion; // { x, y, z, s } -> q = s + ix + jy + kz
        quaternion.x = std::stof(tokens[1]);
        quaternion.y = std::stof(tokens[2]);
        quaternion.z = std::stof(tokens[3]);
        quaternion.w = std::stof(tokens[4]);

        rotMat = MathUtil::QuatToMatrix(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("matrix", tokens[0]) )
    {
      if ( 17 == nbTokens )
      {
        xform[0][0] = std::stof(tokens[1]);
        xform[1][0] = std::stof(tokens[2]);
        xform[2][0] = std::stof(tokens[3]);
        xform[3][0] = std::stof(tokens[4]);
        xform[0][1] = std::stof(tokens[5]);
        xform[1][1] = std::stof(tokens[6]);
        xform[2][1] = std::stof(tokens[7]);
        xform[3][1] = std::stof(tokens[8]);
        xform[0][2] = std::stof(tokens[9]);
        xform[1][2] = std::stof(tokens[10]);
        xform[2][2] = std::stof(tokens[11]);
        xform[3][2] = std::stof(tokens[12]);
        xform[0][3] = std::stof(tokens[13]);
        xform[1][3] = std::stof(tokens[14]);
        xform[2][3] = std::stof(tokens[15]);
        xform[3][3] = std::stof(tokens[16]);
        hasMatrix = true;
      }
      else
        parsingError++;
    }
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError )
  {
    int sphereID = ioScene.AddPrimitive(newSphere);
    if ( sphereID >= 0 )
    {
      int matID = -1;
      if ( !materialName.empty() )
      {
        matID = ioScene.FindMaterialID(materialName);
        if ( matID < 0 )
          std::cout << "Loader : ERROR could not find material " << materialName << std::endl;
      }

      if ( !hasMatrix )
        xform = transMat * rotMat * scaleMat;

      PrimitiveInstance instance(sphereID, matID, xform);
      ioScene.AddPrimitiveInstance(instance);
    }
  }

  return parsingError;
}

// ----------------------------------------------------------------------------
// ParseBox
// ----------------------------------------------------------------------------
int Loader::ParseBox( std::ifstream & iStr, Scene & ioScene )
{
  int parsingError = 0;

  std::string materialName;
  Mat4x4 transMat(1.f), rotMat(1.f), scaleMat(1.f);
  Mat4x4 xform(1.f);
  bool hasMatrix = false;

  Box newBox;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = static_cast<int>(tokens.size());
    if ( !nbTokens || ( '#' == tokens[0][0] ) )
      continue;

    if ( State::ExpectOpenBracket == curState )
    {
      if ( '{' == tokens[0][0] )
        curState = State::ExpectClosingBracket;
      else if ( '}' == tokens[0][0] )
        parsingError++;
      continue;
    }

    if ( State::ExpectClosingBracket == curState )
    {
      if ( '}' == tokens[0][0] )
      {
        curState = State::ExpectNewBlock;
        break;
      }
      else if ( '{' == tokens[0][0] )
      {
        parsingError++;
        continue;
      }
    }

    if ( IsEqual("low", tokens[0]) )
    {
      if ( 4 == nbTokens )
        newBox._Low = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
      else
        parsingError++;
    }
    else if ( IsEqual("high", tokens[0]) )
    {
      if ( 4 == nbTokens )
        newBox._High = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
      else
        parsingError++;
    }
    else if ( IsEqual("material", tokens[0]) )
    {
      if ( 2 == nbTokens )
        materialName = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("position", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        transMat[3][0] = std::stof(tokens[1]);
        transMat[3][1] = std::stof(tokens[2]);
        transMat[3][2] = std::stof(tokens[3]);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("scale", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        scaleMat[0][0] = std::stof(tokens[1]);
        scaleMat[1][1] = std::stof(tokens[2]);
        scaleMat[2][2] = std::stof(tokens[3]);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("rotation", tokens[0]) )
    {
      if ( 5 == nbTokens )
      {
        Vec4 quaternion; // { x, y, z, s } -> q = s + ix + jy + kz
        quaternion.x = std::stof(tokens[1]);
        quaternion.y = std::stof(tokens[2]);
        quaternion.z = std::stof(tokens[3]);
        quaternion.w = std::stof(tokens[4]);

        rotMat = MathUtil::QuatToMatrix(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("matrix", tokens[0]) )
    {
      if ( 17 == nbTokens )
      {
        xform[0][0] = std::stof(tokens[1]);
        xform[1][0] = std::stof(tokens[2]);
        xform[2][0] = std::stof(tokens[3]);
        xform[3][0] = std::stof(tokens[4]);
        xform[0][1] = std::stof(tokens[5]);
        xform[1][1] = std::stof(tokens[6]);
        xform[2][1] = std::stof(tokens[7]);
        xform[3][1] = std::stof(tokens[8]);
        xform[0][2] = std::stof(tokens[9]);
        xform[1][2] = std::stof(tokens[10]);
        xform[2][2] = std::stof(tokens[11]);
        xform[3][2] = std::stof(tokens[12]);
        xform[0][3] = std::stof(tokens[13]);
        xform[1][3] = std::stof(tokens[14]);
        xform[2][3] = std::stof(tokens[15]);
        xform[3][3] = std::stof(tokens[16]);
        hasMatrix = true;
      }
      else
        parsingError++;
    }
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError )
  {
    int boxID = ioScene.AddPrimitive(newBox);
    if ( boxID >= 0 )
    {
      int matID = -1;
      if ( !materialName.empty() )
      {
        matID = ioScene.FindMaterialID(materialName);
        if ( matID < 0 )
          std::cout << "Loader : ERROR could not find material " << materialName << std::endl;
      }

      if ( !hasMatrix )
        xform = transMat * rotMat * scaleMat;

      PrimitiveInstance instance(boxID, matID, xform);
      ioScene.AddPrimitiveInstance(instance);
    }
  }

  return parsingError;
}

// ----------------------------------------------------------------------------
// ParsePlane
// ----------------------------------------------------------------------------
int Loader::ParsePlane( std::ifstream & iStr, Scene & ioScene )
{
  int parsingError = 0;

  std::string materialName;
  Mat4x4 transMat(1.f), rotMat(1.f), scaleMat(1.f);
  Mat4x4 xform(1.f);
  bool hasMatrix = false;

  Plane newPlane;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = static_cast<int>(tokens.size());
    if ( !nbTokens || ( '#' == tokens[0][0] ) )
      continue;

    if ( State::ExpectOpenBracket == curState )
    {
      if ( '{' == tokens[0][0] )
        curState = State::ExpectClosingBracket;
      else if ( '}' == tokens[0][0] )
        parsingError++;
      continue;
    }

    if ( State::ExpectClosingBracket == curState )
    {
      if ( '}' == tokens[0][0] )
      {
        curState = State::ExpectNewBlock;
        break;
      }
      else if ( '{' == tokens[0][0] )
      {
        parsingError++;
        continue;
      }
    }

    if ( IsEqual("origin", tokens[0]) )
    {
      if ( 4 == nbTokens )
        newPlane._Origin = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
      else
        parsingError++;
    }
    else if ( IsEqual("normal", tokens[0]) )
    {
      if ( 4 == nbTokens )
        newPlane._Normal = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
      else
        parsingError++;
    }
    else if ( IsEqual("material", tokens[0]) )
    {
      if ( 2 == nbTokens )
        materialName = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("position", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        transMat[3][0] = std::stof(tokens[1]);
        transMat[3][1] = std::stof(tokens[2]);
        transMat[3][2] = std::stof(tokens[3]);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("scale", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        scaleMat[0][0] = std::stof(tokens[1]);
        scaleMat[1][1] = std::stof(tokens[2]);
        scaleMat[2][2] = std::stof(tokens[3]);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("rotation", tokens[0]) )
    {
      if ( 5 == nbTokens )
      {
        Vec4 quaternion; // { x, y, z, s } -> q = s + ix + jy + kz
        quaternion.x = std::stof(tokens[1]);
        quaternion.y = std::stof(tokens[2]);
        quaternion.z = std::stof(tokens[3]);
        quaternion.w = std::stof(tokens[4]);

        rotMat = MathUtil::QuatToMatrix(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("matrix", tokens[0]) )
    {
      if ( 17 == nbTokens )
      {
        xform[0][0] = std::stof(tokens[1]);
        xform[1][0] = std::stof(tokens[2]);
        xform[2][0] = std::stof(tokens[3]);
        xform[3][0] = std::stof(tokens[4]);
        xform[0][1] = std::stof(tokens[5]);
        xform[1][1] = std::stof(tokens[6]);
        xform[2][1] = std::stof(tokens[7]);
        xform[3][1] = std::stof(tokens[8]);
        xform[0][2] = std::stof(tokens[9]);
        xform[1][2] = std::stof(tokens[10]);
        xform[2][2] = std::stof(tokens[11]);
        xform[3][2] = std::stof(tokens[12]);
        xform[0][3] = std::stof(tokens[13]);
        xform[1][3] = std::stof(tokens[14]);
        xform[2][3] = std::stof(tokens[15]);
        xform[3][3] = std::stof(tokens[16]);
        hasMatrix = true;
      }
      else
        parsingError++;
    }
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError )
  {
    int planeID = ioScene.AddPrimitive(newPlane);
    if ( planeID >= 0 )
    {
      int matID = -1;
      if ( !materialName.empty() )
      {
        matID = ioScene.FindMaterialID(materialName);
        if ( matID < 0 )
          std::cout << "Loader : ERROR could not find material " << materialName << std::endl;
      }

      if ( !hasMatrix )
        xform = transMat * rotMat * scaleMat;

      PrimitiveInstance instance(planeID, matID, xform);
      ioScene.AddPrimitiveInstance(instance);
    }
  }

  return parsingError;
}

// ----------------------------------------------------------------------------
// ParseMeshData
// ----------------------------------------------------------------------------
int Loader::ParseMeshData( std::ifstream & iStr, const std::string & iPath, Scene & ioScene )
{
  int parsingError = 0;

  std::string meshName;
  std::string meshFileName;
  std::string materialName;
  Mat4x4 transMat(1.f), rotMat(1.f), scaleMat(1.f);
  Mat4x4 xform(1.f);
  bool hasMatrix = false;


  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = static_cast<int>(tokens.size());
    if ( !nbTokens || ( '#' == tokens[0][0] ) )
      continue;

    if ( State::ExpectOpenBracket == curState )
    {
      if ( '{' == tokens[0][0] )
        curState = State::ExpectClosingBracket;
      else if ( '}' == tokens[0][0] )
        parsingError++;
      continue;
    }

    if ( State::ExpectClosingBracket == curState )
    {
      if ( '}' == tokens[0][0] )
      {
        curState = State::ExpectNewBlock;
        break;
      }
      else if ( '{' == tokens[0][0] )
      {
        parsingError++;
        continue;
      }
    }

    if ( IsEqual("name", tokens[0]) )
    {
      if ( 2 == nbTokens )
        meshName = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("file", tokens[0]) )
    {
      if ( 2 == nbTokens )
        meshFileName = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("material", tokens[0]) )
    {
      if ( 2 == nbTokens )
        materialName = tokens[1];
      else
        parsingError++;
    }
    else if ( IsEqual("position", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        transMat[3][0] = std::stof(tokens[1]);
        transMat[3][1] = std::stof(tokens[2]);
        transMat[3][2] = std::stof(tokens[3]);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("scale", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        scaleMat[0][0] = std::stof(tokens[1]);
        scaleMat[1][1] = std::stof(tokens[2]);
        scaleMat[2][2] = std::stof(tokens[3]);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("rotation", tokens[0]) )
    {
      if ( 5 == nbTokens )
      {
        Vec4 quaternion; // { x, y, z, s } -> q = s + ix + jy + kz
        quaternion.x = std::stof(tokens[1]);
        quaternion.y = std::stof(tokens[2]);
        quaternion.z = std::stof(tokens[3]);
        quaternion.w = std::stof(tokens[4]);

        rotMat = MathUtil::QuatToMatrix(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("matrix", tokens[0]) )
    {
      if ( 17 == nbTokens )
      {
        xform[0][0] = std::stof(tokens[1]);
        xform[1][0] = std::stof(tokens[2]);
        xform[2][0] = std::stof(tokens[3]);
        xform[3][0] = std::stof(tokens[4]);
        xform[0][1] = std::stof(tokens[5]);
        xform[1][1] = std::stof(tokens[6]);
        xform[2][1] = std::stof(tokens[7]);
        xform[3][1] = std::stof(tokens[8]);
        xform[0][2] = std::stof(tokens[9]);
        xform[1][2] = std::stof(tokens[10]);
        xform[2][2] = std::stof(tokens[11]);
        xform[3][2] = std::stof(tokens[12]);
        xform[0][3] = std::stof(tokens[13]);
        xform[1][3] = std::stof(tokens[14]);
        xform[2][3] = std::stof(tokens[15]);
        xform[3][3] = std::stof(tokens[16]);
        hasMatrix = true;
      }
      else
        parsingError++;
    }
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError && !meshFileName.empty() )
  {
    int meshID = ioScene.AddMesh(iPath + meshFileName);
    if ( meshID >= 0 )
    {
      if ( meshName.empty() )
      {
        fs::path filepath = meshFileName;
        meshName = filepath.filename().string();
      }

      int matID = -1;
      if ( !materialName.empty() )
      {
        matID = ioScene.FindMaterialID(materialName);
        if ( matID < 0 )
          std::cout << "Loader : ERROR could not find material " << materialName << std::endl;
      }

      if ( !hasMatrix )
        xform = transMat * rotMat * scaleMat;

      MeshInstance instance(meshName, meshID, matID, xform);
      ioScene.AddMeshInstance(instance);
    }
  }

  return parsingError;
}

// ----------------------------------------------------------------------------
// ParseGLTF
// ----------------------------------------------------------------------------
int Loader::ParseGLTF( std::ifstream & iStr, const std::string & iPath, Scene & ioScene, RenderSettings & ioSettings )
{
  int parsingError = 0;

  fs::path filepath;
  Mat4x4 transMat(1.f), rotMat(1.f), scaleMat(1.f);
  Mat4x4 xform(1.f);
  bool hasMatrix = false;
  bool isBinary = false;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = static_cast<int>(tokens.size());
    if ( !nbTokens || ( '#' == tokens[0][0] ) )
      continue;

    if ( State::ExpectOpenBracket == curState )
    {
      if ( '{' == tokens[0][0] )
        curState = State::ExpectClosingBracket;
      else if ( '}' == tokens[0][0] )
        parsingError++;
      continue;
    }

    if ( State::ExpectClosingBracket == curState )
    {
      if ( '}' == tokens[0][0] )
      {
        curState = State::ExpectNewBlock;
        break;
      }
      else if ( '{' == tokens[0][0] )
      {
        parsingError++;
        continue;
      }
    }

    if ( IsEqual("file", tokens[0]) )
    {
      if ( 2 == nbTokens )
      {
        filepath = iPath + tokens[1];

        if ( filepath.extension() == ".gltf" )
          isBinary = false;
        else if ( filepath.extension() == ".glb" )
          isBinary = true;
        else
          parsingError++;
      }
      else
        parsingError++;
    }
    else if ( IsEqual("position", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        transMat[3][0] = std::stof(tokens[1]);
        transMat[3][1] = std::stof(tokens[2]);
        transMat[3][2] = std::stof(tokens[3]);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("scale", tokens[0]) )
    {
      if ( 4 == nbTokens )
      {
        scaleMat[0][0] = std::stof(tokens[1]);
        scaleMat[1][1] = std::stof(tokens[2]);
        scaleMat[2][2] = std::stof(tokens[3]);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("rotation", tokens[0]) )
    {
      if ( 5 == nbTokens )
      {
        Vec4 quaternion; // { x, y, z, s } -> q = s + ix + jy + kz
        quaternion.x = std::stof(tokens[1]);
        quaternion.y = std::stof(tokens[2]);
        quaternion.z = std::stof(tokens[3]);
        quaternion.w = std::stof(tokens[4]);

        rotMat = MathUtil::QuatToMatrix(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
      }
      else
        parsingError++;
    }
    else if ( IsEqual("matrix", tokens[0]) )
    {
      if ( 17 == nbTokens )
      {
        xform[0][0] = std::stof(tokens[1]);
        xform[1][0] = std::stof(tokens[2]);
        xform[2][0] = std::stof(tokens[3]);
        xform[3][0] = std::stof(tokens[4]);
        xform[0][1] = std::stof(tokens[5]);
        xform[1][1] = std::stof(tokens[6]);
        xform[2][1] = std::stof(tokens[7]);
        xform[3][1] = std::stof(tokens[8]);
        xform[0][2] = std::stof(tokens[9]);
        xform[1][2] = std::stof(tokens[10]);
        xform[2][2] = std::stof(tokens[11]);
        xform[3][2] = std::stof(tokens[12]);
        xform[0][3] = std::stof(tokens[13]);
        xform[1][3] = std::stof(tokens[14]);
        xform[2][3] = std::stof(tokens[15]);
        xform[3][3] = std::stof(tokens[16]);
        hasMatrix = true;
      }
      else
        parsingError++;
    }
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError && fs::exists(filepath) )
  {
    if ( !hasMatrix )
      xform = transMat * rotMat * scaleMat;

    if ( !Loader::LoadFromGLTF(filepath.string(), xform, ioScene, ioSettings, isBinary) )
      parsingError++;

    if ( parsingError )
      printf("Unable to load gltf %s\n", filepath.string().c_str());
  }

  return parsingError;
}

}
