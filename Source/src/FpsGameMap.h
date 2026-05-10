#ifndef _FpsGameMap_
#define _FpsGameMap_

#include "FpsGame.h"
#include "Light.h"
#include "MathUtil.h"

#include <string>
#include <vector>

namespace RTRT
{

struct FpsMapPlayer
{
  Vec3  _Position = Vec3(0.f, 0.05f, -8.f);
  float _Yaw = 90.f;
  float _Pitch = 0.f;
  int   _Health = -1;
  int   _Armor = -1;
};

struct FpsMapProp
{
  std::string _Name;
  std::string _Path;
  Vec3        _Position = Vec3(0.f);
  Vec3        _Rotation = Vec3(0.f);
  Vec3        _Scale = Vec3(1.f);
  bool        _Visible = true;
};

struct FpsMapWeapon
{
  std::string _Path = "overwatch_junkrats_grenade_launcher/scene.gltf";
  Vec3        _Offset = Vec3(0.44f, -0.33f, 0.83f);
  Vec3        _Rotation = Vec3(-6.5f, -10.5f, 3.f);
  float       _Scale = 1.24f;
  bool        _Visible = true;
};

struct FpsGameMap
{
  std::string                 _Name = "Default FPS Arena";
  std::string                 _Environment = "HDR/syferfontein_18d_clear_1k.hdr";
  FpsMapPlayer                _Player;
  FpsMapWeapon                _Weapon;
  std::vector<FpsSceneObject> _Objects;
  std::vector<FpsMapProp>     _Props;
  std::vector<Light>          _Lights;
  int                         _MaxProjectiles = -1;
  int                         _MaxProjectileAmmo = -1;
  float                       _ProjectileAmmoRefillTime = -1.f;
};

class FpsGameMapLoader
{
public:
  static bool Load( const std::string & iFilename, FpsGameMap & oMap );
};

}

#endif /* _FpsGameMap_ */
