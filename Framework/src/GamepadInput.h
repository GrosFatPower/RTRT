#ifndef _GamepadInput_
#define _GamepadInput_

#include <string>

namespace RTRT
{

// ----------------------------------------------------------------------------
// GamepadDir
// ----------------------------------------------------------------------------
enum class GamepadDir
{
  Up = 0,
  Down,
  Left,
  Right,
  Count
};

// ----------------------------------------------------------------------------
// GamepadInput
// ----------------------------------------------------------------------------
class GamepadInput
{
public:
  GamepadInput();
  virtual ~GamepadInput();

  // Polls GLFW gamepad state once per frame and computes edge transitions
  void Poll( float iDeltaTime );

  bool IsConnected() const { return _Connected; }
  int GetJoystickId() const { return _JoystickId; }
  int GetDeviceSlot() const { return _DeviceSlot; }
  void SetDeviceSlot( int iSlot ) { _DeviceSlot = iSlot; }
  const std::string & GetName() const { return _GamepadName; }

  // Standard GLFW_GAMEPAD_BUTTON_* queries
  bool IsButtonDown( int iButton ) const;
  bool IsButtonPressed( int iButton ) const;
  bool IsButtonReleased( int iButton ) const;

  // Standard GLFW_GAMEPAD_AXIS_* queries with deadzone filtering
  float GetAxis( int iAxis, float iDeadzone = 0.20f ) const;

  // Normalized trigger value in [0.0, 1.0] for GLFW_GAMEPAD_AXIS_LEFT_TRIGGER / RIGHT_TRIGGER
  float GetTrigger( int iTriggerAxis, float iDeadzone = 0.10f ) const;

  bool IsLeftTriggerDown() const { return _CurLeftTriggerDown; }
  bool IsLeftTriggerPressed() const { return ( _CurLeftTriggerDown && !_PrevLeftTriggerDown ); }
  bool IsRightTriggerDown() const { return _CurRightTriggerDown; }
  bool IsRightTriggerPressed() const { return ( _CurRightTriggerDown && !_PrevRightTriggerDown ); }

  // Combined directional queries (D-Pad + optional Left Analog Stick with hysteresis)
  bool IsDirDown( GamepadDir iDir, bool iIncludeLeftStick = true ) const;
  bool IsDirPressed( GamepadDir iDir, bool iIncludeLeftStick = true ) const;
  bool IsDirReleased( GamepadDir iDir, bool iIncludeLeftStick = true ) const;

  // Strict vertical Left Stick Up rising-edge check (prevents diagonal mis-drops)
  bool IsStrictStickUpPressed() const { return ( _CurStrictStickUp && !_PrevStrictStickUp ); }

  // Auto-repeating directional navigation pulse for menus
  bool IsMenuNavRepeated( GamepadDir iDir ) const;

  bool GetAllowStickHardDrop() const { return _AllowStickHardDrop; }
  void SetAllowStickHardDrop( bool iAllow ) { _AllowStickHardDrop = iAllow; }

  float GetStickDeadzone() const { return _StickDeadzone; }
  void SetStickDeadzone( float iDeadzone ) { _StickDeadzone = iDeadzone; }

protected:
  static float ApplyDeadzone( float iValue, float iDeadzone );
  static int DpadButtonForDir( GamepadDir iDir );

protected:
  bool          _Connected          = false;
  int           _DeviceSlot         = 0;
  int           _JoystickId         = -1;
  std::string   _GamepadName        = "No Gamepad";

  unsigned char _CurButtons[15]     = {};
  unsigned char _PrevButtons[15]    = {};

  float         _CurAxes[6]         = {};
  float         _PrevAxes[6]        = {};

  bool          _CurStickDir[4]     = {};
  bool          _PrevStickDir[4]    = {};

  bool          _CurStrictStickUp   = false;
  bool          _PrevStrictStickUp  = false;

  bool          _CurLeftTriggerDown  = false;
  bool          _PrevLeftTriggerDown = false;
  bool          _CurRightTriggerDown  = false;
  bool          _PrevRightTriggerDown = false;

  float         _NavHoldTimers[4]   = {};
  bool          _NavRepeatFired[4]  = {};

  float         _StickDeadzone      = 0.20f;
  bool          _AllowStickHardDrop = true;
};

} // namespace RTRT

#endif /* _GamepadInput_ */
