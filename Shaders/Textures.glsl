/*
 *
 */

#ifndef _TEXTURES_GLSL_
#define _TEXTURES_GLSL_

uniform isamplerBuffer u_TexIndTexture;

#ifndef USE_TEXTURE_BUCKETS
#define USE_TEXTURE_BUCKETS 1
#endif

#if USE_TEXTURE_BUCKETS
uniform sampler2DArray u_TexArrayTexture0;
uniform sampler2DArray u_TexArrayTexture1;
uniform sampler2DArray u_TexArrayTexture2;
uniform sampler2DArray u_TexArrayTexture3;
uniform sampler2DArray u_TexArrayTexture4;
#else
uniform sampler2DArray u_TexArrayTexture0;
uniform int u_TextureArraySize;
#endif

ivec4 GetMaterialTextureMapping( int iTextureID )
{
  return texelFetch(u_TexIndTexture, iTextureID);
}

vec4 SampleMaterialTexture( in ivec4 iMapping, in vec2 iUV )
{
#if USE_TEXTURE_BUCKETS
  int bucketSize = 256;
  if ( iMapping.x == 1 )
    bucketSize = 512;
  else if ( iMapping.x == 2 )
    bucketSize = 1024;
  else if ( iMapping.x == 3 )
    bucketSize = 2048;
  else if ( iMapping.x == 4 )
    bucketSize = 4096;

  // Sample only the populated part of a padded bucket layer.
  vec2 contentSize = max(vec2(iMapping.zw), vec2(1.0));
  vec2 sampleUV = ( vec2(0.5) + fract(iUV) * max(contentSize - vec2(1.0), vec2(0.0)) ) / float(bucketSize);
  if ( iMapping.x == 0 )
    return texture(u_TexArrayTexture0, vec3(sampleUV, float(iMapping.y)));
  if ( iMapping.x == 1 )
    return texture(u_TexArrayTexture1, vec3(sampleUV, float(iMapping.y)));
  if ( iMapping.x == 2 )
    return texture(u_TexArrayTexture2, vec3(sampleUV, float(iMapping.y)));
  if ( iMapping.x == 3 )
    return texture(u_TexArrayTexture3, vec3(sampleUV, float(iMapping.y)));
  if ( iMapping.x == 4 )
    return texture(u_TexArrayTexture4, vec3(sampleUV, float(iMapping.y)));
#else
  vec2 contentSize = max(vec2(iMapping.zw), vec2(1.0));
  vec2 sampleUV = ( vec2(0.5) + fract(iUV) * max(contentSize - vec2(1.0), vec2(0.0)) ) / float(u_TextureArraySize);
  return texture(u_TexArrayTexture0, vec3(sampleUV, float(iMapping.y)));
#endif

  return vec4(1.0);
}

#endif
