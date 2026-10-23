#ifndef _FpsGameHud_
#define _FpsGameHud_

#include "FpsGame.h"

namespace RTRT
{

struct FpsGameHudContext
{
  FpsGameHudContext( const FpsGameSettings & iGameSettings,
                     const FpsGameWorld & iGameWorld );

  const FpsGameSettings & _GameSettings;
  const FpsGameWorld    & _GameWorld;
};

class FpsGameHud
{
public:
  void Draw( const FpsGameHudContext & iContext );
  void DrawCrosshair();
};

}

#endif /* _FpsGameHud_ */
