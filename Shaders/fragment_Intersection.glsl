/*
 *
 */

 // https://raytracing.github.io/books/RayTracingInOneWeekend.html#overview
// Ray direction should be normalized 
bool SphereIntersection( vec4 iSphere, Ray iRay, out float oHitDistance )
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

vec3 GetLightDirSample( vec3 iSamplePos, vec3 iLightPos, float iLightRadius )
{
  vec3 lightSample = iLightPos + RandomUnitVector() * iLightRadius;

  return lightSample - iSamplePos;
}
