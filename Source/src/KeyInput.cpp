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

void KeyInput::AddEvent(int iKey, int iAction)
{
  _KeyEvents[iKey].push(iAction);
}

bool KeyInput::IsKeyDown( int iKey ) const
{
  auto it = _KeyEvents.find(iKey);
  if ( it != _KeyEvents.end() )
  {
    const std::queue<int> & events = it -> second;
    if ( events.size() && ( events.back() == GLFW_PRESS ) )
      return true;
  }

  return false;
}

bool KeyInput::IsKeyReleased( int iKey ) const
{
  auto it = _KeyEvents.find(iKey);
  if ( it != _KeyEvents.end() )
  {
    const std::queue<int> & events = it -> second;
    if ( events.size() && ( events.back() == GLFW_RELEASE ) )
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
      if ( events.back() == GLFW_PRESS )
      {
        events = {};
        events.push(GLFW_PRESS);
      }
      else
        events = {};
    }
  }
}

}
