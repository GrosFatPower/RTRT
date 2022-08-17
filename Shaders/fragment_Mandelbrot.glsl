#version 430 core

in vec2  i_fragUV;
out vec4 o_fragColor;

uniform sampler2D u_ScreenTexture;
uniform sampler2D u_Texture;
uniform vec3      u_Resolution;
uniform bool      u_DirectOutputPass;
uniform float     u_Time;
//uniform float     u_TimeDelta;

#define COLORSCHEME 1

#if HW_PERFORMANCE==1
#define AA 2
#else
#define AA 1
#endif

// Color schemes
#if COLORSCHEME==1
#define INVERTED_GRADIENT
#define MAXITER_POT 300
#define MAXITER_NORMAL 500
#else
#define MAXITER_POT 180
#define MAXITER_NORMAL 300
#endif

// Interation end conditions
#define ER_POT 100000.0
#define ER_NORMAL 100.0

// Constants
#define M_PI 3.1415926535897932384626433832795

// Number of points
#define NUMBER_OF_POINTS 8

// Coordinates with awesome places and zoom values
const vec3 coordinates[NUMBER_OF_POINTS] = vec3[NUMBER_OF_POINTS](
  vec3(-0.774693, 0.1242263647, 14.0),
  vec3(-0.58013, 0.48874, 14.0),
  vec3(-1.77, 0.0, 5.0),
  vec3(-0.744166858, 0.13150536, 13.0),
  vec3(0.41646, -0.210156433, 16.0),
  vec3(-0.7455, 0.1126, 10.0),
  vec3(-1.1604872, 0.2706806, 12.0),
  vec3(-0.735805, 0.196726496, 15.0)
);
const float centerDuration = 31.0;
const float rotationDuration = 53.0;
const vec2 defaultCenter = vec2(-0.6, 0.0);

#if COLORSCHEME==1
const vec4 insideColor = vec4(0.0, 0.0, 0.0, 1.0);
#else
const vec4 insideColor = vec4(0.1, 0.12, 0.15, 1.0);
#endif

float time = 0.0;
int centerIndex = 0;
vec2 currentCenter;
float currentZoom;

vec3 palette(in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d)
{
  return a + b*cos(2.0*M_PI*(c*t+d));
}

vec3 awesomePalette(in float t)
{
  return palette(t, vec3(0.5,0.5,0.5), vec3(0.5,0.5,0.5), vec3(1.0,1.0,1.0), vec3(0.0,0.1,0.2));
}

vec3 rainbow(in float t)
{
  return palette(t, vec3(0.5,0.5,0.5), vec3(0.5,0.5,0.5), vec3(1.0,1.0,1.0), vec3(0.0,0.33,0.67));
}

// Complex multiplication
vec2 cmul(vec2 a, vec2 b)
{
  return vec2(a.x * b.x - a.y * b.y, a.x * b.y + b.x * a.y);
}

// Complex c^2
vec2 cpow2(in vec2 c)
{
  return vec2(c.x * c.x - c.y * c.y, 2.0 * c.x * c.y);
}

// Complex division
vec2 cdiv(in vec2 a, in vec2 b)
{
  return vec2(((a.x*b.x+a.y*b.y)/(b.x*b.x+b.y*b.y)),((a.y*b.x-a.x*b.y)/(b.x*b.x+b.y*b.y)));
}

// Get rotation matrix
mat2 rotate(float theta)
{
  float s = sin(theta);
  float c = cos(theta);
  return mat2(c, -s, s, c);
}

// Potential formula
float potential(in vec2 c)
{
  vec2 z = vec2(0.0, 0.0); // z0
  int iter = 0;
  
  for (iter = 0; iter < MAXITER_POT; ++iter)
  {
    z = cpow2(z) + c; // z_n+1 = z_n^2 + c
    float absZ = length(z); // |z|
    if (absZ > ER_POT)
    {
      return abs(log(log2(absZ)) - (float(iter) + 1.0) * log(2.0));
    }
  }
  
  return -1.0;
}

// Reflection formula
float reflection(in vec2 c)
{
  vec2 z = vec2(0.0, 0.0); // z0
  vec2 dc = vec2(0.0, 0.0); // Derivate of c

  const float h2 = 1.5; // Height of light
  vec2 angle = normalize(vec2(-1.0, 1.0)) * rotate(time / rotationDuration); // Light always from top left

  for (int i = 0; i < MAXITER_NORMAL; i++)
  {
    dc = 2.0 * cmul(dc, z) + vec2(1.0, 0.0);
    z = cpow2(z) + c;

    if (length(z) > ER_NORMAL)
    { // Outside lighting calculation formula
      vec2 slope = normalize(cdiv(z, dc));
      float reflection = dot(slope, angle) + h2;
      reflection = reflection / (1.0 + h2); // Lower value to max 1.0
      if (reflection < 0.0)
      {
        reflection = 0.0;
      }
      return reflection;
    }
  }

  return -1.0;
}

void renderMandelbrot(out vec4 fragColor, in vec2 fragCoord)
{
  // Coordinates [-1, 1]
  vec2 uv = (fragCoord.xy - u_Resolution.xy * 0.5) / min(u_Resolution.x, u_Resolution.y) * 2.0;

  // Mix between base poistion and target position
  float mixFactor = 1.0 - (0.5 + 0.5 * cos(time / centerDuration * 2.0 * M_PI));

  // Zoom and position calculation
  float zoom = exp2(-currentZoom * mixFactor);
  float maxZoom = exp2(-currentZoom);
  vec2 c = mix(currentCenter, defaultCenter, zoom / (1.0 - maxZoom) - maxZoom) + uv * zoom * rotate(time / rotationDuration);

  float pot = potential(c);
  float ref = reflection(c);

#ifdef INVERTED_GRADIENT
  float intensity = 1.0 - sqrt(fract(pot));
  intensity = mix(intensity, ref, 0.5);
  // intensity = 0.8 * (intensity * ref) + 0.2; // Alternative shadows
#else
  float intensity = 0.7 * (fract(pot) * ref) + 0.3;
#endif

#if COLORSCHEME==1
  vec3 color = awesomePalette(time / 50.0 + pot / 40.0);
  if (ref < 0.0) { // Inner color
    fragColor = insideColor;
  }
  else { // Outer color
    //fragColor = vec4(color * intensity, 1.0);
    //fragColor = mix(fragColor, vec4(1.0), intensity * 0.3 + clamp(ref - 0.5, 0.0, 1.0) * pow((1.0 - fract(pot)), 30.0));
    fragColor = vec4(
      color * intensity + // Base color
      vec3(intensity) * 0.3 + // Matte white
      clamp(ref - 0.5, 0.0, 1.0) * pow((1.0 - fract(pot)), 30.0), // Specular
    1.0);
    fragColor = clamp(fragColor, 0.0, 1.0);
  }
#else
  vec3 color = rainbow(pot / 20.0);
  if (pot < 0.0) { // Inner color
    color = insideColor.rgb * min((ref + 0.5), 1.0);
  }
  else { // Outer color
    color = color * intensity;
  }
  fragColor = vec4(color, 1.0);
#endif
}

void RenderImage()
{
  o_fragColor = texture(u_ScreenTexture, i_fragUV);
}

void RenderToTexture()
{
  time = u_Time + centerDuration / 2.0 - 7.0; // Start with zoom
  centerIndex = int(time / centerDuration) % NUMBER_OF_POINTS; // Seleect current target
  currentCenter = coordinates[centerIndex].xy;
  currentZoom = coordinates[centerIndex].z;

  o_fragColor = vec4(0.0);

  // Antialiasing
  const float fraction = 1.0 / float(AA);
  const float fraction2 = fraction / float(AA);
  for (int i = 0; i < AA; i++)
  {
    for (int j = 0; j < AA; j++)
    {
      vec4 color = vec4(0.0);
      vec2 shift = vec2(
        float(i) * fraction + float(AA - j - 1) * fraction2,
        float(j) * fraction + float(i) * fraction2
      );
      renderMandelbrot(color, i_fragUV + shift);
      o_fragColor += clamp(color, 0.0, 1.0);
    }
  }

  o_fragColor = o_fragColor / float(AA * AA);
}

void main()
{
  if ( u_DirectOutputPass )
  {
    RenderImage();
  }
  else
  {
    RenderToTexture();
  }
}
