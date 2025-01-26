#version 430 core

in vec2 fragUV;
out vec4 fragColor;

#include Constants.glsl
#include Structures.glsl
#include RNG.glsl
#include Textures.glsl
#include Material.glsl
#include Scene.glsl
#include Intersections.glsl
#include Sampling.glsl
#include RayTrace.glsl
#include BRDF.glsl
#include ToneMapping.glsl

// ============================================================================
// Uniforms
// ============================================================================

uniform vec2           u_Resolution;
uniform vec2           u_TileOffset;
uniform vec2           u_InvNbTiles;
uniform float          u_Time;
uniform int            u_FrameNum;
uniform int            u_TiledRendering;
uniform int            u_NbCompleteFrames;
uniform int            u_Bounces;
uniform int            u_ToneMapping;
uniform vec3           u_BackgroundColor;
uniform Camera         u_Camera;
uniform int            u_EnableBackground;
uniform int            u_EnableSkybox;
uniform float          u_SkyboxRotation;
uniform sampler2D      u_SkyboxTexture;

uniform int            u_DebugMode;

// ----------------------------------------------------------------------------
// DirectIllumination (PBR)
// Computer Graphics Tutorial - PBR (Physically Based Rendering)
// https://www.youtube.com/watch?v=RRE-F57fbXw&list=WL&index=109
// ----------------------------------------------------------------------------
vec3 DirectIllumination( in Ray iRay, in HitPoint iClosestHit, out Ray oScattered, out vec3 oAttenuation )
{
  Material mat;
  LoadMaterial(iClosestHit, mat);

  vec3 outColor = mat._Emission;

  vec3 F0 = vec3(0.16f * pow(mat._Reflectance, 2.));
  F0 = mix(F0, mat._Albedo, mat._Metallic);

  if ( iClosestHit._FrontFace )
  {
    vec3 V = normalize(iRay._Orig - iClosestHit._Pos);

    // Direct lighting
    for ( int i = 0; i < u_NbLights; ++i )
    {
      vec3 L;
      if ( QUAD_LIGHT == u_Lights[i]._Type )
      {
        L = GetLightDirSample(iClosestHit._Pos, u_Lights[i]._Pos, u_Lights[i]._DirU, u_Lights[i]._DirV);
        //vec3 lightNormal = cross(u_Lights[i]._DirU, u_Lights[i]._DirV);
        //if ( dot(L, lightNormal) > EPSILON )
        //  continue;
      }
      else if ( SPHERE_LIGHT == u_Lights[i]._Type )
        L = GetLightDirSample(iClosestHit._Pos, u_Lights[i]._Pos, u_Lights[i]._Radius);
      else
        L = u_Lights[i]._Pos;

      float distToLight = length(L);
      float invDistToLight = 1. / max(distToLight, EPSILON);
      L = L * invDistToLight;
    
      Ray occlusionTestRay;
      occlusionTestRay._Orig = iClosestHit._Pos + iClosestHit._Normal * RESOLUTION;
      occlusionTestRay._Dir = L;
      if ( !AnyHit(occlusionTestRay, distToLight) )
      {
        float irradiance = max(dot(L, iClosestHit._Normal), 0.f) * invDistToLight * invDistToLight;
        if ( irradiance > 0.f )
          outColor += BRDF(iClosestHit._Normal, V, L, F0, mat) * u_Lights[i]._Emission * irradiance;
      }
    }

    // Image based lighting
    {
      vec3 L = SampleHemisphere( iClosestHit._Normal );

      float irradiance = max( dot( L, iClosestHit._Normal ), 0.f ) * 0.1f;
      if ( irradiance > 0.f )
      {
        Ray occlusionTestRay;
        occlusionTestRay._Orig = iClosestHit._Pos + iClosestHit._Normal * RESOLUTION;
        occlusionTestRay._Dir = L;
        if ( !AnyHit( occlusionTestRay, INFINITY ) )
        {
          vec3 envColor;
          if ( 0 != u_EnableSkybox )
            envColor = SampleSkybox( L, u_SkyboxTexture, u_SkyboxRotation );
          else
            envColor = u_BackgroundColor;
          outColor += BRDF( iClosestHit._Normal, V, L, F0, mat ) * envColor * irradiance;
        }
      }
    }

  }

  // Reflect or refract ?
  double sinTheta = 1.f;
  float refractionRatio = 1.f;
  if ( mat._Opacity < 1.f )
  {
    double cosTheta = dot(-iRay._Dir, iClosestHit._Normal);
    sinTheta = sqrt(1.f - cosTheta * cosTheta);

    if ( iClosestHit._FrontFace )
      refractionRatio = (1. / mat._IOR);
    else
      refractionRatio = mat._IOR;
  }

  //vec3 normal = iClosestHit._Normal;
  vec3 normal = normalize(iClosestHit._Normal + mat._Roughness * RandomVec3());

  if ( ( refractionRatio * sinTheta ) >= 1.f )
  {
    // Must Reflect
    oScattered._Orig = iClosestHit._Pos + normal * RESOLUTION;
    oScattered._Dir  = reflect(iRay._Dir, normal);
    oAttenuation = F0;
  }
  else
  {
    // Can Refract
    oScattered._Orig = iClosestHit._Pos - normal;
    oScattered._Dir  = refract(iRay._Dir, normal, refractionRatio);
    oAttenuation = vec3(1.f - mat._Opacity);
  }

  return clamp(outColor, 0.f, 1.f);
}

// ----------------------------------------------------------------------------
// DebugColor
// ----------------------------------------------------------------------------
vec3 DebugColor( in Ray iRay, in HitPoint iClosestHit, out Ray oScattered, out vec3 oAttenuation )
{
  vec3 outColor;

  Material mat;
  LoadMaterial( iClosestHit, mat );

  if ( 1 == u_DebugMode )
  {
    outColor = mat._Albedo;
  }
  else if ( 2 == u_DebugMode )
  {
    outColor = vec3(mat._Metallic);
  }
  else if ( 3 == u_DebugMode )
  {
    outColor = vec3(mat._Roughness);
  }
  else if ( 4 == u_DebugMode )
  {
    outColor = iClosestHit._Normal;
  }
  else if ( 5 == u_DebugMode )
  {
    outColor = vec3(iClosestHit._UV.x, iClosestHit._UV.y, 0.f);
  }

  return outColor;
}

// ----------------------------------------------------------------------------
// GetRay
// ----------------------------------------------------------------------------
Ray GetRay( in vec2 iCoordUV )
{
  Ray ray;

  float r1 = 2.0 * rand();
  float r2 = 2.0 * rand();

  vec2 jitter;
  jitter.x = ( r1 < 1.0 ) ? ( sqrt(r1) - 1.0 ) : ( 1.0 - sqrt(2.0 - r1) ) ;
  jitter.y = ( r2 < 1.0 ) ? ( sqrt(r2) - 1.0 ) : ( 1.0 - sqrt(2.0 - r2) ) ;
  jitter /= (u_Resolution * 0.5);

  vec2 centeredUV = ( 2. * iCoordUV - 1. ) + jitter;

  float scale = tan(u_Camera._FOV * .5);
  centeredUV.x *= scale;
  centeredUV.y *= ( u_Resolution.y / u_Resolution.x ) * scale;

  if ( u_Camera._LensRadius > EPSILON )
  {
    // FocalDist/Aperture
    vec2 randDisk = u_Camera._LensRadius * RandomInUnitDisk();
    vec3 randOffset = u_Camera._Right * randDisk.x + u_Camera._Up * randDisk.y;

    vec3 focalPoint = u_Camera._Pos + u_Camera._FocalDist * ( u_Camera._Right * centeredUV.x + u_Camera._Up * centeredUV.y + u_Camera._Forward );
    ray._Orig = u_Camera._Pos + randOffset;
    ray._Dir = normalize(focalPoint - ray._Orig);
  }
  else
  {
    ray._Orig = u_Camera._Pos;
    ray._Dir = normalize(u_Camera._Right * centeredUV.x + u_Camera._Up * centeredUV.y + u_Camera._Forward);
  }

  return ray;
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
void main()
{
  // Initialization
  InitRNG(gl_FragCoord.xy, u_FrameNum);

  vec2 coordUV = fragUV;
  if( 1 == u_TiledRendering )
    coordUV = mix(u_TileOffset, u_TileOffset + u_InvNbTiles, fragUV);

  Ray ray = GetRay(coordUV);

  // Ray cast
  vec3 pixelColor = vec3(0.f, 0.f, 0.f);
  vec3 multiplier = vec3(1.f);

  for ( int i = 0; i < u_Bounces; ++i )
  {
    HitPoint closestHit;
    TraceRay(ray, closestHit);
    if ( closestHit._Dist < -RESOLUTION )
    {
      if ( ( 0 == i ) && ( 0 == u_EnableBackground ) )
        break;

      else if ( 0 != u_EnableSkybox )
        pixelColor += SampleSkybox(ray._Dir, u_SkyboxTexture, u_SkyboxRotation) * multiplier;
      else
        pixelColor += u_BackgroundColor * multiplier;
      break;
    }

    if ( closestHit._IsEmitter )
    {
      pixelColor += u_Lights[closestHit._LightID]._Emission * multiplier;
      break;
    }

    Ray scattered;
    vec3 attenuation;
    if ( ( 0 == u_DebugMode ) || ( 6 == u_DebugMode ) )
    {
      pixelColor += DirectIllumination( ray, closestHit, scattered, attenuation ) * multiplier;
      ray = scattered;
      multiplier *= attenuation;
    }
    else
    {
      pixelColor += DebugColor( ray, closestHit, scattered, attenuation );
      break;
    }
  }

  if ( 0 != u_ToneMapping )
  {
    pixelColor = ReinhardToneMapping( pixelColor );
    pixelColor = GammaCorrection( pixelColor );
  }

  //pixelColor = clamp(pixelColor, 0.f, 1.f);
  fragColor = vec4(pixelColor, 1.f);
}
