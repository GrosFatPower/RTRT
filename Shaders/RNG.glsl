/*
 *
 */

//RNG from code by Moroz Mykhailo (https://www.shadertoy.com/view/wltcRS)
//internal RNG state 
uvec4 g_Seed;

void InitRNG( vec2 p, int frame )
{
  g_Seed = uvec4(p, uint(frame), uint(p.x) + uint(p.y));
}

void pcg4d(inout uvec4 v)
{
  v = v * 1664525u + 1013904223u;
  v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
  v = v ^ (v >> 16u);
  v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
}

float rand()
{
  pcg4d(g_Seed); return float(g_Seed.x) / float(0xffffffffu);
}

vec3 RandomVector()
{
  return vec3(rand() * 2. - 1.,rand() * 2. - 1.,rand() * 2. - 1.);
}

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

vec3 RandomUnitVector()
{
  return normalize(vec3(rand() * 2. - 1.,rand() * 2. - 1.,rand() * 2. - 1.));
}

//vec3 RandomUnitVector()
//{
//  return normalize(RandomInUnitSphere());
//}
