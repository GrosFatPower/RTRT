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

void MouseInput::AddEvent(int iButton, int iAction, double iMouseX, double iMouseY)
{
  _ButtonEvents[iButton].push({iAction, iMouseX, iMouseY});
}

bool MouseInput::IsButtonPressed( int iButton ) const
{
  auto it = _ButtonEvents.find(iButton);
  if ( it != _ButtonEvents.end() )
  {
    const std::queue<MouseEvent> & events = it -> second;
    if ( events.size() && ( events.back()._Action == GLFW_PRESS ) )
      return true;
  }

  return false;
}

bool MouseInput::IsButtonReleased( int iButton ) const
{
  auto it = _ButtonEvents.find(iButton);
  if ( it != _ButtonEvents.end() )
  {
    const std::queue<MouseEvent> & events = it -> second;
    if ( events.size() && ( events.back()._Action == GLFW_RELEASE ) )
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
      MouseEvent lastEvent = events.back();
      events = {};

      if ( lastEvent._Action == GLFW_PRESS )
        events.push(lastEvent);
    }
  }
}

}
