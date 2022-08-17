#version 430 core

in vec2 fragUV;
out vec4 fragColor;

void main() {
  fragColor = vec4(fragUV.x, fragUV.y, 0.0, 1.0);
}
