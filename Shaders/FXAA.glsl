/*
 * Adpated from Enterprise PBR Sample Renderer
 * https://github.com/DassaultSystemes-Technology/dspbr-pt
 */

#ifndef _FXAA_GLSL_
#define _FXAA_GLSL_

#ifndef FXAA_REDUCE_MIN
#define FXAA_REDUCE_MIN (1.0 / 128.0)
#endif
#ifndef FXAA_REDUCE_MUL
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#endif
#ifndef FXAA_SPAN_MAX
#define FXAA_SPAN_MAX 8.0
#endif

// ----------------------------------------------------------------------------
// ApplyFXAA
// https://github.com/mattdesl/glsl-fxaa
// ----------------------------------------------------------------------------
vec4 ApplyFXAA( in sampler2D iTex, in vec2 iFragCoord, in vec2 iInvRes )
{
  vec4 color;

  vec3 rgbNW = texture( iTex, ( iFragCoord + vec2( -1.0, -1.0 ) ) * iInvRes ).xyz;
  vec3 rgbNE = texture( iTex, ( iFragCoord + vec2( 1.0, -1.0 ) ) * iInvRes ).xyz;
  vec3 rgbSW = texture( iTex, ( iFragCoord + vec2( -1.0, 1.0 ) ) * iInvRes ).xyz;
  vec3 rgbSE = texture( iTex, ( iFragCoord + vec2( 1.0, 1.0 ) ) * iInvRes ).xyz;
  vec4 texColor = texture( iTex, vec2( iFragCoord * iInvRes ) );
  
  vec3 rgbM = texColor.xyz;
  vec3 luma = vec3( 0.299, 0.587, 0.114 );
  float lumaNW = dot( rgbNW, luma );
  float lumaNE = dot( rgbNE, luma );
  float lumaSW = dot( rgbSW, luma );
  float lumaSE = dot( rgbSE, luma );
  float lumaM = dot( rgbM, luma );
  float lumaMin = min( lumaM, min( min( lumaNW, lumaNE ), min( lumaSW, lumaSE ) ) );
  float lumaMax = max( lumaM, max( max( lumaNW, lumaNE ), max( lumaSW, lumaSE ) ) );
  
  mediump vec2 dir;
  dir.x = -( ( lumaNW + lumaNE ) - ( lumaSW + lumaSE ) );
  dir.y = ( ( lumaNW + lumaSW ) - ( lumaNE + lumaSE ) );
  
  float dirReduce = max( ( lumaNW + lumaNE + lumaSW + lumaSE ) * ( 0.25 * FXAA_REDUCE_MUL ), FXAA_REDUCE_MIN );
  
  float rcpDirMin = 1.0 / ( min( abs( dir.x ), abs( dir.y ) ) + dirReduce );
  dir = min( vec2( FXAA_SPAN_MAX, FXAA_SPAN_MAX ), max( vec2( -FXAA_SPAN_MAX, -FXAA_SPAN_MAX ), dir * rcpDirMin ) ) * iInvRes;
  
  vec3 rgbA = 0.5 * ( texture( iTex, iFragCoord * iInvRes + dir * ( 1.0 / 3.0 - 0.5 ) ).xyz +
                     texture( iTex, iFragCoord * iInvRes + dir * ( 2.0 / 3.0 - 0.5 ) ).xyz );
  vec3 rgbB = rgbA * 0.5 + 0.25 * ( texture( iTex, iFragCoord * iInvRes + dir * -0.5 ).xyz +
                                   texture( iTex, iFragCoord * iInvRes + dir * 0.5 ).xyz );
  
  float lumaB = dot( rgbB, luma );
  if ( ( lumaB < lumaMin ) || ( lumaB > lumaMax ) )
    color = vec4( rgbA, texColor.a );
  else
    color = vec4( rgbB, texColor.a );

  return color;
}

#endif
