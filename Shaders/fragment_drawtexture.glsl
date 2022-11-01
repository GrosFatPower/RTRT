#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform sampler2D u_Texture;
uniform vec3      u_Resolution;
uniform float     u_Time;
uniform float     u_TimeDelta;

const vec2 s = vec2(1, 1.7320508);

void RenderToTexture()
{
  float t = u_Time * 1.25;

  vec2 textUV = { fragUV.x, 1.0 - fragUV.y };
  vec2 screenUV = (textUV - 0.5) * 2.0;
  float vignette = abs(sin(3.0*t+6.0*pow(distance(screenUV,vec2(0.0,0.0)), .65)));

  vec2 uv = ( ( gl_FragCoord.xy * max(u_Resolution.x, u_Resolution.y) ) / min(u_Resolution.x, u_Resolution.y) ) / max(u_Resolution.x, u_Resolution.y);

  float aspect = u_Resolution.y / u_Resolution.x;
  const float tiling = 2.0;
  vec2 distUV = aspect - (screenUV / aspect);
  float distort = length(distUV);
  vec2 uv2 = uv * tiling * aspect - distUV * vignette * .5;
  uv += uv * 16.0 * aspect - distUV * vignette * .5;
  uv = (uv + uv2) * 0.5;

  vec4 hexCenter = round(vec4(uv, uv - vec2(.5, 1.)) / s.xyxy);
  vec4 offset = vec4(uv - hexCenter.xy * s, uv - (hexCenter.zw + .5) * s);
  vec2 dist = dot(offset.xy, offset.xy) < dot(offset.zw, offset.zw) ? offset.xy :offset.zw;

  vec3 col = vec3(0.0);
  col += smoothstep(abs(0.80), -1.0,length(dist)*vignette * (-(vignette)*0.4));
    
  col *= abs((length(screenUV)));
  //col *= abs(sin(t*0.25));
  col *= col*col+col;

  col *= vec3(0.3,0.9,0.8);
    
  col = clamp(col + col * vignette, 0.0, 1.0);
    
  vec3 scr = 0.5*col +  texture(u_Texture, (screenUV*0.5+0.5)-(0.5*(((1.0-dist)*vignette*2.0)*col.x*col.x*length(screenUV*.2)*0.8)-distUV*vignette*col.x*0.01)).xyz;
  col = mix(scr, col, 0.5*col);

  // Output to screen
  fragColor = vec4(col,1.0);
}

void main()
{
  RenderToTexture();
}
