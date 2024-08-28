/*
 *
 */

// ----------------------------------------------------------------------------
// SphereIntersection
// https://raytracing.github.io/books/RayTracingInOneWeekend.html#overview
// Ray direction should be normalized 
// ----------------------------------------------------------------------------
bool SphereIntersection( in vec4 iSphere, in Ray iRay, out float oHitDistance )
{
  vec3 oc = iRay._Orig - iSphere.xyz;

  float halfB = dot( oc, iRay._Dir );
  float c = dot( oc, oc ) - iSphere.w * iSphere.w;
  float discriminant = halfB * halfB - c;

  if ( discriminant < 0.f )
    return false;

  oHitDistance = -halfB - sqrt(discriminant);
  if (  oHitDistance < 0.f ) 
    oHitDistance = -halfB + sqrt(discriminant);

  return true;
}

// ----------------------------------------------------------------------------
// PlaneIntersection
// ----------------------------------------------------------------------------
bool PlaneIntersection( in vec3 iOrig, in vec3 iNormal, in Ray iRay, out float oHitDistance )
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

// ----------------------------------------------------------------------------
// QuadIntersection
// ----------------------------------------------------------------------------
bool QuadIntersection( in vec3 iOrig, in vec3 iDirU, in vec3 iDirV, in Ray iRay, out float oHitDistance )
{
  vec3 n = normalize(cross(iDirU, iDirV));

  if ( PlaneIntersection(iOrig, n, iRay, oHitDistance) )
  {
    vec3 hitPoint = iRay._Orig + oHitDistance * iRay._Dir;
    vec3 p = hitPoint - iOrig;

    float lengthU = length(iDirU);
    float u = dot(p, iDirU);
    if ( ( u >= 0.f ) && ( u <= lengthU * lengthU ) )
    {
      float lengthV = length(iDirV);
      float v = dot(p, iDirV);
      if ( ( v >= 0.f ) && ( v <= lengthV * lengthV ) )
        return true;
    }
  }

  return false;
}

// ----------------------------------------------------------------------------
// BoxIntersection
// Intersection method from Real-Time Rendering and Essential Mathematics for Games (p. 581)
// ----------------------------------------------------------------------------
bool BoxIntersection( in vec3 iLow, in vec3 iHigh, in mat4 iTransform, in Ray iRay, out float oHitDistance )
{
  float tMin = -INFINITY;
  float tMax = INFINITY;

  vec3 boxOrig = vec3(iTransform[3][0], iTransform[3][1], iTransform[3][2]);

  vec3 delta = boxOrig - iRay._Orig;

  for ( int i = 0; i < 3; ++i )
  {
    // Test intersection with the 2 planes perpendicular to the OBB's axis
    vec3 axis = vec3(iTransform[i][0],iTransform[i][1], iTransform[i][2]);
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

// ----------------------------------------------------------------------------
// BoxIntersection
// Journal of Computer Graphics Techniques Vol. 7, No. 3, 2018
// A Ray-Box Intersection Algorithm and Efficient Dynamic Voxel Rendering
// ----------------------------------------------------------------------------
bool BoxIntersection( in vec3 iLow, in vec3 iHigh, in Ray iRay, out float oHitDistance )
{
  vec3 invRaydir = 1.f / iRay._Dir; // Safe ???

  vec3 t0 = (iLow  - iRay._Orig) * invRaydir;
  vec3 t1 = (iHigh - iRay._Orig) * invRaydir;

  vec3 tmin = min(t0,t1);
  vec3 tmax = max(t0,t1);

  oHitDistance = max(max(tmin.x, tmin.y), tmin.z);

  return oHitDistance <= min(min(tmax.x, tmax.y), tmax.z);
}

// ----------------------------------------------------------------------------
// BoxNormal
// Adapted from https://gist.github.com/Shtille/1f98c649abeeb7a18c5a56696546d3cf
// ----------------------------------------------------------------------------
vec3 BoxNormal( in vec3 iLow, in vec3 iHigh, in mat4 iTransform, in vec3 iHitPoint )
{
  // Resolution in local space
  mat4 invTransfo = inverse(iTransform);
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

// ----------------------------------------------------------------------------
// TriangleIntersection
// https://www.shadertoy.com/view/MlGcDz
// ----------------------------------------------------------------------------
bool TriangleIntersection( in Ray iRay, in vec3 iV0, in vec3 iV1, in vec3 iV2, out float oHitDistance, out vec2 oUV )
{
  vec3 v0v1 = iV1 - iV0;
  vec3 v0v2 = iV2 - iV0;
  vec3 rov0 = iRay._Orig - iV0;

  vec3  n = cross( v0v1, v0v2 );
  float dirDotN = dot( iRay._Dir, n );
  if ( abs(dirDotN) < EPSILON )
    return false;
  float invDirDotN = 1.f / dirDotN;

  oHitDistance = dot( -n, rov0 ) * invDirDotN;
  if ( oHitDistance < 0.f )
    return false;

  vec3  q = cross( rov0, iRay._Dir );
  oUV.x = dot( -q, v0v2 ) * invDirDotN;
  oUV.y = dot(  q, v0v1 ) * invDirDotN;

  if( ( oUV.x < 0.f ) || ( oUV.y < 0.f) || ( oUV.x + oUV.y ) > 1.f )
    return false;

  return true;
}

// ----------------------------------------------------------------------------
// TriangleIntersection2
// https://www.mathematik.uni-marburg.de/~thormae/lectures/graphics2/graphics_2_2_eng_web.html#1
// ----------------------------------------------------------------------------
bool TriangleIntersection2( in Ray iRay, in vec3 iV0, in vec3 iV1, in vec3 iV2, out float oHitDistance, out vec2 oUV )
{ 
  vec3 i = iV1 - iV0;
  vec3 j = iV2 - iV0;
  vec3 k = iRay._Orig - iV0;
  //vec3 r = iRay._Dir;
  
  // (t, u, v) = (1 / (r x j) * i) ((k x i) * j, (r x j) *k, (k x i) * r)
  vec3 rxj = cross(iRay._Dir, j);
  float rxji = dot(rxj, i);
  
  // denominator close to zero?
  if ( abs(rxji) < EPSILON )
    return false;
  
  float f = 1.0f / rxji;
  
  // compute oUV.x=u
  oUV.x = dot(rxj, k) * f;
  if ( oUV.x < 0.f || oUV.x > 1.f )
  return false;
  
  // compute v
  vec3 kxi = cross(k, i);
  oUV.y =  dot(kxi, iRay._Dir) * f;
  if ( oUV.y < 0.f || oUV.y > 1.f )
    return false;
  if( oUV.x + oUV.y > 1.0 )
    return false;
  
  // compute t
  oHitDistance =  dot(kxi, j) * f;
  if ( oHitDistance < 0.f )
    return false;

  return true;
}

// ----------------------------------------------------------------------------
// TriangleTangents
// ----------------------------------------------------------------------------
void TriangleTangents( in vec3 iV0, in vec3 iV1, in vec3 iV2, in vec2 iUV0, in vec2 iUV1, in vec2 iUV2, out vec3 oTangent, out vec3 oBitangent )
{
  vec3 deltaPos1 = iV1 - iV0;
  vec3 deltaPos2 = iV2 - iV0;
  vec2 deltaUV1  = iUV1 - iUV0;
  vec2 deltaUV2  = iUV2 - iUV0;

  float invdet = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);

  oTangent   = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * invdet;
  oBitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * invdet;
}
