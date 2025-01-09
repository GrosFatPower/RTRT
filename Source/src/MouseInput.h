#ifndef _MouseInput_
#define _MouseInput_

#include <map>
#include <queue>

namespace RTRT
{

struct MouseEvent
{
  int _Action = 0;
  union { double _MouseX = 0., _OffsetX; };
  union { double _MouseY = 0., _OffsetY; };
};

class MouseInput
{
public:
MouseInput();
virtual ~MouseInput();

bool IsButtonPressed(const int iButton, double & oMouseX, double & oMouseY) const;
bool IsButtonReleased(const int iButton, double & oMouseX, double & oMouseY) const;
bool IsScrolled(double & oOffsetX, double & oOffsetY) const;

void AddButtonEvent(const int iButton, const int iAction, const double iMouseX, const double iMouseY);
void AddScrollEvent(const double iOffsetX, const double iOffsetY);
void ClearLastEvents(const double iCurMouseX, const double iCurMouseY);

protected:

std::map<int, std::queue<MouseEvent>> _ButtonEvents;
std::queue<MouseEvent>                _ScrollEvents;
};

}

#endif /* _MouseInput_ */
