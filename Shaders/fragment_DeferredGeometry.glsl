#version 410 core

// Inputs from vertex shader (expected to provide world-space position, normal and uv)
in vec3 fragWorldPos;
in vec3 fragNormal;
in vec2 fragUV;
flat in int v_MaterialID; // optional: present if vertex shader provides it

// G-buffer MRT outputs
layout(location = 0) out vec4 gAlbedo;   // RGB: albedo, A: unused / opacity
layout(location = 1) out vec4 gNormal;   // RGB: normal encoded in 0..1, A: unused
layout(location = 2) out vec4 gPosition; // RGB: world position, A: unused

// Optional fallback uniform (simple default albedo if no material sampling)
uniform vec3 u_DefaultAlbedo = vec3(0.8, 0.8, 0.8);

void main()
{
  // Normalized world-space normal
  vec3 N = normalize(fragNormal);

  // Simple albedo output: shader can be extended to sample material/texture arrays.
  vec3 albedo = u_DefaultAlbedo;

  // Pack outputs
  gAlbedo = vec4(albedo, 1.0);

  // Store normal in [0,1] range so it fits into unsigned textures easily
  gNormal = vec4(N * 0.5 + 0.5, 1.0);

  // Store world position directly
  gPosition = vec4(fragWorldPos, 1.0);
}
