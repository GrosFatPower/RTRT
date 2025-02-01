#ifndef _ShaderProgram_
#define _ShaderProgram_

#include "Shader.h"
#include "MathUtil.h"
#include <vector>
#include <string>
#include <memory>
#include <GL/glew.h>

namespace RTRT
{

class ShaderProgram
{
public:

  ShaderProgram(const std::vector<std::shared_ptr<Shader>> & iShaders);
  virtual ~ShaderProgram();

  void Use();
  void StopUsing();

  GLuint GetShaderProgramID() const { return _ShaderProgramID; }

  void SetUniform(const std::string & iName, int iVal);
  void SetUniform(const std::string & iName, int iVal1, int iVal2);
  void SetUniform(const std::string & iName, int iVal1, int iVal2, int iVal3);
  void SetUniform(const std::string & iName, int iVal1, int iVal2, int iVal3, int iVal4);
  void SetUniform(const std::string & iName, float iVal);
  void SetUniform(const std::string & iName, float iVal1, float iVal2);
  void SetUniform(const std::string & iName, float iVal1, float iVal2, float iVal3);
  void SetUniform(const std::string & iName, float iVal1, float iVal2, float iVal3, float iVal4);
  void SetUniform(const std::string & iName, Vec2 iVal);
  void SetUniform(const std::string & iName, Vec3 iVal);
  void SetUniform(const std::string & iName, Vec4 iVal);
  void SetUniform(const std::string & iName, Vec2i iVal);
  void SetUniform(const std::string & iName, Vec3i iVal);
  void SetUniform(const std::string & iName, Vec4i iVal);
  void SetUniform(const std::string & iName, Mat4x4 iVal);

  static ShaderProgram * LoadShaders(const ShaderSource & iVertexShaderSrc, const ShaderSource & iFramentShaderSrc);
  static ShaderProgram * LoadShaders(const ShaderSource & iComputeShaderSrc);

private:

  GLuint _ShaderProgramID;
};

}

#endif /* _ShaderProgram_ */
