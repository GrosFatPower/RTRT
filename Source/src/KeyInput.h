#ifndef _KeyInput_
#define _KeyInput_

#include <map>
#include <queue>

namespace RTRT
{

class KeyInput
{
public:
KeyInput();
virtual ~KeyInput();

bool IsKeyDown(int iKey) const;
bool IsKeyReleased(int iKey) const;

void AddEvent(int iKey, int iAction);
void ClearEvents();

protected:

std::map<int, std::queue<int>> _KeyEvents;
};

}

#endif /* _KeyInput_ */
