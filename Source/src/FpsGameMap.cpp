#include "FpsGameMap.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>

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
    if ( IsEqual(blockType, "box") )
      return ParseObjectBlock(ioFile, blockName, true);
    if ( IsEqual(blockType, "collider") )
      return ParseObjectBlock(ioFile, blockName, false);
    if ( IsEqual(blockType, "light") )
      return ParseLightBlock(ioFile);
    if ( IsEqual(blockType, "prop") )
      return ParsePropBlock(ioFile, blockName);
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
      else if ( IsEqual(tokens[0], "half") )
      {
        if ( !ParseVec3(tokens, object._HalfExtents) )
          return Error("invalid object half extents");
        hasHalf = true;
      }
      else if ( IsEqual(tokens[0], "material") && ( 2 == static_cast<int>(tokens.size()) ) )
      {
        if ( !ParseMaterialSlot(tokens[1], object._Material) )
          return Error("invalid material slot '" + tokens[1] + "'");
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
      else
        return Error("invalid prop field");
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
  return parser.Parse();
}

}
