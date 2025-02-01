/*
  Derived from Program.h (MIT License)
  https://github.com/knightcrawler25/GLSL-PathTracer.git
*/

#include "ShaderProgram.h"
#include <stdexcept>

#include "glm/gtc/type_ptr.hpp"

namespace RTRT
{

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
ShaderProgram::ShaderProgram(const std::vector<std::shared_ptr<Shader>> & iShaders)
: _ShaderProgramID(0)
{
  _ShaderProgramID = glCreateProgram();

  for ( unsigned int i = 0; i < iShaders.size(); ++i )
    glAttachShader(_ShaderProgramID, iShaders[i] -> GetShaderID());

  glLinkProgram(_ShaderProgramID);

  for ( unsigned int i = 0; i < iShaders.size(); ++i )
    glDetachShader(_ShaderProgramID, iShaders[i] -> GetShaderID());

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

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
ShaderProgram::~ShaderProgram()
{
  glDeleteProgram(_ShaderProgramID);
  _ShaderProgramID = 0;
}

// ----------------------------------------------------------------------------
// Use
// ----------------------------------------------------------------------------
void ShaderProgram::Use()
{
  glUseProgram(_ShaderProgramID);
}

// ----------------------------------------------------------------------------
// StopUsing
// ----------------------------------------------------------------------------
void ShaderProgram::StopUsing()
{
  glUseProgram(0);
}

// ----------------------------------------------------------------------------
// LoadShaders
// ----------------------------------------------------------------------------
ShaderProgram * ShaderProgram::LoadShaders(const ShaderSource & iVertexShaderSrc, const ShaderSource & iFramentShaderSrc)
{
  std::vector<std::shared_ptr<Shader>> shaders;
  shaders.push_back(std::make_shared<Shader>(iVertexShaderSrc, GL_VERTEX_SHADER));
  shaders.push_back(std::make_shared<Shader>(iFramentShaderSrc, GL_FRAGMENT_SHADER));
  return new ShaderProgram(shaders);
}

// ----------------------------------------------------------------------------
// LoadShaders
// ----------------------------------------------------------------------------
ShaderProgram * ShaderProgram::LoadShaders(const ShaderSource & iComputeShaderSrc)
{
  std::vector<std::shared_ptr<Shader>> shaders;
  shaders.push_back(std::make_shared<Shader>(iComputeShaderSrc, GL_COMPUTE_SHADER));
  return new ShaderProgram(shaders);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, int iVal )
{
  glUniform1i(glGetUniformLocation(_ShaderProgramID, iName.c_str()), iVal);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, int iVal1, int iVal2)
{
  glUniform2i(glGetUniformLocation(_ShaderProgramID, iName.c_str()), iVal1, iVal2);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, int iVal1, int iVal2, int iVal3)
{
  glUniform3i(glGetUniformLocation(_ShaderProgramID, iName.c_str()), iVal1, iVal2, iVal3);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, int iVal1, int iVal2, int iVal3, int iVal4)
{
  glUniform4i(glGetUniformLocation(_ShaderProgramID, iName.c_str()), iVal1, iVal2, iVal3, iVal4);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, float iVal )
{
  glUniform1f(glGetUniformLocation(_ShaderProgramID, iName.c_str()), iVal);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, float iVal1, float iVal2 )
{
  glUniform2f(glGetUniformLocation(_ShaderProgramID, iName.c_str()), iVal1, iVal2);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, float iVal1, float iVal2, float iVal3 )
{
  glUniform3f(glGetUniformLocation(_ShaderProgramID, iName.c_str()), iVal1, iVal2, iVal3);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, float iVal1, float iVal2, float iVal3, float iVal4 )
{
  glUniform4f(glGetUniformLocation(_ShaderProgramID, iName.c_str()), iVal1, iVal2, iVal3, iVal4);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, Vec2 iVal )
{
  this -> SetUniform(iName, iVal.x, iVal.y);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, Vec3 iVal )
{
  this -> SetUniform(iName, iVal.x, iVal.y, iVal.z);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, Vec4 iVal )
{
  this -> SetUniform(iName, iVal.x, iVal.y, iVal.z, iVal.w);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, Vec2i iVal )
{
  this -> SetUniform(iName, iVal.x, iVal.y);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, Vec3i iVal )
{
  this -> SetUniform(iName, iVal.x, iVal.y, iVal.z);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, Vec4i iVal )
{
  this -> SetUniform(iName, iVal.x, iVal.y, iVal.z, iVal.w);
}

// ----------------------------------------------------------------------------
// SetUniform
// ----------------------------------------------------------------------------
void ShaderProgram::SetUniform( const std::string & iName, Mat4x4 iVal )
{
  glUniformMatrix4fv(glGetUniformLocation(_ShaderProgramID, iName.c_str()), 1, GL_FALSE, glm::value_ptr(iVal));
}

}
