#version 430 core

#define EPSILON 0.0001f
#define INFINITY 3.402823466e+38
#define PI 3.14159265359
#define MAX_MATERIAL_COUNT 64
#define MAX_SPHERE_COUNT   64
#define MAX_PLANES_COUNT   64
#define MAX_BOX_COUNT      64
#define MAX_LIGHT_COUNT    64

in vec2 fragUV;
out vec4 fragColor;

// ----------
// Data structures
// ----------

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
  vec4  _CenterRad; // Center.xyz, radius.w
  int   _MaterialID;
};

struct Plane
{
  vec3  _Orig;
  vec3  _Normal;
  int   _MaterialID;
};

struct Box
{
  vec3  _Low;
  vec3  _High;
  mat4  _InvTransfo;
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

// ----------
// Uniforms
// ----------

uniform vec2        u_Resolution;
uniform float       u_Time;
uniform int         u_FrameNum;
uniform int         u_Accumulate;
uniform int         u_Bounces;
uniform vec3        u_BackgroundColor;
uniform Camera      u_Camera;
uniform SphereLight u_SphereLight;
uniform int         u_NbMaterials;
uniform Material    u_Materials[MAX_MATERIAL_COUNT];
uniform int         u_NbSpheres;
uniform Sphere      u_Spheres[MAX_SPHERE_COUNT];
uniform int         u_NbPlanes;
uniform Plane       u_Planes[MAX_PLANES_COUNT];
uniform int         u_NbBoxes;
uniform Box         u_Boxes[MAX_BOX_COUNT];
uniform int         u_EnableSkybox;
uniform sampler2D   u_SkyboxTexture;
uniform sampler2D   u_ScreenTexture;

// ----------
// Utils
// ----------

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

mat4 BuildTranslation( vec3 iTrans )
{
  return mat4(vec4(1.0, 0.0, 0.0, 0.0),
              vec4(0.0, 1.0, 0.0, 0.0),
              vec4(0.0, 0.0, 1.0, 0.0),
              vec4(iTrans, 1.0)        );
}

// https://iquilezles.org/articles/sphereshadow/
bool SphereIntersection( vec4 iSphere, Ray iRay, out float oHitDistance )
{
  vec3 oc = iRay._Orig - iSphere.xyz;

  float b = dot( oc, iRay._Dir );
  float c = dot( oc, oc ) - iSphere.w * iSphere.w;
  float h = b*b - c;

  if( h < 0.0 )
    return false;

  oHitDistance = -b - sqrt( h );
  return true;
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

// https://gist.github.com/DomNomNom/46bb1ce47f68d255fd5d
bool BoxIntersection( vec3 iLow, vec3 iHigh, mat4 iInvTransfo, Ray iRay, out float oHitDistance )
{ 
  //vec3 rayOrig = (iInvTransfo * vec4(iRay._Orig, 1.f)).xyz; // BUG
  //vec3 rayDir  = (iInvTransfo * vec4(iRay._Dir, 1.f)).xyz;  // BUG
  //
  //vec3 invDir = 1.f / rayDir;
  //vec3 tMin = (iLow - rayOrig) * invDir;
  //vec3 tMax = (iHigh - rayOrig) * invDir;
  //
  //vec3 t1 = min(tMin, tMax);
  //vec3 t2 = max(tMin, tMax);
  //
  //vec2 t = max(t1.xx, t1.yz);
  //float tNear = max(t.x, t.y);
  //
  //t = min(t2.xx, t2.yz);
  //float tFar = min(t.x, t.y);
  //
  //oHitDistance = tNear;

  //return ( tNear <= tFar );
  return false;
}

// https://gist.github.com/Shtille/1f98c649abeeb7a18c5a56696546d3cf
vec3 BoxNormal( vec3 iLow, vec3 iHigh, mat4 iInvTransfo, vec3 iHitPoint )
{
  //vec3 hitPoint = (iInvTransfo * vec4(iHitPoint, 1.f)).xyz; // BUG
  //
  //vec3 center = (iLow + iHigh) * .5f;
  //vec3 halfDiag = (iHigh - iLow) * .5f;
  //vec3 pc = hitPoint - center;

  // step(edge,x) : x < edge ? 0 : 1
  //vec3 normal = vec3(0.0);
  //normal += vec3(sign(pc.x), 0.0, 0.0) * step(abs(abs(pc.x) - halfDiag.x), EPSILON);
  //normal += vec3(0.0, sign(pc.y), 0.0) * step(abs(abs(pc.y) - halfDiag.y), EPSILON);
  //normal += vec3(0.0, 0.0, sign(pc.z)) * step(abs(abs(pc.z) - halfDiag.z), EPSILON);

  //return normalize(normal); // -> need to apply the transfo to the normal
  return vec3(1.);
}

// ----------
// Ray tracing
// ----------

bool TraceRay( Ray iRay, out HitPoint oClosestHit )
{
  oClosestHit = HitPoint(-1.f, vec3( 0.f, 0.f, 0.f ), vec3( 0.f, 0.f, 0.f ), -1);

  for ( int i = 0; i < u_NbSpheres; ++i )
  {
    float hitDist = 0.f;
    if ( SphereIntersection(u_Spheres[i]._CenterRad, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) )
      {
        oClosestHit._Dist       = hitDist;
        oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
        oClosestHit._Normal     = normalize(oClosestHit._Pos - u_Spheres[i]._CenterRad.xyz);
        oClosestHit._MaterialID = u_Spheres[i]._MaterialID;
      }
    }
  }

  for ( int i = 0; i < u_NbPlanes; ++i )
  {
    float hitDist = 0.f;
    if ( PlaneIntersection(u_Planes[i]._Orig, u_Planes[i]._Normal, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) )
      {
        oClosestHit._Dist       = hitDist;
        oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
        oClosestHit._Normal     = u_Planes[i]._Normal;
        oClosestHit._MaterialID = u_Planes[i]._MaterialID;
      }
    }
  }

  for ( int i = 0; i < u_NbBoxes; ++i )
  {
    float hitDist = 0.f;
    if ( BoxIntersection(u_Boxes[i]._Low, u_Boxes[i]._High, u_Boxes[i]._InvTransfo, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) )
      {
        oClosestHit._Dist       = hitDist;
        oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
        oClosestHit._Normal     = BoxNormal(u_Boxes[i]._Low, u_Boxes[i]._High, u_Boxes[i]._InvTransfo, oClosestHit._Pos);
        oClosestHit._MaterialID = u_Boxes[i]._MaterialID;
      }
    }
  }

  if ( -1 == oClosestHit._Dist )
    return false;
  return true;
}

bool AnyHit( Ray iRay, float iMaxDist )
{
  for ( int i = 0; i < u_NbSpheres; ++i )
  {
    float hitDist = 0.f;
    if ( SphereIntersection(u_Spheres[i]._CenterRad, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( hitDist < iMaxDist ) )
        return true;
    }
  }

  for ( int i = 0; i < u_NbPlanes; ++i )
  {
    float hitDist = 0.f;
    if ( PlaneIntersection(u_Planes[i]._Orig, u_Planes[i]._Normal, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( hitDist < iMaxDist ) )
        return true;
    }
  }

  for ( int i = 0; i < u_NbBoxes; ++i )
  {
    float hitDist = 0.f;
    if ( BoxIntersection(u_Boxes[i]._Low, u_Boxes[i]._High, u_Boxes[i]._InvTransfo, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( hitDist < iMaxDist ) )
        return true;
    }
  }

  return false;
}

// UV mapping : https://en.wikipedia.org/wiki/UV_mapping
vec3 SampleSkybox( vec3 iRayDir )
{
  float skyboxStrength = 1.0f;
  float skyboxGamma = 0.8F;
  float skyboxCeiling = 10.0f;
  
  vec3 skycolor = texture(u_SkyboxTexture, vec2(0.5 + atan(iRayDir.x, iRayDir.z)/(2*PI), 0.5 + asin(-iRayDir.y)/PI)).xyz;

  return min(vec3(skyboxCeiling), skyboxStrength*pow(skycolor, vec3(1.0/skyboxGamma)));
}

// MAIN

void main()
{
  // Initialization
  InitRNG(gl_FragCoord.xy, u_FrameNum);

  float r1 = 2.0 * rand();
  float r2 = 2.0 * rand();

  vec2 jitter;
  jitter.x = ( r1 < 1.0 ) ? ( sqrt(r1) - 1.0 ) : ( 1.0 - sqrt(2.0 - r1) ) ;
  jitter.y = ( r2 < 1.0 ) ? ( sqrt(r2) - 1.0 ) : ( 1.0 - sqrt(2.0 - r2) ) ;
  jitter /= (u_Resolution * 0.5);

  vec2 centeredUV = ( 2. * fragUV - 1. ) + jitter;

  float scale = tan(u_Camera._FOV * .5);
  centeredUV.x *= scale;
  centeredUV.y *= ( u_Resolution.y / u_Resolution.x ) * scale;

  Ray ray;
  ray._Orig = u_Camera._Pos;
  ray._Dir = normalize(u_Camera._Right * centeredUV.x + u_Camera._Up * centeredUV.y + u_Camera._Forward);

  vec3 pixelColor = vec3(0.f, 0.f, 0.f);
  float multiplier = 1.0f;

  for ( int i = 0; i < u_Bounces; ++i )
  {
    // Ray cast
    HitPoint closestHit;
    TraceRay(ray, closestHit);
    if ( closestHit._Dist < -EPSILON )
    {
      if ( 1 == u_EnableSkybox )
        pixelColor += SampleSkybox(ray._Dir) * multiplier;
      else
        pixelColor += u_BackgroundColor * multiplier;
      
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

  if ( 1 == u_Accumulate )
    fragColor += texture(u_ScreenTexture, fragUV);
}
