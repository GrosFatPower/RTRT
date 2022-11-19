#version 430 core

#define EPSILON 0.0001
#define PI 3.14159265359
#define MAX_MATERIAL_COUNT 64
#define MAX_SPHERE_COUNT   64
#define MAX_LIGHT_COUNT    64

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
  vec3  _Up;
  vec3  _Right;
  vec3  _Forward;
  vec3  _Pos;
  float _FOV;
};

// Uniforms
uniform vec2        u_Resolution;
uniform float       u_Time;
uniform int         u_FrameNum;
uniform Camera      u_Camera;
uniform SphereLight u_SphereLight;
uniform int         u_NbMaterials;
uniform Material    u_Materials[MAX_MATERIAL_COUNT];
uniform int         u_NbSpheres;
uniform Sphere      u_Spheres[MAX_SPHERE_COUNT];

// Constants
const Sphere Spheres[] = { Sphere(vec3( 0.f, 0.f, 0.f ),   .3f, 0),
                           Sphere(vec3( .5f, 1.f, -.5f ),  .2f, 1),
                           Sphere(vec3( -.5f, 2.f, -.5f ), .2f, 2),
                           Sphere(vec3(  3.f, 2.f, -5.f ), .8f, 2),
                           Sphere(vec3( -1.f, 1.f, -3.f ), 1.f, 1) };

const Plane Planes[] = { Plane(vec3( 0.f, -.5f, 0.f ), vec3( 0.f, 1.f, 0.f ), 3) };

const vec3 background = vec3( .3f, .3f, .8f );

const int bounces = 5;

// Utils

//RNG from code by Moroz Mykhailo (https://www.shadertoy.com/view/wltcRS)
//internal RNG state 
uvec4 g_Seed;
ivec2 g_Pixel;

void InitRNG( vec2 p, int frame )
{
  g_Pixel = ivec2(p);
  g_Seed = uvec4(p, uint(frame), uint(p.x) + uint(p.y));
}

void pcg4d(inout uvec4 v)
{
  v = v * 1664525u + 1013904223u;
  v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
  v = v ^ (v >> 16u);
  v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
}

float rand()
{
  pcg4d(g_Seed); return float(g_Seed.x) / float(0xffffffffu);
}

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

bool TraceRay( Ray iRay, out HitPoint oClosestHit )
{
  oClosestHit = HitPoint(-1.f, vec3( 0.f, 0.f, 0.f ), vec3( 0.f, 0.f, 0.f ), -1);

  for ( int i = 0; i < Spheres.length(); ++i )
  {
    float hitDist = 0.f;
    if ( SphereIntersection(Spheres[i]._Center, Spheres[i]._Radius, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) )
      {
        oClosestHit._Dist       = hitDist;
        oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
        oClosestHit._Normal     = normalize(oClosestHit._Pos - Spheres[i]._Center);
        oClosestHit._MaterialID = Spheres[i]._MaterialID;
      }
    }
  }

  for ( int i = 0; i < Planes.length(); ++i )
  {
    float hitDist = 0.f;
    if ( PlaneIntersection(Planes[i]._Orig, Planes[i]._Normal, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) )
      {
        oClosestHit._Dist       = hitDist;
        oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
        oClosestHit._Normal     = Planes[i]._Normal;
        oClosestHit._MaterialID = Planes[i]._MaterialID;
      }
    }
  }

  if ( -1 == oClosestHit._Dist )
    return false;
  return true;
}

bool AnyHit( Ray iRay, float iMaxDist )
{
  for ( int i = 0; i < Spheres.length(); ++i )
  {
    float hitDist = 0.f;
    if ( SphereIntersection(Spheres[i]._Center, Spheres[i]._Radius, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( hitDist < iMaxDist ) )
        return true;
    }
  }

  for ( int i = 0; i < Planes.length(); ++i )
  {
    float hitDist = 0.f;
    if ( PlaneIntersection(Planes[i]._Orig, Planes[i]._Normal, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( hitDist < iMaxDist ) )
        return true;
    }
  }

  return false;
}

void main()
{
  // Initialization
  InitRNG(gl_FragCoord.xy, u_FrameNum);

  vec2 centeredUV = 2. * fragUV - 1.;

  float scale = tan(u_Camera._FOV * .5);
  centeredUV.x *= scale;
  centeredUV.y *= ( u_Resolution.y / u_Resolution.x ) * scale;

  Ray ray;
  ray._Orig = u_Camera._Pos;
  ray._Dir = normalize(u_Camera._Right * centeredUV.x + u_Camera._Up * centeredUV.y + u_Camera._Forward);

  vec3 pixelColor = vec3(0.f, 0.f, 0.f);
  float multiplier = 1.0f;

  for ( int i = 0; i < bounces; ++i )
  {
    // Ray cast
    HitPoint closestHit;
    TraceRay(ray, closestHit);
    if ( closestHit._Dist < -EPSILON )
    {
      pixelColor += background * multiplier;
      break;
    }

    vec3 lightDir = u_SphereLight._Pos - closestHit._Pos;
    float lightDist = length(lightDir);

    lightDir = normalize(lightDir);
    
    vec3 lightIntensity = vec3(0.f, 0.f, 0.f);
    if ( lightDist < u_SphereLight._Radius )
    {
      Ray occlusionRay;
      occlusionRay._Orig = closestHit._Pos + closestHit._Normal * EPSILON;
      occlusionRay._Dir = lightDir;

      if ( !AnyHit(occlusionRay, lightDist) )
        lightIntensity = max(dot(closestHit._Normal, lightDir), 0.0f) * u_SphereLight._Emission;
    }

    pixelColor += u_Materials[closestHit._MaterialID]._Albedo * lightIntensity * multiplier;

    multiplier *= u_Materials[closestHit._MaterialID]._Metallic;

    vec3 randVec3 = vec3(rand() * 2. - 1.,rand() * 2. - 1.,rand() * 2. - 1.);

    ray._Orig = closestHit._Pos + closestHit._Normal * EPSILON;
    ray._Dir = reflect(ray._Dir, closestHit._Normal + u_Materials[closestHit._MaterialID]._Roughness * randVec3 );
  }

  fragColor = vec4(pixelColor, 0.f);
}
