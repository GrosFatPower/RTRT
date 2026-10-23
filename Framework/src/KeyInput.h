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

bool IsKeyDown(const int iKey) const;
bool IsKeyReleased(const int iKey) const;

void AddEvent(const int iKey, const int iAction, const int iMods);
void ClearLastEvents();

protected:

std::map<int, std::queue<KeyEvent>> _KeyEvents;
};

}

#endif /* _KeyInput_ */
