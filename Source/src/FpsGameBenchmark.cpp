#include "FpsGameBenchmark.h"

#include "PathUtils.h"
#include "RenderSettings.h"
#include "Scene.h"

#include <GL/glew.h>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <thread>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/utsname.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sys/utsname.h>
#endif

namespace RTRT
{

namespace
{

const Vec3 g_BenchmarkPosition(17.065f, -0.229f, -45.187f);
constexpr float g_BenchmarkYaw = -147.34f;
constexpr float g_BenchmarkPitch = 0.99f;

std::string JsonEscape( const std::string & iText )
{
  std::string result;
  result.reserve(iText.size());
  for ( char c : iText )
  {
    if ( '"' == c )
      result += "\\\"";
    else if ( '\\' == c )
      result += "\\\\";
    else if ( '\n' == c )
      result += "\\n";
    else if ( '\r' == c )
      result += "\\r";
    else if ( '\t' == c )
      result += "\\t";
    else
      result += c;
  }
  return result;
}

std::string BenchmarkTimestamp()
{
  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  std::tm localTime;
#if defined(_WIN32)
  localtime_s(&localTime, &nowTime);
#else
  localtime_r(&nowTime, &localTime);
#endif

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &localTime);
  return std::string(buffer);
}

std::string BenchmarkCpuBrand()
{
#if defined(__APPLE__)
  char buffer[256] = {};
  size_t bufferSize = sizeof(buffer);
  if ( 0 == sysctlbyname("machdep.cpu.brand_string", buffer, &bufferSize, nullptr, 0) )
    return std::string(buffer);
  bufferSize = sizeof(buffer);
  if ( 0 == sysctlbyname("hw.model", buffer, &bufferSize, nullptr, 0) )
    return std::string(buffer);
  return "unknown";
#elif defined(_WIN32)
  return "windows-cpu";
#elif defined(__linux__)
  return "linux-cpu";
#else
  return "unknown";
#endif
}

std::string BenchmarkOSName()
{
#if defined(__APPLE__) || defined(__linux__)
  struct utsname info;
  if ( 0 == uname(&info) )
    return std::string(info.sysname) + " " + info.release + " " + info.machine;
  return "unknown";
#elif defined(_WIN32)
  return "Windows";
#else
  return "unknown";
#endif
}

std::string BenchmarkCompilerName()
{
#if defined(__clang__)
  return "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__) + "." + std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
  return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
  return "MSVC " + std::to_string(_MSC_VER);
#else
  return "unknown";
#endif
}

std::string BenchmarkBuildMode()
{
#if defined(NDEBUG)
  return "Release";
#else
  return "Debug";
#endif
}

std::string BenchmarkGLString( GLenum iName )
{
  const GLubyte * value = glGetString(iName);
  return value ? reinterpret_cast<const char *>(value) : "";
}

}

// ----------------------------------------------------------------------------
// Start
// ----------------------------------------------------------------------------
void FpsGameBenchmark::Start( FpsRendererMode iRendererMode )
{
  _Running = true;
  _Completed = false;
  _Status = "Running";
  _ResultPath.clear();
  _WarmupDone = 0;
  _SamplesDone = 0;
  _RendererMode = iRendererMode;
  _CpuFrameSeconds = 0.;
  _RendererFrameSeconds = 0.;
  _CpuTotals.clear();
  _RendererTotals.clear();
}

// ----------------------------------------------------------------------------
// Cancel
// ----------------------------------------------------------------------------
void FpsGameBenchmark::Cancel( const char * iReason )
{
  _Running = false;
  _Completed = false;
  _Status = iReason ? iReason : "Cancelled";
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
void FpsGameBenchmark::Update( const std::vector<FpsCpuTiming> & iCpuTimings,
                               const Renderer & iRenderer,
                               FpsRendererMode iActiveRendererMode,
                               bool iRendererReady )
{
  if ( !_Running )
    return;

  if ( _RendererMode != iActiveRendererMode )
  {
    Cancel("Cancelled because the active renderer changed");
    return;
  }

  if ( !iRendererReady )
    return;

  if ( _WarmupDone < std::max(0, _WarmupFrames) )
  {
    ++_WarmupDone;
    return;
  }

  if ( _CpuTotals.empty() )
    _CpuTotals = iCpuTimings;

  double cpuFrameSeconds = 0.;
  for ( int i = 0; i < static_cast<int>(iCpuTimings.size()); ++i )
  {
    if ( !iCpuTimings[i]._Enabled )
      continue;

    cpuFrameSeconds += iCpuTimings[i]._Seconds;
    if ( i < static_cast<int>(_CpuTotals.size()) )
    {
      _CpuTotals[i]._Enabled = true;
      _CpuTotals[i]._Seconds += iCpuTimings[i]._Seconds;
    }
  }
  _CpuFrameSeconds += cpuFrameSeconds;

  std::vector<RenderPassTiming> renderTimings;
  if ( 0 == iRenderer.GetRenderPassTimings(renderTimings) )
  {
    if ( _RendererTotals.empty() )
    {
      _RendererTotals.reserve(renderTimings.size());
      for ( const RenderPassTiming & timing : renderTimings )
      {
        FpsGameBenchmarkPassTiming total;
        total._Name = timing._Name ? timing._Name : "";
        total._GPU = timing._GPU;
        _RendererTotals.push_back(total);
      }
    }

    double rendererFrameSeconds = 0.;
    for ( int i = 0; i < static_cast<int>(renderTimings.size()) && i < static_cast<int>(_RendererTotals.size()); ++i )
    {
      const RenderPassTiming & timing = renderTimings[i];
      if ( !timing._Enabled )
        continue;

      rendererFrameSeconds += timing._Seconds;
      _RendererTotals[i]._Enabled = true;
      _RendererTotals[i]._Seconds += timing._Seconds;
    }
    _RendererFrameSeconds += rendererFrameSeconds;
  }

  ++_SamplesDone;
  if ( _SamplesDone >= std::max(1, _SampleFrames) )
  {
    _Running = false;
    _Completed = true;
    _Status = "Completed";
  }
}

// ----------------------------------------------------------------------------
// SaveResult
// ----------------------------------------------------------------------------
bool FpsGameBenchmark::SaveResult( const FpsGameBenchmarkSaveContext & iContext )
{
  if ( !_Completed || ( _SamplesDone <= 0 ) || !iContext._Settings )
    return false;

  std::error_code ec;
  const std::string timestamp = BenchmarkTimestamp();
  const std::string rendererName = GetRendererName(_RendererMode);
  const std::string resultName = "Test6_" + rendererName + "_" + timestamp;
  const std::filesystem::path outputPath = PathUtils::GetBenchmarkPath(resultName + ".json");
  const std::filesystem::path outputDir = outputPath.parent_path();
  std::filesystem::create_directories(outputDir, ec);
  if ( ec )
    return false;

  const std::filesystem::path sceneSnapshotPath = outputDir / (resultName + ".fpsmap");
  const bool sceneSnapshotSaved = FpsGameMapLoader::Save(sceneSnapshotPath.string(), iContext._SceneSnapshot);
  std::ofstream file(outputPath);
  if ( !file.is_open() )
    return false;

  const RenderSettings & settings = *iContext._Settings;
  const int samples = std::max(1, _SamplesDone);
  file << "{\n";
  file << "  \"benchmark\": \"Test6 " << JsonEscape(rendererName) << " Renderer\",\n";
  file << "  \"timestamp\": \"" << JsonEscape(timestamp) << "\",\n";
  file << "  \"machine\": {\n";
  file << "    \"os\": \"" << JsonEscape(BenchmarkOSName()) << "\",\n";
  file << "    \"cpu\": \"" << JsonEscape(BenchmarkCpuBrand()) << "\",\n";
  file << "    \"hardware_threads\": " << std::thread::hardware_concurrency() << ",\n";
  file << "    \"compiler\": \"" << JsonEscape(BenchmarkCompilerName()) << "\",\n";
  file << "    \"build_mode\": \"" << JsonEscape(BenchmarkBuildMode()) << "\",\n";
  file << "    \"gl_vendor\": \"" << JsonEscape(BenchmarkGLString(GL_VENDOR)) << "\",\n";
  file << "    \"gl_renderer\": \"" << JsonEscape(BenchmarkGLString(GL_RENDERER)) << "\",\n";
  file << "    \"gl_version\": \"" << JsonEscape(BenchmarkGLString(GL_VERSION)) << "\"\n";
  file << "  },\n";
  file << "  \"scene\": {\n";
  file << "    \"map_path\": \"" << JsonEscape(iContext._MapPath) << "\",\n";
  file << "    \"scene_setup_snapshot\": \"" << JsonEscape(sceneSnapshotPath.string()) << "\",\n";
  file << "    \"scene_setup_snapshot_saved\": " << ( sceneSnapshotSaved ? "true" : "false" ) << ",\n";
  file << "    \"map_name\": \"" << JsonEscape(iContext._SceneSnapshot._Name) << "\",\n";
  file << "    \"environment\": \"" << JsonEscape(iContext._SceneSnapshot._Environment) << "\",\n";
  file << "    \"objects\": " << iContext._SceneSnapshot._Objects.size() << ",\n";
  file << "    \"props\": " << iContext._SceneSnapshot._Props.size() << ",\n";
  file << "    \"boids\": " << iContext._SceneSnapshot._Boids.size() << ",\n";
  file << "    \"materials\": " << iContext._SceneSnapshot._Materials.size() << ",\n";
  file << "    \"lights\": " << iContext._SceneSnapshot._Lights.size();
  if ( iContext._Scene )
  {
    file << ",\n";
    file << "    \"scene_meshes\": " << iContext._Scene -> GetNbMeshes() << ",\n";
    file << "    \"scene_mesh_instances\": " << iContext._Scene -> GetNbMeshInstances() << ",\n";
    file << "    \"scene_materials\": " << iContext._Scene -> GetNbMaterials() << "\n";
  }
  else
    file << "\n";
  file << "  },\n";
  file << "  \"camera\": {\n";
  file << "    \"position\": [" << iContext._Player._Position.x << ", " << iContext._Player._Position.y << ", " << iContext._Player._Position.z << "],\n";
  file << "    \"yaw\": " << iContext._Player._Yaw << ",\n";
  file << "    \"pitch\": " << iContext._Player._Pitch << ",\n";
  file << "    \"benchmark_position\": [" << g_BenchmarkPosition.x << ", " << g_BenchmarkPosition.y << ", " << g_BenchmarkPosition.z << "],\n";
  file << "    \"benchmark_yaw\": " << g_BenchmarkYaw << ",\n";
  file << "    \"benchmark_pitch\": " << g_BenchmarkPitch << "\n";
  file << "  },\n";
  file << "  \"render_settings\": {\n";
  file << "    \"renderer\": \"" << JsonEscape(rendererName) << "\",\n";
  file << "    \"window_resolution\": [" << settings._WindowResolution.x << ", " << settings._WindowResolution.y << "],\n";
  file << "    \"render_resolution\": [" << settings._RenderResolution.x << ", " << settings._RenderResolution.y << "],\n";
  file << "    \"render_scale\": " << settings._RenderScale << ",\n";
  file << "    \"tile_resolution\": [" << settings._TileResolution.x << ", " << settings._TileResolution.y << "],\n";
  file << "    \"tiled_rendering\": " << ( settings._TiledRendering ? "true" : "false" ) << ",\n";
  file << "    \"nb_threads\": " << settings._NbThreads << ",\n";
  file << "    \"sampling\": " << (int)settings._Sampling << ",\n";
  file << "    \"shading_type\": " << (int)settings._ShadingType << ",\n";
  file << "    \"w_buffer\": " << ( settings._WBuffer ? "true" : "false" ) << ",\n";
  file << "    \"tone_mapping\": " << ( settings._ToneMapping ? "true" : "false" ) << ",\n";
  file << "    \"gamma\": " << settings._Gamma << ",\n";
  file << "    \"exposure\": " << settings._Exposure << "\n";
  file << "  },\n";
  file << "  \"samples\": {\n";
  file << "    \"warmup_frames\": " << _WarmupFrames << ",\n";
  file << "    \"sample_frames\": " << samples << ",\n";
  file << "    \"cpu_frame_average_ms\": " << ( _CpuFrameSeconds * 1000. / samples ) << ",\n";
  file << "    \"renderer_pass_average_ms\": " << ( _RendererFrameSeconds * 1000. / samples ) << "\n";
  file << "  },\n";
  file << "  \"cpu_timings_ms\": {\n";
  bool first = true;
  for ( const FpsCpuTiming & timing : _CpuTotals )
  {
    if ( !timing._Enabled )
      continue;
    if ( !first )
      file << ",\n";
    file << "    \"" << JsonEscape(timing._Name ? timing._Name : "") << "\": " << ( timing._Seconds * 1000. / samples );
    first = false;
  }
  file << "\n";
  file << "  },\n";
  file << "  \"renderer_timings_ms\": {\n";
  first = true;
  for ( const FpsGameBenchmarkPassTiming & timing : _RendererTotals )
  {
    if ( !timing._Enabled )
      continue;
    if ( !first )
      file << ",\n";
    file << "    \"" << JsonEscape(timing._Name) << "\": " << ( timing._Seconds * 1000. / samples );
    first = false;
  }
  file << "\n";
  file << "  }\n";
  file << "}\n";

  _ResultPath = outputPath.string();
  return true;
}

// ----------------------------------------------------------------------------
// GetPosition
// ----------------------------------------------------------------------------
const Vec3 & FpsGameBenchmark::GetPosition() { return g_BenchmarkPosition; }

// ----------------------------------------------------------------------------
// GetYaw
// ----------------------------------------------------------------------------
float FpsGameBenchmark::GetYaw() { return g_BenchmarkYaw; }

// ----------------------------------------------------------------------------
// GetPitch
// ----------------------------------------------------------------------------
float FpsGameBenchmark::GetPitch() { return g_BenchmarkPitch; }

// ----------------------------------------------------------------------------
// GetRendererName
// ----------------------------------------------------------------------------
const char * FpsGameBenchmark::GetRendererName( FpsRendererMode iRendererMode )
{
  switch ( iRendererMode )
  {
  case FpsRendererMode::Deferred:        return "Deferred";
  case FpsRendererMode::Software:        return "Software";
  case FpsRendererMode::PhotoPathTracer: return "PhotoPathTracer";
  }
  return "Unknown";
}

}
