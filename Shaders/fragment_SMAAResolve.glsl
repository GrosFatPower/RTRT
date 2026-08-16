#version 410 core

in vec2 fragUV;
layout(location = 0) out vec4 fragColor;

uniform sampler2D u_Input;
uniform sampler2D u_Weights;
uniform vec2 u_InvResolution;

void main()
{
  vec4 center = texture(u_Input, fragUV);
  vec4 blendWeights = texture(u_Weights, fragUV);
  vec4 left = texture(u_Input, fragUV - vec2(u_InvResolution.x, 0.0));
  vec4 right = texture(u_Input, fragUV + vec2(u_InvResolution.x, 0.0));
  vec4 top = texture(u_Input, fragUV - vec2(0.0, u_InvResolution.y));
  vec4 bottom = texture(u_Input, fragUV + vec2(0.0, u_InvResolution.y));

  // Blend only across edges found by the directional search pass.
  float totalWeight = blendWeights.x + blendWeights.y + blendWeights.z + blendWeights.w;
  fragColor = ( center + left * blendWeights.x + right * blendWeights.y + top * blendWeights.z + bottom * blendWeights.w ) / ( 1.0 + totalWeight );
}
