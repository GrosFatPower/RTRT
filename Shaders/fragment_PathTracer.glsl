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
#include DisneyBSDF.glsl

// ============================================================================
// Uniforms
// ============================================================================

uniform vec2           u_Resolution;
uniform vec2           u_TileOffset;
uniform vec2           u_InvNbTiles;
uniform float          u_Time;
uniform int            u_FrameNum;
uniform int            u_NbCompleteFrames;
uniform vec3           u_BackgroundColor;
uniform Camera         u_Camera;
uniform int            u_ToneMapping       = 1;
uniform int            u_TiledRendering    = 0;
uniform int            u_RussianRoulette   = 1;
uniform int            u_NbSamplesPerPixel = 1;
uniform int            u_NbBounces         = 1;
uniform int            u_EnableBackground  = 0;
uniform int            u_EnableEnvMap      = 0;
uniform float          u_EnvMapRotation    = 0.f;
uniform float          u_EnvMapTotalWeight = 1.f;
uniform float          u_EnvMapIntensity   = 1.f;
uniform vec2           u_EnvMapRes;
uniform sampler2D      u_EnvMap;
uniform sampler2D      u_EnvMapCDF;

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
// DirectLight
// ----------------------------------------------------------------------------
vec3 DirectLight( in Ray iRay, in HitPoint iClosestHit, in Material iMat, float iEta )
{
  vec3 Ld = vec3(0.);
  vec3 scatterPos = iClosestHit._Pos + iClosestHit._Normal * RESOLUTION;

  // Environment Mapping
  if ( u_EnableEnvMap > 0 )
  {
    vec3 lightDir;
    vec4 envMapColPdf = SampleEnvMap(u_EnvMap, u_EnvMapRotation , u_EnvMapRes, u_EnvMapCDF, u_EnvMapTotalWeight, lightDir);
    float lightPdf = envMapColPdf.w;

    float cosTheta = dot(iClosestHit._Normal, lightDir);
    if ( ( cosTheta > 0. ) && ( lightPdf > EPSILON ) )
    {
      Ray shadowRay = Ray(scatterPos, lightDir);
      if ( !AnyHit( shadowRay, INFINITY ) )
      {
        //float pdf = cosTheta / PI;
        //vec3 f = BRDF(iClosestHit._Normal, -iRay._Dir, lightDir, iMat) * cosTheta;
        float pdf = 0.f;
        vec3 f = DisneyEval( iClosestHit, iMat, iEta, -iRay._Dir, lightDir, pdf );
    
        float misWeight = PowerHeuristic(lightPdf, pdf);
        if ( misWeight > 0. )
          Ld += misWeight * envMapColPdf.rgb * f * u_EnvMapIntensity / lightPdf;
      }
    }
  }

  // Lights sampling
  {
    // Choose a light source randomly
    int lightInd = int(rand() * u_NbLights);

    // Get direction to it
    vec3 lightDir = GetLightDirSample(scatterPos, u_Lights[lightInd]);

    float distToLight = length(lightDir);
    normalize(lightDir);

    float cosTheta = dot(iClosestHit._Normal, lightDir);
    if ( cosTheta > 0. )
    {
      Ray shadowRay = Ray(scatterPos, lightDir);

      // Get the pdf value for this direction
      float lightPdf = 0.0f;
      for ( int j = 0; j < u_NbLights; ++j )
      {
        lightPdf += LightPDF(u_Lights[j], shadowRay);
      }
      lightPdf /= u_NbLights;

      if ( lightPdf > EPSILON )
      {
        if ( !AnyHit(shadowRay, distToLight) )
        {
          float pdf = 0.f;
          vec3 f = DisneyEval( iClosestHit, iMat, iEta, -iRay._Dir, lightDir, pdf );
    
          float misWeight = PowerHeuristic(lightPdf, pdf);
          if ( misWeight > 0. )
            Ld += misWeight * u_Lights[lightInd]._Emission * f / lightPdf;
        }
      }
    }
  }

  return Ld;
}

// ----------------------------------------------------------------------------
// Scatter
// ----------------------------------------------------------------------------
bool Scatter( in Ray iRay, in HitPoint iClosestHit, in Material iMat, in float iEta, out ScatterRecord oScatterRecord )
{
  oScatterRecord = ScatterRecord(SCATTER_NONE, vec3(0.f), 0.f, vec3(0.f));

  oScatterRecord._Attenuation = DisneySample( iClosestHit, iMat, iEta, -iRay._Dir, oScatterRecord._Dir, oScatterRecord._P );

  if ( oScatterRecord._P < EPSILON )
    return false;

  oScatterRecord._Type = SCATTER_RANDOM;
  return true;
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

  ScatterRecord scatterSample = ScatterRecord(SCATTER_RANDOM, vec3(0.f), 0.f, vec3(0.f));

  float eta = 1.5f;
  Ray ray = iStartRay;
  for ( int depth = 0; ; ++depth )
  {
    HitPoint closestHit;
    TraceRay(ray, closestHit);

    // NO HIT
    if ( closestHit._Dist < -RESOLUTION )
    {
      if ( ( depth > 0 ) || ( 1 == u_EnableBackground ) )
      {
        if ( u_EnableEnvMap > 0 )
        {
          vec4 envMapColPdf = SampleEnvMap(ray._Dir, u_EnvMap, u_EnvMapRotation , u_EnvMapRes, u_EnvMapTotalWeight);

          float misWeight = 1.;
          // Gather radiance from envmap and use scatterSample.pdf from previous bounce for MIS
          if ( depth > 0 )
            misWeight = PowerHeuristic(scatterSample._P, envMapColPdf.w);

          if( misWeight > 0. )
            radiance += misWeight * envMapColPdf.rgb * throughput * u_EnvMapIntensity;
        }
        else
          radiance += u_BackgroundColor * throughput;
      }

      break;
    }

    // DEBUG
    if ( u_DebugMode > 1 )
    {
      radiance += DebugColor( ray, closestHit );
      break;
    }

    // EMITTER
    if ( closestHit._IsEmitter )
    {
      float misWeight = 1.;
      if ( ( depth > 0 ) && ( closestHit._LightID >= 0 ) )
      {
        float lightP = LightPDF(u_Lights[closestHit._LightID], ray);
        misWeight = PowerHeuristic(scatterSample._P, lightP);
      }

      if ( misWeight > 0. )
        radiance += misWeight * u_Lights[closestHit._LightID]._Emission * throughput;
      break;
    }

    // MATERIAL PROPERTIES
    Material mat;
    LoadMaterial(closestHit, mat);
    eta = ( closestHit._FrontFace ) ? ( 1.f / mat._IOR ) : ( mat._IOR );

    radiance += mat._Emission * throughput; // Emission from meshes is not importance sampled

    if ( depth >= MaxPathSegments )
      break; // Maximum depth reached

    // DIRECT LIGHT
    if ( SCATTER_RANDOM == scatterSample._Type )
    {
      radiance += DirectLight(ray, closestHit, mat, eta) * throughput;
    }

    // SCATTER
    Scatter(ray, closestHit, mat, eta, scatterSample);
    if ( SCATTER_NONE == scatterSample._Type )
      break;

    throughput *= scatterSample._Attenuation / ( scatterSample._P + EPSILON );

    // NEXT RAY
    ray._Orig = closestHit._Pos + scatterSample._Dir * RESOLUTION;
    ray._Dir = scatterSample._Dir;

    if ( depth >= u_NbBounces )
    {
      // Russian Roulette
      if ( u_RussianRoulette > 0 )
      {
        float maxThroughput = max( throughput.x, max( throughput.y, throughput.z ) );
        float q = min( maxThroughput + EPSILON, 0.95 ); // Lower throughput will lead to higher probability to cancel the path
        if ( rand() > q )
          break;
        float rrWeight = 1.0f / q;
        throughput *= rrWeight;
      }
      else
        break;
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

  vec3 radiance = vec3(0.f);
  for ( int i = 0; i < u_NbSamplesPerPixel; ++i )
  {
    Ray ray = GetRay(coordUV);
    radiance += PathSample(ray);
  }
  radiance /= u_NbSamplesPerPixel;

  if ( 0 != u_ToneMapping )
  {
    radiance = ReinhardToneMapping( radiance );
    radiance = GammaCorrection( radiance );
  }
  else
    radiance = clamp(radiance, 0.f, 1.f);

  fragColor = vec4(radiance, 1.f);
}
