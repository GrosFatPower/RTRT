#ifndef _Camera_
#define _Camera_

#include "Math.h"

namespace RTRT
{

class Camera
{
public:
  Camera(Vec3 iPos, Vec3 iLookAt, float iFOV);

  Vec3 GetPos()     const { return _Pos; }
  Vec3 GetUp()      const { return _Up; }
  Vec3 GetRight()   const { return _Right; }
  Vec3 GetForward() const { return _Forward; }

  void SetFOV( float iFOV );
  float GetFOV() const { return _FOV; }

private:

  void Update();

  Vec3 _Pos;
  Vec3 _Up;
  Vec3 _Right;
  Vec3 _Forward;
  Vec3 _WorldUp;
  Vec3 _Pivot;

  float _FOV;
  float _Pitch;
  float _Yaw;
  float _Radius;
};

}

#endif /* _Camera_ */