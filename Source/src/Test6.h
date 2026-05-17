#ifndef _Test6_
#define _Test6_

#include "BaseTest.h"
#include "FpsGame.h"
#include "FpsGameMap.h"
#include "KeyInput.h"
#include "MouseInput.h"
#include "Renderer.h"
#include "RenderSettings.h"
#include "Scene.h"

#include <filesystem>
#include <memory>

struct GLFWwindow;

namespace RTRT
{

enum class FpsEditableKind
{
  None = 0,
  Box,
  Collider,
  Prop,
  Light,
  PlayerSpawn,
  Weapon
};

struct FpsEditorSelection
{
  FpsEditableKind _Kind = FpsEditableKind::None;
  int             _Index = -1;
  int             _SceneInstanceID = -1;
};

struct FpsGameEditor
{
  bool               _Enabled = false;
  bool               _Dirty = false;
  bool               _PreviousShowViewWeapon = true;
  bool               _PreviousFreeLook = false;
  bool               _ShowColliderHelpers = true;
  bool               _ShowLightHelpers = true;
  FpsEditorSelection _Selection;
  int                _SelectedMaterial = 0;
  char               _SavePath[512] = {};
  char               _LoadPath[512] = {};
  char               _NewMaterialName[128] = "new_material";
  char               _NewPropName[128] = "New Prop";
  int                _NewPropAssetIndex = -1;
  std::vector<std::string> _PropAssetPaths;
  std::vector<bool>  _ObjectInstanceVisible;
};

class Test6 : public BaseTest
{
public:
  Test6( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test6();

  int Run();

  static const char * GetTestHeader();

protected:
  static void KeyCallback( GLFWwindow * iWindow, const int iKey, const int iScancode, const int iAction, const int iMods );
  static void MouseButtonCallback( GLFWwindow * iWindow, const int iButton, const int iAction, const int iMods );
  static void MouseScrollCallback( GLFWwindow * iWindow, const double iOffsetX, const double iOffsetY );
  static void FramebufferSizeCallback( GLFWwindow * iWindow, const int iWidth, const int iHeight );

protected:
  int InitializeUI();
  int InitializeScene();
  int InitializeRenderer();
  int ProcessInput();
  int UpdateGame();
  int DrawUI();
  void DrawDebugPanel();
  void DrawEditorPanel();
  int DrawEditorGizmo();
  int DrawEditorOverlays();
  void DrawHUD();
  int DrawSettingsUI();
  void DrawCrosshair();
  int UpdateCPUTime();
  void SyncFramebufferResolution( bool iNotifyRenderer = false );
  void SetMouseCaptured( bool iCaptured );
  void ApplyRendererDefaults();
  void SetEditorMode( bool iEnabled );
  void SetEditorPathBuffers();
  void RefreshEditorPropAssets();
  void SyncMapFromRuntimeSettings();
  void SyncEditorObject( int iObjectIndex );
  void SyncEditorProp( int iPropIndex );
  void SyncEditorLight( int iLightIndex );
  void EnsureEditorObjectVisibility();
  void ApplyEditorObjectVisibility();
  bool BuildPickingRay( double iMouseX, double iMouseY, Vec3 & oRayOrigin, Vec3 & oRayDir ) const;
  bool PickEditorSelection( double iMouseX, double iMouseY, FpsEditorSelection & oSelection ) const;
  void MarkEditorDirty() { _Editor._Dirty = true; }

protected:
  std::unique_ptr<Scene>    _Scene;
  RenderSettings            _Settings;
  std::unique_ptr<Renderer> _Renderer;
  bool                      _ReloadScene = false;
  bool                      _ReloadRenderer = false;

  FpsGameSettings           _GameSettings;
  FpsGameWorld              _GameWorld;
  FpsGameSceneBinding       _SceneBinding;
  FpsGameMap                _Map;
  std::string               _MapPath;
  bool                      _MapLoaded = false;
  FpsGameEditor             _Editor;

  KeyInput                  _KeyInput;
  MouseInput                _MouseInput;

  bool                      _MouseCaptured = false;
  bool                      _ShowDebugPanel = true;
  bool                      _HasLastMousePos = false;
  double                    _LastMouseX = 0.;
  double                    _LastMouseY = 0.;

  double                    _CPUTime = 0.;
  double                    _DeltaTime = 0.;
  double                    _FrameRate = 0.;
  double                    _FrameTime = 0.;
  unsigned int              _NbRenderedFrames = 0;

  bool                      _RenderToFile = false;
  std::filesystem::path     _CaptureOutputPath;
};

}

#endif /* _Test6_ */
