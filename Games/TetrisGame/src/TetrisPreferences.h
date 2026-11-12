#ifndef _TetrisPreferences_
#define _TetrisPreferences_

#include "UserPreferences.h"
#include "TetrisGameLogic.h"
#include "TetrisSceneManager.h"
#include "TetrisTypes.h"

namespace Tetris
{
using namespace RTRT;

// ----------------------------------------------------------------------------
// TetrisPreferences
// ----------------------------------------------------------------------------
struct TetrisPreferences : public UserPreferences
{
  TetrisHandlingConfig    _Handling;
  TetrisRendererMode      _RendererMode      = TetrisRendererMode::Deferred;
  TetrisMaterialPreset    _MaterialPreset    = TetrisMaterialPreset::GlossyPlastic;
  TetrisCameraPreset      _CameraPreset      = TetrisCameraPreset::Frontal3D;
  TetrisLineClearAnimMode _LineClearAnim     = TetrisLineClearAnimMode::Dynamic;
  float                   _BackplateOpacity  = 0.85f;
  Vec3                    _BoardEdgeColor    = Vec3(0.0f, 0.85f, 0.95f);
  bool                    _DynamicLighting   = true;
  bool                    _CameraMotion      = true;
  float                   _CameraMotionSpeed = 1.0f;
  float                   _BoardSwayAmplitude= 0.35f;
  int                     _StartLevel        = 1;
  bool                    _ShowGhost         = true;

  // Apply to game systems
  void ApplyToTetris( TetrisSceneManager & ioSceneMgr,
                      Scene & ioScene,
                      TetrisHandlingConfig & ioHandling,
                      TetrisGameLogic & ioLogic,
                      TetrisRendererMode & ioRendererMode ) const;

  // Capture current state from game systems
  void CaptureFromTetris( const TetrisSceneManager & iSceneMgr,
                          const TetrisHandlingConfig & iHandling,
                          const TetrisGameLogic & iLogic,
                          TetrisRendererMode iRendererMode );

  // Serialization via nlohmann::json (loads/saves both base UserPreferences + Tetris settings)
  bool LoadFromFile( const std::string & iFilePath );
  bool SaveToFile( const std::string & iFilePath ) const;

  static std::string GetDefaultFilePath();
};

} // namespace Tetris

#endif /* _TetrisPreferences_ */
