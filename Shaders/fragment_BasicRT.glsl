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
uniform vec2   u_Resolution;
uniform Camera u_Camera;

// Constants
const Material green  = Material(0, vec3( .1f, .8f, .1f ), vec3( 0.f, 0.f, 0.f ), .4f, .0f);
const Material red    = Material(1, vec3( .8f, .1f, .1f ), vec3( 0.f, 0.f, 0.f ), .3f, .5f);
const Material blue   = Material(2, vec3( .1f, .1f, .8f ), vec3( 0.f, 0.f, 0.f ), .2f, .3f);
const Material orange = Material(3, vec3( .8f, .6f, .2f ), vec3( 0.f, 0.f, 0.f ), .1f, .7f);
const Material Materials[4] = { green, red, blue, orange };

const SphereLight light = SphereLight(vec3( 2.f, 10.f, .5f ), vec3( 1.f, 1.f, .5f ), 10.f);

const Sphere Spheres[] = { Sphere(vec3( 0.f, 0.f, 0.f ),   .3f, 0),
                           Sphere(vec3( .5f, 1.f, -.5f ),  .2f, 1),
                           Sphere(vec3( -.5f, 2.f, -.5f ), .2f, 2),
                           Sphere(vec3(  3.f, 2.f, -5.f ), .8f, 2),
                           Sphere(vec3( -1.f, 1.f, -3.f ), 1.f, 1) };

const Plane Planes[] = { Plane(vec3( 0.f, -.5f, 0.f ), vec3( 0.f, 1.f, 0.f ), 3) };

const vec3 background = vec3( .3f, .3f, .8f );

const int bounces = 5;

// Utils
float rand(vec2 co)
{
  return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
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

bool TraceRay( Ray iRay, out HitPoint closestHit )
{
  closestHit = HitPoint(-1.f, vec3( 0.f, 0.f, 0.f ), vec3( 0.f, 0.f, 0.f ), -1);

  for ( int i = 0; i < Spheres.length(); ++i )
  {
    float hitDist = 0.f;
    if ( SphereIntersection(Spheres[i]._Center, Spheres[i]._Radius, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( ( hitDist < closestHit._Dist ) || ( -1.f == closestHit._Dist ) ) )
      {
        closestHit._Dist       = hitDist;
        closestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
        closestHit._Normal     = normalize(closestHit._Pos - Spheres[i]._Center);
        closestHit._MaterialID = Spheres[i]._MaterialID;
      }
    }
  }

  for ( int i = 0; i < Planes.length(); ++i )
  {
    float hitDist = 0.f;
    if ( PlaneIntersection(Planes[i]._Orig, Planes[i]._Normal, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( ( hitDist < closestHit._Dist ) || ( -1.f == closestHit._Dist ) ) )
      {
        closestHit._Dist       = hitDist;
        closestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
        closestHit._Normal     = Planes[i]._Normal;
        closestHit._MaterialID = Planes[i]._MaterialID;
      }
    }
  }

  if ( -1 == closestHit._Dist )
    return false;
  return true;
}

void main()
{
  const float aspectRatio = u_Resolution.x / u_Resolution.y;

  // Initialization
  vec2 centeredUV = ( fragUV * 2.f - vec2(1.f) ) * vec2(aspectRatio, 1.f);

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

    vec3 lightDir =  normalize(light._Pos - closestHit._Pos);
    float lightDist = length(lightDir);
    
    HitPoint occlusionHit;
    Ray occlusionRay;
    occlusionRay._Orig = closestHit._Pos + closestHit._Normal * EPSILON;
    occlusionRay._Dir = lightDir;
    TraceRay(occlusionRay, occlusionHit);

    vec3 lightIntensity = vec3(0.f, 0.f, 0.f);
    if ( ( occlusionHit._Dist > lightDist ) || ( occlusionHit._Dist < -EPSILON ) )
      lightIntensity = max(dot(closestHit._Normal, lightDir), 0.0f) * light._Emission;

    pixelColor += Materials[closestHit._MaterialID]._Albedo * lightIntensity * multiplier;

    multiplier *= Materials[closestHit._MaterialID]._Metallic;

    vec3 randVec3 = vec3(rand(vec2(-.5f, .5f)),rand(vec2(-.5f, .5f)),rand(vec2(-.5f, .5f)));

    ray._Orig = closestHit._Pos + closestHit._Normal * EPSILON;
    ray._Dir = reflect(ray._Dir, closestHit._Normal);
  }

  fragColor = vec4(pixelColor, 0.f);
}
