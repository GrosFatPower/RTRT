#version 430 core

in vec2 fragUV;
out vec4 fragColor;

#include Constants.glsl
#include Structures.glsl
#include RNG.glsl
#include fragment_Intersection.glsl

// ----------
// Uniforms
// ----------

#define MAX_MATERIAL_COUNT 32
#define MAX_SPHERE_COUNT   32
#define MAX_PLANES_COUNT   32
#define MAX_BOX_COUNT      32
#define MAX_LIGHT_COUNT    32

uniform vec2        u_Resolution;
uniform float       u_Time;
uniform float       u_Gamma;
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
uniform float       u_SkyboxRotation;
uniform sampler2D   u_SkyboxTexture;
uniform sampler2D   u_ScreenTexture;

// ----------
// Ray tracing
// ----------

bool TraceRay( Ray iRay, out HitPoint oClosestHit )
{
  oClosestHit = HitPoint(-1.f, vec3( 0.f, 0.f, 0.f ), vec3( 0.f, 0.f, 0.f ), -1, true);

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

  if ( dot(iRay._Dir, oClosestHit._Normal) > 0.f )
  {
    oClosestHit._FrontFace = false;
    oClosestHit._Normal = -oClosestHit._Normal;
  }
  else
    oClosestHit._FrontFace = true;

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
// iRayDir should be normalized
// (U,V) = normalized spherical coordinates
vec3 SampleSkybox( vec3 iRayDir )
{
  float theta = asin(iRayDir.y);
  float phi   = atan(iRayDir.z, iRayDir.x);
  vec2 uv = vec2(.5f + phi * INV_TWO_PI, .5f - theta * INV_PI) + vec2(u_SkyboxRotation, 0.0);

  vec3 skycolor = texture(u_SkyboxTexture, uv).rgb;
  return skycolor;
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
vec3 PBR( Ray iRay, HitPoint iClosestHit, out Ray oScattered, out vec3 oAttenuation )
{
  double cosTheta = dot(-iRay._Dir, iClosestHit._Normal);

  vec3 outColor = u_Materials[iClosestHit._MaterialID]._Emission;

  vec3 F0 = u_Materials[iClosestHit._MaterialID]._Albedo * u_Materials[iClosestHit._MaterialID]._Metallic * u_Materials[iClosestHit._MaterialID]._Opacity; // test : reflectance value

  if ( iClosestHit._FrontFace )
  {
    // Soft Shadows
    vec3 L = GetLightDirSample(iClosestHit._Pos, u_SphereLight._Pos, u_SphereLight._Radius);
    float distToLight = length(L);
    L = normalize(L);
    
    Ray occlusionTestRay;
    occlusionTestRay._Orig = iClosestHit._Pos + iClosestHit._Normal * RESOLUTION;
    occlusionTestRay._Dir = L;
    if ( !AnyHit(occlusionTestRay, distToLight) )
    {
      vec3 N = iClosestHit._Normal;
      vec3 V = normalize(iRay._Orig - iClosestHit._Pos);
      vec3 H = normalize(V + L);

      float alpha = pow(u_Materials[iClosestHit._MaterialID]._Roughness, 2.f);

      vec3 Ks = F(F0, V, H);
      vec3 Kd = ( vec3(1.f) - Ks ) * ( 1.f - u_Materials[iClosestHit._MaterialID]._Metallic );

      vec3 lambert = u_Materials[iClosestHit._MaterialID]._Albedo * INV_PI;

      vec3 cookTorranceNum = D(alpha, N, H) * G(alpha, N, V, L) * F(F0, V, H);   // DGF
      float cookTorranceDenom = 4.f * max(dot(V, N), 0.f) * max(dot(L, N), 0.f); // 4(V.N)(L.N)
      vec3 cookTorrance = cookTorranceNum / max(cookTorranceDenom, EPSILON);     // 

      vec3 BRDF = Kd * lambert + cookTorrance; // kd * fDiffuse + ks * fSpecular

      outColor += BRDF  * u_SphereLight._Emission * max(dot(L, N), 0.f);
    }
  }

  // Reflect or refract ?
  double sinTheta = 1.f;
  float refractionRatio = 1.f;
  
  if ( u_Materials[iClosestHit._MaterialID]._Opacity < 1.f )
  {
    sinTheta = sqrt(1.f - cosTheta * cosTheta);

    refractionRatio = (1. / u_Materials[iClosestHit._MaterialID]._IOR);
    if ( !iClosestHit._FrontFace )
      refractionRatio = u_Materials[iClosestHit._MaterialID]._IOR;
  }

  vec3 normal = normalize(iClosestHit._Normal + u_Materials[iClosestHit._MaterialID]._Roughness * RandomVector());

  if ( ( refractionRatio * sinTheta ) >= 1.f )
  {
    // Must Reflect
    oScattered._Orig = iClosestHit._Pos + normal * RESOLUTION;
    oScattered._Dir  = reflect(iRay._Dir, normal);
  }
  else
  {
    // Can Refract
    oScattered._Orig = iClosestHit._Pos - normal;
    oScattered._Dir  = refract(iRay._Dir, normal, refractionRatio);
  }
  oAttenuation = F0;

  return clamp(outColor, 0.f, 1.f);
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
  //float multiplier = 1.0f;
  vec3 multiplier = vec3(1.f);

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
    //multiplier *= u_Materials[closestHit._MaterialID]._Metallic;
    //ray._Orig = closestHit._Pos + closestHit._Normal * RESOLUTION;
    //ray._Dir = reflect(ray._Dir, closestHit._Normal + u_Materials[closestHit._MaterialID]._Roughness * RandomVector() );

    Ray scattered;
    vec3 attenuation;
    pixelColor += PBR(ray, closestHit, scattered, attenuation) * multiplier;
    ray = scattered;
    multiplier *= attenuation;
  }

  // Apply gamma correction
  pixelColor = pow(pixelColor, vec3(1. / u_Gamma));
  pixelColor = clamp(pixelColor, 0.f, 1.f);

  fragColor = vec4(pixelColor, 0.f);

  if ( 1 == u_Accumulate )
    fragColor += texture(u_ScreenTexture, fragUV);
}
