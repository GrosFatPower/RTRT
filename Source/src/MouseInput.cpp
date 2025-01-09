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

void MouseInput::AddButtonEvent(const int iButton, const int iAction, const double iMouseX, const double iMouseY)
{
  _ButtonEvents[iButton].push({iAction, iMouseX, iMouseY});
}

void MouseInput::AddScrollEvent(const double iOffsetX, const double iOffsetY)
{
  _ScrollEvents.push({0, iOffsetX, iOffsetY});
}

bool MouseInput::IsButtonPressed( const int iButton, double & oMouseX, double & oMouseY ) const
{
  auto it = _ButtonEvents.find(iButton);
  if ( it != _ButtonEvents.end() )
  {
    const std::queue<MouseEvent> & events = it -> second;
    if ( events.size() && ( events.back()._Action == GLFW_PRESS ) )
    {
      oMouseX = events.back()._MouseX;
      oMouseY = events.back()._MouseY;
      return true;
    }
  }

  return false;
}

bool MouseInput::IsButtonReleased( const int iButton, double & oMouseX, double & oMouseY ) const
{
  auto it = _ButtonEvents.find(iButton);
  if ( it != _ButtonEvents.end() )
  {
    const std::queue<MouseEvent> & events = it -> second;
    if ( events.size() && ( events.back()._Action == GLFW_RELEASE ) )
    {
      oMouseX = events.back()._MouseX;
      oMouseY = events.back()._MouseY;
      return true;
    }
  }

  return false;
}

bool MouseInput::IsScrolled(double & oOffsetX, double & oOffsetY) const
{
  if ( _ScrollEvents.size() )
  {
    oOffsetX = _ScrollEvents.back()._OffsetX;
    oOffsetY = _ScrollEvents.back()._OffsetY;
    return true;
  }

  return false;
}

void MouseInput::ClearLastEvents( const double iCurMouseX, const double iCurMouseY )
{
  for ( auto & [Button, events] : _ButtonEvents )
  {
    if ( events.size() )
    {
      MouseEvent lastEvent = events.back();
      events = {};

      if ( lastEvent._Action == GLFW_PRESS )
      {
        lastEvent._MouseX = iCurMouseX;
        lastEvent._MouseY = iCurMouseY;
        events.push(lastEvent);
      }
    }
  }

  _ScrollEvents = {};
}

}
