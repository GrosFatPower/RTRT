#version 430 core

#define EPSILON 0.0001
#define PI 3.14159265359

in vec2 fragUV;
out vec4 fragColor;

// Data structures
struct Ray
{
  vec3 _Orig;
  vec3 _Dir;
};

struct Material
{
  int   _ID;
  vec3  _Albedo;
  vec3  _Emission;
  float _Metallic;
  float _Roughness;
};

struct SphereLight
{
  vec3  _Pos;
  vec3  _Emission;
  float _Radius;
};

struct HitPoint
{
  float _Dist;
  vec3  _Pos;
  vec3  _Normal;
  int   _MaterialID;
};

struct Sphere
{
  vec3  _Center;
  float _Radius;
  int   _MaterialID;
};

struct Plane
{
  vec3  _Orig;
  vec3  _Normal;
  int   _MaterialID;
};

struct Camera
{
  vec3 _Up;
  vec3 _Right;
  vec3 _Forward;
  vec3 _Pos;
};

// Uniforms
//uniform float u_AspectRatio;

// Utils
bool SphereIntersection( vec3 iSphereCenter, float _Radius, Ray iRay, out float oHitDistance )
{
  float t = dot(iSphereCenter - iRay._Orig, iRay._Dir);
  vec3 p = iRay._Orig + iRay._Dir * t;

  float y = length(iSphereCenter - p);
  if ( y < _Radius )
  { 
    float x =  sqrt(_Radius*_Radius - y*y);
    float t1 = t-x;
    if ( t1 >  0 )
    {
      oHitDistance = t1;
      return true;
    }
  }
  
  return false;
}

bool PlaneIntersection( vec3 iOrig, vec3 iNormal, Ray iRay, out float oHitDistance )
{ 
  float denom = dot(iNormal, iRay._Dir);

  if ( abs(denom) > EPSILON )
  { 
    vec3 d = iOrig - iRay._Orig; 
    oHitDistance = dot(d, iNormal) / denom; 
    return ( oHitDistance >= EPSILON );
  } 
 
  return false; 
} 

void main()
{
  const float aspectRatio = 1.77f;

  // Initialization
  vec2 centeredUV = ( fragUV * 2.f - vec2(1.f) ) * vec2(aspectRatio, 1.f);

  Camera cam;
  cam._Up      = vec3(0.f, 1.f, 0.f);
  cam._Right   = vec3(1.f, 0.f, 0.f);
  cam._Forward = vec3(0.f, 0.f, -1.f);
  cam._Pos     = vec3(0.f, 0.f,  2.f);

  Ray cameraRay;
  cameraRay._Orig = cam._Pos;
  cameraRay._Dir = normalize(cam._Right * centeredUV.x + cam._Up * centeredUV.y + cam._Forward);

  const Material green  = Material(0, vec3( .1f, .8f, .1f ), vec3( 0.f, 0.f, 0.f ), 0.f, 0.f);
  const Material red    = Material(1, vec3( .8f, .1f, .1f ), vec3( 0.f, 0.f, 0.f ), 0.f, 0.f);
  const Material blue   = Material(2, vec3( .1f, .1f, .8f ), vec3( 0.f, 0.f, 0.f ), 0.f, 0.f);
  const Material orange = Material(3, vec3( .8f, .5f, .3f ), vec3( 0.f, 0.f, 0.f ), 0.f, 0.f);
  const Material Materials[4] = { green, red, blue, orange };

  const SphereLight light = SphereLight(vec3( -5.f, -5.f, -5.f ), vec3( 1.f, 1.f, 1.f ), 10.f);

  Sphere Spheres[3];
  Spheres[0] = Sphere(vec3( 0.f, 0.f, 0.f ),   .3f, 0);
  Spheres[1] = Sphere(vec3( .5f, 1.f, -.5f ),  .2f, 1);
  Spheres[2] = Sphere(vec3( -.5f, 2.f, -.5f ), .2f, 2);

  Plane Planes[1];
  Planes[0] = Plane(vec3( 0.f, -1.f, 0.f ), vec3( 0.f, 1.f, 0.f ), 3);

  const vec3 background = vec3( 1.f, 1.f, 1.f );

  // Ray cast
  HitPoint closestHit = HitPoint(-1.f, vec3( 0.f, 0.f, 0.f ), vec3( 0.f, 0.f, 0.f ), -1);

  for ( int i = 0; i < Spheres.length(); ++i )
  {
    float hitDist = 0.f;
    if ( SphereIntersection(Spheres[i]._Center, Spheres[i]._Radius, cameraRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( hitDist > closestHit._Dist ) )
      {
        closestHit._Dist       = hitDist;
        closestHit._Pos        = cameraRay._Orig + hitDist * cameraRay._Dir;
        closestHit._Normal     = normalize(closestHit._Pos - Spheres[i]._Center);
        closestHit._MaterialID = Spheres[i]._MaterialID;
      }
    }
  }

  for ( int i = 0; i < Planes.length(); ++i )
  {
    float hitDist = 0.f;
    if ( PlaneIntersection(Planes[i]._Orig, Planes[i]._Normal, cameraRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( hitDist > closestHit._Dist ) )
      {
        closestHit._Dist       = hitDist;
        closestHit._Pos        = cameraRay._Orig + hitDist * cameraRay._Dir;
        closestHit._Normal     = Planes[i]._Normal;
        closestHit._MaterialID = Planes[i]._MaterialID;
      }
    }
  }

  vec3 pixelColor;
  if ( closestHit._Dist > EPSILON )
  {
    pixelColor = Materials[closestHit._MaterialID]._Albedo;
  }
  else
    pixelColor = background;

  fragColor = vec4(pixelColor, 0.f);
}
