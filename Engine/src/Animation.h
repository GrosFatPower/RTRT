#ifndef _Animation_
#define _Animation_

#include <string>
#include <vector>
#include "MathUtil.h"

namespace RTRT
{

enum class AnimationInterpolation
{
  Linear = 0,
  Step,
  CubicSpline
};

enum class AnimationTargetPath
{
  Translation = 0,
  Rotation,
  Scale,
  Weights
};

class AnimationSampler
{
public:
  AnimationInterpolation _Interpolation = AnimationInterpolation::Linear;
  std::vector<float>     _InputTimes;
  std::vector<Vec4>      _OutputValues; // Vec3 packed in (x,y,z,0) or Quat in (x,y,z,w); CubicSpline stores 3x Vec4 per key (inTangent, value, outTangent)

  float GetMinTime() const;
  float GetMaxTime() const;

  Vec3 SampleVec3( float iTime ) const;
  Vec4 SampleQuat( float iTime ) const;

private:
  void FindKeyframeInterval( float iTime, size_t & oIdx0, size_t & oIdx1, float & oT, float & oDeltaTime ) const;
};

class AnimationChannel
{
public:
  int                 _SamplerIndex = -1;
  int                 _TargetNodeIndex = -1;
  AnimationTargetPath _Path = AnimationTargetPath::Translation;
};

class AnimationClip
{
public:
  std::string                   _Name;
  float                         _Duration = 0.0f;
  std::vector<AnimationSampler> _Samplers;
  std::vector<AnimationChannel> _Channels;

  void ComputeDuration();
};

class SceneNode
{
public:
  std::string      _Name;
  int              _ParentIndex = -1;
  std::vector<int> _ChildrenIndices;

  // Bind / initial local TRS and local fallback matrix
  Vec3   _InitialTranslation = Vec3(0.0f);
  Vec4   _InitialRotation    = Vec4(0.0f, 0.0f, 0.0f, 1.0f); // (x, y, z, w)
  Vec3   _InitialScale       = Vec3(1.0f);
  bool   _HasTRS             = true;
  Mat4x4 _InitialLocalMatrix = Mat4x4(1.0f);

  // Animated local TRS
  Vec3   _Translation        = Vec3(0.0f);
  Vec4   _Rotation           = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
  Vec3   _Scale              = Vec3(1.0f);

  Mat4x4 _LocalTransform     = Mat4x4(1.0f);
  Mat4x4 _GlobalTransform    = Mat4x4(1.0f);

  std::vector<int> _MeshInstanceIndices;

  Mat4x4 ComputeLocalTransform() const;
  void   ResetToInitialPose();
};

class Skin
{
public:
  std::string         _Name;
  int                 _SkeletonRootNode = -1;
  int                 _OwnerNodeIndex   = -1;
  std::vector<int>    _JointNodeIndices;
  std::vector<Mat4x4> _InverseBindMatrices;
};

class AnimationState
{
public:
  bool  _Playing     = false;
  bool  _Loop        = true;
  int   _ActiveClip  = 0;
  float _CurrentTime = 0.0f;
  float _Speed       = 1.0f;
};

} // namespace RTRT

#endif // _Animation_
