#version 410 core

#include Lights.glsl
#include Sampling.glsl

in vec2 fragUV; // from fullscreen quad vertex shader
out vec4 fragColor;

// G-buffer samplers (bound by the C++ code to texture units 0..3)
uniform sampler2D u_GAlbedo;
uniform sampler2D u_GNormal;
uniform sampler2D u_GPosition;
uniform sampler2D u_GDepth;

// Simple lighting parameters (tweak in C++ or expose more uniforms)
uniform vec3 u_Ambient = vec3(0.001, 0.001, 0.001);
uniform vec3 u_BackgroundColor = vec3(1.0, 1.0, 1.0);
uniform vec2 u_Resolution;
uniform Camera u_Camera;

// EnvMap
uniform int       u_EnableBackground  = 0;
uniform int       u_EnableEnvMap      = 0;
uniform float     u_EnvMapRotation    = 0.f;
uniform vec2      u_EnvMapRes;
uniform sampler2D u_EnvMap;

void main()
{
  // Read G-buffer
  vec3 albedo  = texture(u_GAlbedo, fragUV).rgb;
  vec3 N       = normalize(texture(u_GNormal, fragUV).xyz * 2.0 - 1.0); // [0,1] -> [-1,1]
  vec3 pos     = texture(u_GPosition, fragUV).xyz;
  float depth  = texture(u_GDepth, fragUV).x;

  if ( depth >= 1.0 ) // far plane, no geometry
  {
    if ( u_EnableEnvMap > 0 )
    {
      // Compute view direction
      vec2 centeredUV = fragUV * 2.0 - 1.0;

      float scale = tan(u_Camera._FOV * .5);
      centeredUV.x *= scale;
      centeredUV.y *= ( u_Resolution.y / u_Resolution.x ) * scale;

      vec3 V = normalize(u_Camera._Right * centeredUV.x + u_Camera._Up * centeredUV.y + u_Camera._Forward);

      fragColor = vec4(SampleSkybox(V, u_EnvMap, u_EnvMapRotation), 1.);
    }
    else if ( u_EnableBackground > 0 )
    {
	  fragColor = vec4(u_BackgroundColor, 1.);
    }
    else
      fragColor = vec4(0., 0., 0., 1.);
	return;
  }

  // Shading
  vec4 alpha = vec4(0.);
  for ( int i = 0; i < u_NbLights; ++i )
  {
    float ambientStrength = .1;
    float diffuse = 0.;
    float specular = 0.;

    vec3 L;
    if ( u_Lights[i]._Type == DISTANT_LIGHT )
	  L = u_Lights[i]._Pos;
	else
	  L = normalize(u_Lights[i]._Pos - pos);

    diffuse = max(0., dot(N, L));

    vec3 V = normalize(u_Camera._Pos - pos);
    vec3 H = reflect(-L, N);

	float specPow = 32.0;
    float specularStrength = 0.5f;
    specular = pow(max(dot(V, H), 0.), specPow) * specularStrength;

    alpha += min(diffuse + ambientStrength + specular, 1.) * vec4(normalize(u_Lights[i]._Emission), 1.);
  }

  fragColor = min(vec4(albedo, 1.) * alpha, vec4(1.));
}
