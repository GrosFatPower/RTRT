#ifndef _FpsGameTiming_
#define _FpsGameTiming_

namespace FPS
{

struct FpsCpuTiming
{
  const char * _Name = "";
  double       _Seconds = 0.;
  bool         _Enabled = false;
};

}

#endif /* _FpsGameTiming_ */
