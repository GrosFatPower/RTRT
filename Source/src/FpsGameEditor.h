#ifndef _FpsGameEditor_
#define _FpsGameEditor_

#include "FpsGame.h"
#include "FpsGameMap.h"
#include "KeyInput.h"
#include "RenderSettings.h"

#include <string>
#include <vector>

struct GLFWwindow;

namespace RTRT
{

class Renderer;
class Scene;

struct FpsGameEditorContext
{
  FpsGameEditorContext( GLFWwindow * iWindow,
                        Scene * iScene,
                        Renderer * iRenderer,
                        RenderSettings & iSettings,
                        const KeyInput & iKeyInput,
                        FpsGameSettings & iGameSettings,
                        FpsGameWorld & iGameWorld,
                        FpsGameSceneBinding & iSceneBinding,
                        FpsGameMap & iMap,
                        std::string & iMapPath,
                        bool & iMapLoaded,
                        bool & iReloadScene );

  GLFWwindow           * _Window = nullptr;
  Scene                 * _Scene = nullptr;
  Renderer              * _Renderer = nullptr;
  RenderSettings        & _Settings;
  const KeyInput        & _KeyInput;
  FpsGameSettings       & _GameSettings;
  FpsGameWorld          & _GameWorld;
  FpsGameSceneBinding   & _SceneBinding;
  FpsGameMap            & _Map;
  std::string           & _MapPath;
  bool                  & _MapLoaded;
  bool                  & _ReloadScene;
};

enum class FpsEditableKind
{
  None = 0,
  Box,
  Collider,
  Prop,
  Light,
  Boids,
  PlayerSpawn,
  Weapon
};

struct FpsEditorSelection
{
  FpsEditableKind _Kind = FpsEditableKind::None;
  int             _Index = -1;
  int             _SceneInstanceID = -1;
};

class FpsGameEditor
{
public:
  bool IsEnabled() const { return _Enabled; }
  bool IsDirty() const { return _Dirty; }
  void SetDirty( bool iDirty ) { _Dirty = iDirty; }
  void MarkDirty() { _Dirty = true; }
  const FpsEditorSelection & GetSelection() const { return _Selection; }

  bool SetMode( FpsGameEditorContext & ioContext, bool iEnabled );
  void SetPathBuffers( const std::string & iMapPath );
  void SetStatus( const std::string & iMessage );
  void RefreshPropAssets();
  void SyncMapFromRuntimeSettings( FpsGameEditorContext & ioContext );

  void SyncObject( FpsGameEditorContext & ioContext, int iObjectIndex );
  void SyncProp( FpsGameEditorContext & ioContext, int iPropIndex );
  void SyncLight( FpsGameEditorContext & ioContext, int iLightIndex );

  bool DeleteSelectedItem( FpsGameEditorContext & ioContext );
  bool DuplicateSelectedItem( FpsGameEditorContext & ioContext );
  bool DeleteSelectedMaterial( FpsGameEditorContext & ioContext );
  bool DuplicateSelectedMaterial( FpsGameEditorContext & ioContext );

  void EnsureObjectVisibility( FpsGameEditorContext & ioContext );
  void ApplyObjectVisibility( FpsGameEditorContext & ioContext );
  bool BuildPickingRay( const FpsGameEditorContext & iContext, double iMouseX, double iMouseY, Vec3 & oRayOrigin, Vec3 & oRayDir ) const;
  bool PickSelection( const FpsGameEditorContext & iContext, double iMouseX, double iMouseY, FpsEditorSelection & oSelection ) const;
  void HandleMousePick( const FpsGameEditorContext & iContext, double iMouseX, double iMouseY );

  void DrawDockspace();
  void DrawPanels( FpsGameEditorContext & ioContext );
  void DrawScenePanel( FpsGameEditorContext & ioContext );
  void DrawInspectorPanel( FpsGameEditorContext & ioContext );
  void DrawMaterialsPanel( FpsGameEditorContext & ioContext );
  void DrawSettingsPanel( FpsGameEditorContext & ioContext );
  int DrawOverlays( FpsGameEditorContext & ioContext );
  int DrawGizmo( FpsGameEditorContext & ioContext );

protected:
  bool               _Enabled = false;
  bool               _Dirty = false;
  bool               _PreviousShowViewWeapon = true;
  bool               _PreviousFreeLook = false;
  bool               _ShowColliderHelpers = true;
  bool               _ShowLightHelpers = true;
  bool               _ShowBoidsHelpers = true;
  bool               _ShowScenePanel = true;
  bool               _ShowInspectorPanel = true;
  bool               _ShowMaterialsPanel = true;
  bool               _ShowSettingsPanel = true;
  bool               _ResetDockLayout = false;
  FpsEditorSelection _Selection;
  int                _SelectedMaterial = 0;
  char               _SavePath[512] = {};
  char               _LoadPath[512] = {};
  char               _NewMaterialName[128] = "new_material";
  char               _NewPropName[128] = "New Prop";
  int                _NewPropAssetIndex = -1;
  std::vector<std::string> _PropAssetPaths;
  std::vector<bool>  _ObjectInstanceVisible;
  std::string        _StatusMessage;
};

}

#endif /* _FpsGameEditor_ */
