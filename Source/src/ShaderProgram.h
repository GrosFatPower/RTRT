#ifndef _ShaderProgram_
#define _ShaderProgram_

#include "Shader.h"
#include <vector>
#include <GL/glew.h>

namespace RTRT
{

class ShaderProgram
{
public:

  ShaderProgram(const std::vector<Shader> iShaders);
  virtual ~ShaderProgram();

  void Use();
  void StopUsing();

  GLuint GetShaderProgramID() const { return _ShaderProgramID; }

  static ShaderProgram * LoadShaders(const ShaderSource & iVertexShaderSrc, const ShaderSource & iFramentShaderSrc);

private:

  GLuint _ShaderProgramID;
};

}

#endif /* _ShaderProgram_ */
