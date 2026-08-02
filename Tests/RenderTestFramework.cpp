#include "RenderTestFramework.h"

#include "RenderSettings.h"
#include "Scene.h"

#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace RTRT
{

namespace Tests
{

// ----------------------------------------------------------------------------
// RenderTestCase
// ----------------------------------------------------------------------------
void RenderTestCase::ApplySettings( RenderSettings & ioSettings ) const
{
  ioSettings._WindowResolution = Vec2i(_Width, _Height);
  ioSettings._RenderResolution = Vec2i(_Width, _Height);
  ioSettings._RenderScale = 100;
  ioSettings._FXAA = false;
  ioSettings._ToneMapping = false;

  if ( RendererBackend::DeferredRenderer == _Backend )
  {
    ioSettings._SpecularIBL = true;
    ioSettings._SSR = true;
  }
  else if ( RendererBackend::PathTracer == _Backend )
  {
    ioSettings._Accumulate = true;
    ioSettings._AutoScale = true;
    ioSettings._Denoise = false;
    ioSettings._NbSamplesPerPixel = 1;
    ioSettings._Bounces = 4;
  }
}

// ----------------------------------------------------------------------------
// ApplyScene
// ----------------------------------------------------------------------------
void RenderTestCase::ApplyScene( Scene & ioScene ) const
{
  if ( _OverrideCamera )
  {
    ioScene.GetCamera().Initialize(_CameraPosition, _CameraPivot, _CameraFOV);
    ioScene.GetCamera().SetZNearFar(_CameraNear, _CameraFar);
  }
}

// ----------------------------------------------------------------------------
// WritePFM
// ----------------------------------------------------------------------------
bool WritePFM( const std::filesystem::path & iPath, const RenderImage & iImage )
{
  if ( !iImage.IsValid() )
    return false;

  std::error_code error;
  std::filesystem::create_directories(iPath.parent_path(), error);

  std::ofstream output(iPath, std::ios::binary);
  if ( !output )
    return false;

  output << "PF\n" << iImage._Width << " " << iImage._Height << "\n-1.0\n";
  // PFM stores scanlines from the bottom of the image upward.
  for ( int y = iImage._Height - 1; y >= 0; --y )
  {
    for ( int x = 0; x < iImage._Width; ++x )
    {
      const size_t index = ((size_t)y * (size_t)iImage._Width + (size_t)x) * 4u;
      output.write(reinterpret_cast<const char *>(&iImage._Pixels[index]), sizeof(float) * 3);
    }
  }

  return output.good();
}

// ----------------------------------------------------------------------------
// ReadPFM
// ----------------------------------------------------------------------------
bool ReadPFM( const std::filesystem::path & iPath, RenderImage & oImage )
{
  std::ifstream input(iPath, std::ios::binary);
  std::string header;
  int width = 0;
  int height = 0;
  float scale = 0.f;

  if ( !input || !(input >> header) || ( "PF" != header ) || !(input >> width >> height >> scale ) || ( width <= 0 ) || ( height <= 0 ) || ( scale >= 0.f ) )
    return false;

  input.get();
  oImage._Width = width;
  oImage._Height = height;
  oImage._Pixels.assign((size_t)width * (size_t)height * 4u, 1.f);

  for ( int y = height - 1; y >= 0; --y )
  {
    for ( int x = 0; x < width; ++x )
    {
      const size_t index = ((size_t)y * (size_t)width + (size_t)x) * 4u;
      input.read(reinterpret_cast<char *>(&oImage._Pixels[index]), sizeof(float) * 3);
      if ( !input )
        return false;
    }
  }

  return true;
}

// ----------------------------------------------------------------------------
// WriteDiagnosticPNG
// ----------------------------------------------------------------------------
bool WriteDiagnosticPNG( const std::filesystem::path & iPath, const RenderImage & iImage, float iScale )
{
  if ( !iImage.IsValid() )
    return false;

  std::error_code error;
  std::filesystem::create_directories(iPath.parent_path(), error);

  std::vector<unsigned char> pixels((size_t)iImage._Width * (size_t)iImage._Height * 4u);
  for ( size_t i = 0; i < (size_t)iImage._Width * (size_t)iImage._Height; ++i )
  {
    for ( int channel = 0; channel < 3; ++channel )
    {
      const float value = std::max(0.f, iImage._Pixels[i * 4u + (size_t)channel] * iScale);
      const float mapped = value / ( 1.f + value );
      pixels[i * 4u + (size_t)channel] = (unsigned char)(std::pow(mapped, 1.f / 2.2f) * 255.f + .5f);
    }
    pixels[i * 4u + 3u] = 255;
  }

  return ( 0 != stbi_write_png(iPath.string().c_str(), iImage._Width, iImage._Height, 4, pixels.data(), iImage._Width * 4) );
}

// ----------------------------------------------------------------------------
// WriteDiffPNG
// ----------------------------------------------------------------------------
bool WriteDiffPNG( const std::filesystem::path & iPath, const RenderImage & iActual, const RenderImage & iExpected )
{
  if ( !iActual.IsValid() || !iExpected.IsValid() || ( iActual._Width != iExpected._Width ) || ( iActual._Height != iExpected._Height ) )
    return false;

  RenderImage diff;
  diff._Width = iActual._Width;
  diff._Height = iActual._Height;
  diff._Pixels.assign(iActual._Pixels.size(), 1.f);

  for ( size_t i = 0; i < (size_t)diff._Width * (size_t)diff._Height; ++i )
  {
    const float error = std::max(std::abs(iActual._Pixels[i * 4u] - iExpected._Pixels[i * 4u]),
                                 std::max(std::abs(iActual._Pixels[i * 4u + 1u] - iExpected._Pixels[i * 4u + 1u]),
                                          std::abs(iActual._Pixels[i * 4u + 2u] - iExpected._Pixels[i * 4u + 2u])));
    diff._Pixels[i * 4u] = error * 16.f;
    diff._Pixels[i * 4u + 1u] = 0.f;
    diff._Pixels[i * 4u + 2u] = 0.f;
  }

  return WriteDiagnosticPNG(iPath, diff, 1.f);
}

// ----------------------------------------------------------------------------
// CompareImages
// ----------------------------------------------------------------------------
bool CompareImages( const RenderImage & iActual, const RenderImage & iExpected, float iPixelErrorThreshold, ImageMetrics & oMetrics )
{
  oMetrics = ImageMetrics();
  if ( !iActual.IsValid() || !iExpected.IsValid() || ( iActual._Width != iExpected._Width ) || ( iActual._Height != iExpected._Height ) )
    return false;

  const size_t pixelCount = (size_t)iActual._Width * (size_t)iActual._Height;
  float totalError = 0.f;
  for ( size_t i = 0; i < pixelCount; ++i )
  {
    float pixelError = 0.f;
    for ( int channel = 0; channel < 3; ++channel )
    {
      const float error = std::abs(iActual._Pixels[i * 4u + (size_t)channel] - iExpected._Pixels[i * 4u + (size_t)channel]);
      totalError += error;
      pixelError = std::max(pixelError, error);
      oMetrics._MaxAbsoluteError = std::max(oMetrics._MaxAbsoluteError, error);
    }

    if ( pixelError > iPixelErrorThreshold )
      ++oMetrics._MismatchCount;
  }

  oMetrics._MeanAbsoluteError = totalError / (float)(pixelCount * 3u);
  oMetrics._MismatchRatio = (float)oMetrics._MismatchCount / (float)pixelCount;
  return true;
}

// ----------------------------------------------------------------------------
// MatchesThresholds
// ----------------------------------------------------------------------------
bool MatchesThresholds( const ImageMetrics & iMetrics, const RenderTestCase & iTestCase )
{
  return ( iMetrics._MeanAbsoluteError <= iTestCase._MeanAbsoluteErrorThreshold ) &&
         ( iMetrics._MaxAbsoluteError <= iTestCase._MaxAbsoluteErrorThreshold ) &&
         ( iMetrics._MismatchRatio <= iTestCase._MismatchRatioThreshold );
}

// ----------------------------------------------------------------------------
// GetRenderTestCases
// ----------------------------------------------------------------------------
std::vector<RenderTestCase> GetRenderTestCases()
{
  return
  {
    { "software_textured_box", "TexturedBox.scene", RendererBackend::SoftwareRasterizer, 1280, 720, 1, 0.000001f, 0.00001f, 0.00001f, 0.f, "Tests/Baselines/software_textured_box.pfm" },
    { "software_cornell", "cornell_box.scene", RendererBackend::SoftwareRasterizer, 720, 720, 1, 0.000001f, 0.00001f, 0.00001f, 0.f, "Tests/Baselines/software_cornell.pfm", true, Vec3(.276f, .265f, -.75f), Vec3(.276f, .265f, .1f), 40.f, .5f },
    { "deferred_ibl_ssr", "tungsten-material-testball.scene", RendererBackend::DeferredRenderer, 1280, 720, 3, 0.003f, 0.08f, 0.02f, 0.005f, "Tests/Baselines/deferred_ibl_ssr.pfm" },
    { "deferred_cornell", "cornell_box.scene", RendererBackend::DeferredRenderer, 720, 720, 3, 0.003f, 0.08f, 0.02f, 0.005f, "Tests/Baselines/deferred_cornell.pfm", true, Vec3(.276f, .265f, -.75f), Vec3(.276f, .265f, .1f), 40.f, .5f },
    { "pathtracer_cornell", "cornell_box.scene", RendererBackend::PathTracer, 720, 720, 6, 0.02f, 0.25f, 0.08f, 0.02f, "Tests/Baselines/pathtracer_cornell.pfm", true, Vec3(.276f, .265f, -.75f), Vec3(.276f, .265f, .1f), 40.f, .5f }
  };
}

// ----------------------------------------------------------------------------
// RunUnitTests
// ----------------------------------------------------------------------------
int RunUnitTests( const std::filesystem::path & iArtifactsDir )
{
  RenderImage image;
  image._Width = 2;
  image._Height = 1;
  image._Pixels = { 1.f, .5f, .25f, 1.f, .25f, .5f, 1.f, 1.f };

  const std::filesystem::path imagePath = iArtifactsDir / "unit_image.pfm";
  RenderImage loaded;
  if ( !WritePFM(imagePath, image) || !ReadPFM(imagePath, loaded) || ( loaded._Pixels != image._Pixels ) )
  {
    std::cerr << "Unit test failed: PFM read/write." << std::endl;
    return 1;
  }

  RenderImage changed = image;
  changed._Pixels[0] = .5f;
  ImageMetrics metrics;
  if ( !CompareImages(changed, image, .1f, metrics) || ( metrics._MismatchCount != 1u ) || ( metrics._MaxAbsoluteError != .5f ) )
  {
    std::cerr << "Unit test failed: image comparison." << std::endl;
    return 1;
  }

  if ( !WriteDiffPNG(iArtifactsDir / "unit_diff.png", changed, image) )
  {
    std::cerr << "Unit test failed: diagnostic image output." << std::endl;
    return 1;
  }

  std::cout << "Unit tests passed." << std::endl;
  return 0;
}

}

}
