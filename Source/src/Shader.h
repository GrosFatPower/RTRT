#ifndef _Shader_
#define _Shader_

#include <string>
#include <GL/glew.h>

#define ShaderSource RTRT::Shader::Source

namespace RTRT
{

class Shader
{
public:

  struct Source
  {
    std::string _Src;
    std::string _Path;
  };

public:

  Shader(const ShaderSource & iShaderSource, GLuint iShaderType);
  virtual ~Shader();

  static ShaderSource LoadShader(std::string iShaderPath, std::string iIncludeIndentifier = "#include");
  
  GLuint GetShaderType() const { return _ShaderType; }
  GLuint GetShaderID() const { return _ShaderID; }

private:

  GLuint _ShaderType;
  GLuint _ShaderID;
};

}

#endif /* _Shader_ */
