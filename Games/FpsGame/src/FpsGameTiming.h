#ifndef _FpsGameTiming_
#define _FpsGameTiming_

namespace RTRT
{

struct FpsCpuTiming
{
  const char * _Name = "";
  double       _Seconds = 0.;
  bool         _Enabled = false;
};

}

#endif /* _FpsGameTiming_ */
