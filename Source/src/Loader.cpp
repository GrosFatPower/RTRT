/*
  This is a modified version of the original code : https://github.com/knightcrawler25/GLSL-PathTracer
  already derived from: https://github.com/mmacklin/tinsel
*/

#include "Loader.h"
#include "Scene.h"
#include "MeshData.h"
#include "Light.h"
#include "Camera.h"
#include "Math.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <map>
#include <vector>
#include <iostream>
#include <algorithm>
#include <filesystem> // C++17

namespace fs = std::filesystem;

namespace RTRT
{

enum class State
{
  ExpectNewBlock,
  ExpectOpenBracket,
  ExpectClosingBracket,
};

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

bool Loader::LoadScene(const std::string & iFilename, Scene * oScene)
{
  oScene = nullptr;

  fs::path filepath = iFilename;

  if ( ".scene" == filepath.extension() )
    return Loader::LoadFromSceneFile(iFilename, oScene);

  return false;
}

bool Loader::LoadFromSceneFile(const std::string & iFilename, Scene * oScene)
{
  oScene = nullptr;

  fs::path filepath = iFilename;
  filepath.remove_filename();

  std::ifstream file(iFilename);

  if ( !file.is_open() )
  {
    printf("Loader : Couldn't open %s for reading\n", iFilename.c_str());
    return false;
  }

  printf("Loading Scene...\n");

  oScene = new Scene;

  int parsingError = 0;
  State curState = State::ExpectNewBlock;

  std::string line;
  while( std::getline( file, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = tokens.size();
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
    if ( ( 2 == nbTokens ) && ( "material" == tokens[0] ) )
    {
      std::cout << "New material : " << tokens[1] << std::endl;

      std::string materialName = tokens[1];

      Material newMaterial;
      parsingError += Loader::ParseMaterial(file, newMaterial, *oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Material - END
    //--------------------------------------------


    //--------------------------------------------
    // Mesh - START
    if ( "mesh" == tokens[0] )
    {
      std::cout << "New mesh" << std::endl;

      MeshData newMesh;
      parsingError += Loader::ParseMeshData(file, newMesh);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Mesh - END
    //--------------------------------------------


    //--------------------------------------------
    // Light - START
    if ( "light" == tokens[0] )
    {
      std::cout << "New light" << std::endl;

      Light newLight;
      parsingError += Loader::ParseLight(file, newLight);

      if ( !parsingError )
      {
        oScene -> _Lights.push_back(newLight);

        curState = State::ExpectNewBlock;
      }
    }
    // Light - END
    //--------------------------------------------


    //--------------------------------------------
    // Camera - START
    if ( "camera" == tokens[0] )
    {
      std::cout << "New camera" << std::endl;

      Camera newCamera;
      parsingError += Loader::ParseCamera(file, newCamera);

      if ( !parsingError )
      {
        oScene -> _Camera = newCamera;

        curState = State::ExpectNewBlock;
      }
    }
    // Camera - END
    //--------------------------------------------


    //--------------------------------------------
    // Renderer - START
    if ( "renderer" == tokens[0] )
    {
      std::cout << "New renderer" << std::endl;

      /*RenderOtions renderer*/
      parsingError += Loader::ParseRenderer(file/*, renderer*/);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Renderer - END
    //--------------------------------------------


    //--------------------------------------------
    // GLTF - START
    if ( "gltf" == tokens[0] )
    {
      std::cout << "New gltf model" << std::endl;

      parsingError += Loader::ParseGLTF(file, *oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // GLTF - END
    //--------------------------------------------
   

  }

  if ( parsingError )
  {
    printf("ERROR\n");
    delete oScene;
    oScene = nullptr;
  }
  else
    printf("DONE\n");

  if ( oScene )
    return true;
  return false;
}

bool Loader::LoadMeshData( const std::string & iFilename, MeshData * oMeshData )
{
  oMeshData = nullptr;

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err, warn;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, &warn, iFilename.c_str(), 0, true);
  if (!ret)
  {
    std::cout << "Loader : Unable to load object" << iFilename << std::endl;
    if ( !err.empty() )
      std::cout << err;
    if ( !warn.empty() )
      std::cout << warn;
    return false;
  }
  else if ( !warn.empty() )
  {
    std::cout << "Loader : loading object" << iFilename << " ..."<< std::endl;
    std::cout << warn;
  }

  oMeshData = new MeshData;

  oMeshData -> _Filename = iFilename;

  size_t nbVertices = attrib.vertices.size() / 3;
  for ( unsigned int i = 0; i < nbVertices; ++i )
  {
    Vec3 coords;

    coords.x = attrib.vertices[3 * i + 0];
    coords.y = attrib.vertices[3 * i + 1];
    coords.z = attrib.vertices[3 * i + 2];

    oMeshData -> _Vertices.push_back(coords);
  }

  size_t nbNormals = attrib.normals.size() / 3;
  for ( unsigned int i = 0; i < nbNormals; ++i )
  {
    Vec3 normal;
    normal.x = attrib.normals[3 * i + 0];
    normal.y = attrib.normals[3 * i + 1];
    normal.z = attrib.normals[3 * i + 2];

    oMeshData -> _Normals.push_back(normal);
  }

  size_t nbUVs = attrib.texcoords.size() / 3;
  for ( unsigned int i = 0; i < nbUVs; ++i )
  {
    Vec2 uv;
    uv.x = attrib.texcoords[2 * i + 0];
    uv.y = attrib.texcoords[2 * i + 1];

    oMeshData -> _UVs.push_back(uv);
  }

  for ( size_t s = 0; s < shapes.size(); ++s )
  {
    tinyobj::shape_t & shape = shapes[s];

    size_t index_offset = 0;
    for ( size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f )
    {
      for ( size_t v = 0; v < 3; ++v )
      {
        tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

        Vec3i indices;
        indices.x = idx.vertex_index;
        indices.y = idx.normal_index;
        indices.z = idx.texcoord_index;

        oMeshData -> _Indices.push_back(indices);
      }
      index_offset += 3;
    }
  }
  oMeshData -> _NbFaces = (int) oMeshData -> _Indices.size();

  return true;
}

int Loader::ParseMaterial( std::ifstream & iStr, Material & oMaterial, Scene & ioScene )
{
  int parsingError = 0;

  std::string albedoTexName;
  std::string metallicRoughnessTTexName;
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
    int nbTokens = tokens.size();
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

    if ( "color" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oMaterial._BaseColor = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "emission" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oMaterial._Emission = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "mediumcolor" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oMaterial._MediumColor = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "opacity" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._Opacity = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "alphaCutoff" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._AlphaCutoff = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "metallic" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._Metallic = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "roughness" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._Roughness = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "subsurface" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._Subsurface = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "speculartint" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._SpecularTint = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "anisotropic" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._Anisotropic = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "sheen" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._Sheen = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "sheenTint" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._SheenTint = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "clearcoat" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._Clearcoat = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "clearcoatgloss" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._ClearcoatGloss = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "spectrans" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._SpecTrans = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "ior" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._IOR = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "mediumdensity" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._MediumDensity = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "mediumanisotropy" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oMaterial._MediumAnisotropy = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "albedotexture" == tokens[0] )
    {
      if ( 2 == nbTokens )
        albedoTexName = tokens[1];
      else
        parsingError++;
    }
    else if ( "metallicroughnesstexture" == tokens[0] )
    {
      if ( 2 == nbTokens )
        metallicRoughnessTTexName = tokens[1];
      else
        parsingError++;
    }
    else if ( "normaltexture" == tokens[0] )
    {
      if ( 2 == nbTokens )
        normalTexName = tokens[1];
      else
        parsingError++;
    }
    else if ( "emissiontexture" == tokens[0] )
    {
      if ( 2 == nbTokens )
        emissionTexName = tokens[1];
      else
        parsingError++;
    }
    else if ( "alphamode" == tokens[0] )
    {
      if ( 2 == nbTokens )
        alphaMode = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "mediumtype" == tokens[0] )
    {
      if ( 2 == nbTokens )
        mediumType = std::stof(tokens[1]);
      else
        parsingError++;
    }
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError )
  {
    if ( "opaque" == alphaMode )
      oMaterial._AlphaMode = (float)AlphaMode::Opaque;
    else if ( "blend" == alphaMode )
      oMaterial._AlphaMode = (float)AlphaMode::Blend;
    else if ( "mask" == alphaMode )
      oMaterial._AlphaMode = (float)AlphaMode::Mask;

    if ( "absorb" == mediumType )
      oMaterial._MediumType = (float)MediumType::Absorb;
    else if ( "scatter" == mediumType )
      oMaterial._MediumType = (float)MediumType::Scatter;
    else if ( "emissive" == mediumType )
      oMaterial._MediumType = (float)MediumType::Emissive;

    // ToDo Add textures du scene and retrieve IDs
  }

  return parsingError;
}

int Loader::ParseLight( std::ifstream & iStr, Light & oLight )
{
  int parsingError = 0;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = tokens.size();
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

    if ( "position" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oLight._Pos = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "emission" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oLight._Emission = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "v1" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oLight._DirU = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "v2" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oLight._DirV = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "radius" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oLight._Radius = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "type" == tokens[0] )
    {
      if ( 2 == nbTokens )
      {
        if ( "quad" == tokens[1] )
          oLight._Type = (float) LightType::RectLight;
        else if ( "sphere" == tokens[1] )
          oLight._Type = (float) LightType::SphereLight;
        else if ( "distant" == tokens[1] )
          oLight._Type = (float) LightType::DistantLight;
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
    if ( LightType::RectLight == (LightType)oLight._Type )
    {
      oLight._Area = glm::length(glm::cross(oLight._DirU, oLight._DirV));
    }
    else if ( LightType::SphereLight == (LightType)oLight._Type )
    {
      oLight._Area = 4.0f * M_PI * oLight._Radius * oLight._Radius;
    }
    else if ( LightType::DistantLight == (LightType)oLight._Type )
    {
      oLight._Area = 0.f;
    }
  }

  return parsingError;
}

int Loader::ParseCamera( std::ifstream & iStr, Camera & oCamera )
{
  int parsingError = 0;

  Mat4x4 xform;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = tokens.size();
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

    if ( "position" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oCamera._Pos = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "lookat" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oCamera._Pivot = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "aperture" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oCamera._Aperture = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "focaldist" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oCamera._FocalDist = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "fov" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oCamera._FOV = (MathUtil::ToRadians(std::stof(tokens[1])));
      else
        parsingError++;
    }
    else if ( "matrix" == tokens[0] )
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

        Vec3 forward = Vec3(xform[2][0], xform[2][1], xform[2][2]);
        oCamera._Pos = Vec3(xform[3][0], xform[3][1], xform[3][2]);
        oCamera._Pivot = oCamera._Pos + forward;
      }
      else
        parsingError++;
    }

  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  if ( !parsingError )
    oCamera.Update();

  return parsingError;
}

int Loader::ParseRenderer( std::ifstream & iStr/*, RenderOtions & oRenderer*/ )
{
  int parsingError = 0;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = tokens.size();
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
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  return parsingError;
}

int Loader::ParseMeshData( std::ifstream & iStr, MeshData & oMeshData )
{
  int parsingError = 0;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = tokens.size();
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
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  return parsingError;
}

int Loader::ParseGLTF( std::ifstream & iStr, Scene & ioScene )
{
  int parsingError = 0;

  State curState = State::ExpectOpenBracket;
  std::string line;
  while( std::getline( iStr, line ) && !parsingError )
  {
    std::vector<std::string> tokens;
    Tokenize(line, tokens);
    int nbTokens = tokens.size();
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
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  return parsingError;
}

}
