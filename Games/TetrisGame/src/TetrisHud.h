#ifndef _TetrisHud_
#define _TetrisHud_

#include "RenderSettings.h"
#include "RenderStatsUI.h"
#include "TetrisGameLogic.h"
#include "TetrisSceneManager.h"
#include "TetrisTypes.h"

namespace RTRT
{

class Renderer;

class TetrisHud
{
public:
  TetrisHud();
  virtual ~TetrisHud();

  int Draw( TetrisGameLogic & ioLogic,
            TetrisSceneManager & ioSceneMgr,
            Scene & ioScene,
            Renderer * ioRenderer,
            RenderSettings & ioSettings,
            TetrisRendererMode & ioRendererMode,
            TetrisHandlingConfig & ioHandling,
            double iFrameRate,
            double iDeltaTime,
            unsigned int iNbRenderedFrames,
            bool & oReloadRenderer );

  bool IsSettingsOpen() const { return _ShowSettingsWindow; }
  void ToggleSettings() { _ShowSettingsWindow = !_ShowSettingsWindow; }

protected:
  void DrawGameplayOverlay( const TetrisGameLogic & iLogic );
  void DrawActionBanner( const TetrisGameLogic & iLogic );
  void DrawGameOverModal( TetrisGameLogic & ioLogic );
  void DrawSettingsWindow( TetrisGameLogic & ioLogic,
                           TetrisSceneManager & ioSceneMgr,
                           Scene & ioScene,
                           Renderer * ioRenderer,
                           RenderSettings & ioSettings,
                           TetrisRendererMode & ioRendererMode,
                           TetrisHandlingConfig & ioHandling,
                           double iFrameRate,
                           double iDeltaTime,
                           unsigned int iNbRenderedFrames,
                           bool & oReloadRenderer );

protected:
  bool               _ShowSettingsWindow = false;
  RenderStatsUIState _StatsUIState;
};

}

#endif /* _TetrisHud_ */
