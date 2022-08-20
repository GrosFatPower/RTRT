#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform sampler2D u_Texture;
uniform vec3      u_Resolution;
uniform bool      u_DirectOutputPass;
uniform float     u_Time;
uniform float     u_TimeDelta;

// Created by inigo quilez - iq/2013
//   https://www.youtube.com/c/InigoQuilez
//   https://iquilezles.org
// I share this piece (art and code) here in Shadertoy and through its Public API, only for educational purposes. 
// You cannot use, sell, share or host this piece or modifications of it as part of your own commercial or non-commercial product, website or project.
// You can share a link to it or an unmodified screenshot of it provided you attribute "by Inigo Quilez, @iquilezles and iquilezles.org". 
// If you are a teacher, lecturer, educator or similar and these conditions are too restrictive for your needs, please contact me and we'll work it out.


// See here for more information on smooth iteration count:
//
// https://iquilezles.org/articles/msetsmooth


// increase this if you have a very fast GPU
#define AA 2

float mandelbrot( in vec2 c )
{
  #if 1
  {
      float c2 = dot(c, c);
      // skip computation inside M1 - https://iquilezles.org/articles/mset1bulb
      if( 256.0*c2*c2 - 96.0*c2 + 32.0*c.x - 3.0 < 0.0 ) return 0.0;
      // skip computation inside M2 - https://iquilezles.org/articles/mset2bulb
      if( 16.0*(c2+2.0*c.x+1.0) - 1.0 < 0.0 ) return 0.0;
  }
  #endif
  
  const float B = 256.0;
  float l = 0.0;
  vec2 z  = vec2(0.0);
  for( int i=0; i<512; i++ )
  {
    z = vec2( z.x*z.x - z.y*z.y, 2.0*z.x*z.y ) + c;
    if( dot(z,z)>(B*B) ) break;
    l += 1.0;
  }
  
  if( l>511.0 ) return 0.0;
  
  // ------------------------------------------------------
  // smooth interation count
  //float sl = l - log(log(length(z))/log(B))/log(2.0);
  
  // equivalent optimized smooth interation count
  float sl = l - log2(log2(dot(z,z))) + 4.0;
  
  float al = smoothstep( -0.1, 0.0, sin(0.5*6.2831*u_Time ) );
  l = mix( l, sl, al );
  
  return l;
}

void renderMandelbrot( out vec4 oFragColor, in vec2 iFragCoord )
{
  vec3 col = vec3(0.0);
  
#if AA>1
  for( int m=0; m<AA; m++ )
  for( int n=0; n<AA; n++ )
  {
      vec2 p = (-u_Resolution.xy + 2.0*(iFragCoord.xy+vec2(float(m),float(n))/float(AA)))/u_Resolution.y;
      float w = float(AA*m+n);
      float time = u_Time + 0.5*(1.0/24.0)*w/float(AA*AA);
#else    
      vec2 p = (-u_Resolution.xy + 2.0*iFragCoord.xy)/u_Resolution.y;
      float time = iTime;
#endif
    
      float zoo = 0.62 + 0.38*cos(.07*time);
      float coa = cos( 0.15*(1.0-zoo)*time );
      float sia = sin( 0.15*(1.0-zoo)*time );
      zoo = pow( zoo,8.0);
      vec2 xy = vec2( p.x*coa-p.y*sia, p.x*sia+p.y*coa);
      vec2 c = vec2(-.745,.186) + xy*zoo;
      
      float l = mandelbrot(c);
      
      col += 0.5 + 0.5*cos( 3.0 + l*0.15 + vec3(0.0,0.6,1.0));
#if AA>1
  }
  col /= float(AA*AA);
#endif

  oFragColor = vec4( col, 1.0 );
}

void RenderToTexture()
{
  renderMandelbrot(fragColor, gl_FragCoord .xy);
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
