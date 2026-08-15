#include "FpsGameMap.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Local helpers
// ----------------------------------------------------------------------------
static std::string ToLower( std::string iText )
{
  std::transform(iText.begin(), iText.end(), iText.begin(),
                 []( unsigned char iChar ) { return static_cast<char>(std::tolower(iChar)); });
  return iText;
}

static bool IsEqual( const std::string & iA, const std::string & iB )
{
  return ToLower(iA) == ToLower(iB);
}

static std::vector<std::string> Tokenize( const std::string & iLine )
{
  std::vector<std::string> tokens;
  std::string token;
  bool inQuote = false;

  for ( char c : iLine )
  {
    if ( !inQuote && ( '#' == c ) )
      break;

    if ( '"' == c )
    {
      if ( inQuote )
      {
        tokens.push_back(token);
        token.clear();
        inQuote = false;
      }
      else
      {
        if ( !token.empty() )
        {
          tokens.push_back(token);
          token.clear();
        }
        inQuote = true;
      }
      continue;
    }

    if ( inQuote )
    {
      token.push_back(c);
      continue;
    }

    if ( std::isspace(static_cast<unsigned char>(c)) )
    {
      if ( !token.empty() )
      {
        tokens.push_back(token);
        token.clear();
      }
      continue;
    }

    if ( ( '{' == c ) || ( '}' == c ) )
    {
      if ( !token.empty() )
      {
        tokens.push_back(token);
        token.clear();
      }
      tokens.push_back(std::string(1, c));
      continue;
    }

    token.push_back(c);
  }

  if ( !token.empty() )
    tokens.push_back(token);

  return tokens;
}

static bool ParseFloat( const std::string & iToken, float & oValue )
{
  try
  {
    size_t pos = 0;
    oValue = std::stof(iToken, &pos);
    return pos == iToken.size();
  }
  catch ( ... )
  {
    return false;
  }
}

static bool ParseInt( const std::string & iToken, int & oValue )
{
  try
  {
    size_t pos = 0;
    oValue = std::stoi(iToken, &pos);
    return pos == iToken.size();
  }
  catch ( ... )
  {
    return false;
  }
}

static bool ParseBool( const std::string & iToken, bool & oValue )
{
  if ( IsEqual(iToken, "true") || IsEqual(iToken, "1") || IsEqual(iToken, "yes") )
  {
    oValue = true;
    return true;
  }
  if ( IsEqual(iToken, "false") || IsEqual(iToken, "0") || IsEqual(iToken, "no") )
  {
    oValue = false;
    return true;
  }
  return false;
}

static bool ParseVec3( const std::vector<std::string> & iTokens, Vec3 & oValue )
{
  if ( 4 != static_cast<int>(iTokens.size()) )
    return false;

  return ParseFloat(iTokens[1], oValue.x)
      && ParseFloat(iTokens[2], oValue.y)
      && ParseFloat(iTokens[3], oValue.z);
}

static bool ParseVec4( const std::vector<std::string> & iTokens, Vec4 & oValue )
{
  if ( 5 != static_cast<int>(iTokens.size()) )
    return false;

  return ParseFloat(iTokens[1], oValue.x)
      && ParseFloat(iTokens[2], oValue.y)
      && ParseFloat(iTokens[3], oValue.z)
      && ParseFloat(iTokens[4], oValue.w);
}

static Vec4 OrientationFromEuler( const Vec3 & iRotation )
{
  const Vec3 rotRad(MathUtil::ToRadians(iRotation.x),
                    MathUtil::ToRadians(iRotation.y),
                    MathUtil::ToRadians(iRotation.z));

  Mat4x4 transform(1.f);
  transform = transform * glm::rotate(rotRad.x, Vec3(1.f, 0.f, 0.f));
  transform = transform * glm::rotate(rotRad.y, Vec3(0.f, 1.f, 0.f));
  transform = transform * glm::rotate(rotRad.z, Vec3(0.f, 0.f, 1.f));

  const glm::quat quat = glm::normalize(glm::quat_cast(glm::mat3(transform)));
  return Vec4(quat.x, quat.y, quat.z, quat.w);
}

static bool ParseMaterialSlot( const std::string & iToken, FpsMaterialSlot & oMaterial )
{
  if ( IsEqual(iToken, "floor") )
    oMaterial = FpsMaterialSlot::Floor;
  else if ( IsEqual(iToken, "wall") )
    oMaterial = FpsMaterialSlot::Wall;
  else if ( IsEqual(iToken, "pillar") )
    oMaterial = FpsMaterialSlot::Pillar;
  else if ( IsEqual(iToken, "crate") )
    oMaterial = FpsMaterialSlot::Crate;
  else if ( IsEqual(iToken, "accent") )
    oMaterial = FpsMaterialSlot::Accent;
  else
    return false;

  return true;
}

static const char * MaterialSlotName( FpsMaterialSlot iMaterial )
{
  switch ( iMaterial )
  {
    case FpsMaterialSlot::Floor:  return "floor";
    case FpsMaterialSlot::Wall:   return "wall";
    case FpsMaterialSlot::Pillar: return "pillar";
    case FpsMaterialSlot::Crate:  return "crate";
    case FpsMaterialSlot::Accent: return "accent";
    default:                      return "wall";
  }
}

static bool ParsePropCollisionMode( const std::string & iToken, FpsPropCollisionMode & oMode )
{
  if ( IsEqual(iToken, "none") || IsEqual(iToken, "false") || IsEqual(iToken, "0") )
    oMode = FpsPropCollisionMode::None;
  else if ( IsEqual(iToken, "bounds") || IsEqual(iToken, "true") || IsEqual(iToken, "1") )
    oMode = FpsPropCollisionMode::Bounds;
  else if ( IsEqual(iToken, "compound") )
    oMode = FpsPropCollisionMode::Compound;
  else
    return false;

  return true;
}

static const char * PropCollisionModeName( FpsPropCollisionMode iMode )
{
  switch ( iMode )
  {
    case FpsPropCollisionMode::Compound: return "compound";
    case FpsPropCollisionMode::Bounds: return "bounds";
    case FpsPropCollisionMode::None:
    default:                           return "none";
  }
}

static bool ParseAlphaMode( const std::string & iToken, float & oAlphaMode )
{
  if ( IsEqual(iToken, "opaque") )
    oAlphaMode = (float)AlphaMode::Opaque;
  else if ( IsEqual(iToken, "blend") )
    oAlphaMode = (float)AlphaMode::Blend;
  else if ( IsEqual(iToken, "mask") )
    oAlphaMode = (float)AlphaMode::Mask;
  else
    return false;

  return true;
}

static const char * AlphaModeName( const Material & iMaterial )
{
  const AlphaMode alphaMode = MaterialAlphaMode(iMaterial);
  if ( AlphaMode::Blend == alphaMode )
    return "blend";
  if ( AlphaMode::Mask == alphaMode )
    return "mask";
  return "opaque";
}

static Material MakeFpsMaterial( const Vec3 & iAlbedo, float iRoughness, float iMetallic = 0.f, float iReflectance = 0.5f )
{
  Material material;
  material._Albedo = iAlbedo;
  material._Roughness = iRoughness;
  material._Metallic = iMetallic;
  material._Reflectance = iReflectance;
  return material;
}

static void AddOrReplaceMaterial( std::vector<FpsMapMaterial> & ioMaterials, const std::string & iName, const Material & iMaterial )
{
  for ( FpsMapMaterial & material : ioMaterials )
  {
    if ( IsEqual(material._Name, iName) )
    {
      material._Material = iMaterial;
      return;
    }
  }

  FpsMapMaterial material;
  material._Name = iName;
  material._Material = iMaterial;
  ioMaterials.push_back(material);
}

static void AddMaterialIfMissing( std::vector<FpsMapMaterial> & ioMaterials, const std::string & iName, const Material & iMaterial )
{
  for ( const FpsMapMaterial & material : ioMaterials )
  {
    if ( IsEqual(material._Name, iName) )
      return;
  }

  FpsMapMaterial material;
  material._Name = iName;
  material._Material = iMaterial;
  ioMaterials.push_back(material);
}

static bool ParseRendererMode( const std::string & iToken, FpsRendererMode & oMode )
{
  if ( IsEqual(iToken, "deferred") )
  {
    oMode = FpsRendererMode::Deferred;
    return true;
  }
  if ( IsEqual(iToken, "software") )
  {
    oMode = FpsRendererMode::Software;
    return true;
  }
  if ( IsEqual(iToken, "photo") || IsEqual(iToken, "photopathtracer") )
  {
    oMode = FpsRendererMode::PhotoPathTracer;
    return true;
  }
  return false;
}

static const char * RendererModeName( FpsRendererMode iMode )
{
  if ( FpsRendererMode::Software == iMode )
    return "software";
  if ( FpsRendererMode::PhotoPathTracer == iMode )
    return "photo";
  return "deferred";
}

static bool ParseShadingType( const std::string & iToken, ShadingType & oType )
{
  if ( IsEqual(iToken, "flat") )
  {
    oType = ShadingType::Flat;
    return true;
  }
  if ( IsEqual(iToken, "phong") )
  {
    oType = ShadingType::Phong;
    return true;
  }
  if ( IsEqual(iToken, "pbr") )
  {
    oType = ShadingType::PBR;
    return true;
  }
  return false;
}

static const char * ShadingTypeName( ShadingType iType )
{
  if ( ShadingType::Flat == iType )
    return "flat";
  return ( ShadingType::PBR == iType ) ? "pbr" : "phong";
}

class FpsGameMapParser
{
public:
  FpsGameMapParser( const std::string & iFilename, FpsGameMap & oMap )
  : _Filename(iFilename), _Map(oMap)
  {
  }

  bool Parse()
  {
    std::ifstream file(_Filename);
    if ( !file.is_open() )
    {
      std::cout << "FpsGameMap : couldn't open " << _Filename << " for reading" << std::endl;
      return false;
    }

    _Map = FpsGameMap();

    std::vector<std::string> tokens;
    while ( ReadLine(file, tokens) )
    {
      if ( "}" == tokens[0] )
        return Error("unexpected closing bracket");
      if ( !ParseBlock(file, tokens) )
        return false;
    }

    return true;
  }

protected:
  bool ReadLine( std::ifstream & ioFile, std::vector<std::string> & oTokens )
  {
    std::string line;
    while ( std::getline(ioFile, line) )
    {
      _LineNumber++;
      oTokens = Tokenize(line);
      if ( !oTokens.empty() )
        return true;
    }
    return false;
  }

  bool ReadRequiredLine( std::ifstream & ioFile, std::vector<std::string> & oTokens )
  {
    if ( ReadLine(ioFile, oTokens) )
      return true;
    return Error("unexpected end of file");
  }

  bool ParseBlock( std::ifstream & ioFile, const std::vector<std::string> & iTokens )
  {
    if ( iTokens.empty() )
      return true;

    std::string blockType = iTokens[0];
    std::string blockName;
    bool hasOpenBracket = false;

    for ( int i = 1; i < static_cast<int>(iTokens.size()); ++i )
    {
      if ( "{" == iTokens[i] )
        hasOpenBracket = true;
      else if ( "}" == iTokens[i] )
        return Error("unexpected closing bracket in block header");
      else if ( blockName.empty() )
        blockName = iTokens[i];
      else
        return Error("too many tokens in block header");
    }

    if ( !hasOpenBracket )
    {
      std::vector<std::string> openTokens;
      if ( !ReadRequiredLine(ioFile, openTokens) )
        return false;
      if ( ( 1 != static_cast<int>(openTokens.size()) ) || ( "{" != openTokens[0] ) )
        return Error("expected opening bracket");
    }

    if ( IsEqual(blockType, "map") )
      return ParseMapBlock(ioFile, blockName);
    if ( IsEqual(blockType, "player") )
      return ParsePlayerBlock(ioFile);
    if ( IsEqual(blockType, "settings") )
      return ParseSettingsBlock(ioFile);
    if ( IsEqual(blockType, "render") )
      return ParseRenderBlock(ioFile);
    if ( IsEqual(blockType, "material") )
      return ParseMaterialBlock(ioFile, blockName);
    if ( IsEqual(blockType, "box") )
      return ParseObjectBlock(ioFile, blockName, true);
    if ( IsEqual(blockType, "collider") )
      return ParseObjectBlock(ioFile, blockName, false);
    if ( IsEqual(blockType, "light") )
      return ParseLightBlock(ioFile);
    if ( IsEqual(blockType, "prop") )
      return ParsePropBlock(ioFile, blockName);
    if ( IsEqual(blockType, "boids") )
      return ParseBoidsBlock(ioFile, blockName);
    if ( IsEqual(blockType, "weapon") )
      return ParseWeaponBlock(ioFile);

    return Error("unknown block type '" + blockType + "'");
  }

  bool IsClosingBracket( const std::vector<std::string> & iTokens )
  {
    if ( ( 1 == static_cast<int>(iTokens.size()) ) && ( "}" == iTokens[0] ) )
      return true;
    return false;
  }

  bool ParseMapBlock( std::ifstream & ioFile, const std::string & iName )
  {
    if ( !iName.empty() )
      _Map._Name = iName;

    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
        return true;
      if ( IsEqual(tokens[0], "environment") && ( 2 == static_cast<int>(tokens.size()) ) )
        _Map._Environment = tokens[1];
      else
        return Error("invalid map field");
    }
    return false;
  }

  bool ParsePlayerBlock( std::ifstream & ioFile )
  {
    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
        return true;
      if ( IsEqual(tokens[0], "position") )
      {
        if ( !ParseVec3(tokens, _Map._Player._Position) )
          return Error("invalid player position");
      }
      else if ( IsEqual(tokens[0], "yaw") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], _Map._Player._Yaw) )
          return Error("invalid player yaw");
      }
      else if ( IsEqual(tokens[0], "pitch") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], _Map._Player._Pitch) )
          return Error("invalid player pitch");
      }
      else if ( IsEqual(tokens[0], "health") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], _Map._Player._Health) )
          return Error("invalid player health");
      }
      else if ( IsEqual(tokens[0], "armor") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], _Map._Player._Armor) )
          return Error("invalid player armor");
      }
      else
        return Error("invalid player field");
    }
    return false;
  }

  bool ParseSettingsBlock( std::ifstream & ioFile )
  {
    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
        return true;
      if ( IsEqual(tokens[0], "maxprojectiles") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], _Map._MaxProjectiles) )
          return Error("invalid maxprojectiles value");
      }
      else if ( IsEqual(tokens[0], "projectileammo") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], _Map._MaxProjectileAmmo) )
          return Error("invalid projectileammo value");
      }
      else if ( IsEqual(tokens[0], "projectileammorefill") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], _Map._ProjectileAmmoRefillTime) )
          return Error("invalid projectileammorefill value");
      }
      else
        return Error("invalid settings field");
    }
    return false;
  }

  bool ParseRenderBlock( std::ifstream & ioFile )
  {
    FpsMapRenderSettings & settings = _Map._RenderSettings;
    settings._HasRenderSettings = true;

    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
        return true;
      if ( IsEqual(tokens[0], "renderer") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseRendererMode(tokens[1], settings._RendererMode) )
          return Error("invalid render renderer");
      }
      else if ( IsEqual(tokens[0], "cameraznear") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._CameraZNear) )
          return Error("invalid render cameraZNear");
      }
      else if ( IsEqual(tokens[0], "camerazfar") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._CameraZFar) )
          return Error("invalid render cameraZFar");
      }
      else if ( ( IsEqual(tokens[0], "camerafov") || IsEqual(tokens[0], "fov") ) && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._CameraFOV) )
          return Error("invalid render cameraFOV");
      }
      else if ( IsEqual(tokens[0], "renderscale") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], settings._RenderScale) )
          return Error("invalid render renderScale");
      }
      else if ( IsEqual(tokens[0], "showlights") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], settings._ShowLights) )
          return Error("invalid render showLights");
      }
      else if ( IsEqual(tokens[0], "tonemapping") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], settings._ToneMapping) )
          return Error("invalid render toneMapping");
      }
      else if ( IsEqual(tokens[0], "gamma") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._Gamma) )
          return Error("invalid render gamma");
      }
      else if ( IsEqual(tokens[0], "exposure") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._Exposure) )
          return Error("invalid render exposure");
      }
      else if ( IsEqual(tokens[0], "shadowmapping") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], settings._ShadowMapping) )
          return Error("invalid render shadowMapping");
      }
      else if ( IsEqual(tokens[0], "shadowmapresolution") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], settings._ShadowMapResolution) )
          return Error("invalid render shadowMapResolution");
      }
      else if ( IsEqual(tokens[0], "shadowbias") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._ShadowBias) )
          return Error("invalid render shadowBias");
      }
      else if ( IsEqual(tokens[0], "maxshadowcastinglights") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], settings._MaxShadowCastingLights) )
          return Error("invalid render maxShadowCastingLights");
      }
      else if ( IsEqual(tokens[0], "ssao") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], settings._SSAO) )
          return Error("invalid render ssao");
      }
      else if ( IsEqual(tokens[0], "ssaoblur") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], settings._SSAOBlur) )
          return Error("invalid render ssaoBlur");
      }
      else if ( IsEqual(tokens[0], "ssaoradius") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._SSAORadius) )
          return Error("invalid render ssaoRadius");
      }
      else if ( IsEqual(tokens[0], "ssaobias") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._SSAOBias) )
          return Error("invalid render ssaoBias");
      }
      else if ( IsEqual(tokens[0], "ssaointensity") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._SSAOIntensity) )
          return Error("invalid render ssaoIntensity");
      }
      else if ( IsEqual(tokens[0], "ssaokernelsize") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], settings._SSAOKernelSize) )
          return Error("invalid render ssaoKernelSize");
      }
      else if ( IsEqual(tokens[0], "ssr") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], settings._SSR) )
          return Error("invalid render ssr");
      }
      else if ( IsEqual(tokens[0], "ssrintensity") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._SSRIntensity) )
          return Error("invalid render ssrIntensity");
      }
      else if ( IsEqual(tokens[0], "ssrmaxroughness") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._SSRMaxRoughness) )
          return Error("invalid render ssrMaxRoughness");
      }
      else if ( IsEqual(tokens[0], "ssrmaxsteps") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], settings._SSRMaxSteps) )
          return Error("invalid render ssrMaxSteps");
      }
      else if ( IsEqual(tokens[0], "ssrstepsize") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._SSRStepSize) )
          return Error("invalid render ssrStepSize");
      }
      else if ( IsEqual(tokens[0], "ssrmaxdistance") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._SSRMaxDistance) )
          return Error("invalid render ssrMaxDistance");
      }
      else if ( IsEqual(tokens[0], "ssrthickness") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._SSRThickness) )
          return Error("invalid render ssrThickness");
      }
      else if ( IsEqual(tokens[0], "ssrfade") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._SSRFade) )
          return Error("invalid render ssrFade");
      }
      else if ( IsEqual(tokens[0], "pbrdirectlighting") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], settings._PBRDirectLighting) )
          return Error("invalid render pbrDirectLighting");
      }
      else if ( IsEqual(tokens[0], "directlightintensity") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._DirectLightIntensity) )
          return Error("invalid render directLightIntensity");
      }
      else if ( ( IsEqual(tokens[0], "iblmaxroughness") || IsEqual(tokens[0], "speculariblmaxroughness") ) && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], settings._SpecularIBLMaxRoughness) )
          return Error("invalid render iblMaxRoughness");
      }
      else if ( IsEqual(tokens[0], "lighting") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseShadingType(tokens[1], settings._ShadingType) )
          return Error("invalid render lighting");
      }
      else if ( IsEqual(tokens[0], "bounces") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], settings._Bounces) )
          return Error("invalid render bounces");
      }
      else if ( ( IsEqual(tokens[0], "spp") || IsEqual(tokens[0], "samplesperpixel") ) && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], settings._NbSamplesPerPixel) )
          return Error("invalid render spp");
      }
      else if ( IsEqual(tokens[0], "denoise") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], settings._Denoise) )
          return Error("invalid render denoise");
      }
      else
        return Error("invalid render field");
    }
    return false;
  }

  bool ParseMaterialBlock( std::ifstream & ioFile, const std::string & iName )
  {
    if ( iName.empty() )
      return Error("material block is missing a name");

    FpsMapMaterial mapMaterial;
    mapMaterial._Name = iName;
    bool hasAlbedo = false;

    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
      {
        if ( !hasAlbedo )
          return Error("material is missing albedo");
        AddOrReplaceMaterial(_Map._Materials, mapMaterial._Name, mapMaterial._Material);
        return true;
      }

      if ( IsEqual(tokens[0], "albedo") )
      {
        if ( !ParseVec3(tokens, mapMaterial._Material._Albedo) )
          return Error("invalid material albedo");
        hasAlbedo = true;
      }
      else if ( IsEqual(tokens[0], "roughness") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], mapMaterial._Material._Roughness) )
          return Error("invalid material roughness");
      }
      else if ( IsEqual(tokens[0], "metallic") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], mapMaterial._Material._Metallic) )
          return Error("invalid material metallic");
      }
      else if ( IsEqual(tokens[0], "reflectance") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], mapMaterial._Material._Reflectance) )
          return Error("invalid material reflectance");
      }
      else if ( IsEqual(tokens[0], "emission") )
      {
        if ( !ParseVec3(tokens, mapMaterial._Material._Emission) )
          return Error("invalid material emission");
      }
      else if ( IsEqual(tokens[0], "opacity") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], mapMaterial._Material._Opacity) )
          return Error("invalid material opacity");
      }
      else if ( IsEqual(tokens[0], "alphamode") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseAlphaMode(tokens[1], mapMaterial._Material._AlphaMode) )
          return Error("invalid material alpha mode");
      }
      else
        return Error("invalid material field");
    }
    return false;
  }

  bool ParseObjectBlock( std::ifstream & ioFile, const std::string & iName, bool iVisibleByDefault )
  {
    FpsSceneObject object;
    object._Name = iName.empty() ? ( iVisibleByDefault ? "Box" : "Collider" ) : iName;
    object._Collidable = true;
    object._Visible = iVisibleByDefault;

    bool hasCenter = false;
    bool hasHalf = false;
    bool hasMaterial = !iVisibleByDefault;

    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
      {
        if ( !hasCenter || !hasHalf || !hasMaterial )
          return Error("box/collider is missing a required field");
        _Map._Objects.push_back(object);
        return true;
      }

      if ( IsEqual(tokens[0], "center") )
      {
        if ( !ParseVec3(tokens, object._Center) )
          return Error("invalid object center");
        hasCenter = true;
      }
      else if ( IsEqual(tokens[0], "rotation") )
      {
        if ( !ParseVec3(tokens, object._Rotation) )
          return Error("invalid object rotation");
        object._Orientation = OrientationFromEuler(object._Rotation);
      }
      else if ( IsEqual(tokens[0], "orientation") )
      {
        if ( !ParseVec4(tokens, object._Orientation) )
          return Error("invalid object orientation");
      }
      else if ( IsEqual(tokens[0], "half") )
      {
        if ( !ParseVec3(tokens, object._HalfExtents) )
          return Error("invalid object half extents");
        hasHalf = true;
      }
      else if ( IsEqual(tokens[0], "material") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        FpsMaterialSlot materialSlot;
        if ( ParseMaterialSlot(tokens[1], materialSlot) )
          object._Material = materialSlot;
        object._MaterialName = tokens[1];
        hasMaterial = true;
      }
      else if ( IsEqual(tokens[0], "collidable") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], object._Collidable) )
          return Error("invalid collidable flag");
      }
      else if ( IsEqual(tokens[0], "visible") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], object._Visible) )
          return Error("invalid visible flag");
      }
      else
        return Error("invalid box/collider field");
    }
    return false;
  }

  bool ParseLightBlock( std::ifstream & ioFile )
  {
    Light light;
    bool hasType = false;
    bool hasEmission = false;
    bool hasIntensity = false;
    bool hasPosition = false;
    bool hasDirection = false;

    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
      {
        const LightType type = (LightType)(int)light._Type;
        if ( !hasType || !hasEmission || !hasIntensity )
          return Error("light is missing a required field");
        if ( ( LightType::DistantLight == type ) && !hasDirection )
          return Error("distant light requires direction");
        if ( ( LightType::DistantLight != type ) && !hasPosition )
          return Error("local light requires position");
        if ( LightType::SphereLight == type )
          light._Area = 4.0f * static_cast<float>(M_PI) * light._Radius * light._Radius;
        _Map._Lights.push_back(light);
        return true;
      }

      if ( IsEqual(tokens[0], "type") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( IsEqual(tokens[1], "distant") )
          light._Type = (float)LightType::DistantLight;
        else if ( IsEqual(tokens[1], "sphere") )
          light._Type = (float)LightType::SphereLight;
        else if ( IsEqual(tokens[1], "rect") )
          light._Type = (float)LightType::RectLight;
        else
          return Error("invalid light type");
        hasType = true;
      }
      else if ( IsEqual(tokens[0], "position") )
      {
        if ( !ParseVec3(tokens, light._Pos) )
          return Error("invalid light position");
        hasPosition = true;
      }
      else if ( IsEqual(tokens[0], "direction") )
      {
        if ( !ParseVec3(tokens, light._Pos) )
          return Error("invalid light direction");
        hasDirection = true;
      }
      else if ( IsEqual(tokens[0], "emission") )
      {
        if ( !ParseVec3(tokens, light._Emission) )
          return Error("invalid light emission");
        hasEmission = true;
      }
      else if ( IsEqual(tokens[0], "intensity") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], light._Intensity) )
          return Error("invalid light intensity");
        hasIntensity = true;
      }
      else if ( IsEqual(tokens[0], "radius") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], light._Radius) )
          return Error("invalid light radius");
      }
      else if ( IsEqual(tokens[0], "area") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], light._Area) )
          return Error("invalid light area");
      }
      else if ( IsEqual(tokens[0], "diru") )
      {
        if ( !ParseVec3(tokens, light._DirU) )
          return Error("invalid rect light diru");
      }
      else if ( IsEqual(tokens[0], "dirv") )
      {
        if ( !ParseVec3(tokens, light._DirV) )
          return Error("invalid rect light dirv");
      }
      else if ( IsEqual(tokens[0], "castshadow") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], light._CastShadow) )
          return Error("invalid castshadow flag");
      }
      else if ( IsEqual(tokens[0], "shadowradius") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], light._ShadowRadius) )
          return Error("invalid shadowradius value");
      }
      else
        return Error("invalid light field");
    }
    return false;
  }

  bool ParsePropBlock( std::ifstream & ioFile, const std::string & iName )
  {
    FpsMapProp prop;
    prop._Name = iName.empty() ? "Prop" : iName;
    bool hasPath = false;

    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
      {
        if ( !hasPath )
          return Error("prop is missing path");
        _Map._Props.push_back(prop);
        return true;
      }

      if ( IsEqual(tokens[0], "path") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        prop._Path = tokens[1];
        hasPath = true;
      }
      else if ( IsEqual(tokens[0], "position") )
      {
        if ( !ParseVec3(tokens, prop._Position) )
          return Error("invalid prop position");
      }
      else if ( IsEqual(tokens[0], "rotation") )
      {
        if ( !ParseVec3(tokens, prop._Rotation) )
          return Error("invalid prop rotation");
      }
      else if ( IsEqual(tokens[0], "scale") )
      {
        if ( 2 == static_cast<int>(tokens.size()) )
        {
          float scale = 1.f;
          if ( !ParseFloat(tokens[1], scale) )
            return Error("invalid prop scale");
          prop._Scale = Vec3(scale);
        }
        else if ( !ParseVec3(tokens, prop._Scale) )
          return Error("invalid prop scale");
      }
      else if ( IsEqual(tokens[0], "visible") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], prop._Visible) )
          return Error("invalid prop visible flag");
      }
      else if ( IsEqual(tokens[0], "collision") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParsePropCollisionMode(tokens[1], prop._CollisionMode) )
          return Error("invalid prop collision mode");
      }
      else if ( IsEqual(tokens[0], "collider") )
      {
        std::string colliderName;
        bool hasOpenBracket = false;
        for ( int i = 1; i < static_cast<int>(tokens.size()); ++i )
        {
          if ( "{" == tokens[i] )
            hasOpenBracket = true;
          else if ( colliderName.empty() )
            colliderName = tokens[i];
          else
            return Error("invalid prop collider header");
        }

        if ( !hasOpenBracket )
        {
          std::vector<std::string> openTokens;
          if ( !ReadRequiredLine(ioFile, openTokens) )
            return false;
          if ( ( 1 != static_cast<int>(openTokens.size()) ) || ( "{" != openTokens[0] ) )
            return Error("expected prop collider opening bracket");
        }

        if ( !ParsePropColliderBlock(ioFile, colliderName, prop) )
          return false;
      }
      else
        return Error("invalid prop field");
    }
    return false;
  }

  bool ParsePropColliderBlock( std::ifstream & ioFile, const std::string & iName, FpsMapProp & ioProp )
  {
    FpsMapPropCollider collider;
    collider._Name = iName.empty() ? "Collider" : iName;

    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
      {
        collider._HalfExtents = MathUtil::Max(collider._HalfExtents, Vec3(0.001f));
        ioProp._Colliders.push_back(collider);
        return true;
      }

      if ( IsEqual(tokens[0], "center") )
      {
        if ( !ParseVec3(tokens, collider._Center) )
          return Error("invalid prop collider center");
      }
      else if ( IsEqual(tokens[0], "rotation") )
      {
        if ( !ParseVec3(tokens, collider._Rotation) )
          return Error("invalid prop collider rotation");
      }
      else if ( IsEqual(tokens[0], "half") )
      {
        if ( !ParseVec3(tokens, collider._HalfExtents) )
          return Error("invalid prop collider half extents");
      }
      else
        return Error("invalid prop collider field");
    }
    return false;
  }

  bool ParseBoidsBlock( std::ifstream & ioFile, const std::string & iName )
  {
    FpsMapBoids boids;
    boids._Name = iName.empty() ? "Boids" : iName;

    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
      {
        boids._Settings._Count = std::max(0, boids._Settings._Count);
        boids._Settings._Seed = std::max(1u, boids._Settings._Seed);
        boids._Settings._BoundsRadius = std::max(0.001f, boids._Settings._BoundsRadius);
        boids._Settings._BoundsHeight = std::max(0.001f, boids._Settings._BoundsHeight);
        boids._Settings._MinSpeed = std::max(0.001f, boids._Settings._MinSpeed);
        boids._Settings._MaxSpeed = std::max(boids._Settings._MinSpeed, boids._Settings._MaxSpeed);
        boids._Settings._MaxForce = std::max(0.001f, boids._Settings._MaxForce);
        boids._Settings._NeighborRadius = std::max(0.001f, boids._Settings._NeighborRadius);
        boids._Settings._SeparationRadius = MathUtil::Clamp(boids._Settings._SeparationRadius, 0.001f, boids._Settings._NeighborRadius);
        boids._Settings._Scale = std::max(0.001f, boids._Settings._Scale);
        _Map._Boids.push_back(boids);
        return true;
      }

      if ( ( IsEqual(tokens[0], "visible") || IsEqual(tokens[0], "enabled") ) && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], boids._Visible) )
          return Error("invalid boids visible flag");
      }
      else if ( IsEqual(tokens[0], "count") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseInt(tokens[1], boids._Settings._Count) )
          return Error("invalid boids count");
      }
      else if ( IsEqual(tokens[0], "seed") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        int seed = 1;
        if ( !ParseInt(tokens[1], seed) )
          return Error("invalid boids seed");
        boids._Settings._Seed = static_cast<unsigned int>(std::max(seed, 1));
      }
      else if ( IsEqual(tokens[0], "center") || IsEqual(tokens[0], "boundscenter") )
      {
        if ( !ParseVec3(tokens, boids._Settings._BoundsCenter) )
          return Error("invalid boids bounds center");
      }
      else if ( ( IsEqual(tokens[0], "radius") || IsEqual(tokens[0], "boundsradius") ) && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._BoundsRadius) )
          return Error("invalid boids bounds radius");
      }
      else if ( ( IsEqual(tokens[0], "height") || IsEqual(tokens[0], "boundsheight") ) && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._BoundsHeight) )
          return Error("invalid boids bounds height");
      }
      else if ( IsEqual(tokens[0], "minspeed") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._MinSpeed) )
          return Error("invalid boids min speed");
      }
      else if ( IsEqual(tokens[0], "maxspeed") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._MaxSpeed) )
          return Error("invalid boids max speed");
      }
      else if ( IsEqual(tokens[0], "maxforce") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._MaxForce) )
          return Error("invalid boids max force");
      }
      else if ( IsEqual(tokens[0], "neighborradius") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._NeighborRadius) )
          return Error("invalid boids neighbor radius");
      }
      else if ( IsEqual(tokens[0], "separationradius") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._SeparationRadius) )
          return Error("invalid boids separation radius");
      }
      else if ( IsEqual(tokens[0], "separation") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._SeparationWeight) )
          return Error("invalid boids separation weight");
      }
      else if ( IsEqual(tokens[0], "alignment") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._AlignmentWeight) )
          return Error("invalid boids alignment weight");
      }
      else if ( IsEqual(tokens[0], "cohesion") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._CohesionWeight) )
          return Error("invalid boids cohesion weight");
      }
      else if ( IsEqual(tokens[0], "boundsweight") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._BoundsWeight) )
          return Error("invalid boids bounds weight");
      }
      else if ( IsEqual(tokens[0], "scale") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], boids._Settings._Scale) )
          return Error("invalid boids scale");
      }
      else if ( IsEqual(tokens[0], "color") )
      {
        if ( !ParseVec3(tokens, boids._Settings._Color) )
          return Error("invalid boids color");
      }
      else
        return Error("invalid boids field");
    }
    return false;
  }

  bool ParseWeaponBlock( std::ifstream & ioFile )
  {
    std::vector<std::string> tokens;
    while ( ReadRequiredLine(ioFile, tokens) )
    {
      if ( IsClosingBracket(tokens) )
        return true;

      if ( IsEqual(tokens[0], "path") && ( 2 == static_cast<int>(tokens.size()) ) )
        _Map._Weapon._Path = tokens[1];
      else if ( IsEqual(tokens[0], "offset") )
      {
        if ( !ParseVec3(tokens, _Map._Weapon._Offset) )
          return Error("invalid weapon offset");
      }
      else if ( IsEqual(tokens[0], "rotation") )
      {
        if ( !ParseVec3(tokens, _Map._Weapon._Rotation) )
          return Error("invalid weapon rotation");
      }
      else if ( IsEqual(tokens[0], "scale") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseFloat(tokens[1], _Map._Weapon._Scale) )
          return Error("invalid weapon scale");
      }
      else if ( IsEqual(tokens[0], "visible") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseBool(tokens[1], _Map._Weapon._Visible) )
          return Error("invalid weapon visible flag");
      }
      else
        return Error("invalid weapon field");
    }
    return false;
  }

  bool Error( const std::string & iMessage )
  {
    std::cout << "FpsGameMap : " << _Filename << ":" << _LineNumber << " : " << iMessage << std::endl;
    return false;
  }

protected:
  std::string  _Filename;
  int          _LineNumber = 0;
  FpsGameMap & _Map;
};

// ----------------------------------------------------------------------------
// Load
// ----------------------------------------------------------------------------
bool FpsGameMapLoader::Load( const std::string & iFilename, FpsGameMap & oMap )
{
  FpsGameMapParser parser(iFilename, oMap);
  if ( !parser.Parse() )
    return false;

  SeedDefaultMaterials(oMap);
  return true;
}

// ----------------------------------------------------------------------------
// SeedDefaultMaterials
// ----------------------------------------------------------------------------
void FpsGameMapLoader::SeedDefaultMaterials( FpsGameMap & ioMap )
{
  AddMaterialIfMissing(ioMap._Materials, "floor",  MakeFpsMaterial(Vec3(0.42f, 0.43f, 0.44f), 0.18f, 0.f, 0.9f));
  AddMaterialIfMissing(ioMap._Materials, "wall",   MakeFpsMaterial(Vec3(0.34f, 0.37f, 0.42f), 0.65f));
  AddMaterialIfMissing(ioMap._Materials, "pillar", MakeFpsMaterial(Vec3(0.58f, 0.55f, 0.49f), 0.55f));
  AddMaterialIfMissing(ioMap._Materials, "crate",  MakeFpsMaterial(Vec3(0.80f, 0.24f, 0.13f), 0.45f));
  AddMaterialIfMissing(ioMap._Materials, "accent", MakeFpsMaterial(Vec3(0.12f, 0.42f, 0.68f), 0.16f, 0.f, 0.85f));
}

static void WriteVec3( std::ofstream & ioFile, const char * iName, const Vec3 & iValue )
{
  ioFile << "  " << iName << " " << iValue.x << " " << iValue.y << " " << iValue.z << "\n";
}

static void WriteVec4( std::ofstream & ioFile, const char * iName, const Vec4 & iValue )
{
  ioFile << "  " << iName << " " << iValue.x << " " << iValue.y << " " << iValue.z << " " << iValue.w << "\n";
}

static std::string ObjectMaterialName( const FpsSceneObject & iObject )
{
  if ( !iObject._MaterialName.empty() )
    return iObject._MaterialName;
  return MaterialSlotName(iObject._Material);
}

// ----------------------------------------------------------------------------
// Save
// ----------------------------------------------------------------------------
bool FpsGameMapLoader::Save( const std::string & iFilename, const FpsGameMap & iMap )
{
  FpsGameMap map = iMap;
  SeedDefaultMaterials(map);

  std::ofstream file(iFilename);
  if ( !file.is_open() )
  {
    std::cout << "FpsGameMap : couldn't open " << iFilename << " for writing" << std::endl;
    return false;
  }

  file << std::fixed << std::setprecision(6);
  file << "# Test6 FPS map.\n";
  file << "# Coordinates are Y-up. Rotations are Euler XYZ degrees.\n\n";

  file << "map \"" << map._Name << "\" {\n";
  file << "  environment \"" << map._Environment << "\"\n";
  file << "}\n\n";

  file << "player {\n";
  WriteVec3(file, "position", map._Player._Position);
  file << "  yaw " << map._Player._Yaw << "\n";
  file << "  pitch " << map._Player._Pitch << "\n";
  if ( map._Player._Health >= 0 )
    file << "  health " << map._Player._Health << "\n";
  if ( map._Player._Armor >= 0 )
    file << "  armor " << map._Player._Armor << "\n";
  file << "}\n\n";

  file << "settings {\n";
  if ( map._MaxProjectiles > 0 )
    file << "  maxprojectiles " << map._MaxProjectiles << "\n";
  if ( map._MaxProjectileAmmo >= 0 )
    file << "  projectileammo " << map._MaxProjectileAmmo << "\n";
  if ( map._ProjectileAmmoRefillTime > 0.f )
    file << "  projectileammorefill " << map._ProjectileAmmoRefillTime << "\n";
  file << "}\n\n";

  if ( map._RenderSettings._HasRenderSettings )
  {
    const FpsMapRenderSettings & settings = map._RenderSettings;
    file << "render {\n";
    file << "  renderer " << RendererModeName(settings._RendererMode) << "\n";
    file << "  cameraZNear " << settings._CameraZNear << "\n";
    file << "  cameraZFar " << settings._CameraZFar << "\n";
    file << "  cameraFOV " << settings._CameraFOV << "\n";
    file << "  renderScale " << settings._RenderScale << "\n";
    file << "  showLights " << ( settings._ShowLights ? "true" : "false" ) << "\n";
    file << "  toneMapping " << ( settings._ToneMapping ? "true" : "false" ) << "\n";
    file << "  gamma " << settings._Gamma << "\n";
    file << "  exposure " << settings._Exposure << "\n";
    file << "  shadowMapping " << ( settings._ShadowMapping ? "true" : "false" ) << "\n";
    file << "  shadowMapResolution " << settings._ShadowMapResolution << "\n";
    file << "  shadowBias " << settings._ShadowBias << "\n";
    file << "  maxShadowCastingLights " << settings._MaxShadowCastingLights << "\n";
    file << "  ssao " << ( settings._SSAO ? "true" : "false" ) << "\n";
    file << "  ssaoBlur " << ( settings._SSAOBlur ? "true" : "false" ) << "\n";
    file << "  ssaoRadius " << settings._SSAORadius << "\n";
    file << "  ssaoBias " << settings._SSAOBias << "\n";
    file << "  ssaoIntensity " << settings._SSAOIntensity << "\n";
    file << "  ssaoKernelSize " << settings._SSAOKernelSize << "\n";
    file << "  ssr " << ( settings._SSR ? "true" : "false" ) << "\n";
    file << "  ssrIntensity " << settings._SSRIntensity << "\n";
    file << "  ssrMaxRoughness " << settings._SSRMaxRoughness << "\n";
    file << "  ssrMaxSteps " << settings._SSRMaxSteps << "\n";
    file << "  ssrStepSize " << settings._SSRStepSize << "\n";
    file << "  ssrMaxDistance " << settings._SSRMaxDistance << "\n";
    file << "  ssrThickness " << settings._SSRThickness << "\n";
    file << "  ssrFade " << settings._SSRFade << "\n";
    file << "  pbrDirectLighting " << ( settings._PBRDirectLighting ? "true" : "false" ) << "\n";
    file << "  directLightIntensity " << settings._DirectLightIntensity << "\n";
    file << "  iblMaxRoughness " << settings._SpecularIBLMaxRoughness << "\n";
    file << "  lighting " << ShadingTypeName(settings._ShadingType) << "\n";
    file << "  bounces " << settings._Bounces << "\n";
    file << "  spp " << settings._NbSamplesPerPixel << "\n";
    file << "  denoise " << ( settings._Denoise ? "true" : "false" ) << "\n";
    file << "}\n\n";
  }

  std::set<std::string> usedMaterials;
  for ( const FpsSceneObject & object : map._Objects )
  {
    if ( object._Visible )
      usedMaterials.insert(ObjectMaterialName(object));
  }

  for ( const FpsMapMaterial & material : map._Materials )
  {
    if ( usedMaterials.find(material._Name) == usedMaterials.end() )
      continue;

    file << "material \"" << material._Name << "\" {\n";
    WriteVec3(file, "albedo", material._Material._Albedo);
    file << "  roughness " << material._Material._Roughness << "\n";
    file << "  metallic " << material._Material._Metallic << "\n";
    file << "  reflectance " << material._Material._Reflectance << "\n";
    WriteVec3(file, "emission", material._Material._Emission);
    file << "  opacity " << material._Material._Opacity << "\n";
    file << "  alphamode " << AlphaModeName(material._Material) << "\n";
    file << "}\n\n";
  }

  for ( const FpsSceneObject & object : map._Objects )
  {
    file << ( object._Visible ? "box" : "collider" ) << " \"" << object._Name << "\" {\n";
    WriteVec3(file, "center", object._Center);
    WriteVec3(file, "rotation", object._Rotation);
    WriteVec4(file, "orientation", object._Orientation);
    WriteVec3(file, "half", object._HalfExtents);
    if ( object._Visible )
      file << "  material \"" << ObjectMaterialName(object) << "\"\n";
    file << "  collidable " << ( object._Collidable ? "true" : "false" ) << "\n";
    file << "  visible " << ( object._Visible ? "true" : "false" ) << "\n";
    file << "}\n\n";
  }

  for ( const FpsMapProp & prop : map._Props )
  {
    file << "prop \"" << prop._Name << "\" {\n";
    file << "  path \"" << prop._Path << "\"\n";
    WriteVec3(file, "position", prop._Position);
    WriteVec3(file, "rotation", prop._Rotation);
    WriteVec3(file, "scale", prop._Scale);
    file << "  visible " << ( prop._Visible ? "true" : "false" ) << "\n";
    file << "  collision " << PropCollisionModeName(prop._CollisionMode) << "\n";
    if ( FpsPropCollisionMode::Compound == prop._CollisionMode )
    {
      for ( const FpsMapPropCollider & collider : prop._Colliders )
      {
        file << "\n";
        file << "  collider \"" << collider._Name << "\" {\n";
        file << "  ";
        WriteVec3(file, "center", collider._Center);
        file << "  ";
        WriteVec3(file, "rotation", collider._Rotation);
        file << "  ";
        WriteVec3(file, "half", collider._HalfExtents);
        file << "  }\n";
      }
    }
    file << "}\n\n";
  }

  for ( const FpsMapBoids & boids : map._Boids )
  {
    const BoidSettings & settings = boids._Settings;
    file << "boids \"" << boids._Name << "\" {\n";
    file << "  visible " << ( boids._Visible ? "true" : "false" ) << "\n";
    file << "  count " << settings._Count << "\n";
    file << "  seed " << settings._Seed << "\n";
    WriteVec3(file, "center", settings._BoundsCenter);
    file << "  radius " << settings._BoundsRadius << "\n";
    file << "  height " << settings._BoundsHeight << "\n";
    file << "  minspeed " << settings._MinSpeed << "\n";
    file << "  maxspeed " << settings._MaxSpeed << "\n";
    file << "  maxforce " << settings._MaxForce << "\n";
    file << "  neighborradius " << settings._NeighborRadius << "\n";
    file << "  separationradius " << settings._SeparationRadius << "\n";
    file << "  separation " << settings._SeparationWeight << "\n";
    file << "  alignment " << settings._AlignmentWeight << "\n";
    file << "  cohesion " << settings._CohesionWeight << "\n";
    file << "  boundsweight " << settings._BoundsWeight << "\n";
    file << "  scale " << settings._Scale << "\n";
    WriteVec3(file, "color", settings._Color);
    file << "}\n\n";
  }

  for ( const Light & light : map._Lights )
  {
    const LightType type = (LightType)(int)light._Type;
    file << "light {\n";
    if ( LightType::DistantLight == type )
    {
      file << "  type distant\n";
      WriteVec3(file, "direction", light._Pos);
    }
    else if ( LightType::RectLight == type )
    {
      file << "  type rect\n";
      WriteVec3(file, "position", light._Pos);
      WriteVec3(file, "diru", light._DirU);
      WriteVec3(file, "dirv", light._DirV);
      file << "  area " << light._Area << "\n";
    }
    else
    {
      file << "  type sphere\n";
      WriteVec3(file, "position", light._Pos);
      file << "  radius " << light._Radius << "\n";
    }
    WriteVec3(file, "emission", light._Emission);
    file << "  intensity " << light._Intensity << "\n";
    file << "  castshadow " << ( light._CastShadow ? "true" : "false" ) << "\n";
    file << "  shadowradius " << light._ShadowRadius << "\n";
    file << "}\n\n";
  }

  file << "weapon {\n";
  file << "  path \"" << map._Weapon._Path << "\"\n";
  WriteVec3(file, "offset", map._Weapon._Offset);
  WriteVec3(file, "rotation", map._Weapon._Rotation);
  file << "  scale " << map._Weapon._Scale << "\n";
  file << "  visible " << ( map._Weapon._Visible ? "true" : "false" ) << "\n";
  file << "}\n";

  return true;
}

}
