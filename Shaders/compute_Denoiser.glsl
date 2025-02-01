/*
 * Designed with ChatGPT
 */

#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 0) uniform image2D u_InputImage;
layout(rgba32f, binding = 1) uniform image2D u_OutputImage;

uniform float u_Threshold = 0.05; // Noise threshold (similar to wavelet thresholding)

// ----------------------------------------------------------------------------
// Soft thresholding function (similar to wavelet denoising)
// ----------------------------------------------------------------------------
vec4 SoftThreshold( in vec4 iValue, in float iThreshold )
{
  return sign(iValue) * max(abs(iValue) - iThreshold, 0.0);
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
void main()
{
  ivec2 uv = ivec2( gl_GlobalInvocationID.xy );
  ivec2 size = imageSize( u_InputImage );

  if ( uv.x >= size.x || uv.y >= size.y )
    return; // Éviter les accès hors limites

  // Étape 1 : Lecture des 4 pixels voisins pour la transformée en ondelettes Haar
  vec4 A = imageLoad( u_InputImage, uv );
  vec4 B = imageLoad( u_InputImage, uv + ivec2( 1, 0 ) );
  vec4 C = imageLoad( u_InputImage, uv + ivec2( 0, 1 ) );
  vec4 D = imageLoad( u_InputImage, uv + ivec2( 1, 1 ) );

  // Étape 2 : Transformée de Haar (décomposition)
  vec4 avg = ( A + B + C + D ) * 0.25; // Approximation basse fréquence
  vec4 horiz = ( A + B - C - D ) * 0.5;  // Détails horizontaux
  vec4 vert = ( A - B + C - D ) * 0.5;  // Détails verticaux
  vec4 diag = ( A - B - C + D ) * 0.5;  // Détails diagonaux

  // Étape 3 : Filtrage du bruit (Seuillage)
  horiz = SoftThreshold( horiz, u_Threshold );
  vert = SoftThreshold( vert, u_Threshold );
  diag = SoftThreshold( diag, u_Threshold );

  // Étape 4 : Reconstruction de l’image filtrée
  vec4 denoisedA = avg + ( horiz + vert + diag ) * 0.5;
  vec4 denoisedB = avg + ( horiz - vert - diag ) * 0.5;
  vec4 denoisedC = avg + ( -horiz + vert - diag ) * 0.5;
  vec4 denoisedD = avg + ( -horiz - vert + diag ) * 0.5;

  // Écriture des pixels dans l’image de sortie
  imageStore( u_OutputImage, uv, denoisedA );
  imageStore( u_OutputImage, uv + ivec2( 1, 0 ), denoisedB );
  imageStore( u_OutputImage, uv + ivec2( 0, 1 ), denoisedC );
  imageStore( u_OutputImage, uv + ivec2( 1, 1 ), denoisedD );
}
