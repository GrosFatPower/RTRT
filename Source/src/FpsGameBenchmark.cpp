#include "FpsGameBenchmark.h"

#include "PathUtils.h"
#include "RenderSettings.h"
#include "Scene.h"
#include "SoftwareRasterizer.h"

#include <GL/glew.h>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cmath>
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

Vec3 g_BenchmarkPosition(17.065f, -0.229f, -45.187f);
float g_BenchmarkYaw = -147.34f;
float g_BenchmarkPitch = 0.99f;

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

void WriteDistribution( std::ofstream & ioFile, const FpsGameBenchmarkDistribution & iDistribution, const char * iIndent )
{
  ioFile << iIndent << "\"mean_ms\": " << iDistribution._Mean * 1000. << ",\n";
  ioFile << iIndent << "\"median_ms\": " << iDistribution._Median * 1000. << ",\n";
  ioFile << iIndent << "\"minimum_ms\": " << iDistribution._Minimum * 1000. << ",\n";
  ioFile << iIndent << "\"p95_ms\": " << iDistribution._P95 * 1000. << ",\n";
  ioFile << iIndent << "\"standard_deviation_ms\": " << iDistribution._StandardDeviation * 1000. << "\n";
}

void WriteSamples( std::ofstream & ioFile, const std::vector<double> & iSamples )
{
  ioFile << "[";
  for ( size_t i = 0; i < iSamples.size(); ++i )
  {
    if ( i )
      ioFile << ", ";
    ioFile << iSamples[i] * 1000.;
  }
  ioFile << "]";
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
  _CurrentRepetition = 0;
  _RendererMode = iRendererMode;
  _CpuFrameSeconds = 0.;
  _RendererFrameSeconds = 0.;
  _CpuTotals.clear();
  _RendererTotals.clear();
  _CpuFrameSamples.clear();
  _CpuTimingSamples.clear();
  _CpuFrameDistribution = {};
  _SoftwareCounterTotals.clear();
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
  {
    _CpuTotals = iCpuTimings;
    _CpuTimingSamples.resize(iCpuTimings.size());
    for ( FpsCpuTiming & timing : _CpuTotals )
      timing._Seconds = 0.;
  }

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
  _CpuFrameSamples.push_back(cpuFrameSeconds);
  for ( int i = 0; i < static_cast<int>(iCpuTimings.size()) && i < static_cast<int>(_CpuTimingSamples.size()); ++i )
  {
    if ( iCpuTimings[i]._Enabled )
      _CpuTimingSamples[i].push_back(iCpuTimings[i]._Seconds);
  }

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
        total._Inclusive = timing._Inclusive;
        _RendererTotals.push_back(total);
      }
    }

    double rendererFrameSeconds = 0.;
    for ( int i = 0; i < static_cast<int>(renderTimings.size()) && i < static_cast<int>(_RendererTotals.size()); ++i )
    {
      const RenderPassTiming & timing = renderTimings[i];
      if ( !timing._Enabled )
        continue;

      if ( !timing._Inclusive )
        rendererFrameSeconds += timing._Seconds;
      _RendererTotals[i]._Enabled = true;
      _RendererTotals[i]._Seconds += timing._Seconds;
      _RendererTotals[i]._Samples.push_back(timing._Seconds);
    }
    _RendererFrameSeconds += rendererFrameSeconds;
  }

  if ( const SoftwareRasterizer * software = const_cast<Renderer &>(iRenderer).AsSoftwareRasterizer() )
  {
    const SoftwareRasterizerStats & stats = software -> GetStats();
    _SoftwareCounterTotals["input_instances"] += stats._InputInstances;
    _SoftwareCounterTotals["visible_instances"] += stats._VisibleInstances;
    _SoftwareCounterTotals["rejected_instances"] += stats._RejectedInstances;
    _SoftwareCounterTotals["avoided_vertices"] += stats._AvoidedVertices;
    _SoftwareCounterTotals["avoided_triangles"] += stats._AvoidedTriangles;
    _SoftwareCounterTotals["changed_instances"] += stats._ChangedInstances;
    _SoftwareCounterTotals["transformed_vertices"] += stats._TransformedVertices;
    _SoftwareCounterTotals["refreshed_vertices"] += stats._RefreshedVertices;
    _SoftwareCounterTotals["refreshed_triangles"] += stats._RefreshedTriangles;
    _SoftwareCounterTotals["input_triangles"] += stats._InputTriangles;
    _SoftwareCounterTotals["clipped_triangles"] += stats._ClippedTriangles;
    _SoftwareCounterTotals["binned_triangles"] += stats._BinnedTriangles;
    _SoftwareCounterTotals["depth_winning_pixels"] += stats._DepthWinningPixels;
    _SoftwareCounterTotals["covered_pixels"] += stats._CoveredPixels;
    _SoftwareCounterTotals["shaded_pixels"] += stats._ShadedPixels;
    _SoftwareCounterTotals["tile_jobs"] += stats._TileJobs;
    _SoftwareCounterTotals["copied_bytes"] += stats._CopiedBytes;
    _SoftwareCounterTotals["hit_buffer_bytes"] += stats._HitBufferBytes;
    _SoftwareCounterTotals["masked_fragments_tested"] += stats._MaskedFragmentsTested;
    _SoftwareCounterTotals["masked_fragments_rejected"] += stats._MaskedFragmentsRejected;
    _SoftwareCounterTotals["transparent_hits_generated"] += stats._TransparentHitsGenerated;
    _SoftwareCounterTotals["transparent_hits_shaded"] += stats._TransparentHitsShaded;
    _SoftwareCounterTotals["transparent_pixels"] += stats._TransparentPixels;
    _SoftwareCounterTotals["max_transparent_layers"] += stats._MaxTransparentLayers;
    _SoftwareCounterTotals["transparent_hit_buffer_bytes"] += stats._TransparentHitBufferBytes;
    _SoftwareCounterTotals["average_transparent_layers"] += stats._AverageTransparentLayers;
  }

  ++_SamplesDone;
  if ( _SamplesDone >= std::max(1, _SampleFrames) )
  {
    ++_CurrentRepetition;
    if ( _CurrentRepetition < std::max(1, _Repetitions) )
    {
      _WarmupDone = 0;
      _SamplesDone = 0;
      _Status = "Running repetition " + std::to_string(_CurrentRepetition + 1);
    }
    else
    {
      _Running = false;
      _Completed = true;
      _Status = "Completed";
      _CpuFrameDistribution = ComputeDistribution(_CpuFrameSamples);
      for ( FpsGameBenchmarkPassTiming & timing : _RendererTotals )
        timing._Distribution = ComputeDistribution(timing._Samples);
    }
  }
}

// ----------------------------------------------------------------------------
// ComputeDistribution
// ----------------------------------------------------------------------------
FpsGameBenchmarkDistribution FpsGameBenchmark::ComputeDistribution( const std::vector<double> & iSamples )
{
  FpsGameBenchmarkDistribution result;
  if ( iSamples.empty() )
    return result;

  std::vector<double> sorted = iSamples;
  std::sort(sorted.begin(), sorted.end());
  for ( double value : sorted )
    result._Mean += value;
  result._Mean /= sorted.size();
  result._Minimum = sorted.front();
  const size_t middle = sorted.size() / 2;
  result._Median = ( sorted.size() % 2 ) ? sorted[middle] : ( sorted[middle - 1] + sorted[middle] ) * .5;
  result._P95 = sorted[std::min(sorted.size() - 1, static_cast<size_t>(std::ceil(sorted.size() * .95)) - 1)];

  double variance = 0.;
  for ( double value : sorted )
  {
    const double delta = value - result._Mean;
    variance += delta * delta;
  }
  result._StandardDeviation = std::sqrt(variance / sorted.size());
  return result;
}

// ----------------------------------------------------------------------------
// SaveResult
// ----------------------------------------------------------------------------
bool FpsGameBenchmark::SaveResult( const FpsGameBenchmarkSaveContext & iContext )
{
  if ( !_Completed || _CpuFrameSamples.empty() || !iContext._Settings )
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
  const int samples = static_cast<int>(_CpuFrameSamples.size());
  file << "{\n";
  file << "  \"schema_version\": 2,\n";
  file << "  \"benchmark\": \"Test6 " << JsonEscape(rendererName) << " Renderer\",\n";
  file << "  \"label\": \"" << JsonEscape(_Label) << "\",\n";
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
  file << "    \"repetitions\": " << _Repetitions << ",\n";
  file << "    \"warmup_frames\": " << _WarmupFrames << ",\n";
  file << "    \"sample_frames_per_repetition\": " << _SampleFrames << ",\n";
  file << "    \"total_sample_frames\": " << samples << ",\n";
  file << "    \"cpu_frame\": {\n";
  WriteDistribution(file, _CpuFrameDistribution, "      ");
  file << "    },\n";
  file << "    \"cpu_frame_samples_ms\": ";
  WriteSamples(file, _CpuFrameSamples);
  file << "\n";
  file << "  },\n";
  file << "  \"cpu_timings\": {\n";
  bool first = true;
  for ( int i = 0; i < static_cast<int>(_CpuTotals.size()); ++i )
  {
    const FpsCpuTiming & timing = _CpuTotals[i];
    if ( !timing._Enabled )
      continue;
    if ( !first )
      file << ",\n";
    const FpsGameBenchmarkDistribution distribution = ( i < static_cast<int>(_CpuTimingSamples.size()) ) ? ComputeDistribution(_CpuTimingSamples[i]) : FpsGameBenchmarkDistribution();
    file << "    \"" << JsonEscape(timing._Name ? timing._Name : "") << "\": {\n";
    WriteDistribution(file, distribution, "      ");
    file << "    }";
    first = false;
  }
  file << "\n";
  file << "  },\n";
  file << "  \"renderer_timings\": {\n";
  first = true;
  for ( const FpsGameBenchmarkPassTiming & timing : _RendererTotals )
  {
    if ( !timing._Enabled )
      continue;
    if ( !first )
      file << ",\n";
    file << "    \"" << JsonEscape(timing._Name) << "\": {\n";
    file << "      \"device\": \"" << ( timing._GPU ? "GPU" : "CPU" ) << "\",\n";
    file << "      \"inclusive\": " << ( timing._Inclusive ? "true" : "false" ) << ",\n";
    WriteDistribution(file, timing._Distribution, "      ");
    file << "    }";
    first = false;
  }
  file << "\n";
  file << "  }";

  if ( const SoftwareRasterizer * software = iContext._Renderer ? const_cast<Renderer *>(iContext._Renderer) -> AsSoftwareRasterizer() : nullptr )
  {
    file << ",\n";
    file << "  \"software_configuration\": {\n";
    file << "    \"simd_enabled\": " << ( software -> GetEnableSIMD() ? "true" : "false" ) << ",\n";
    file << "    \"simd_mode\": \"" << software -> GetSIMDMode() << "\",\n";
    file << "    \"tile_size\": " << software -> GetTileSize() << ",\n";
    file << "    \"thread_count\": " << settings._NbThreads << ",\n";
    file << "    \"debug_mode\": " << software -> GetDebugMode() << ",\n";
    file << "    \"transparency\": " << ( settings._Transparency ? "true" : "false" ) << ",\n";
    file << "    \"optimization_flags\": {\n";
    file << "      \"incremental_instance_refresh\": " << ( software -> GetEnableIncrementalRefresh() ? "true" : "false" ) << ",\n";
    file << "      \"compact_hits\": " << ( software -> GetEnableCompactHits() ? "true" : "false" ) << ",\n";
    file << "      \"direct_color_writes\": " << ( software -> GetEnableDirectColorWrites() ? "true" : "false" ) << ",\n";
    file << "      \"frustum_culling\": " << ( software -> GetEnableFrustumCulling() ? "true" : "false" ) << ",\n";
    file << "      \"pbo_upload\": " << ( software -> GetEnablePBOUpload() ? "true" : "false" ) << "\n";
    file << "    }\n";
    file << "  },\n";
    file << "  \"software_counters_average\": {\n";
    first = true;
    for ( const auto & counter : _SoftwareCounterTotals )
    {
      if ( !first )
        file << ",\n";
      file << "    \"" << JsonEscape(counter.first) << "\": " << counter.second / samples;
      first = false;
    }
    file << "\n  }";
  }
  file << "\n";
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
// SetPose
// ----------------------------------------------------------------------------
void FpsGameBenchmark::SetPose( const Vec3 & iPosition, float iYaw, float iPitch )
{
  g_BenchmarkPosition = iPosition;
  g_BenchmarkYaw = iYaw;
  g_BenchmarkPitch = iPitch;
}

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
