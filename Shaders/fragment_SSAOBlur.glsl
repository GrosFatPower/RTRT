#version 410 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_SSAOInput;
uniform sampler2D u_GDepth;
uniform sampler2D u_GNormal;
uniform vec2 u_Resolution;
uniform int u_EnableBlur = 1;

void main()
{
  float centerDepth = texture(u_GDepth, fragUV).r;
  if ( centerDepth >= 1.0 )
  {
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
    return;
  }

  float centerAO = texture(u_SSAOInput, fragUV).r;
  if ( u_EnableBlur == 0 )
  {
    fragColor = vec4(centerAO, 0.0, 0.0, 1.0);
    return;
  }

  vec3 centerNormal = normalize(texture(u_GNormal, fragUV).xyz * 2.0 - 1.0);
  vec2 texel = 1.0 / u_Resolution;

  float sum = 0.0;
  float weightSum = 0.0;
  for ( int y = -1; y <= 1; ++y )
  {
    for ( int x = -1; x <= 1; ++x )
    {
      vec2 offset = vec2(x, y) * texel;
      vec2 sampleUV = fragUV + offset;
      float sampleDepth = texture(u_GDepth, sampleUV).r;
      if ( sampleDepth >= 1.0 )
        continue;

      vec3 sampleNormal = normalize(texture(u_GNormal, sampleUV).xyz * 2.0 - 1.0);
      float sampleAO = texture(u_SSAOInput, sampleUV).r;
      float depthWeight = 1.0 / (1.0 + abs(sampleDepth - centerDepth) * 200.0);
      float normalWeight = pow(max(dot(centerNormal, sampleNormal), 0.0), 16.0);
      float spatialWeight = 1.0 / (1.0 + float(x * x + y * y));
      float weight = depthWeight * normalWeight * spatialWeight;

      sum += sampleAO * weight;
      weightSum += weight;
    }
  }

  float ao = ( weightSum > 0.0 ) ? ( sum / weightSum ) : centerAO;
  fragColor = vec4(ao, 0.0, 0.0, 1.0);
}
