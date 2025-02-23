#ifndef _GLUtil_
#define _GLUtil_

#include <string>

#include <GL/glew.h>


#define GL_TEX_UNIT(x) ( GL_TEXTURE0 + (unsigned int)x._Slot )

namespace RTRT
{

using TextureSlot = unsigned int;

struct GLTexture
{
  GLuint            _ID;
  GLenum            _Target;
  const TextureSlot _Slot;
};

struct GLFrameBuffer
{
  GLuint    _ID;
  GLTexture _Tex;
};

struct GLTextureBuffer
{
  GLuint    _ID;
  GLTexture _Tex;
};

class GLUtil
{
public:

// DeleteTEX
static void DeleteTEX( GLTexture & ioTEX )
{
  if ( ioTEX._ID )
    glDeleteTextures(1, &ioTEX._ID);
  ioTEX._ID = 0;
}

// DeleteFBO
static void DeleteFBO( GLFrameBuffer & ioFBO )
{
  if ( ioFBO._ID )
    glDeleteFramebuffers(1, &ioFBO._ID);
  DeleteTEX(ioFBO._Tex);
  ioFBO._ID;
}

// DeleteTBO
static void DeleteTBO( GLTextureBuffer & ioTBO )
{
  if ( ioTBO._ID )
    glDeleteBuffers(1, &ioTBO._ID);
  DeleteTEX(ioTBO._Tex);
  ioTBO._ID = 0;
}

// InitializeTBO
static void InitializeTBO( GLTextureBuffer & ioTBO, GLsizeiptr iSize, const void * iData, GLenum iInternalformat )
{
  glGenBuffers(1, &ioTBO._ID);
  glBindBuffer(GL_TEXTURE_BUFFER, ioTBO._ID);
  glBufferData(GL_TEXTURE_BUFFER, iSize, iData, GL_STATIC_DRAW);
  glGenTextures(1, &ioTBO._Tex._ID);
  glBindTexture(GL_TEXTURE_BUFFER, ioTBO._Tex._ID);
  glTexBuffer(GL_TEXTURE_BUFFER, iInternalformat, ioTBO._ID);
}

// ResizeFBO
static void ResizeFBO( GLFrameBuffer & ioFBO, GLint iInternalFormat, GLsizei iWidth, GLsizei iHeight, GLenum iFormat, GLenum iType )
{
  glBindTexture(GL_TEXTURE_2D, ioFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, iInternalFormat, iWidth, iHeight, 0, iFormat, iType, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, ioFBO._ID);
  glClear(GL_COLOR_BUFFER_BIT);
}

// ActivateTexture
static void ActivateTexture( GLTexture & iTex )
{
  glActiveTexture(GL_TEX_UNIT(iTex));
  glBindTexture(iTex._Target, iTex._ID);
}

// LoadTexture
static void LoadTexture( GLenum iTarget, GLint iInternalformat, GLsizei iWidth, GLsizei iHeight, GLenum iFormat, GLenum iType, const void * iData, GLTexture & ioTex )
{
  glGenTextures(1, &ioTex._ID);
  glBindTexture(iTarget, ioTex._ID);
  glTexImage2D(iTarget, 0, iInternalformat, iWidth, iHeight, 0, iFormat, iType, iData);
  glTexParameteri(iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(iTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(iTarget, 0);
}

// UniformArrayElementName
static std::string UniformArrayElementName( const char * iUniformArrayName, int iIndex, const char * iAttributeName )
{
  return std::string(iUniformArrayName).append("[").append(std::to_string(iIndex)).append("].").append(iAttributeName);
}

};

}

#endif /* _GLUtil_ */
