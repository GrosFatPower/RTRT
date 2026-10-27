#version 410 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_GNormal;
uniform sampler2D u_GPosition;
uniform sampler2D u_GDepth;
uniform sampler2D u_SSAONoise;

uniform mat4 u_View;
uniform mat4 u_Proj;
uniform vec2 u_Resolution;
uniform int u_EnableSSAO = 1;
uniform float u_SSAORadius = 0.5;
uniform float u_SSAOBias = 0.025;
uniform float u_SSAOMaxDistance = 80.0;
uniform float u_SSAOFadeDistance = 20.0;
uniform int u_KernelSize = 16;
uniform vec3 u_KernelSamples[32];

void main()
{
  float depth = texture(u_GDepth, fragUV).r;
  if ( ( u_EnableSSAO == 0 ) || ( depth >= 1.0 ) )
  {
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
    return;
  }

  vec3 normalWS = normalize(texture(u_GNormal, fragUV).xyz * 2.0 - 1.0);
  vec3 fragPosWS = texture(u_GPosition, fragUV).xyz;
  vec3 fragPosVS = (u_View * vec4(fragPosWS, 1.0)).xyz;
  float viewDist = -fragPosVS.z;
  if ( viewDist >= u_SSAOMaxDistance )
  {
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
    return;
  }
  float distFade = clamp((u_SSAOMaxDistance - viewDist) / max(u_SSAOFadeDistance, 0.001), 0.0, 1.0);
  bool isOrtho = (u_Proj[3][3] == 1.0);
  float projRadiusPixels = (u_SSAORadius * u_Proj[1][1] / (isOrtho ? 1.0 : max(viewDist, 0.001))) * (u_Resolution.y * 0.5);
  float radiusFade = smoothstep(1.0, 2.5, projRadiusPixels);

  // View matrix is orthonormal (rotation + translation), so inverse-transpose == rotation for normals here.
  vec3 normalVS = normalize(mat3(u_View) * normalWS);
  float adaptiveBias = max(u_SSAOBias, viewDist * 0.001);

  vec2 noiseScale = u_Resolution / 4.0;
  vec3 randomVec = normalize(texture(u_SSAONoise, fragUV * noiseScale).xyz * 2.0 - 1.0);
  vec3 tangent = randomVec - normalVS * dot(randomVec, normalVS);
  if ( length(tangent) < 0.001 )
  {
    vec3 up = abs(normalVS.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    tangent = normalize(cross(up, normalVS));
  }
  else
    tangent = normalize(tangent);
  vec3 bitangent = normalize(cross(normalVS, tangent));
  mat3 TBN = mat3(tangent, bitangent, normalVS);

  int kernelSize = clamp(u_KernelSize, 1, 32);
  float occlusion = 0.0;

  for ( int i = 0; i < kernelSize; ++i )
  {
    vec3 samplePosVS = fragPosVS + TBN * u_KernelSamples[i] * u_SSAORadius;
    vec4 offset = u_Proj * vec4(samplePosVS, 1.0);
    if ( offset.w <= 0.0 )
      continue;

    offset.xyz /= offset.w;
    vec2 sampleUV = offset.xy * 0.5 + 0.5;
    if ( ( sampleUV.x <= 0.0 ) || ( sampleUV.x >= 1.0 ) || ( sampleUV.y <= 0.0 ) || ( sampleUV.y >= 1.0 ) )
      continue;

    float sampleDepth = texture(u_GDepth, sampleUV).r;
    if ( sampleDepth >= 1.0 )
      continue;

    vec3 sampleWorldPos = texture(u_GPosition, sampleUV).xyz;
    vec3 samplePosFetchedVS = (u_View * vec4(sampleWorldPos, 1.0)).xyz;

    float sampleDist = length(samplePosFetchedVS - fragPosVS);
    float rangeCheck = smoothstep(1.0, 0.0, sampleDist / u_SSAORadius);
    occlusion += ( samplePosFetchedVS.z >= samplePosVS.z + adaptiveBias ? 1.0 : 0.0 ) * rangeCheck;
  }

  float ao = 1.0 - ( occlusion / float(kernelSize) ) * distFade * radiusFade;
  fragColor = vec4(clamp(ao, 0.0, 1.0), 0.0, 0.0, 1.0);
}
