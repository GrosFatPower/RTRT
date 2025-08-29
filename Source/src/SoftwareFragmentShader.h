#ifndef _SoftwareFragmentShader_
#define _SoftwareFragmentShader_

#include "RasterData.h"
#include "MathUtil.h"
#include "SIMDUtils.h"

namespace RTRT
{

class SoftwareFragmentShader
{
public:

  SoftwareFragmentShader() {}
  virtual ~SoftwareFragmentShader() {}

  virtual Vec4 Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri) = 0;

protected:

};

class BlinnPhongFragmentShader : public SoftwareFragmentShader
{
public:

  BlinnPhongFragmentShader(const RasterData::DefaultUniform & iUniforms) : SoftwareFragmentShader(), _Uniforms(iUniforms) { }

  virtual ~BlinnPhongFragmentShader(){}

  virtual Vec4 Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri);

protected:

  RasterData::DefaultUniform _Uniforms;
};

class PBRFragmentShader : public SoftwareFragmentShader
{
public:

  PBRFragmentShader(const RasterData::DefaultUniform & iUniforms) : SoftwareFragmentShader(), _Uniforms(iUniforms) { }

  virtual ~PBRFragmentShader(){}

  virtual Vec4 Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri);

protected:

  RasterData::DefaultUniform _Uniforms;
};

class DepthFragmentShader : public SoftwareFragmentShader
{
public:

  DepthFragmentShader(const RasterData::DefaultUniform& iUniforms) : SoftwareFragmentShader(), _Uniforms(iUniforms) {}

  virtual ~DepthFragmentShader() {}

  virtual Vec4 Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri);

protected:

  RasterData::DefaultUniform _Uniforms;
};

class NormalFragmentShader : public SoftwareFragmentShader
{
public:

  NormalFragmentShader(const RasterData::DefaultUniform& iUniforms) : SoftwareFragmentShader(), _Uniforms(iUniforms) {}

  virtual ~NormalFragmentShader() {}

  virtual Vec4 Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri);

protected:

  RasterData::DefaultUniform _Uniforms;
};

class WireFrameFragmentShader : public SoftwareFragmentShader
{
public:

  WireFrameFragmentShader(const RasterData::DefaultUniform& iUniforms) : SoftwareFragmentShader(), _Uniforms(iUniforms) {}

  virtual ~WireFrameFragmentShader() {}

  virtual Vec4 Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri);

protected:

  RasterData::DefaultUniform _Uniforms;
};

}

#endif /* _SoftwareFragmentShader_ */
