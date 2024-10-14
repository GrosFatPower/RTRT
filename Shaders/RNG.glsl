/*
 *
 */

// ----------------------------------------------------------------------------
// InitRNG
// RNG from code by Moroz Mykhailo (https://www.shadertoy.com/view/wltcRS)
// ----------------------------------------------------------------------------
//internal RNG state 
uvec4 g_Seed;
void InitRNG( in vec2 p, in int frame )
{
  g_Seed = uvec4(p, uint(frame), uint(p.x) + uint(p.y));
}

// ----------------------------------------------------------------------------
// pcg4d
// ----------------------------------------------------------------------------
void pcg4d( inout uvec4 v )
{
  v = v * 1664525u + 1013904223u;
  v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
  v = v ^ (v >> 16u);
  v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
}

// ----------------------------------------------------------------------------
// rand
// ----------------------------------------------------------------------------
float rand()
{
  pcg4d(g_Seed);
  return float(g_Seed.x) / float(0xffffffffu);
}

// ----------------------------------------------------------------------------
// RandomVector
// ----------------------------------------------------------------------------
vec3 RandomVector()
{
  return vec3(rand() * 2. - 1.,rand() * 2. - 1.,rand() * 2. - 1.);
}

// ----------------------------------------------------------------------------
// RandomInUnitSphere
// ----------------------------------------------------------------------------
vec3 RandomInUnitSphere()
{
  vec3 v;

  while ( true )
  {
    v = RandomVector();
    if ( dot(v, v) < 1.f )
      break;
  }

  return v;
}

// ----------------------------------------------------------------------------
// SampleHemisphere
// ----------------------------------------------------------------------------
vec3 SampleHemisphere( in vec3 iNormal )
{
  vec3 v;

  while ( true )
  {
    v = RandomVector();

    float dotProd = dot(v, v);
    if ( ( dotProd < 1.f ) && ( dotProd != 0 ) )
    {
      v /= dotProd;
      if ( dot(iNormal,v) < 0.f )
        v *= -1.f;
      break;
    }
  }

  return v;
}

// ----------------------------------------------------------------------------
// RandomUnitVector
// ----------------------------------------------------------------------------
vec3 RandomUnitVector()
{
  return normalize(vec3(rand() * 2. - 1.,rand() * 2. - 1.,rand() * 2. - 1.));
}

//vec3 RandomUnitVector()
//{
//  return normalize(RandomInUnitSphere());
//}
