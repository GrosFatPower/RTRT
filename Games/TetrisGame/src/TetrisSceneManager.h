#ifndef _TetrisSceneManager_
#define _TetrisSceneManager_

#include "Camera.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshInstance.h"
#include "RenderSettings.h"
#include "Scene.h"
#include "TetrisGameLogic.h"
#include "TetrisTypes.h"

#include <memory>
#include <random>
#include <string>
#include <vector>

namespace RTRT
{
class Renderer;
}

namespace Tetris
{
using namespace RTRT;

class TetrisSceneManager
{
public:
  TetrisSceneManager();
  virtual ~TetrisSceneManager();

  int Initialize( Scene & ioScene );
  int SyncScene( Scene & ioScene, const TetrisGameLogic & iLogic, float iDeltaTime );
  int SyncScene( Scene & ioScene, const TetrisGameLogic * iLogics, int iNumPlayers, float iDeltaTime );
  int SyncCamera( Scene & ioScene, const TetrisGameLogic & iLogic );
  int SyncCamera( Scene & ioScene, const TetrisGameLogic * iLogics, int iNumPlayers );

  int SyncTitleShowcase( Scene & ioScene, float iTime );
  int SyncTitleCamera( Scene & ioScene, float iTime );

  void SetMaterialPreset( Scene & ioScene, TetrisMaterialPreset iPreset );
  TetrisMaterialPreset GetMaterialPreset() const { return _MaterialPreset; }

  void SetCameraPreset( TetrisCameraPreset iPreset ) { _CameraPreset = iPreset; }
  TetrisCameraPreset GetCameraPreset() const { return _CameraPreset; }

  void SetLineClearAnimMode( TetrisLineClearAnimMode iMode ) { _LineClearAnimMode = iMode; }
  TetrisLineClearAnimMode GetLineClearAnimMode() const { return _LineClearAnimMode; }

  void SetShowGhost( bool iShow ) { _ShowGhost = iShow; }
  bool GetShowGhost() const { return _ShowGhost; }

  void SetBackplateOpacity( Scene & ioScene, float iOpacity );
  float GetBackplateOpacity() const { return _BackplateOpacity; }

  void SetDynamicPieceLighting( bool iEnable ) { _DynamicPieceLighting = iEnable; }
  bool GetDynamicPieceLighting() const { return _DynamicPieceLighting; }

  void SetInGameCameraMotion( bool iEnable ) { _InGameCameraMotion = iEnable; }
  bool GetInGameCameraMotion() const { return _InGameCameraMotion; }

  void SetBackgroundMotionSpeed( float iSpeed ) { _BgMotionSpeed = iSpeed; }
  float GetBackgroundMotionSpeed() const { return _BgMotionSpeed; }

  void SetBoardSwayAmplitude( float iAmp ) { _BoardSwayAmplitude = iAmp; }
  float GetBoardSwayAmplitude() const { return _BoardSwayAmplitude; }

  void UpdateBackgroundMotion( RenderSettings & ioSettings, float iTime );

  // Background / HDR Environment management
  void ApplyTitleBackground( Scene & ioScene, RenderSettings & ioSettings, Renderer * ioRenderer );
  void SelectNextRandomBackground( Scene & ioScene, RenderSettings & ioSettings, Renderer * ioRenderer );
  void ApplyBackgroundByIndex( int iIndex, Scene & ioScene, RenderSettings & ioSettings, Renderer * ioRenderer );

  void SetRotateBackgroundOnLevelUp( bool iEnable ) { _RotateBgOnLevelUp = iEnable; }
  bool GetRotateBackgroundOnLevelUp() const { return _RotateBgOnLevelUp; }

  int GetCurrentBackgroundIndex() const { return _CurrentBgIndex; }
  const std::vector<std::string> & GetBackgroundNames() const { return _BackgroundNames; }

  // Orbit camera manipulation for FreeOrbit preset
  void OrbitYawPitch( float iDeltaYaw, float iDeltaPitch );
  void OrbitZoom( float iDeltaRadius );

  static Vec3 GetBoardOrigin( int iPlayerIndex, int iNumPlayers );
  static Vec3 CellToWorld( int iX, int iY, const Vec3 & iBoardOrigin = Vec3(0.0f) );

protected:
  Mesh * CreateGhostCageMesh( const std::string & iName, float iSize = 0.94f, float iThickness = 0.08f );
  Mesh * CreateEnclosureMesh( const std::string & iName );
  Mesh * CreateBackplateMesh( const std::string & iName );
  Mesh * CreateSkylineMesh( const std::string & iName );

  int SetupMaterials( Scene & ioScene );
  int SetupLights( Scene & ioScene );
  int SetupInstances( Scene & ioScene );

  int MaterialIDForPiece( TetrisPieceType iType ) const;

  void RefillBackgroundBag();

  TetrisLineClearAnimMode ResolveLineClearAnimMode( int iNumLines, TetrisTSpinType iTSpin, bool iPerfectClear );
  void TriggerLineClearFx( int iPlayerIndex, const TetrisGameLogic & iLogic, const Vec3 & iBoardOrigin );
  void UpdateAndSyncLineClearFx( int iPlayerIndex, const Vec3 & iBoardOrigin, std::vector<MeshInstance> & ioInstances, float iDeltaTime );
  void ResetLineClearFx( int iPlayerIndex );
  void SyncSingleBoard( int iPlayerIndex, const Vec3 & iBoardOrigin, Scene & ioScene, const TetrisGameLogic & iLogic, float iDeltaTime );
  void HideBoard( int iPlayerIndex, Scene & ioScene );

protected:
  struct TetrisClearFxBlock
  {
    bool                    _Active      = false;
    TetrisLineClearAnimMode _Mode        = TetrisLineClearAnimMode::ClassicBlink;
    TetrisPieceType         _PieceType   = TetrisPieceType::I;
    int                     _GridX       = 0;
    int                     _GridY       = 0;
    Vec3                    _Pos         = Vec3(0.f);
    Vec3                    _Vel         = Vec3(0.f);
    Vec3                    _EulerAngles = Vec3(0.f);
    Vec3                    _AngularVel  = Vec3(0.f);
    float                   _Scale       = 0.92f;
    float                   _Elapsed     = 0.f;
    float                   _Delay       = 0.f;
    float                   _Duration    = 0.4f;
  };

  TetrisMaterialPreset    _MaterialPreset    = TetrisMaterialPreset::GlossyPlastic;
  TetrisCameraPreset      _CameraPreset      = TetrisCameraPreset::Frontal3D;
  TetrisLineClearAnimMode _LineClearAnimMode = TetrisLineClearAnimMode::Dynamic;
  bool                 _ShowGhost      = true;
  float                _BackplateOpacity = 0.45f;
  bool                 _DynamicPieceLighting = true;
  bool                 _InGameCameraMotion   = true;
  float                _BgMotionSpeed        = 1.0f;
  float                _BoardSwayAmplitude   = 0.0f;
  float                _BaseSkyBoxRotation   = 0.0f;
  bool                 _RotateBgOnLevelUp    = true;

  // Background rotation state
  std::vector<std::string> _BackgroundNames;
  std::vector<int>         _BackgroundBag;
  int                      _CurrentBgIndex     = 0;
  bool                     _IsTitleBgActive    = true;
  int                      _LastObservedLevel  = 1;
  std::mt19937             _BgRng;

  // Orbit camera parameters
  float                _OrbitRadius    = 27.0f;
  float                _OrbitYaw       = 0.f;
  float                _OrbitPitch     = 0.f;

  // Scene resource IDs
  int                  _BeveledCubeMeshID   = -1;
  int                  _GhostCageMeshID     = -1;
  int                  _EnclosureMeshID     = -1;
  int                  _BackplateMeshID     = -1;
  int                  _SkylineMeshID       = -1;

  int                  _PieceMaterialIDs[(int)TetrisPieceType::Count];
  int                  _GhostMaterialID     = -1;
  int                  _ChassisMaterialID   = -1;
  int                  _BackplateMaterialID = -1;
  int                  _SkylineMaterialID   = -1;
  int                  _FlashMaterialID     = -1;

  // Pre-allocated instance indices in Scene (per player board [0..1])
  int                  _MatrixInstanceStart[2]  = { -1, -1 }; // 220 instances for matrix
  int                  _ActiveInstanceStart[2]  = { -1, -1 }; // 4 instances
  int                  _GhostInstanceStart[2]   = { -1, -1 }; // 4 instances
  int                  _HoldInstanceStart[2]    = { -1, -1 }; // 4 instances
  int                  _NextInstanceStart[2]    = { -1, -1 }; // 20 instances (5 x 4)
  int                  _ClearFxInstanceStart[2] = { -1, -1 }; // 40 instances (4 rows x 10 cols)
  int                  _EnclosureInstanceID[2]  = { -1, -1 };
  int                  _BackplateInstanceID[2]  = { -1, -1 };
  int                  _SkylineInstanceID[2]    = { -1, -1 };

  // Line clear animation state (per player board [0..1])
  unsigned int                         _LastSeenClearEventId[2] = { 0, 0 };
  std::array<TetrisClearFxBlock, 40>   _ClearFxBlocks[2]        = {};
  std::array<float, 22>                _RowVisualYOffset[2]     = {};
  std::array<float, 22>                _RowDropVelocity[2]      = {};
  float                                _RowDropHoldTimer[2]     = { 0.0f, 0.0f };

  // Light indices (per player board [0..1])
  int                  _ActivePieceLightIdx[2] = { -1, -1 };
  int                  _LineClearLightIdx[2]   = { -1, -1 };
  float                _FlashLightIntensity[2] = { 0.f, 0.f };
};

}

#endif /* _TetrisSceneManager_ */
