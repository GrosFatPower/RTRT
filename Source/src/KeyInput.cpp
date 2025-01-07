#include "KeyInput.h"

#include <GLFW/glfw3.h> // Will drag system OpenGL headers

namespace RTRT
{

KeyInput::KeyInput()
{
}

KeyInput::~KeyInput()
{
}

void KeyInput::AddEvent(int iKey, int iAction, int iMods)
{
  _KeyEvents[iKey].push({iAction, iMods});
}

bool KeyInput::IsKeyDown( int iKey ) const
{
  auto it = _KeyEvents.find(iKey);
  if ( it != _KeyEvents.end() )
  {
    const std::queue<KeyEvent> & events = it -> second;
    if ( events.size() && ( events.back()._Action == GLFW_PRESS ) )
      return true;
  }

  return false;
}

bool KeyInput::IsKeyReleased( int iKey ) const
{
  auto it = _KeyEvents.find(iKey);
  if ( it != _KeyEvents.end() )
  {
    const std::queue<KeyEvent> & events = it -> second;
    if ( events.size() && ( events.back()._Action == GLFW_RELEASE ) )
      return true;
  }

  return false;
}

void KeyInput::ClearEvents()
{
  for ( auto & [key, events] : _KeyEvents )
  {
    if ( events.size() )
    {
      KeyEvent lastEvent = events.back();
      events = {};

      if ( lastEvent._Action == GLFW_PRESS )
        events.push(lastEvent);
    }
  }
}

}
