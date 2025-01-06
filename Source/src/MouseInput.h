#ifndef _MouseInput_
#define _MouseInput_

#include <map>
#include <queue>

namespace RTRT
{

class MouseInput
{
public:
MouseInput();
virtual ~MouseInput();

bool IsButtonPressed(int iButton) const;
bool IsButtonReleased(int iButton) const;

void AddEvent(int iButton, int iAction);
void ClearEvents();

protected:

std::map<int, std::queue<int>> _ButtonEvents;
};

}

#endif /* _MouseInput_ */
