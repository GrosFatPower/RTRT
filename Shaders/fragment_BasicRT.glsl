#version 430 core

#define EPSILON 1e-9f
#define RESOLUTION 0.001f
#define INFINITY 3.402823466e+38
#define PI 3.14159265359
#define MAX_MATERIAL_COUNT 32
#define MAX_SPHERE_COUNT   32
#define MAX_PLANES_COUNT   32
#define MAX_BOX_COUNT      32
#define MAX_LIGHT_COUNT    32

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
  vec3  _Reflectance;
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
  mat4  _Transfom;
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

// Intersection method from Real-Time Rendering and Essential Mathematics for Games (p. 581)
bool BoxIntersection( vec3 iLow, vec3 iHigh, mat4 iTransfom, Ray iRay, out float oHitDistance )
{
  float tMin = -INFINITY;
  float tMax = INFINITY;

  vec3 boxOrig = vec3(iTransfom[3][0], iTransfom[3][1], iTransfom[3][2]);

  vec3 delta = boxOrig - iRay._Orig;

  for ( int i = 0; i < 3; ++i )
  {
    // Test intersection with the 2 planes perpendicular to the OBB's axis
    vec3 axis = vec3(iTransfom[i][0],iTransfom[i][1], iTransfom[i][2]);
    float e = dot(axis, delta);
    float f = dot(iRay._Dir, axis);

    if ( abs(f) > EPSILON )
    { // Standard case

      float t1 = ( e +  iLow[i] ) / f; // Intersection with the "left" plane
      float t2 = ( e + iHigh[i] ) / f; // Intersection with the "right" plane

      // t1 and t2 now contain distances betwen ray origin and ray-plane intersections
      // We want t1 to represent the nearest intersection, 
      // so if it's not the case, invert t1 and t2
      if ( t1 > t2 )
      {
        float w = t1;
        t1 = t2;
        t2 = w;
      }

      // tMax is the nearest "far" intersection (amongst the X,Y and Z planes pairs)
      if ( t2 < tMax )
        tMax = t2;
      // tMin is the farthest "near" intersection (amongst the X,Y and Z planes pairs)
      if ( t1 > tMin )
        tMin = t1;

      // If "far" is closer than "near", then there is NO intersection.
      if ( tMax < tMin )
        return false;
    }
    else
    { // Rare case : the ray is almost parallel to the planes, so they don't have any "intersection"
      if ( ( ( -e +  iLow[i] ) > 0.f )
        || ( ( -e + iHigh[i] ) < 0.f ) )
        return false;
    }
  }

  oHitDistance = tMin;

  return true;
}

// Adapted from https://gist.github.com/Shtille/1f98c649abeeb7a18c5a56696546d3cf
vec3 BoxNormal( vec3 iLow, vec3 iHigh, mat4 iTransfom, vec3 iHitPoint )
{
  // Resolution in local space
  mat4 invTransfo = inverse(iTransfom);
  vec3 localHitP = (invTransfo * vec4(iHitPoint, 1.f)).xyz;
  
  vec3 center = (iLow + iHigh) * .5f;
  vec3 halfDiag = (iHigh - iLow) * .5f;
  vec3 pc = localHitP - center;

  // step(edge,x) : x < edge ? 0 : 1
  vec3 normal = vec3(0.0);
  normal += vec3(sign(pc.x), 0.0, 0.0) * step(abs(abs(pc.x) - halfDiag.x), RESOLUTION);
  normal += vec3(0.0, sign(pc.y), 0.0) * step(abs(abs(pc.y) - halfDiag.y), RESOLUTION);
  normal += vec3(0.0, 0.0, sign(pc.z)) * step(abs(abs(pc.z) - halfDiag.z), RESOLUTION);

  // To world space
  normal = (transpose(invTransfo) * vec4(normal, 1.f)).xyz;

  return normalize(normal);
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
    if ( BoxIntersection(u_Boxes[i]._Low, u_Boxes[i]._High, u_Boxes[i]._Transfom, iRay, hitDist) )
    {
      if ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) )
      {
        oClosestHit._Dist       = hitDist;
        oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
        oClosestHit._Normal     = BoxNormal(u_Boxes[i]._Low, u_Boxes[i]._High, u_Boxes[i]._Transfom, oClosestHit._Pos);
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
    if ( BoxIntersection(u_Boxes[i]._Low, u_Boxes[i]._High, u_Boxes[i]._Transfom, iRay, hitDist) )
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

vec3 ComputeColor( HitPoint iClosestHit )
{
  vec3 lightDir = u_SphereLight._Pos - iClosestHit._Pos;
  float lightDist = length(lightDir);

  lightDir = normalize(lightDir);
    
  vec3 lightIntensity = vec3(0.f, 0.f, 0.f);
  if ( lightDist < u_SphereLight._Radius )
  {
    Ray occlusionRay;
    occlusionRay._Orig = iClosestHit._Pos + iClosestHit._Normal * RESOLUTION;
    occlusionRay._Dir = lightDir;

    if ( !AnyHit(occlusionRay, lightDist) )
      lightIntensity = max(dot(iClosestHit._Normal, lightDir), 0.0f) * u_SphereLight._Emission;
  }

  vec3 pixelColor = u_Materials[iClosestHit._MaterialID]._Albedo * lightIntensity;

  return pixelColor;
}

// GGX/Trowbridge-Reitz Normal Distribution Function
float D( float iAlpha, vec3 iN, vec3 iH )
{
  float num = pow(iAlpha, 2.f);

  float NdotH = max(dot(iN, iH), 0.f);
  float denom = PI * pow(pow(NdotH, 2.f) * (pow(iAlpha, 2.f) - 1.f) + 1.f, 2.f);

  return num / max(denom, EPSILON);
}

// Schlick-Beckmann Geometry Shadowing Function
float G1( float iAlpha, vec3 iN, vec3 iX )
{
  float NdotX = max(dot(iN, iX), 0.f);

  float num = NdotX;

  float k = iAlpha / 2.f;
  float denom = NdotX * ( 1.f - k ) + k;

  return num / max(denom, EPSILON);
}

// Smith Model
float G( float iAlpha, vec3 iN, vec3 iV, vec3 iL )
{
  return G1(iAlpha, iN, iV) * G1(iAlpha, iN, iL);
}

// Fresnel-Schlick Function
vec3 F( vec3 iF0, vec3 iV, vec3 iH )
{
  return iF0 + (vec3(1.f) - iF0) * pow(1.f - max(dot(iV, iH), 0.f), 5.f);
}

// Computer Graphics Tutorial - PBR (Physically Based Rendering)
// https://www.youtube.com/watch?v=RRE-F57fbXw&list=WL&index=109
vec3 PBR( Ray iRay, HitPoint iClosestHit )
{
  vec3 outColor = u_Materials[iClosestHit._MaterialID]._Emission;

  vec3 L = u_SphereLight._Pos - iClosestHit._Pos;
  float distToLight = length(L);
  L = normalize(L);

  Ray occlusionRay;
  occlusionRay._Orig = iClosestHit._Pos + iClosestHit._Normal * RESOLUTION;
  occlusionRay._Dir = L;
  if ( !AnyHit(occlusionRay, distToLight) )
  {
    vec3 N = iClosestHit._Normal;
    vec3 V = normalize(iRay._Orig - iClosestHit._Pos);
    vec3 H = normalize(V + L);

    float alpha = pow(u_Materials[iClosestHit._MaterialID]._Roughness, 2.f);

    vec3 F0 = u_Materials[iClosestHit._MaterialID]._Albedo * vec3(u_Materials[iClosestHit._MaterialID]._Metallic); // test : reflectance value

    vec3 Ks = F(F0, V, H);
    vec3 Kd = ( vec3(1.f) - Ks ) * ( 1.f - u_Materials[iClosestHit._MaterialID]._Metallic );

    vec3 lambert = u_Materials[iClosestHit._MaterialID]._Albedo / PI;

    vec3 cookTorranceNum = D(alpha, N, H) * G(alpha, N, V, L) * F(F0, V, H);   // DGF
    float cookTorranceDenom = 4.f * max(dot(V, N), 0.f) * max(dot(L, N), 0.f); // 4(V.N)(L.N)
    vec3 cookTorrance = cookTorranceNum / max(cookTorranceDenom, EPSILON);     // 

    vec3 BRDF = Kd * lambert + cookTorrance; // kd * fDiffuse + ks * fSpecular

    outColor += BRDF  * u_SphereLight._Emission * max(dot(L, N), 0.f);
  }

  return min(outColor, vec3(1.f));
}

// ----------
// MAIN
// ----------

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

  // Ray cast
  for ( int i = 0; i < u_Bounces; ++i )
  {
    HitPoint closestHit;
    TraceRay(ray, closestHit);
    if ( closestHit._Dist < -RESOLUTION )
    {
      if ( 1 == u_EnableSkybox )
        pixelColor += SampleSkybox(ray._Dir) * multiplier;
      else
        pixelColor += u_BackgroundColor * multiplier;
      break;
    }

    //pixelColor += ComputeColor(closestHit) * multiplier;
    pixelColor += PBR(ray, closestHit) * multiplier;

    multiplier *= u_Materials[closestHit._MaterialID]._Metallic;

    vec3 randVec3 = vec3(rand() * 2. - 1.,rand() * 2. - 1.,rand() * 2. - 1.);
    ray._Orig = closestHit._Pos + closestHit._Normal * RESOLUTION;
    ray._Dir = reflect(ray._Dir, closestHit._Normal + u_Materials[closestHit._MaterialID]._Roughness * randVec3 );
  }

  fragColor = vec4(pixelColor, 0.f);

  if ( 1 == u_Accumulate )
    fragColor += texture(u_ScreenTexture, fragUV);
}
