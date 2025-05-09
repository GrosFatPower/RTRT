/*
 *
 */

#ifndef _RAYTRACE_GLSL_
#define _RAYTRACE_GLSL_

#define OPTIM_AABB
#define BLAS_TRAVERSAL
#define TLAS_TRAVERSAL
//#define BACKFACE_CULLING

#if defined(BLAS_TRAVERSAL) || defined(BLAS_TRAVERSAL)
#include BVH.glsl
#endif

#include Constants.glsl
#include Structures.glsl
#include Scene.glsl
#include Material.glsl
#include Intersections.glsl
#include Sampling.glsl

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
#ifdef BACKFACE_CULLING
  float totalDist = 0.;
  while ( TraceRay_ThroughTLAS( iRay, closestHit ) )
  {
    if ( dot( iRay._Dir, closestHit._Normal ) > 0.f ) // BackFace culling
    {
      iRay._Orig = closestHit._Pos + iRay._Dir * RESOLUTION;
      totalDist += closestHit._Dist + RESOLUTION;
    }
    else
    {
      oClosestHit = closestHit;
      break;
    }
  }
  closestHit._Dist += totalDist;
#else
  if ( TraceRay_ThroughTLAS( iRay, closestHit ) )
  {
    oClosestHit = closestHit;
  }
#endif

#elif defined(BLAS_TRAVERSAL)
  for ( int ind = 0; ind < u_NbMeshInstances; ++ind )
  {
    ivec2 meshMatID = texelFetch(u_TLASMeshMatIDTexture, ind).xy;
    vec4 right   = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 0, 0), 0).xyzw;
    vec4 up      = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 1, 0), 0).xyzw;
    vec4 forward = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 2, 0), 0).xyzw;
    vec4 trans   = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 3, 0), 0).xyzw;
    mat4 transform = mat4(right, up, forward, trans);

    ivec2 blasRange = texelFetch(u_BLASNodesRangeTexture, meshMatID.x).xy;
    ivec2 triRange  = texelFetch(u_BLASPackedIndicesRangeTexture, meshMatID.x).xy;
    blasRange.x *= 3;

    HitPoint closestHit;
    if ( TraceRay_ThroughBLAS(iRay, transform, blasRange.x, triRange.x, meshMatID.y, oClosestHit._Dist, closestHit ) )
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

            vec2 texUV = uvMatID0.xy * ( 1 - uv.x - uv.y ) + uvMatID1.xy * uv.x + uvMatID2.xy * uv.y;
            if ( IsOpaque(int(uvMatID0.z), texUV) )
            {
              oClosestHit._Dist       = hitDist;
              oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
              oClosestHit._Normal     = normalize( ( 1 - uv.x - uv.y ) * norm0 + uv.x * norm1 + uv.y * norm2 );
              oClosestHit._UV         = texUV;
              oClosestHit._MaterialID = int(uvMatID0.z);
              TriangleTangents(v0, v1, v2, uvMatID0.xy, uvMatID1.xy, uvMatID2.xy, oClosestHit._Tangent, oClosestHit._Bitangent);
            }
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

        vec2 texUV = uvMatID0.xy * ( 1 - uv.x - uv.y ) + uvMatID1.xy * uv.x + uvMatID2.xy * uv.y;
        if ( IsOpaque(iMatID, texUV) )
        {
          oClosestHit._Dist       = hitDist;
          oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
          oClosestHit._Normal     = normalize( ( 1 - uv.x - uv.y ) * norm0 + uv.x * norm1 + uv.y * norm2 );
          oClosestHit._UV         = texUV;
          oClosestHit._MaterialID = int(uvMatID0.z);
          TriangleTangents(v0, v1, v2, uvMatID0.xy, uvMatID1.xy, uvMatID2.xy, oClosestHit._Tangent, oClosestHit._Bitangent);
        }
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
    oClosestHit._Normal    = -oClosestHit._Normal;
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
    mat4 transform = mat4(right, up, forward, trans);

    ivec2 blasRange = texelFetch(u_BLASNodesRangeTexture, meshMatID.x).xy;
    ivec2 triRange  = texelFetch(u_BLASPackedIndicesRangeTexture, meshMatID.x).xy;
    blasRange.x *= 3;

    if ( AnyHit_ThroughBLAS(iRay, transform, blasRange.x, triRange.x, meshMatID.y, iMaxDist ) )
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


#endif
