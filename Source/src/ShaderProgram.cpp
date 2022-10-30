/*
  Derived from Program.h (MIT License)
  https://github.com/knightcrawler25/GLSL-PathTracer.git
*/

#include "ShaderProgram.h"
#include <stdexcept>

namespace RTRT
{

ShaderProgram::ShaderProgram(const std::vector<Shader> iShaders)
: _ShaderProgramID(0)
{
  _ShaderProgramID = glCreateProgram();

  for ( unsigned int i = 0; i < iShaders.size(); ++i )
    glAttachShader(_ShaderProgramID, iShaders[i].GetShaderID());

  glLinkProgram(_ShaderProgramID);

  for ( unsigned int i = 0; i < iShaders.size(); ++i )
  {
    glDetachShader(_ShaderProgramID, iShaders[i].GetShaderID());
    glDeleteProgram(iShaders[i].GetShaderID());
  }

  GLint success = 0;
  glGetProgramiv(_ShaderProgramID, GL_LINK_STATUS, &success);
  if ( success == GL_FALSE )
  {
    std::string msg("Error while linking program\n");
    GLint logSize = 0;
    glGetProgramiv(_ShaderProgramID, GL_INFO_LOG_LENGTH, &logSize);
    char* info = new char[logSize + 1];
    glGetShaderInfoLog(_ShaderProgramID, logSize, NULL, info);
    msg += info;
    delete[] info;
    glDeleteProgram(_ShaderProgramID);
    _ShaderProgramID = 0;
    printf("Error %s\n", msg.c_str());
    throw std::runtime_error(msg.c_str());
  }
}

ShaderProgram::~ShaderProgram()
{
  glDeleteProgram(_ShaderProgramID);
  _ShaderProgramID = 0;
}

void ShaderProgram::Use()
{
  glUseProgram(_ShaderProgramID);
}

void ShaderProgram::StopUsing()
{
  glUseProgram(0);
}

ShaderProgram * ShaderProgram::LoadShaders(const ShaderSource & iVertexShaderSrc, const ShaderSource & iFramentShaderSrc)
{
  std::vector<Shader> shaders;
  shaders.push_back(Shader(iVertexShaderSrc, GL_VERTEX_SHADER));
  shaders.push_back(Shader(iFramentShaderSrc, GL_FRAGMENT_SHADER));
  return new ShaderProgram(shaders);
}

}
