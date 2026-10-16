/*
 *
 */

#ifndef _RNG_GLSL_
#define _RNG_GLSL_

// ----------------------------------------------------------------------------
// InitRNG
// RNG from code by Moroz Mykhailo (https://www.shadertoy.com/view/wltcRS)
// ----------------------------------------------------------------------------
//internal RNG state 
uvec4 g_Seed;
uvec4 g_PixelSeed;
uint  g_SampleIndex;
uint  g_Dimension;

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
// RadicalInverse
// ----------------------------------------------------------------------------
float RadicalInverse( in uint iBase, in uint iIndex )
{
  float invBase = 1.f / float(iBase);
  float invBaseN = 1.f;
  uint reversedDigits = 0u;
  uint n = iIndex;
  while ( n > 0u )
  {
    uint next = n / iBase;
    uint digit = n - next * iBase;
    reversedDigits = reversedDigits * iBase + digit;
    invBaseN *= invBase;
    n = next;
  }
  return min(float(reversedDigits) * invBaseN, 0.99999994f);
}

// ----------------------------------------------------------------------------
// Sobol2Index
// Exact 2nd-dimension Sobol generator matrix over F_2 (Sierpinski butterfly)
// ----------------------------------------------------------------------------
uint Sobol2Index( in uint iIndex )
{
  uint n = iIndex;
  n ^= ( n >> 1u ) & 0x55555555u;
  n ^= ( n >> 2u ) & 0x33333333u;
  n ^= ( n >> 4u ) & 0x0f0f0f0fu;
  n ^= ( n >> 8u ) & 0x00ff00ffu;
  n ^= ( n >> 16u );
  return n;
}

// ----------------------------------------------------------------------------
// HashU32
// ----------------------------------------------------------------------------
uint HashU32( in uint iX )
{
  uint x = iX;
  x ^= x >> 16u; x *= 0x7feb352du;
  x ^= x >> 15u; x *= 0x846ca68bu;
  x ^= x >> 16u;
  return x;
}

// ----------------------------------------------------------------------------
// HashCombine
// ----------------------------------------------------------------------------
uint HashCombine( in uint iSeed, in uint iValue )
{
  return HashU32(iSeed ^ ( iValue + 0x9e3779b9u + ( iSeed << 6u ) + ( iSeed >> 2u ) ));
}

// ----------------------------------------------------------------------------
// NestedUniformScramble
// Hash-based Owen scrambling (Burley 2020, "Practical Hash-based Owen Scrambling")
// ----------------------------------------------------------------------------
uint NestedUniformScramble( in uint iX, in uint iSeed )
{
  uint x = bitfieldReverse(iX);
  x += iSeed;
  x ^= x * 0x6c50b47cu;
  x ^= x * 0xb82f1e52u;
  x ^= x * 0xc7afe638u;
  x ^= x * 0x8d22f6e6u;
  return bitfieldReverse(x);
}

// ----------------------------------------------------------------------------
// ToUnitFloat
// 24-bit mantissa conversion, strictly in [0, 1)
// ----------------------------------------------------------------------------
float ToUnitFloat( in uint iBits )
{
  return float(iBits >> 8u) * ( 1.f / 16777216.f );
}

vec2 g_PairCache;

// ----------------------------------------------------------------------------
// SobolOwenPair
// Owen-scrambled 2D Sobol point. Each pair of dimensions is shuffled and
// scrambled with its own seed so pairs are statistically independent.
// ----------------------------------------------------------------------------
vec2 SobolOwenPair( in uint iIndex, in uint iPairSeed )
{
  uint shuffled = NestedUniformScramble(iIndex, HashCombine(iPairSeed, 0u));
  uint s0 = NestedUniformScramble(bitfieldReverse(shuffled), HashCombine(iPairSeed, 1u));
  uint s1 = NestedUniformScramble(bitfieldReverse(Sobol2Index(shuffled)), HashCombine(iPairSeed, 2u));
  return vec2(ToUnitFloat(s0), ToUnitFloat(s1));
}

// ----------------------------------------------------------------------------
// InitRNG
// iSample is the global sample index of this pixel (frame * spp + sample)
// ----------------------------------------------------------------------------
void InitRNG( in vec2 p, in int iSample )
{
  g_PixelSeed = uvec4(uint(p.x), uint(p.y), uint(p.x) * 1973u + uint(p.y) * 9277u + 1u, uint(p.x) ^ (uint(p.y) << 16u));
  pcg4d(g_PixelSeed);
  g_SampleIndex = uint(max(iSample, 0));
  g_Dimension = 0u;
  g_PairCache = vec2(0.f);

  g_Seed = uvec4(p, uint(iSample), uint(p.x) + uint(p.y));
}

// ----------------------------------------------------------------------------
// rand
// Every pair of consecutive dimensions is an independent Owen-scrambled
// Sobol (0,2)-sequence: stratified over frames, unbiased per pixel.
// ----------------------------------------------------------------------------
float rand()
{
  uint dim = g_Dimension++;
  if ( 0u == ( dim & 1u ) )
  {
    g_PairCache = SobolOwenPair(g_SampleIndex, HashCombine(g_PixelSeed.x, dim >> 1u));
    return g_PairCache.x;
  }
  return g_PairCache.y;
}

// ----------------------------------------------------------------------------
// RandomVec2
// ----------------------------------------------------------------------------
vec2 RandomVec2()
{
  return vec2(rand() * 2. - 1.,rand() * 2. - 1.);
}

// ----------------------------------------------------------------------------
// RandomVec3
// ----------------------------------------------------------------------------
vec3 RandomVec3()
{
  return vec3(rand() * 2. - 1.,rand() * 2. - 1.,rand() * 2. - 1.);
}

// ----------------------------------------------------------------------------
// RandomInUnitDisk
// ----------------------------------------------------------------------------
vec2 RandomInUnitDisk()
{
  vec2 v;

  while ( true )
  {
    v = RandomVec2();
    if ( dot(v, v) < 1.f )
      break;
  }

  return v;
}

// ----------------------------------------------------------------------------
// RandomInUnitSphere
// ----------------------------------------------------------------------------
vec3 RandomInUnitSphere()
{
  vec3 v;

  while ( true )
  {
    v = RandomVec3();
    if ( dot(v, v) < 1.f )
      break;
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

#endif
