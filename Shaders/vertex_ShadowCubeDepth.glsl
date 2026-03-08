#version 410 core

layout(location = 0) in vec3 a_Position;

uniform mat4  u_Model;
uniform mat4  u_LightViewProj;

out vec3 fragWorldPos;

void main()
{
  vec4 worldPos = u_Model * vec4(a_Position, 1.0);
  fragWorldPos = worldPos.xyz;
  gl_Position = u_LightViewProj * worldPos;
}
