/*
 *
 */

#ifndef _BVH_GLSL_
#define _BVH_GLSL_

#include Constants.glsl
#include Structures.glsl
#include Intersections.glsl
#include Material.glsl
#include RNG.glsl

// ============================================================================
// Uniforms
// ============================================================================

uniform samplerBuffer  u_TLASNodesTexture;
uniform isamplerBuffer u_TLASMeshMatIDTexture;
uniform sampler2D      u_TLASTransformsTexture;
uniform samplerBuffer  u_BLASNodesTexture;
uniform isamplerBuffer u_BLASNodesRangeTexture;
uniform isamplerBuffer u_BLASPackedIndicesTexture;
uniform isamplerBuffer u_BLASPackedIndicesRangeTexture;
uniform samplerBuffer  u_BLASPackedVtxTexture;
uniform samplerBuffer  u_BLASPackedNormTexture;
uniform samplerBuffer  u_BLASPackedUVTexture;

// ============================================================================
// Ray tracing
// ============================================================================

// ----------------------------------------------------------------------------
// TraceRay_ThroughBLAS
// ----------------------------------------------------------------------------
bool TraceRay_ThroughBLAS( in Ray iRay, in mat4 iInvTransfo, in int iBlasNodesOffset, in int iTriOffset, in int iMatID, in float iMaxDist, out HitPoint oClosestHit )
{
  InitializeHitPoint(oClosestHit);

  int   index       = 0; // BLAS Root
  bool  leftHit     = false;
  bool  rightHit    = false;
  float leftDist    = 0.f;
  float rightDist   = 0.f;
  float hitDist     = 0.f;

  vec3 leftBboxMin;
  vec3 leftBboxMax;
  vec3 rightBboxMin;
  vec3 rightBboxMax;

  int   Stack[64]; // BLAS Max depth = 64
  int   topPtr = 0;
  Stack[topPtr++] = -1;

  Ray transRay;
  transRay._Orig = (iInvTransfo * vec4(iRay._Orig, 1.0)).xyz;
  transRay._Dir  = (iInvTransfo * vec4(iRay._Dir, 0.0)).xyz;

  leftBboxMin  = texelFetch(u_BLASNodesTexture, iBlasNodesOffset    ).xyz;
  leftBboxMax  = texelFetch(u_BLASNodesTexture, iBlasNodesOffset + 1).xyz;
  leftHit   = BoxIntersection(leftBboxMin, leftBboxMax, transRay, leftDist);
  if ( !leftHit )
    index = -1;

  while ( index != -1 )
  {
    ivec3 LcRcLeaf = ivec3(texelFetch(u_BLASNodesTexture, iBlasNodesOffset + index * 3 + 2).xyz);

    int leftIndex  = int(LcRcLeaf.x); // or first triangle index
    int rightIndex = int(LcRcLeaf.y); // or nb triangles
    int leaf       = int(LcRcLeaf.z);

    if ( leaf > 0 ) // BLAS Leaf node
    {
      int firstTriIdx = leftIndex * 3;

      for ( int i = 0; i < rightIndex; i++ ) // rightIndex == nbPrimitives
      {
        ivec3 vInd0 = ivec3(texelFetch(u_BLASPackedIndicesTexture, iTriOffset + firstTriIdx + i * 3     ).xyz);
        ivec3 vInd1 = ivec3(texelFetch(u_BLASPackedIndicesTexture, iTriOffset + firstTriIdx + i * 3 + 1 ).xyz);
        ivec3 vInd2 = ivec3(texelFetch(u_BLASPackedIndicesTexture, iTriOffset + firstTriIdx + i * 3 + 2 ).xyz);

        vec3 v0 = texelFetch(u_BLASPackedVtxTexture, vInd0.x).xyz;
        vec3 v1 = texelFetch(u_BLASPackedVtxTexture, vInd1.x).xyz;
        vec3 v2 = texelFetch(u_BLASPackedVtxTexture, vInd2.x).xyz;

        hitDist = 0.f;
        vec2 uv;
        if ( TriangleIntersection(transRay, v0, v1, v2, hitDist, uv) )
        {
          if ( ( hitDist > 0.f ) && ( ( hitDist < iMaxDist ) || ( -1.f == iMaxDist ) ) )
          {
            vec2 uvID0 = texelFetch(u_BLASPackedUVTexture, vInd0.z).xy;
            vec2 uvID1 = texelFetch(u_BLASPackedUVTexture, vInd1.z).xy;
            vec2 uvID2 = texelFetch(u_BLASPackedUVTexture, vInd2.z).xy;

            vec2 uv = uvID0 * ( 1 - uv.x - uv.y ) + uvID1 * uv.x + uvID2 * uv.y;

            bool isOpaque = true;
            if ( iMatID >= 0 )
            {
              float opacity = LoadOpacityValues( iMatID, uv );
              if ( ( opacity < 1.f ) && ( rand() > opacity ) )
                isOpaque = false;
            }

            if ( isOpaque )
            {
              vec3 norm0 = texelFetch(u_BLASPackedNormTexture, vInd0.y).xyz;
              vec3 norm1 = texelFetch(u_BLASPackedNormTexture, vInd1.y).xyz;
              vec3 norm2 = texelFetch(u_BLASPackedNormTexture, vInd2.y).xyz;

              iMaxDist                = hitDist;
              oClosestHit._Dist       = hitDist;
              oClosestHit._Pos        = iRay._Orig + hitDist * iRay._Dir;
              oClosestHit._Normal     = normalize( ( 1 - uv.x - uv.y ) * norm0 + uv.x * norm1 + uv.y * norm2 );
              oClosestHit._UV         = uv;
              oClosestHit._MaterialID = iMatID;
              TriangleTangents(v0, v1, v2, uvID0, uvID1, uvID2, oClosestHit._Tangent, oClosestHit._Bitangent);
            }
          }
        }
      }
    }
    else
    {
      leftBboxMin  = texelFetch(u_BLASNodesTexture, iBlasNodesOffset + leftIndex  * 3    ).xyz;
      leftBboxMax  = texelFetch(u_BLASNodesTexture, iBlasNodesOffset + leftIndex  * 3 + 1).xyz;
      rightBboxMin = texelFetch(u_BLASNodesTexture, iBlasNodesOffset + rightIndex * 3    ).xyz;
      rightBboxMax = texelFetch(u_BLASNodesTexture, iBlasNodesOffset + rightIndex * 3 + 1).xyz;
      
      leftHit  = BoxIntersection(leftBboxMin, leftBboxMax, transRay, leftDist);
      rightHit = BoxIntersection(rightBboxMin, rightBboxMax, transRay, rightDist);

      if ( iMaxDist > 0 )
      {
        if ( leftHit && ( leftDist > iMaxDist ) )
          leftHit = false;
        if ( rightHit && ( rightDist > iMaxDist ) )
          rightHit = false;
      }

      if ( leftHit && rightHit )
      {
        if ( leftDist > rightDist )
        {
          index = rightIndex;
          Stack[topPtr++] = leftIndex;
        }
        else
        {
          index = leftIndex;
          Stack[topPtr++] = rightIndex;
        }
        continue;
      }
      else if ( leftHit )
      {
        index = leftIndex;
        continue;
      }
      else if ( rightHit )
      {
        index = rightIndex;
        continue;
      }
    }

    index = Stack[--topPtr];
  }

  if ( oClosestHit._Dist > 0.f )
    return true;
  return false;
}

// ----------------------------------------------------------------------------
// AnyHit_ThroughBLAS
// ----------------------------------------------------------------------------
bool AnyHit_ThroughBLAS( in Ray iRay, in mat4 iInvTransfo, in int iBlasNodesOffset, in int iTriOffset, in int iMatID, in float iMaxDist )
{
  int   index       = 0; // BLAS Root
  bool  leftHit     = false;
  bool  rightHit    = false;
  float leftDist    = 0.f;
  float rightDist   = 0.f;
  float hitDist     = 0.f;

  vec3 leftBboxMin;
  vec3 leftBboxMax;
  vec3 rightBboxMin;
  vec3 rightBboxMax;

  int   Stack[64]; // BLAS Max depth = 64
  int   topPtr = 0;
  Stack[topPtr++] = -1;

  Ray transRay;
  transRay._Orig = (iInvTransfo * vec4(iRay._Orig, 1.0)).xyz;
  transRay._Dir  = (iInvTransfo * vec4(iRay._Dir, 0.0)).xyz;

  leftBboxMin  = texelFetch(u_BLASNodesTexture, iBlasNodesOffset    ).xyz;
  leftBboxMax  = texelFetch(u_BLASNodesTexture, iBlasNodesOffset + 1).xyz;
  leftHit   = BoxIntersection(leftBboxMin, leftBboxMax, transRay, leftDist);
  if ( !leftHit )
    index = -1;

  while ( index != -1 )
  {
    ivec3 LcRcLeaf = ivec3(texelFetch(u_BLASNodesTexture, iBlasNodesOffset + index * 3 + 2).xyz);

    int leftIndex  = int(LcRcLeaf.x); // or first triangle index
    int rightIndex = int(LcRcLeaf.y); // or nb triangles
    int leaf       = int(LcRcLeaf.z);

    if ( leaf > 0 ) // BLAS Leaf node
    {
      int firstTriIdx = leftIndex * 3;

      for ( int i = 0; i < rightIndex; i++ ) // rightIndex == nbPrimitives
      {
        ivec3 vInd0 = ivec3(texelFetch(u_BLASPackedIndicesTexture, iTriOffset + firstTriIdx + i * 3     ).xyz);
        ivec3 vInd1 = ivec3(texelFetch(u_BLASPackedIndicesTexture, iTriOffset + firstTriIdx + i * 3 + 1 ).xyz);
        ivec3 vInd2 = ivec3(texelFetch(u_BLASPackedIndicesTexture, iTriOffset + firstTriIdx + i * 3 + 2 ).xyz);

        vec3 v0 = texelFetch(u_BLASPackedVtxTexture, vInd0.x).xyz;
        vec3 v1 = texelFetch(u_BLASPackedVtxTexture, vInd1.x).xyz;
        vec3 v2 = texelFetch(u_BLASPackedVtxTexture, vInd2.x).xyz;

        hitDist = 0.f;
        vec2 uv;
        if ( TriangleIntersection(transRay, v0, v1, v2, hitDist, uv) )
        {
          if ( ( hitDist > 0.f ) && ( hitDist < iMaxDist ) )
          {
            vec2 uvID0 = texelFetch(u_BLASPackedUVTexture, vInd0.z).xy;
            vec2 uvID1 = texelFetch(u_BLASPackedUVTexture, vInd1.z).xy;
            vec2 uvID2 = texelFetch(u_BLASPackedUVTexture, vInd2.z).xy;

            vec2 uv = uvID0 * ( 1 - uv.x - uv.y ) + uvID1 * uv.x + uvID2 * uv.y;

            bool isOpaque = true;
            if ( iMatID >= 0 )
            {
              float opacity = LoadOpacityValues( iMatID, uv );
              if ( ( opacity < 1.f ) && ( rand() > opacity ) )
                isOpaque = false;
            }

            if ( isOpaque )
              return true;
          }
        }
      }
    }
    else
    {
      leftBboxMin  = texelFetch(u_BLASNodesTexture, iBlasNodesOffset + leftIndex  * 3    ).xyz;
      leftBboxMax  = texelFetch(u_BLASNodesTexture, iBlasNodesOffset + leftIndex  * 3 + 1).xyz;
      rightBboxMin = texelFetch(u_BLASNodesTexture, iBlasNodesOffset + rightIndex * 3    ).xyz;
      rightBboxMax = texelFetch(u_BLASNodesTexture, iBlasNodesOffset + rightIndex * 3 + 1).xyz;
      
      leftHit  = BoxIntersection(leftBboxMin, leftBboxMax, transRay, leftDist);
      rightHit = BoxIntersection(rightBboxMin, rightBboxMax, transRay, rightDist);

      if ( leftHit && rightHit )
      {
        if ( leftDist > rightDist )
        {
          index = rightIndex;
          Stack[topPtr++] = leftIndex;
        }
        else
        {
          index = leftIndex;
          Stack[topPtr++] = rightIndex;
        }
        continue;
      }
      else if ( leftHit )
      {
        index = leftIndex;
        continue;
      }
      else if ( rightHit )
      {
        index = rightIndex;
        continue;
      }
    }

    index = Stack[--topPtr];
  }

  return false;
}

// ----------------------------------------------------------------------------
// TraceRay_ThroughTLAS
// ----------------------------------------------------------------------------
bool TraceRay_ThroughTLAS( in Ray iRay, out HitPoint oClosestHit )
{
  InitializeHitPoint(oClosestHit);

  int   index       = 0; // TLAS Root
  bool  leftHit     = false;
  bool  rightHit    = false;
  float leftDist    = 0.f;
  float rightDist   = 0.f;
  float hitDist     = 0.f;

  vec3 leftBboxMin;
  vec3 leftBboxMax;
  vec3 rightBboxMin;
  vec3 rightBboxMax;

  int   Stack[64]; // BLAS Max depth = 64
  int   topPtr = 0;
  Stack[topPtr++] = -1;

  leftBboxMin  = texelFetch(u_TLASNodesTexture, 0).xyz;
  leftBboxMax  = texelFetch(u_TLASNodesTexture, 1).xyz;
  leftHit   = BoxIntersection(leftBboxMin, leftBboxMax, iRay, leftDist);
  if ( !leftHit )
    index = -1;

  while ( index >= 0 )
  {
    ivec3 LcRcLeaf = ivec3(texelFetch(u_TLASNodesTexture, index * 3 + 2).xyz);

    int leftIndex  = int(LcRcLeaf.x); // or first MeshInstance
    int rightIndex = int(LcRcLeaf.y); // or nb Mesh instances
    int leaf       = int(LcRcLeaf.z);

    if ( leaf < 0 ) // TLAS Leaf node
    {
      for ( int j = 0; j < rightIndex; j++ ) // rightIndex == nbMeshesInstances
      {
        int ind = leftIndex + j;
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
          oClosestHit = closestHit;
      }
    }
    else
    {
      leftBboxMin  = texelFetch(u_TLASNodesTexture, leftIndex  * 3    ).xyz;
      leftBboxMax  = texelFetch(u_TLASNodesTexture, leftIndex  * 3 + 1).xyz;
      rightBboxMin = texelFetch(u_TLASNodesTexture, rightIndex * 3    ).xyz;
      rightBboxMax = texelFetch(u_TLASNodesTexture, rightIndex * 3 + 1).xyz;
      
      leftHit  = BoxIntersection(leftBboxMin, leftBboxMax, iRay, leftDist);
      rightHit = BoxIntersection(rightBboxMin, rightBboxMax, iRay, rightDist);

      if ( oClosestHit._Dist > 0.f )
      {
        if ( leftHit && ( leftDist > oClosestHit._Dist  ) )
          leftHit = false;
        if ( rightHit && ( rightDist > oClosestHit._Dist  ) )
          rightHit = false;
      }

      if ( leftHit && rightHit )
      {
        if ( leftDist > rightDist )
        {
          index = rightIndex;
          Stack[topPtr++] = leftIndex;
        }
        else
        {
          index = leftIndex;
          Stack[topPtr++] = rightIndex;
        }
        continue;
      }
      else if ( leftHit )
      {
        index = leftIndex;
        continue;
      }
      else if ( rightHit )
      {
        index = rightIndex;
        continue;
      }
    }
    index = Stack[--topPtr];
  }

  if ( oClosestHit._Dist > 0.f )
    return true;
  return false;
}

// ----------------------------------------------------------------------------
// AnyHit_ThroughTLAS
// ----------------------------------------------------------------------------
bool AnyHit_ThroughTLAS( in Ray iRay, in float iMaxDist )
{
  int   index       = 0; // TLAS Root
  bool  leftHit     = false;
  bool  rightHit    = false;
  float leftDist    = 0.f;
  float rightDist   = 0.f;

  vec3 leftBboxMin;
  vec3 leftBboxMax;
  vec3 rightBboxMin;
  vec3 rightBboxMax;

  int   Stack[64]; // BLAS Max depth = 64
  int   topPtr = 0;
  Stack[topPtr++] = -1;

  leftBboxMin  = texelFetch(u_TLASNodesTexture, 0).xyz;
  leftBboxMax  = texelFetch(u_TLASNodesTexture, 1).xyz;
  leftHit   = BoxIntersection(leftBboxMin, leftBboxMax, iRay, leftDist);
  if ( !leftHit )
    index = -1;

  while ( index >= 0 )
  {
    ivec3 LcRcLeaf = ivec3(texelFetch(u_TLASNodesTexture, index * 3 + 2).xyz);

    int leftIndex  = int(LcRcLeaf.x); // or first MeshInstance
    int rightIndex = int(LcRcLeaf.y); // or nb Mesh instances
    int leaf       = int(LcRcLeaf.z);

    if ( leaf < 0 ) // TLAS Leaf node
    {
      for ( int j = 0; j < rightIndex; j++ ) // rightIndex == nbMeshesInstances
      {
        int ind = leftIndex + j;
        ivec2 meshMatID = texelFetch(u_TLASMeshMatIDTexture, ind).xy;

        vec4 right   = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 0, 0), 0).xyzw;
        vec4 up      = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 1, 0), 0).xyzw;
        vec4 forward = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 2, 0), 0).xyzw;
        vec4 trans   = texelFetch(u_TLASTransformsTexture, ivec2(ind * 4 + 3, 0), 0).xyzw;
        mat4 invTransform = inverse(mat4(right, up, forward, trans));

        ivec2 blasRange = texelFetch(u_BLASNodesRangeTexture, meshMatID.x).xy;
        ivec2 triRange  = texelFetch(u_BLASPackedIndicesRangeTexture, meshMatID.x).xy;
        blasRange.x *= 3;

        if ( AnyHit_ThroughBLAS(iRay, invTransform, blasRange.x, triRange.x, meshMatID.y, iMaxDist ) )
          return true;
      }
    }
    else
    {
      leftBboxMin  = texelFetch(u_TLASNodesTexture, leftIndex  * 3    ).xyz;
      leftBboxMax  = texelFetch(u_TLASNodesTexture, leftIndex  * 3 + 1).xyz;
      rightBboxMin = texelFetch(u_TLASNodesTexture, rightIndex * 3    ).xyz;
      rightBboxMax = texelFetch(u_TLASNodesTexture, rightIndex * 3 + 1).xyz;
      
      leftHit  = BoxIntersection(leftBboxMin, leftBboxMax, iRay, leftDist);
      rightHit = BoxIntersection(rightBboxMin, rightBboxMax, iRay, rightDist);

      if ( iMaxDist > 0 )
      {
        if ( leftHit && ( leftDist > iMaxDist ) )
          leftHit = false;
        if ( rightHit && ( rightDist > iMaxDist ) )
          rightHit = false;
      }

      if ( leftHit && rightHit )
      {
        if ( leftDist > rightDist )
        {
          index = rightIndex;
          Stack[topPtr++] = leftIndex;
        }
        else
        {
          index = leftIndex;
          Stack[topPtr++] = rightIndex;
        }
        continue;
      }
      else if ( leftHit )
      {
        index = leftIndex;
        continue;
      }
      else if ( rightHit )
      {
        index = rightIndex;
        continue;
      }
    }
    index = Stack[--topPtr];
  }

  return false;
}

#endif
