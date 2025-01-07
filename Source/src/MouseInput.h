#ifndef _MouseInput_
#define _MouseInput_

#include <map>
#include <queue>

namespace RTRT
{

struct MouseEvent
{
  int    _Action;
  double _MouseX = 0.;
  double _MouseY = 0.;
};

class MouseInput
{
public:
MouseInput();
virtual ~MouseInput();

bool IsButtonPressed(int iButton) const;
bool IsButtonReleased(int iButton) const;

void AddEvent(int iButton, int iAction, double iMouseX, double iMouseY);
void ClearEvents();

protected:

std::map<int, std::queue<MouseEvent>> _ButtonEvents;
};

}

#endif /* _MouseInput_ */
