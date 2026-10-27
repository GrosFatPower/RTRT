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
  for ( int y = -2; y <= 2; ++y )
  {
    for ( int x = -2; x <= 2; ++x )
    {
      vec2 offset = vec2(x, y) * texel;
      vec2 sampleUV = fragUV + offset;
      if ( any(lessThan(sampleUV, vec2(0.0))) || any(greaterThan(sampleUV, vec2(1.0))) )
        continue;

      float sampleDepth = texture(u_GDepth, sampleUV).r;
      if ( sampleDepth >= 1.0 )
        continue;

      vec3 sampleNormal = normalize(texture(u_GNormal, sampleUV).xyz * 2.0 - 1.0);
      float sampleAO = texture(u_SSAOInput, sampleUV).r;
      float depthWeight = 1.0 / (1.0 + abs(sampleDepth - centerDepth) * 100.0);
      float normalWeight = pow(max(dot(centerNormal, sampleNormal), 0.0), 16.0);
      float spatialWeight = exp(-float(x * x + y * y) / 4.5);
      float weight = depthWeight * normalWeight * spatialWeight;

      sum += sampleAO * weight;
      weightSum += weight;
    }
  }

  float ao = ( weightSum > 0.0 ) ? ( sum / weightSum ) : centerAO;
  fragColor = vec4(ao, 0.0, 0.0, 1.0);
}
