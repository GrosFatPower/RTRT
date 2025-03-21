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
uniform int            u_EnableEnvMap;
uniform float          u_EnvMapRotation;
uniform float          u_EnvMapTotalWeight;
uniform vec2           u_EnvMapRes;
uniform sampler2D      u_EnvMap;

uniform int            u_DebugMode;


// ----------------------------------------------------------------------------
// DebugColor
// ----------------------------------------------------------------------------
vec3 DebugColor( in Ray iRay, in HitPoint iClosestHit )
{
  vec3 outColor;

  Material mat;
  LoadMaterial( iClosestHit, mat );

  if ( 2 == u_DebugMode )
  {
    outColor = mat._Albedo;
  }
  else if ( 3 == u_DebugMode )
  {
    outColor = vec3(mat._Metallic);
  }
  else if ( 4 == u_DebugMode )
  {
    outColor = vec3(mat._Roughness);
  }
  else if ( 5 == u_DebugMode )
  {
    outColor = iClosestHit._Normal;
  }
  else if ( 6 == u_DebugMode )
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
    //vec3 focalPoint = u_Camera._Pos + u_Camera._FocalDist * normalize( u_Camera._Right * centeredUV.x + u_Camera._Up * centeredUV.y + u_Camera._Forward );
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
// Scatter
// ----------------------------------------------------------------------------
bool Scatter( in Ray iRay, in HitPoint iClosestHit, in Material iMat, out ScatterRecord oScatterRecord )
{
  InitializeScatterRecord(oScatterRecord);

  if ( ( iMat._Opacity == 1.f ) && !iClosestHit._FrontFace )
    return false;

  // TMP
  if ( iMat._Roughness < RESOLUTION ) // Perfect mirror
  {
    oScatterRecord._Type = SCATTER_EXPLICIT;
    oScatterRecord._Dir  = reflect(iRay._Dir, iClosestHit._Normal);

    oScatterRecord._P = 1.;
    oScatterRecord._Attenuation = iMat._Albedo;
  }
  else
  {
    oScatterRecord._Type = SCATTER_RANDOM;

    if ( rand() > iMat._Roughness )
    {
      vec3 normal = normalize(iClosestHit._Normal + iMat._Roughness * RandomVec3());
      oScatterRecord._Dir  = reflect(iRay._Dir, normal);
    }
    else
      oScatterRecord._Dir = UniformSampleonOrientedHemisphere(iClosestHit._Tangent, iClosestHit._Bitangent ,iClosestHit._Normal);

    float cosTheta = max(dot(iClosestHit._Normal, oScatterRecord._Dir), 0.f);
    oScatterRecord._P = cosTheta / PI;
    oScatterRecord._Attenuation = BRDF(iClosestHit._Normal, -iRay._Dir, oScatterRecord._Dir, iMat) * cosTheta;
  }

  return true;
}

// ----------------------------------------------------------------------------
// ScatterToDirection
// ----------------------------------------------------------------------------
void ScatterToDirection( in Ray iRay, in HitPoint iClosestHit, in vec3 iDir, in Material iMat, out ScatterRecord oScatterRecord )
{
  InitializeScatterRecord(oScatterRecord);
  oScatterRecord._Dir = iDir;

  // TMP
  float cosTheta = dot(iClosestHit._Normal, iDir);
  if ( cosTheta <= 0.f )
    return;

  oScatterRecord._P = cosTheta / PI;
  oScatterRecord._Attenuation = BRDF(iClosestHit._Normal, -iRay._Dir, iDir, iMat) * cosTheta;
}

// ----------------------------------------------------------------------------
// PathSample
// ----------------------------------------------------------------------------
vec3 PathSample( in Ray iStartRay )
{
  // Ray cast
  vec3 radiance = vec3(0.f);
  vec3 throughput = vec3(1.f);

  const int MaxPathSegments = 128;

  Ray ray = iStartRay;
  for ( int i = 0; i < MaxPathSegments; ++i )
  {
    HitPoint closestHit;
    TraceRay(ray, closestHit);

    if ( closestHit._Dist < -RESOLUTION )
    {
      if ( ( 0 == i ) && ( 0 == u_EnableBackground ) )
        break;

      if ( 0 != u_EnableEnvMap )
        radiance += SampleSkybox(ray._Dir, u_EnvMap, u_EnvMapRotation) * throughput;
      else
        radiance += u_BackgroundColor * throughput;
      break;
    }

    if ( u_DebugMode > 1 )
    {
      radiance += DebugColor( ray, closestHit );
      break;
    }

    if ( closestHit._IsEmitter )
    {
      radiance += u_Lights[closestHit._LightID]._Emission * throughput;
      break;
    }

    Material mat;
    LoadMaterial(closestHit, mat);

    ScatterRecord sr;
    Scatter(ray, closestHit, mat, sr);

    radiance += mat._Emission * throughput;

    if ( SCATTER_NONE == sr._Type )
      break;

    vec3 nextThroughput = throughput * ( sr._Attenuation / ( sr._P + EPSILON ) );

    Ray scatteredRay;
    scatteredRay._Orig = closestHit._Pos + closestHit._Normal * RESOLUTION;
    scatteredRay._Dir = sr._Dir;

    // Sample light source directly for MIS
    if ( ( SCATTER_RANDOM == sr._Type ) && ( u_NbLights > 0 ) )
    {
      // Update the throughput for the next path segment
      float lightsP = 0.0f;
      for ( int j = 0; j < u_NbLights; ++j )
      {
        lightsP += LightPDF(u_Lights[j], scatteredRay);
      }
      lightsP /= u_NbLights;
      nextThroughput *= PowerHeuristic(sr._P, lightsP);

      // Choose a light source randomly
      int lightInd = int(rand() * u_NbLights);

      // Get direction to it
      vec3 lightDir = GetLightDirSample(scatteredRay._Orig, u_Lights[lightInd]);

      float distToLight = length(lightDir);
      normalize(lightDir);

      Ray lightRay;
      lightRay._Orig = scatteredRay._Orig;
      lightRay._Dir = lightDir;

      // Get the pdf value for this direction
      float lightDirP = 0.0f;
      for ( int j = 0; j < u_NbLights; ++j )
      {
        lightDirP += LightPDF(u_Lights[j], lightRay);
      }
      lightDirP /= u_NbLights;

      if ( lightDirP > EPSILON )
      {
        // Get information about a ray going from our current hit point in this direction
        ScatterRecord lightSR;
        ScatterToDirection(ray, closestHit, lightRay._Dir, mat, lightSR);

        if ( lightSR._P > EPSILON )
        {
          // Shoot a ray from our current hit point in this direction
          // check if we hit the hot spot we chose
          if ( !AnyHit(lightRay, distToLight) )
          {
            // Add the contribution of the hot spot using the power heuristic weight
            float weight = PowerHeuristic(lightDirP, lightSR._P);
            radiance += throughput *  ( lightSR._Attenuation / lightDirP ) * weight * u_Lights[lightInd]._Emission;
          }
        }
      }
    }

    throughput = nextThroughput;
    ray = scatteredRay;

    // Russian Roulette
    if ( i >= u_Bounces )
    {
      float maxThroughput = max(throughput.x, max(throughput.y, throughput.z));
      float q = min(maxThroughput + EPSILON, 0.95); // Lower throughput will lead to higher probability to cancel the path
      if ( rand() > q )
        break;
      float rrWeight = 1.0f / q;
      throughput *= rrWeight;
    }
  }

  return radiance;
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

  int nSPP = 1; // Nb samples per pixels

  vec3 radiance = vec3(0.f);
  for ( int i = 0; i < nSPP; ++i )
  {
    Ray ray = GetRay(coordUV);
    radiance += PathSample(ray);
  }
  radiance /= nSPP;

  if ( 0 != u_ToneMapping )
  {
    radiance = ReinhardToneMapping( radiance );
    radiance = GammaCorrection( radiance );
  }
  else
    radiance = clamp(radiance, 0.f, 1.f);

  fragColor = vec4(radiance, 1.f);
}
