#version 410 core

// Per-vertex attributes (must match VBO layout: location 0 pos, 1 normal, 2 uv)
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;

// Per-instance uniforms (set from C++)
uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Proj;
uniform int  u_MaterialID;
uniform vec3 u_CameraPos;
uniform vec3 u_CameraUp;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraForward;
uniform vec2 u_ZNearFar;

// Outputs to fragment shader
out vec3 fragWorldPos;
out vec3 fragNormal;
out vec2 fragUV;
flat out int v_MaterialID;

void main()
{
  // World-space position
  vec4 worldPos = u_Model * vec4(a_Position, 1.0);
  fragWorldPos = worldPos.xyz;

  // Normal transform: use inverse-transpose of model matrix
  mat3 normalMat = mat3(transpose(inverse(u_Model)));
  fragNormal = normalize(normalMat * a_Normal);

  fragUV = a_UV;
  v_MaterialID = u_MaterialID; // forwarded as flat int

  // Clip-space position
  gl_Position = u_Proj * u_View * worldPos;
}
