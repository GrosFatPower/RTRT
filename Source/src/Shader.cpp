/*
  Derived from ShaderInclude.h & Shader.h (MIT License)
  https://github.com/knightcrawler25/GLSL-PathTracer.git
*/

#include "Shader.h"

#include <iostream>
#include <fstream>
#include <sstream>

// Remove the file name and store the path to this folder
void GetFilePath(const std::string & iFullPath, std::string& oPathWithoutFileName)
{
  size_t found = iFullPath.find_last_of("/\\");
  oPathWithoutFileName = iFullPath.substr(0, found + 1);
}

namespace RTRT
{

Shader::Shader(const ShaderSource & iShaderSource, GLuint iShaderType)
: _ShaderType(iShaderType)
, _ShaderID(0)
{
  _ShaderID = glCreateShader(_ShaderType);

  if ( 0 )
    std::cout << iShaderSource._Src << std::endl;

  printf("Compiling Shader %s\n", iShaderSource._Path.c_str());
  const GLchar* src = (const GLchar*)iShaderSource._Src.c_str();
    
  glShaderSource(_ShaderID, 1, &src, 0);
  glCompileShader(_ShaderID);

  GLint success = 0;
  glGetShaderiv(_ShaderID, GL_COMPILE_STATUS, &success);
  if ( success == GL_FALSE )
  {
    std::string msg;
    GLint logSize = 0;
    glGetShaderiv(_ShaderID, GL_INFO_LOG_LENGTH, &logSize);
    char* info = new char[logSize + 1];
    glGetShaderInfoLog(_ShaderID, logSize, NULL, info);
    msg += iShaderSource._Path;
    msg += "\n";
    msg += info;
    delete[] info;
    glDeleteShader(_ShaderID);
    _ShaderID = 0;
    printf("Shader compilation error %s\n", msg.c_str());
    throw std::runtime_error(msg.c_str());
  }
}

Shader::~Shader()
{
  //glDeleteShader(_ShaderID);
  _ShaderID = 0;
}

ShaderSource Shader::LoadShader(std::string iShaderPath, std::string iIncludeIndentifier)
{
  iIncludeIndentifier += ' ';
  static bool isRecursiveCall = false;

  std::string fullSourceCode = "";
  std::ifstream file(iShaderPath);

  if (!file.is_open())
  {
    std::cerr << "ERROR: could not open the shader at: " << iShaderPath << "\n" << std::endl;
    return ShaderSource{ fullSourceCode, iShaderPath };
  }

  std::string lineBuffer;
  while (std::getline(file, lineBuffer))
  {
    // Look for the new shader include identifier
    if (lineBuffer.find(iIncludeIndentifier) != lineBuffer.npos)
    {
      // Remove the include identifier, this will cause the path to remain
      lineBuffer.erase(0, iIncludeIndentifier.size());

      // The include path is relative to the current shader file path
      std::string pathOfThisFile;
      GetFilePath(iShaderPath, pathOfThisFile);
      lineBuffer.insert(0, pathOfThisFile);

      // By using recursion, the new include file can be extracted
      // and inserted at this location in the shader source code
      isRecursiveCall = true;
      fullSourceCode += LoadShader(lineBuffer)._Src;

      // Do not add this line to the shader source code, as the include
      // path would generate a compilation issue in the final source code
      continue;
    }

    fullSourceCode += lineBuffer + '\n';
  }

  // Only add the null terminator at the end of the complete file,
  // essentially skipping recursive function calls this way
  if (!isRecursiveCall)
    fullSourceCode += '\0';

  file.close();

  return ShaderSource{ fullSourceCode, iShaderPath };
}

}
