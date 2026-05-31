#ifndef _FpsGameMap_
#define _FpsGameMap_

#include "Boids.h"
#include "FpsGame.h"
#include "Light.h"
#include "Material.h"
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

struct FpsMapPropCollider
{
  std::string _Name = "Collider";
  Vec3        _Center = Vec3(0.f);
  Vec3        _Rotation = Vec3(0.f);
  Vec3        _HalfExtents = Vec3(0.5f);
};

struct FpsMapProp
{
  std::string _Name;
  std::string _Path;
  Vec3        _Position = Vec3(0.f);
  Vec3        _Rotation = Vec3(0.f);
  Vec3        _Scale = Vec3(1.f);
  bool        _Visible = true;
  FpsPropCollisionMode _CollisionMode = FpsPropCollisionMode::None;
  std::vector<FpsMapPropCollider> _Colliders;
};

struct FpsMapWeapon
{
  std::string _Path = "overwatch_junkrats_grenade_launcher/scene.gltf";
  Vec3        _Offset = Vec3(0.44f, -0.33f, 0.83f);
  Vec3        _Rotation = Vec3(-6.5f, -10.5f, 3.f);
  float       _Scale = 1.24f;
  bool        _Visible = true;
};

struct FpsMapMaterial
{
  std::string _Name;
  Material    _Material;
};

struct FpsMapBoids
{
  std::string  _Name = "Boids";
  BoidSettings _Settings;
  bool         _Visible = true;
};

struct FpsMapRenderSettings
{
  bool            _HasRenderSettings = false;
  FpsRendererMode _RendererMode = FpsRendererMode::Deferred;
  float           _CameraZNear = 0.05f;
  float           _CameraZFar = 200.f;
  float           _CameraFOV = 85.f;
  int             _RenderScale = 100;
  bool            _ShowLights = false;
  bool            _ToneMapping = true;
  float           _Gamma = 2.f;
  float           _Exposure = 1.5f;
  bool            _ShadowMapping = true;
  int             _ShadowMapResolution = 1024;
  float           _ShadowBias = 0.002f;
  int             _MaxShadowCastingLights = 4;
  bool            _SSAO = true;
  bool            _SSAOBlur = true;
  float           _SSAORadius = 0.5f;
  float           _SSAOBias = 0.025f;
  float           _SSAOIntensity = 1.f;
  int             _SSAOKernelSize = 16;
  bool            _SSR = true;
  float           _SSRIntensity = 0.6f;
  float           _SSRMaxRoughness = 0.55f;
  int             _SSRMaxSteps = 48;
  float           _SSRStepSize = 0.18f;
  float           _SSRMaxDistance = 35.f;
  float           _SSRThickness = 0.25f;
  float           _SSRFade = 0.18f;
  bool            _PBRDirectLighting = true;
  float           _DirectLightIntensity = 1.f;
  float           _SpecularIBLMaxRoughness = 0.5f;
  int             _Bounces = 1;
  int             _NbSamplesPerPixel = 1;
  bool            _Denoise = false;
};

struct FpsGameMap
{
  std::string                 _Name = "Default FPS Arena";
  std::string                 _Environment = "HDR/syferfontein_18d_clear_1k.hdr";
  FpsMapPlayer                _Player;
  FpsMapWeapon                _Weapon;
  FpsMapRenderSettings        _RenderSettings;
  std::vector<FpsMapMaterial> _Materials;
  std::vector<FpsSceneObject> _Objects;
  std::vector<FpsMapProp>     _Props;
  std::vector<FpsMapBoids>    _Boids;
  std::vector<Light>          _Lights;
  int                         _MaxProjectiles = -1;
  int                         _MaxProjectileAmmo = -1;
  float                       _ProjectileAmmoRefillTime = -1.f;
};

class FpsGameMapLoader
{
public:
  static bool Load( const std::string & iFilename, FpsGameMap & oMap );
  static bool Save( const std::string & iFilename, const FpsGameMap & iMap );
  static void SeedDefaultMaterials( FpsGameMap & ioMap );
};

}

#endif /* _FpsGameMap_ */
