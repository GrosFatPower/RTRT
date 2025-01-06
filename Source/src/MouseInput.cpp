#include "MouseInput.h"

#include <GLFW/glfw3.h> // Will drag system OpenGL headers

namespace RTRT
{

MouseInput::MouseInput()
{
}

MouseInput::~MouseInput()
{
}

void MouseInput::AddEvent(int iButton, int iAction)
{
  _ButtonEvents[iButton].push(iAction);
}

bool MouseInput::IsButtonPressed( int iButton ) const
{
  auto it = _ButtonEvents.find(iButton);
  if ( it != _ButtonEvents.end() )
  {
    const std::queue<int> & events = it -> second;
    if ( events.size() && ( events.back() == GLFW_PRESS ) )
      return true;
  }

  return false;
}

bool MouseInput::IsButtonReleased( int iButton ) const
{
  auto it = _ButtonEvents.find(iButton);
  if ( it != _ButtonEvents.end() )
  {
    const std::queue<int> & events = it -> second;
    if ( events.size() && ( events.back() == GLFW_RELEASE ) )
      return true;
  }

  return false;
}

void MouseInput::ClearEvents()
{
  for ( auto & [Button, events] : _ButtonEvents )
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
