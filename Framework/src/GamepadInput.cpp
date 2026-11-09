#include "GamepadInput.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace RTRT
{

namespace
{
  constexpr float kStickPressThreshold   = 0.52f;
  constexpr float kStickReleaseThreshold = 0.34f;
  constexpr float kTriggerPressThreshold = 0.45f;
  constexpr float kTriggerRelThreshold   = 0.25f;
  constexpr float kNavInitialDelay       = 0.28f;
  constexpr float kNavRepeatInterval     = 0.10f;
}

// ----------------------------------------------------------------------------
// CTOR / DTOR
// ----------------------------------------------------------------------------
GamepadInput::GamepadInput()
{
  for ( int i = 0; i < 6; ++i )
  {
    _CurAxes[i]  = ( ( i == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER ) || ( i == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER ) ) ? -1.0f : 0.0f;
    _PrevAxes[i] = _CurAxes[i];
  }
}

GamepadInput::~GamepadInput()
{
}

// ----------------------------------------------------------------------------
// ApplyDeadzone
// ----------------------------------------------------------------------------
float GamepadInput::ApplyDeadzone( float iValue, float iDeadzone )
{
  const float absVal = std::fabs(iValue);
  if ( absVal <= iDeadzone )
    return 0.0f;

  const float sign = ( iValue >= 0.0f ) ? 1.0f : -1.0f;
  const float remapped = ( absVal - iDeadzone ) / std::max(0.001f, 1.0f - iDeadzone);
  return sign * std::min(1.0f, remapped);
}

// ----------------------------------------------------------------------------
// DpadButtonForDir
// ----------------------------------------------------------------------------
int GamepadInput::DpadButtonForDir( GamepadDir iDir )
{
  switch ( iDir )
  {
    case GamepadDir::Up:    return GLFW_GAMEPAD_BUTTON_DPAD_UP;
    case GamepadDir::Down:  return GLFW_GAMEPAD_BUTTON_DPAD_DOWN;
    case GamepadDir::Left:  return GLFW_GAMEPAD_BUTTON_DPAD_LEFT;
    case GamepadDir::Right: return GLFW_GAMEPAD_BUTTON_DPAD_RIGHT;
    default:                return GLFW_GAMEPAD_BUTTON_DPAD_UP;
  }
}

// ----------------------------------------------------------------------------
// Poll
// ----------------------------------------------------------------------------
void GamepadInput::Poll( float iDeltaTime )
{
  std::memcpy(_PrevButtons, _CurButtons, sizeof(_CurButtons));
  std::memcpy(_PrevAxes, _CurAxes, sizeof(_CurAxes));
  std::memcpy(_PrevStickDir, _CurStickDir, sizeof(_CurStickDir));
  _PrevStrictStickUp    = _CurStrictStickUp;
  _PrevLeftTriggerDown  = _CurLeftTriggerDown;
  _PrevRightTriggerDown = _CurRightTriggerDown;

  int activeJid = -1;
  bool isMappedGamepad = false;
  const int targetSlot = std::max(0, _DeviceSlot);

  // Scan for mapped Xbox/SDL gamepads first and pick the targetSlot-th connected device
  int foundCount = 0;
  for ( int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid )
  {
    if ( ( GLFW_TRUE == glfwJoystickPresent(jid) ) && ( GLFW_TRUE == glfwJoystickIsGamepad(jid) ) )
    {
      if ( foundCount == targetSlot )
      {
        activeJid = jid;
        isMappedGamepad = true;
        break;
      }
      ++foundCount;
    }
  }

  // Fallback to any unmapped present joystick if targetSlot not yet satisfied
  if ( activeJid < 0 )
  {
    for ( int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid )
    {
      if ( ( GLFW_TRUE == glfwJoystickPresent(jid) ) && ( GLFW_TRUE != glfwJoystickIsGamepad(jid) ) )
      {
        if ( foundCount == targetSlot )
        {
          activeJid = jid;
          isMappedGamepad = false;
          break;
        }
        ++foundCount;
      }
    }
  }

  if ( activeJid < 0 )
  {
    _Connected   = false;
    _JoystickId  = -1;
    _GamepadName = "No Gamepad";
    std::memset(_CurButtons, 0, sizeof(_CurButtons));
    for ( int i = 0; i < 6; ++i )
      _CurAxes[i] = ( ( i == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER ) || ( i == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER ) ) ? -1.0f : 0.0f;
    std::memset(_CurStickDir, 0, sizeof(_CurStickDir));
    _CurStrictStickUp    = false;
    _CurLeftTriggerDown  = false;
    _CurRightTriggerDown = false;
    for ( int d = 0; d < 4; ++d )
    {
      _NavHoldTimers[d]  = 0.0f;
      _NavRepeatFired[d] = false;
    }
    return;
  }

  _Connected  = true;
  _JoystickId = activeJid;

  if ( isMappedGamepad )
  {
    const char * name = glfwGetGamepadName(activeJid);
    _GamepadName = name ? name : "Xbox / Standard Gamepad";

    GLFWgamepadstate state;
    if ( GLFW_TRUE == glfwGetGamepadState(activeJid, &state) )
    {
      for ( int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; ++b )
        _CurButtons[b] = ( GLFW_PRESS == state.buttons[b] ) ? 1 : 0;
      for ( int a = 0; a <= GLFW_GAMEPAD_AXIS_LAST; ++a )
        _CurAxes[a] = state.axes[a];
    }
  }
  else
  {
    const char * name = glfwGetJoystickName(activeJid);
    _GamepadName = name ? name : "Generic USB Controller";

    std::memset(_CurButtons, 0, sizeof(_CurButtons));
    int btnCount = 0;
    const unsigned char * btns = glfwGetJoystickButtons(activeJid, &btnCount);
    if ( btns )
    {
      const int copyCount = std::min(btnCount, 15);
      for ( int b = 0; b < copyCount; ++b )
        _CurButtons[b] = ( GLFW_PRESS == btns[b] ) ? 1 : 0;
    }

    int hatCount = 0;
    const unsigned char * hats = glfwGetJoystickHats(activeJid, &hatCount);
    if ( hats && ( hatCount > 0 ) )
    {
      if ( hats[0] & GLFW_HAT_UP )    _CurButtons[GLFW_GAMEPAD_BUTTON_DPAD_UP]    = 1;
      if ( hats[0] & GLFW_HAT_RIGHT ) _CurButtons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = 1;
      if ( hats[0] & GLFW_HAT_DOWN )  _CurButtons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]  = 1;
      if ( hats[0] & GLFW_HAT_LEFT )  _CurButtons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT]  = 1;
    }

    int axisCount = 0;
    const float * axes = glfwGetJoystickAxes(activeJid, &axisCount);
    if ( axes )
    {
      const int copyAxes = std::min(axisCount, 6);
      for ( int a = 0; a < copyAxes; ++a )
        _CurAxes[a] = axes[a];
    }
  }

  // Evaluate Left Stick digital directions with hysteresis
  const float lx = _CurAxes[GLFW_GAMEPAD_AXIS_LEFT_X];
  const float ly = _CurAxes[GLFW_GAMEPAD_AXIS_LEFT_Y];

  const int idxUp    = static_cast<int>(GamepadDir::Up);
  const int idxDown  = static_cast<int>(GamepadDir::Down);
  const int idxLeft  = static_cast<int>(GamepadDir::Left);
  const int idxRight = static_cast<int>(GamepadDir::Right);

  _CurStickDir[idxUp]    = _PrevStickDir[idxUp]    ? ( ly < -kStickReleaseThreshold ) : ( ly < -kStickPressThreshold );
  _CurStickDir[idxDown]  = _PrevStickDir[idxDown]  ? ( ly >  kStickReleaseThreshold ) : ( ly >  kStickPressThreshold );
  _CurStickDir[idxLeft]  = _PrevStickDir[idxLeft]  ? ( lx < -kStickReleaseThreshold ) : ( lx < -kStickPressThreshold );
  _CurStickDir[idxRight] = _PrevStickDir[idxRight] ? ( lx >  kStickReleaseThreshold ) : ( lx >  kStickPressThreshold );

  // Strict vertical stick up for Hard Drop (prevents accidental diagonal drops while moving left/right)
  if ( _PrevStrictStickUp )
    _CurStrictStickUp = ( ly < -0.45f );
  else
    _CurStrictStickUp = ( ly < -0.75f ) && ( std::fabs(lx) < 0.34f );

  // Evaluate Triggers with hysteresis
  const float lt = GetTrigger(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, 0.05f);
  const float rt = GetTrigger(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.05f);

  _CurLeftTriggerDown  = _PrevLeftTriggerDown  ? ( lt > kTriggerRelThreshold ) : ( lt > kTriggerPressThreshold );
  _CurRightTriggerDown = _PrevRightTriggerDown ? ( rt > kTriggerRelThreshold ) : ( rt > kTriggerPressThreshold );

  // Update menu navigation auto-repeat timers
  for ( int d = 0; d < 4; ++d )
  {
    const GamepadDir dir = static_cast<GamepadDir>(d);
    const bool downNow   = IsDirDown(dir, true);
    const bool pressed   = IsDirPressed(dir, true);

    _NavRepeatFired[d] = false;
    if ( pressed )
    {
      _NavHoldTimers[d]  = kNavInitialDelay;
      _NavRepeatFired[d] = true;
    }
    else if ( downNow )
    {
      _NavHoldTimers[d] -= iDeltaTime;
      if ( _NavHoldTimers[d] <= 0.0f )
      {
        _NavHoldTimers[d] += kNavRepeatInterval;
        _NavRepeatFired[d] = true;
      }
    }
    else
    {
      _NavHoldTimers[d] = 0.0f;
    }
  }
}

// ----------------------------------------------------------------------------
// IsButtonDown / IsButtonPressed / IsButtonReleased
// ----------------------------------------------------------------------------
bool GamepadInput::IsButtonDown( int iButton ) const
{
  if ( !_Connected || ( iButton < 0 ) || ( iButton > GLFW_GAMEPAD_BUTTON_LAST ) )
    return false;
  return ( _CurButtons[iButton] != 0 );
}

bool GamepadInput::IsButtonPressed( int iButton ) const
{
  if ( !_Connected || ( iButton < 0 ) || ( iButton > GLFW_GAMEPAD_BUTTON_LAST ) )
    return false;
  return ( ( _CurButtons[iButton] != 0 ) && ( _PrevButtons[iButton] == 0 ) );
}

bool GamepadInput::IsButtonReleased( int iButton ) const
{
  if ( !_Connected || ( iButton < 0 ) || ( iButton > GLFW_GAMEPAD_BUTTON_LAST ) )
    return false;
  return ( ( _CurButtons[iButton] == 0 ) && ( _PrevButtons[iButton] != 0 ) );
}

// ----------------------------------------------------------------------------
// GetAxis / GetTrigger
// ----------------------------------------------------------------------------
float GamepadInput::GetAxis( int iAxis, float iDeadzone ) const
{
  if ( !_Connected || ( iAxis < 0 ) || ( iAxis > GLFW_GAMEPAD_AXIS_LAST ) )
    return 0.0f;
  return ApplyDeadzone(_CurAxes[iAxis], iDeadzone);
}

float GamepadInput::GetTrigger( int iTriggerAxis, float iDeadzone ) const
{
  if ( !_Connected || ( ( iTriggerAxis != GLFW_GAMEPAD_AXIS_LEFT_TRIGGER ) && ( iTriggerAxis != GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER ) ) )
    return 0.0f;

  // GLFW maps triggers to [-1.0, +1.0] where -1.0 is released and +1.0 is fully pressed
  const float norm = std::clamp(( _CurAxes[iTriggerAxis] + 1.0f ) * 0.5f, 0.0f, 1.0f);
  if ( norm <= iDeadzone )
    return 0.0f;
  return std::min(1.0f, ( norm - iDeadzone ) / std::max(0.001f, 1.0f - iDeadzone));
}

// ----------------------------------------------------------------------------
// IsDirDown / IsDirPressed / IsDirReleased / IsMenuNavRepeated
// ----------------------------------------------------------------------------
bool GamepadInput::IsDirDown( GamepadDir iDir, bool iIncludeLeftStick ) const
{
  if ( !_Connected )
    return false;

  const int dpadBtn = DpadButtonForDir(iDir);
  if ( _CurButtons[dpadBtn] != 0 )
    return true;

  if ( iIncludeLeftStick )
  {
    const int idx = static_cast<int>(iDir);
    if ( ( idx >= 0 ) && ( idx < 4 ) && _CurStickDir[idx] )
      return true;
  }

  return false;
}

bool GamepadInput::IsDirPressed( GamepadDir iDir, bool iIncludeLeftStick ) const
{
  if ( !_Connected )
    return false;

  const int dpadBtn = DpadButtonForDir(iDir);
  const int idx     = static_cast<int>(iDir);

  const bool curDown  = ( _CurButtons[dpadBtn] != 0 )  || ( iIncludeLeftStick && ( idx >= 0 ) && ( idx < 4 ) && _CurStickDir[idx] );
  const bool prevDown = ( _PrevButtons[dpadBtn] != 0 ) || ( iIncludeLeftStick && ( idx >= 0 ) && ( idx < 4 ) && _PrevStickDir[idx] );

  return ( curDown && !prevDown );
}

bool GamepadInput::IsDirReleased( GamepadDir iDir, bool iIncludeLeftStick ) const
{
  if ( !_Connected )
    return false;

  const int dpadBtn = DpadButtonForDir(iDir);
  const int idx     = static_cast<int>(iDir);

  const bool curDown  = ( _CurButtons[dpadBtn] != 0 )  || ( iIncludeLeftStick && ( idx >= 0 ) && ( idx < 4 ) && _CurStickDir[idx] );
  const bool prevDown = ( _PrevButtons[dpadBtn] != 0 ) || ( iIncludeLeftStick && ( idx >= 0 ) && ( idx < 4 ) && _PrevStickDir[idx] );

  return ( !curDown && prevDown );
}

bool GamepadInput::IsMenuNavRepeated( GamepadDir iDir ) const
{
  if ( !_Connected )
    return false;

  const int idx = static_cast<int>(iDir);
  if ( ( idx < 0 ) || ( idx >= 4 ) )
    return false;

  return _NavRepeatFired[idx];
}

} // namespace RTRT
