#ifndef _KeyInput_
#define _KeyInput_

#include <map>
#include <queue>

namespace RTRT
{

struct KeyEvent
{
  int _Action = 0;
  int _Mods   = 0;
};

class KeyInput
{
public:
KeyInput();
virtual ~KeyInput();

bool IsKeyDown(int iKey) const;
bool IsKeyReleased(int iKey) const;

void AddEvent(int iKey, int iAction, int iMods);
void ClearEvents();

protected:

std::map<int, std::queue<KeyEvent>> _KeyEvents;
};

}

#endif /* _KeyInput_ */
