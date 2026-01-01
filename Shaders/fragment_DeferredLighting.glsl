#version 410 core

in vec2 fragUV; // from fullscreen quad vertex shader
out vec4 fragColor;

// G-buffer samplers (bound by the C++ code to texture units 0..3)
uniform sampler2D u_GAlbedo;
uniform sampler2D u_GNormal;
uniform sampler2D u_GPosition;
uniform sampler2D u_GDepth;

// Simple lighting parameters (tweak in C++ or expose more uniforms)
uniform vec3 u_LightDir    = normalize(vec3(-0.5, -1.0, -0.3)); // direction TO light
uniform vec3 u_LightColor  = vec3(1.0, 1.0, 1.0);
uniform vec3 u_Ambient     = vec3(0.08, 0.08, 0.08);
uniform vec3 u_CameraPos;

void main()
{
  // Read G-buffer
  vec3 albedo = texture(u_GAlbedo, fragUV).rgb;
  vec3 encN   = texture(u_GNormal, fragUV).rgb;
  vec3 pos    = texture(u_GPosition, fragUV).rgb;

  // Reconstruct normal from encoded [0,1] -> [-1,1]
  vec3 N = normalize(encN * 2.0 - 1.0);

  // Simple Blinn-Phong with a single distant light
  vec3 L = normalize(u_LightDir); // direction to light (assumed normalized)
  float NdotL = max(dot(N, L), 0.0);

  // Diffuse
  vec3 diffuse = albedo * NdotL;

  // Specular (simple power)
  vec3 V = normalize(u_CameraPos - pos);
  vec3 H = normalize(V + L);
  float specPow = 32.0;
  float spec = pow(max(dot(N, H), 0.0), specPow);

  vec3 final = u_Ambient * albedo + u_LightColor * (diffuse + vec3(spec) * 0.2);

  fragColor = vec4(final, 1.0);
}
