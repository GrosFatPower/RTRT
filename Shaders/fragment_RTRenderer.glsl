#version 430 core

in vec2 fragUV;
out vec4 fragColor;

#include Constants.glsl
#include Structures.glsl
#include RNG.glsl
#include Intersections.glsl
#include Sampling.glsl
#include BRDF.glsl
#include ToneMapping.glsl

#define OPTIM_AABB
#define BLAS_TRAVERSAL
#define TLAS_TRAVERSAL

#if defined(BLAS_TRAVERSAL) || defined(BLAS_TRAVERSAL)
#include BVH.glsl
#endif

// ============================================================================
// Uniforms
// ============================================================================

#define MAX_SPHERE_COUNT   32
#define MAX_PLANES_COUNT   32
#define MAX_BOX_COUNT      32
#define MAX_LIGHT_COUNT    32

uniform vec2           u_Resolution;
uniform float          u_Time;
uniform int            u_FrameNum;
uniform int            u_Accumulate;
uniform int            u_Bounces;
uniform int            u_ToneMapping;
uniform vec3           u_BackgroundColor;
uniform Camera         u_Camera;
uniform Light          u_Lights[MAX_LIGHT_COUNT];
uniform int            u_NbLights;
uniform int            u_ShowLights;
uniform int            u_NbSpheres;
uniform Sphere         u_Spheres[MAX_SPHERE_COUNT];
uniform int            u_NbPlanes;
uniform Plane          u_Planes[MAX_PLANES_COUNT];
uniform int            u_NbBoxes;
uniform Box            u_Boxes[MAX_BOX_COUNT];
uniform int            u_NbTriangles;
uniform int            u_NbMeshInstances;
uniform int            u_EnableSkybox;
uniform int            u_EnableBackground;
uniform float          u_SkyboxRotation;
uniform sampler2D      u_SkyboxTexture;
uniform sampler2D      u_ScreenTexture;
uniform sampler2D      u_MaterialsTexture;
uniform samplerBuffer  u_VtxTexture;
uniform samplerBuffer  u_VtxNormTexture;
uniform samplerBuffer  u_VtxUVTexture;
uniform samplerBuffer  u_MeshBBoxTexture;
uniform isamplerBuffer u_VtxIndTexture;
uniform isamplerBuffer u_TexIndTexture;
uniform isamplerBuffer u_MeshIDRangeTexture;
uniform sampler2DArray u_TexArrayTexture;

uniform int            u_DebugMode;

// ============================================================================
// Ray tracing
// ============================================================================

// ----------------------------------------------------------------------------
// TraceRay
// ----------------------------------------------------------------------------
bool TraceRay( in Ray iRay, out HitPoint oClosestHit )
{
  InitializeHitPoint(oClosestHit);

  float hitDist = 0.f;

  // Meshes
  // ------

#ifdef TLAS_TRAVERSAL
  HitPoint closestHit;
  if ( TraceRay_ThroughTLAS( iRay, closestHit ) )
  {
    oClosestHit = closestHit;
  }
#elif defined(BLAS_TRAVERSAL)
  for ( int ind = 0; ind < u_NbMeshInstances; ++ind )
  {
    ivec2 meshMatID = texelFetch(u_TLASMeshMatIDTexture, ind).xy;
    vec4 right   = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 0, 0), 0).xyzw;
    vec4 up      = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 1, 0), 0).xyzw;
    vec4 forward = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 2, 0), 0).xyzw;
    vec4 trans   = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 3, 0), 0).xyzw;
    mat4 invTransform = inverse(mat4(right, up, forward, trans));

    ivec2 blasRange = texelFetch(u_BLASNodesRangeTexture, meshMatID.x).xy;
    ivec2 triRange  = texelFetch(u_BLASPackedIndicesRangeTexture, meshMatID.x).xy;
    blasRange.x *= 3;

    HitPoint closestHit;
    if ( TraceRay_ThroughBLAS(iRay, invTransform, blasRange.x, triRange.x, meshMatID.y, oClosestHit._Dist, closestHit ) )
    {
      oClosestHit = closestHit;
    }
  }
#elif defined(OPTIM_AABB)
  for ( int i = 0; i < u_NbMeshInstances; ++i )
  {
    int ind = i*2;
    vec3 low  = texelFetch(u_MeshBBoxTexture, ind).xyz;
    vec3 high = texelFetch(u_MeshBBoxTexture, ind + 1).xyz;

    hitDist = 0.f;
    //mat4 identity = mat4(1.f);
    //if ( BoxIntersection(low, high, identity, iRay, hitDist) && ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) ) )
    if ( BoxIntersection(low, high, iRay, hitDist) && ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) ) )
    {
      int startIdx = texelFetch(u_MeshIDRangeTexture, ind).x;
      int endIdx   = texelFetch(u_MeshIDRangeTexture, ind + 1).x;

      for ( int i = startIdx; i <= endIdx; i += 3 )
      {
        ivec3 vInd0 = ivec3(texelFetch(u_VtxIndTexture, i).xyz);
        ivec3 vInd1 = ivec3(texelFetch(u_VtxIndTexture, i+1).xyz);
        ivec3 vInd2 = ivec3(texelFetch(u_VtxIndTexture, i+2).xyz);

        vec3 v0 = texelFetch(u_VtxTexture, vInd0.x).xyz;
        vec3 v1 = texelFetch(u_VtxTexture, vInd1.x).xyz;
        vec3 v2 = texelFetch(u_VtxTexture, vInd2.x).xyz;

        hitDist = 0.f;
        vec2 uv;
        if ( TriangleIntersection(iRay, v0, v1, v2, hitDist, uv) )
        {
          if ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) )
          {
            vec3 norm0 = texelFetch(u_VtxNormTexture, vInd0.y).xyz;
            vec3 norm1 = texelFetch(u_VtxNormTexture, vInd1.y).xyz;
            vec3 norm2 = texelFetch(u_VtxNormTexture, vInd2.y).xyz;

            vec3 uvMatID0 = texelFetch(u_VtxUVTexture, vInd0.z).xyz;
            vec3 uvMatID1 = texelFetch(u_VtxUVTexture, vInd1.z).xyz;
            vec3 uvMatID2 = texelFetch(u_VtxUVTexture, vInd2.z).xyz;

            oClosestHit._Dist       = hitDist;
            oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
            oClosestHit._Normal     = normalize( ( 1 - uv.x - uv.y ) * norm0 + uv.x * norm1 + uv.y * norm2 );
            oClosestHit._UV         = uvMatID0.xy * ( 1 - uv.x - uv.y ) + uvMatID1.xy * uv.x + uvMatID2.xy * uv.y;
            oClosestHit._MaterialID = int(uvMatID0.z);
            TriangleTangents(v0, v1, v2, uvMatID0.xy, uvMatID1.xy, uvMatID2.xy, oClosestHit._Tangent, oClosestHit._Bitangent);
          }
        }
      }
    }
  }

#else
  for ( int i = 0; i < u_NbTriangles; ++i )
  {
    ivec3 vInd0 = ivec3(texelFetch(u_VtxIndTexture, i*3).xyz);
    ivec3 vInd1 = ivec3(texelFetch(u_VtxIndTexture, i*3+1).xyz);
    ivec3 vInd2 = ivec3(texelFetch(u_VtxIndTexture, i*3+2).xyz);

    vec3 v0 = texelFetch(u_VtxTexture, vInd0.x).xyz;
    vec3 v1 = texelFetch(u_VtxTexture, vInd1.x).xyz;
    vec3 v2 = texelFetch(u_VtxTexture, vInd2.x).xyz;

    hitDist = 0.f;
    vec2 uv;
    if ( TriangleIntersection(iRay, v0, v1, v2, hitDist, uv) )
    {
      if ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) )
      {
        vec3 norm0 = texelFetch(u_VtxNormTexture, vInd0.y).xyz;
        vec3 norm1 = texelFetch(u_VtxNormTexture, vInd1.y).xyz;
        vec3 norm2 = texelFetch(u_VtxNormTexture, vInd2.y).xyz;

        vec3 uvMatID0 = texelFetch(u_VtxUVTexture, vInd0.z).xyz;
        vec3 uvMatID1 = texelFetch(u_VtxUVTexture, vInd1.z).xyz;
        vec3 uvMatID2 = texelFetch(u_VtxUVTexture, vInd2.z).xyz;

        oClosestHit._Dist       = hitDist;
        oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
        oClosestHit._Normal     = normalize( ( 1 - uv.x - uv.y ) * norm0 + uv.x * norm1 + uv.y * norm2 );
        oClosestHit._UV         = uvMatID0.xy * ( 1 - uv.x - uv.y ) + uvMatID1.xy * uv.x + uvMatID2.xy * uv.y;
        oClosestHit._MaterialID = int(uvMatID0.z);
        TriangleTangents(v0, v1, v2, uvMatID0.xy, uvMatID1.xy, uvMatID2.xy, oClosestHit._Tangent, oClosestHit._Bitangent);
      }
    }
  }
#endif

  // Primitives
  // ----------

  for ( int i = 0; i < u_NbSpheres; ++i )
  {
    hitDist = 0.f;
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
    hitDist = 0.f;
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
    hitDist = 0.f;
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

  // Lights
  // ------

  if ( 0 != u_ShowLights )
  {
    for ( int i = 0; i < u_NbLights; ++i )
    {
      hitDist = 0.f;
      if ( ( SPHERE_LIGHT == u_Lights[i]._Type ) && SphereIntersection(vec4(u_Lights[i]._Pos, u_Lights[i]._Radius), iRay, hitDist) )
      {
        if ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) )
        {
          oClosestHit._Dist       = hitDist;
          oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
          oClosestHit._MaterialID = -1;
          oClosestHit._LightID    = i;
          oClosestHit._IsEmitter  = true;
        }
      }
      else if ( ( QUAD_LIGHT == u_Lights[i]._Type ) && QuadIntersection(u_Lights[i]._Pos, u_Lights[i]._DirU, u_Lights[i]._DirV, iRay, hitDist) )
      {
        if ( ( hitDist > 0.f ) && ( ( hitDist < oClosestHit._Dist ) || ( -1.f == oClosestHit._Dist ) ) )
        {
          oClosestHit._Dist       = hitDist;
          oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
          oClosestHit._MaterialID = -1;
          oClosestHit._LightID    = i;
          oClosestHit._IsEmitter  = true;
        }
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

// ----------------------------------------------------------------------------
// AnyHit
// ----------------------------------------------------------------------------
bool AnyHit( in Ray iRay, in float iMaxDist )
{
  // Meshes
  // ------

#ifdef TLAS_TRAVERSAL
  if ( AnyHit_ThroughTLAS( iRay, iMaxDist ) )
  {
    return true;
  }
#elif defined(BLAS_TRAVERSAL)
  for ( int ind = 0; ind < u_NbMeshInstances; ++ind )
  {
    ivec2 meshMatID = texelFetch(u_TLASMeshMatIDTexture, ind).xy;
    vec4 right   = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 0, 0), 0).xyzw;
    vec4 up      = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 1, 0), 0).xyzw;
    vec4 forward = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 2, 0), 0).xyzw;
    vec4 trans   = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 3, 0), 0).xyzw;
    mat4 invTransform = inverse(mat4(right, up, forward, trans));

    ivec2 blasRange = texelFetch(u_BLASNodesRangeTexture, meshMatID.x).xy;
    ivec2 triRange  = texelFetch(u_BLASPackedIndicesRangeTexture, meshMatID.x).xy;
    blasRange.x *= 3;

    if ( AnyHit_ThroughBLAS(iRay, invTransform, blasRange.x, triRange.x, iMaxDist ) )
      return true;
  }
#elif defined(OPTIM_AABB)
  for ( int i = 0; i < u_NbMeshInstances; ++i )
  {
    int ind = i*2;
    vec3 low  = texelFetch(u_MeshBBoxTexture, ind).xyz;
    vec3 high = texelFetch(u_MeshBBoxTexture, ind + 1).xyz;

    float hitDist = 0.f;
    mat4 identity = mat4(1.f);
    if ( BoxIntersection(low, high, identity, iRay, hitDist) && ( hitDist > 0.f ) )
    //if ( BBoxIntersection(iRay, low, high) )
    {
      int startIdx = texelFetch(u_MeshIDRangeTexture, ind).x;
      int endIdx   = texelFetch(u_MeshIDRangeTexture, ind + 1).x;

      for ( int i = startIdx; i <= endIdx; i += 3 )
      {
        ivec3 vInd0 = ivec3(texelFetch(u_VtxIndTexture, i).xyz);
        ivec3 vInd1 = ivec3(texelFetch(u_VtxIndTexture, i+1).xyz);
        ivec3 vInd2 = ivec3(texelFetch(u_VtxIndTexture, i+2).xyz);

        vec3 v0 = texelFetch(u_VtxTexture, vInd0.x).xyz;
        vec3 v1 = texelFetch(u_VtxTexture, vInd1.x).xyz;
        vec3 v2 = texelFetch(u_VtxTexture, vInd2.x).xyz;

        hitDist = 0.f;
        vec2 uv;
        if ( TriangleIntersection(iRay, v0, v1, v2, hitDist, uv) )
        {
          if ( ( hitDist > 0.f ) && ( hitDist < iMaxDist ) )
            return true;
        }
      }
    }
  }

#else
  for ( int i = 0; i < u_NbTriangles; ++i )
  {
    ivec3 vInd0 = ivec3(texelFetch(u_VtxIndTexture, i*3).xyz);
    ivec3 vInd1 = ivec3(texelFetch(u_VtxIndTexture, i*3+1).xyz);
    ivec3 vInd2 = ivec3(texelFetch(u_VtxIndTexture, i*3+2).xyz);

    vec3 v0 = texelFetch(u_VtxTexture, vInd0.x).xyz;
    vec3 v1 = texelFetch(u_VtxTexture, vInd1.x).xyz;
    vec3 v2 = texelFetch(u_VtxTexture, vInd2.x).xyz;

    float hitDist = 0.f;
    vec2 uv;
    if ( TriangleIntersection(iRay, v0, v1, v2, hitDist, uv) )
    {
      if ( ( hitDist > 0.f ) && ( hitDist < iMaxDist ) )
        return true;
    }
  }
#endif

  // Primitives
  // ----------

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

// ----------------------------------------------------------------------------
// LoadMaterial
// ----------------------------------------------------------------------------
void LoadMaterial( inout HitPoint ioClosestHit, out Material oMat )
{
  if ( ioClosestHit._MaterialID < 0 )
    return;

  int index = ioClosestHit._MaterialID * 8;

  vec4 Params1 = texelFetch(u_MaterialsTexture, ivec2(index + 0, 0), 0); // _Albedo.r       | _Albedo.g               | _Albedo.b       | _ID
  vec4 Params2 = texelFetch(u_MaterialsTexture, ivec2(index + 1, 0), 0); // _Emission.r     | _Emission.g             | _Emission.b     | _Anisotropic
  vec4 Params3 = texelFetch(u_MaterialsTexture, ivec2(index + 2, 0), 0); // _MediumColor.r  | _MediumColor.g          | _MediumColor.b  | _MediumAnisotropy
  vec4 Params4 = texelFetch(u_MaterialsTexture, ivec2(index + 3, 0), 0); // _Metallic       | _Roughness              | _Subsurface     | _SpecularTint
  vec4 Params5 = texelFetch(u_MaterialsTexture, ivec2(index + 4, 0), 0); // _Sheen          | _SheenTint              | _Clearcoat      | _ClearcoatGloss
  vec4 Params6 = texelFetch(u_MaterialsTexture, ivec2(index + 5, 0), 0); // _SpecTrans      | _IOR                    | _MediumType     | _MediumDensity
  vec4 Params7 = texelFetch(u_MaterialsTexture, ivec2(index + 6, 0), 0); // _BaseColorTexId | _MetallicRoughnessTexID | _NormalMapTexID | _EmissionMapTexID
  vec4 Params8 = texelFetch(u_MaterialsTexture, ivec2(index + 7, 0), 0); // _Opacity        | _AlphaMode              | _AlphaCutoff    | _Reflectance

  oMat._Albedo                 = Params1.rgb;
  oMat._ID                     = int(Params1.w);

  oMat._Emission               = Params2.rgb;
  oMat._Anisotropic            = Params2.w;

  // oMat._MediumColor           = Params3.xyz;
  // oMat._MediumAnisotropy      = Params3.w;

  oMat._Metallic               = Params4.x;
  oMat._Roughness              = Params4.y;
  oMat._Subsurface             = Params4.z;
  oMat._SpecularTint           = Params4.w;

  oMat._Sheen                  = Params5.x;
  oMat._SheenTint              = Params5.y;
  oMat._Clearcoat              = Params5.z;
  oMat._ClearcoatGloss         = Params5.w;

  oMat._SpecularTrans          = Params6.x;
  oMat._IOR                    = Params6.y;
  // oMat._MediumType            = Params6.z;
  // oMat._MediumDensity         = Params6.w;

  int baseColorTexID           = int(Params7.x);
  int metallicRoughnessTexID   = int(Params7.y);
  int normalMapTexID           = int(Params7.z);
  int emissionMapTexID         = int(Params7.w);

  oMat._Opacity                = Params8.x;
  // oMat._AlphaMode             = Params8.y;
  // oMat._AlphaCutoff           = Params8.z;
  oMat._Reflectance            = Params8.w;

  if ( baseColorTexID >= 0 )
  {
    int texArrayID = texelFetch(u_TexIndTexture, baseColorTexID).x;
    if ( texArrayID >= 0 )
      oMat._Albedo = texture(u_TexArrayTexture, vec3(ioClosestHit._UV, float(texArrayID))).rgb * oMat._Albedo;
  }

  if ( normalMapTexID >= 0 )
  {
    int texArrayID = texelFetch(u_TexIndTexture, normalMapTexID).x;
    if ( texArrayID >= 0 )
    {  
      vec3 texNormal = texture(u_TexArrayTexture, vec3(ioClosestHit._UV, float(texArrayID))).xyz;
      texNormal = normalize(texNormal * 2.0 - 1.0);
      ioClosestHit._Normal = normalize(ioClosestHit._Tangent * texNormal.x + ioClosestHit._Bitangent * texNormal.y + ioClosestHit._Normal * texNormal.z);
    }
  }

  if ( metallicRoughnessTexID >= 0 )
  {
    int texArrayID = texelFetch(u_TexIndTexture, metallicRoughnessTexID).x;
    if ( texArrayID >= 0 )
    {  
      vec2 metalRoughness = texture(u_TexArrayTexture, vec3(ioClosestHit._UV, float(texArrayID))).rg;
      oMat._Metallic = metalRoughness.x;
      oMat._Roughness = metalRoughness.y;
    }
  }

  if ( emissionMapTexID >= 0 )
  {
    int texArrayID = texelFetch(u_TexIndTexture, emissionMapTexID).x;
    if ( texArrayID >= 0 )
      oMat._Emission = texture(u_TexArrayTexture, vec3(ioClosestHit._UV, float(texArrayID))).rgb;
  }
}

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
  vec3 normal = normalize(iClosestHit._Normal + mat._Roughness * RandomVector());

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
// main
// ----------------------------------------------------------------------------
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
  vec3 multiplier = vec3(1.f);

  // Ray cast
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
    if ( 0 == u_DebugMode )
    {
      pixelColor += DirectIllumination( ray, closestHit, scattered, attenuation ) * multiplier;
      ray = scattered;
      multiplier *= attenuation;
    }
    else
    {
      pixelColor += DebugColor( ray, closestHit, scattered, attenuation ) * multiplier;
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

  if ( 1 == u_Accumulate )
    fragColor += texture(u_ScreenTexture, fragUV);
}
