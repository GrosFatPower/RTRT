#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform sampler2D u_Texture;
uniform vec3      u_Resolution;
uniform vec4      u_Mouse;
uniform bool      u_DirectOutputPass;
uniform float     u_Time;
uniform float     u_TimeDelta;
uniform int       u_Frame;
uniform float     u_FrameRate;

//Make random values more random
float randSeed = 0.;

#define PI 3.14159265359

#define FLOORI(x) float(int(floor(x)))

float sinNorm(float x)
{
  return sin(x)*0.5+0.5;
}

float line(in int lineWidth, in vec2 pos, in vec2 point, in vec3 iResolution)
{
  float normalizedLineRadius = (float(lineWidth) / iResolution.y) / 2.;
  float edgeWidth = 1. / iResolution.y;
  if(normalizedLineRadius<1./iResolution.x)
    return 0.;
  return smoothstep(pos.y-normalizedLineRadius,pos.y-edgeWidth,point.y-normalizedLineRadius+edgeWidth) * 
    (1.-smoothstep(pos.y+normalizedLineRadius-edgeWidth, pos.y+normalizedLineRadius+edgeWidth, point.y));
}

float smoothVal(in float x, in float max)
{
  return clamp(smoothstep(0.0,1.0,x/max)*(1.-smoothstep(0.0,1.0,x/max))*4.,0.,1.);
}

//f(x) = amplitude*sinNormalized(frequency*x-offsetX)+d
float normSinFunct(in float amplitude, in float freq, in float offsetX, in float offsetY, in float x)
{
  return amplitude*sinNorm(freq*x-offsetX)+offsetY;
}

float rand(float seed)
{
  return fract(sin(dot(vec2(seed, seed / PI) ,vec2(12.9898,78.233))) * 43758.5453);   
}

vec3 rgb2hsv(vec3 c)
{
  vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
  vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
  vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
  
  float d = q.x - min(q.w, q.y);
  float e = 1.0e-10;
  return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

/* old function
float smoothRand(float interval, float seed) {
    float next = rand(1.+floor(iTime/interval)+seed);
    float curr = rand(floor(iTime/interval)+seed);
    randSeed++;
    return mix(curr, next, fract(iTime/interval));
}
*/

float smoothRand(float interval, float seed)
{
  float next = rand(.000001*FLOORI(1000000.*(1.+FLOORI(u_Time/interval)+seed+randSeed)));
  float curr = rand(.000001*FLOORI(1000000.*(FLOORI(u_Time/interval)+seed+randSeed)));
  randSeed++;
  return mix(curr, next, fract(u_Time/interval));
}

float f(vec2 point)
{
  return sin(point.x*2.+u_Time*1.275)+point.y;   
}

vec2 grad( in vec2 x )
{
  vec2 h = vec2( 0.01, 0.0 );
  return vec2( f(x+h.xy) - f(x-h.xy),
               f(x+h.yx) - f(x-h.yx) )/(2.0*h.x);
}

//https://iquilezles.org/articles/distance
float color( in vec2 point, in int lineWidth, in vec3 iResolution)
{
  float v = f( point );
  vec2  g = grad( point );
  float de = abs(v)/length(g);
  float normalizedLineRadius = (float(max(5,lineWidth)) / iResolution.y) / 2.;
  float edgeWidth = 1. / iResolution.y;
  if(normalizedLineRadius<1./iResolution.x)
    return 0.;
  float eps = max(1./iResolution.x, 1./iResolution.y)*normalizedLineRadius;
  return 1.-clamp(smoothstep( 0., normalizedLineRadius, de ), 0., 1.);
}

void RenderScene( out vec4 oFragColor, in vec2 iFragCoord )
{    
  //init
  vec2 uv = iFragCoord/u_Resolution.xy;
  vec2 point = vec2( iFragCoord.xy - 0.5*u_Resolution.xy );
  point = 2.0 * point.xy / u_Resolution.y;
  float x,y,z = 0.;
 
  //Limit "frames" for uniterrupted animation
  float frameRatio = floor(u_FrameRate/ 30.);
  if(mod(float(u_Frame), frameRatio) != 0.)
  {
    float decay = sinNorm(u_Time*0.789)*0.5+0.1;
    oFragColor += texture(u_ScreenTexture, uv) * (1.-(1./(decay*u_FrameRate)));
    
    //Clamp to prevent overexposure
    oFragColor.r = clamp(oFragColor.r, 0., 1.);
    oFragColor.g = clamp(oFragColor.g, 0., 1.);
    oFragColor.b = clamp(oFragColor.b, 0., 1.);
    return;   
  }
    
  //Scaling
  const float maxZoom = 1.5;
  const float minZoom = 0.5;
  const float changeInterval = 2.;
  float nextZ = rand(1.+floor(u_Time/changeInterval));
  float currZ = rand(floor(u_Time/changeInterval));
  z=minZoom+(maxZoom-minZoom)*mix(currZ, nextZ, fract(u_Time/changeInterval));
  point/=vec2(z);
  
  //Rotation
  float rot = smoothRand(0.5,354.856)*PI;
  point=vec2(cos(rot)*point.x+sin(rot)*point.y, -sin(rot)*point.x+cos(rot)*point.y);
  
  //Translation
  point.x+=smoothRand(1.,842.546)*2.-1.;
  //No need to translate y here, bc y is set by the function in "f(point)" and the rotation.
  
  //Line
  const float minLength = 0.25;
  const float maxLength=0.5;
  float lineLength=minLength+smoothRand(4.,0.846)*(maxLength-minLength)+minLength;
  float linePoint = (point.x+lineLength/2.) / lineLength;
  //				clamp - make sure the value is in bounds
  //						  smoothVal - make the line thinner at the ends
  int lineWidth = int(clamp(floor(smoothVal(linePoint*100., 100.)*u_Resolution.x*0.025*z), 2., floor(u_Resolution.x*0.025*z)));//max(3,int((u_Resolution.x*0.1)*  pow((point.x*(1./lineLength)),3.)  ));
  if(point.x >= -lineLength / 2. && point.x <= lineLength / 2.)
  { //Only show a small segment
    oFragColor+=color(vec2(point.x,point.y), lineWidth, u_Resolution);//line(lineWidth, vec2(x,y), point, u_Resolution);
  }
  /*if(point.x-x<0.005) {
      oFragColor = vec4(1.);
  }
  if(point.y-y<0.005) {
  //oFragColor = vec4(1.);
  }*/
  //Color
  oFragColor.rgb*=hsv2rgb(vec3(fract(u_Time/7.), sinNorm(u_Time*rand(135.54))*0.4+0.6,1.));
  oFragColor.rgb+=pow((oFragColor.r+oFragColor.g+oFragColor.b)/3.+0.25,3.)-pow(0.25,3.);
  
  //Fade
  float decay = sinNorm(u_Time*0.789)*0.5+0.25;
  oFragColor += texture(u_ScreenTexture, uv) * (1.-(1./(decay*u_FrameRate)));
  
  //Clamp to prevent overexposure
  oFragColor.r = clamp(oFragColor.r, 0., 2.);
  oFragColor.g = clamp(oFragColor.g, 0., 2.);
  oFragColor.b = clamp(oFragColor.b, 0., 2.);
  oFragColor.a = 0.;
}

void RenderToTexture()
{
  RenderScene(fragColor, gl_FragCoord .xy);
}

void RenderImage()
{
  fragColor = texture(u_ScreenTexture, fragUV);
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
