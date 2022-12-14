/*
  This is a modified version of the original code : https://github.com/knightcrawler25/GLSL-PathTracer
  already derived from: https://github.com/mmacklin/tinsel
*/

#include "Loader.h"
#include "Scene.h"
#include "Mesh.h"
#include "Primitive.h"
#include "Light.h"
#include "Camera.h"
#include "RenderSettings.h"
#include "Math.h"
#include <map>
#include <vector>
#include <iostream>
#include <algorithm>
#include <filesystem> // C++17
#include <sstream>
#include <fstream>

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

bool Loader::LoadScene(const std::string & iFilename, Scene *& oScene, RenderSettings & oRenderSettings)
{
  oScene = nullptr;

  fs::path filepath = iFilename;

  if ( ".scene" == filepath.extension() )
    return Loader::LoadFromSceneFile(iFilename, oScene, oRenderSettings);

  return false;
}

bool Loader::LoadFromSceneFile(const std::string & iFilename, Scene *& oScene, RenderSettings & oRenderSettings )
{
  oScene = nullptr;

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

      parsingError += Loader::ParseMaterial(file, path, materialName, *oScene);

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

      parsingError += Loader::ParseMeshData(file, path, *oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Mesh - END
    //--------------------------------------------


    //--------------------------------------------
    // Sphere - START
    if ( "sphere" == tokens[0] )
    {
      std::cout << "New sphere" << std::endl;

      parsingError += Loader::ParseSphere(file, *oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Sphere - END
    //--------------------------------------------


    //--------------------------------------------
    // Box - START
    if ( "box" == tokens[0] )
    {
      std::cout << "New box" << std::endl;

      parsingError += Loader::ParseBox(file, *oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Box - END
    //--------------------------------------------


    //--------------------------------------------
    // Plane - START
    if ( "plane" == tokens[0] )
    {
      std::cout << "New plane" << std::endl;

      parsingError += Loader::ParsePlane(file, *oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Plane - END
    //--------------------------------------------


    //--------------------------------------------
    // Light - START
    if ( "light" == tokens[0] )
    {
      std::cout << "New light" << std::endl;

      parsingError += Loader::ParseLight(file, *oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Light - END
    //--------------------------------------------


    //--------------------------------------------
    // Camera - START
    if ( "camera" == tokens[0] )
    {
      std::cout << "New camera" << std::endl;

      parsingError += Loader::ParseCamera(file, *oScene);

      if ( !parsingError )
        curState = State::ExpectNewBlock;
    }
    // Camera - END
    //--------------------------------------------


    //--------------------------------------------
    // Renderer - START
    if ( "renderer" == tokens[0] )
    {
      std::cout << "New renderer" << std::endl;

      RenderSettings settings;
      parsingError += Loader::ParseRenderSettings(file, settings, *oScene);

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
        newMaterial._Albedo = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "emission" == tokens[0] )
    {
      if ( 4 == nbTokens )
        newMaterial._Emission = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "mediumcolor" == tokens[0] )
    {
      if ( 4 == nbTokens )
        newMaterial._MediumColor = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "opacity" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._Opacity = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "alphaCutoff" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._AlphaCutoff = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "metallic" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._Metallic = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "roughness" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._Roughness = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "subsurface" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._Subsurface = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "speculartint" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._SpecularTint = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "anisotropic" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._Anisotropic = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "sheen" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._Sheen = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "sheenTint" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._SheenTint = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "clearcoat" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._Clearcoat = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "clearcoatgloss" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._ClearcoatGloss = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "spectrans" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._SpecTrans = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "ior" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._IOR = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "mediumdensity" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._MediumDensity = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "mediumanisotropy" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newMaterial._MediumAnisotropy = std::stof(tokens[1]);
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
        metallicRoughnessTexName = tokens[1];
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
      newMaterial._BaseColorTexId = ioScene.AddTexture(iPath + albedoTexName);
    if ( !metallicRoughnessTexName.empty() )
      newMaterial._MetallicRoughnessTexID = ioScene.AddTexture(iPath + metallicRoughnessTexName);
    if ( !normalTexName.empty() )
      newMaterial._NormalMapTexID = ioScene.AddTexture(iPath + normalTexName);
    if ( !emissionTexName.empty() )
      newMaterial._EmissionMapTexID = ioScene.AddTexture(iPath + emissionTexName);

     ioScene.AddMaterial(newMaterial, iMaterialName);
  }

  return parsingError;
}

int Loader::ParseLight( std::ifstream & iStr, Scene & ioScene )
{
  int parsingError = 0;

  Light newLight;

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
        newLight._Pos = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "emission" == tokens[0] )
    {
      if ( 4 == nbTokens )
        newLight._Emission = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "v1" == tokens[0] )
    {
      if ( 4 == nbTokens )
        newLight._DirU = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "v2" == tokens[0] )
    {
      if ( 4 == nbTokens )
        newLight._DirV = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "radius" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newLight._Radius = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "type" == tokens[0] )
    {
      if ( 2 == nbTokens )
      {
        if ( "quad" == tokens[1] )
          newLight._Type = (float) LightType::RectLight;
        else if ( "sphere" == tokens[1] )
          newLight._Type = (float) LightType::SphereLight;
        else if ( "distant" == tokens[1] )
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
      newLight._Area = glm::length(glm::cross(newLight._DirU, newLight._DirV));
    }
    else if ( LightType::SphereLight == (LightType)newLight._Type )
    {
      newLight._Area = 4.0f * M_PI * newLight._Radius * newLight._Radius;
    }
    else if ( LightType::DistantLight == (LightType)newLight._Type )
    {
      newLight._Area = 0.f;
    }

    ioScene.AddLight(newLight);
  }

  return parsingError;
}

int Loader::ParseCamera( std::ifstream & iStr, Scene & ioScene )
{
  int parsingError = 0;

  Vec3 pos({0.f, 0.f, -1.f});
  Vec3 lookAt({0.f, 0.f, 0.f});
  float fov = 80.f;
  float focalDist = -1.f;
  float aperture  = -1.f;

  Mat4x4 xform;
  bool hasMatrix = false;

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
        pos = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "lookat" == tokens[0] )
    {
      if ( 4 == nbTokens )
        lookAt = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "aperture" == tokens[0] )
    {
      if ( 2 == nbTokens )
        aperture = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "focaldist" == tokens[0] )
    {
      if ( 2 == nbTokens )
        focalDist = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "fov" == tokens[0] )
    {
      if ( 2 == nbTokens )
        fov = std::stof(tokens[1]);
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

    ioScene.SetCamera(newCamera);
  }

  return parsingError;
}

int Loader::ParseRenderSettings( std::ifstream & iStr, RenderSettings & oSettings, Scene & ioScene )
{
  int parsingError = 0;

  bool hasRenderResolution = false;
  bool hasWindowResolution = false;

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

    if ( "resolution" == tokens[0] )
    {
      if ( 3 == nbTokens )
      {
        hasRenderResolution = true;
        oSettings._RenderResolution = Vec2i(std::stoi(tokens[1]), std::stoi(tokens[2]));
      }
      else
        parsingError++;
    }
    else if ( "windowresolution" == tokens[0] )
    {
      if ( 3 == nbTokens )
      {
        hasWindowResolution = true;
        oSettings._WindowResolution = Vec2i(std::stoi(tokens[1]), std::stoi(tokens[2]));
      }
      else
        parsingError++;
    }
    else if ( "backgroundcolor" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oSettings._BackgroundColor = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "uniformlightcolor" == tokens[0] )
    {
      if ( 4 == nbTokens )
        oSettings._UniformLightCol = Vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
      else
        parsingError++;
    }
    else if ( "enablebackground" == tokens[0] )
    {
      if ( 2 == nbTokens )
      {
        if ( "true" == tokens[1] )
          oSettings._EnableBackGround = true;
        else if ( "false" == tokens[1] )
          oSettings._EnableBackGround = false;
        else
          parsingError++;
      }
      else
        parsingError++;
    }
    else if ( "enableskybox" == tokens[0] )
    {
      if ( 2 == nbTokens )
      {
        if ( "true" == tokens[1] )
          oSettings._EnableSkybox = true;
        else if ( "false" == tokens[1] )
          oSettings._EnableSkybox = false;
        else
          parsingError++;
      }
      else
        parsingError++;
    }
    else if ( "enableuniformlight" == tokens[0] )
    {
      if ( 2 == nbTokens )
      {
        if ( "true" == tokens[1] )
          oSettings._EnableUniformLight = true;
        else if ( "false" == tokens[1] )
          oSettings._EnableUniformLight = false;
        else
          parsingError++;
      }
      else
        parsingError++;
    }
    else if ( "bounces" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oSettings._Bounces =  std::stoi(tokens[1]);
      else
        parsingError++;
    }
    else if ( "maxdepth" == tokens[0] )
    {
      if ( 2 == nbTokens )
        oSettings._Bounces =  std::stoi(tokens[1]);
      else
        parsingError++;
    }
    //else if ( "background" == tokens[0] )
    //{
    //  if ( 2 == nbTokens )
    //  {
    //  }
    //  else
    //    parsingError++;
    //}
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
  }

  return parsingError;
}

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

    if ( "radius" == tokens[0] )
    {
      if ( 2 == nbTokens )
        newSphere._Radius = std::stof(tokens[1]);
      else
        parsingError++;
    }
    else if ( "material" == tokens[0] )
    {
      if ( 2 == nbTokens )
        materialName = tokens[1];
      else
        parsingError++;
    }
    else if ( "position" == tokens[0] )
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
    else if ( "scale" == tokens[0] )
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
    else if ( "rotation" == tokens[0] )
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

    if ( "low" == tokens[0] )
    {
      if ( 4 == nbTokens )
        newBox._Low = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
      else
        parsingError++;
    }
    else if ( "high" == tokens[0] )
    {
      if ( 4 == nbTokens )
        newBox._High = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
      else
        parsingError++;
    }
    else if ( "material" == tokens[0] )
    {
      if ( 2 == nbTokens )
        materialName = tokens[1];
      else
        parsingError++;
    }
    else if ( "position" == tokens[0] )
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
    else if ( "scale" == tokens[0] )
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
    else if ( "rotation" == tokens[0] )
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

    if ( "origin" == tokens[0] )
    {
      if ( 4 == nbTokens )
        newPlane._Origin = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
      else
        parsingError++;
    }
    else if ( "normal" == tokens[0] )
    {
      if ( 4 == nbTokens )
        newPlane._Normal = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
      else
        parsingError++;
    }
    else if ( "material" == tokens[0] )
    {
      if ( 2 == nbTokens )
        materialName = tokens[1];
      else
        parsingError++;
    }
    else if ( "position" == tokens[0] )
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
    else if ( "scale" == tokens[0] )
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
    else if ( "rotation" == tokens[0] )
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

    if ( "name" == tokens[0] )
    {
      if ( 2 == nbTokens )
        meshName = tokens[1];
      else
        parsingError++;
    }
    else if ( "file" == tokens[0] )
    {
      if ( 2 == nbTokens )
        meshFileName = tokens[1];
      else
        parsingError++;
    }
    else if ( "material" == tokens[0] )
    {
      if ( 2 == nbTokens )
        materialName = tokens[1];
      else
        parsingError++;
    }
    else if ( "position" == tokens[0] )
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
    else if ( "scale" == tokens[0] )
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
    else if ( "rotation" == tokens[0] )
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

    // TODO
  }
  if ( State::ExpectNewBlock != curState )
    parsingError++;

  return parsingError;
}

}
