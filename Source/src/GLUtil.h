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
  GLuint            _Handle = 0;
  GLenum            _Target;
  const TextureSlot _Slot;
  GLint             _InternalFormat = GL_RGBA32F;
  GLenum            _DataFormat     = GL_RGBA;
  GLenum            _DataType       = GL_FLOAT;
};

struct GLFrameBuffer
{
  GLuint                 _Handle = 0;
  std::vector<GLTexture> _Tex;
};

struct GLTextureBuffer
{
  GLuint    _Handle = 0;
  GLTexture _Tex;
};

class GLUtil
{
public:

// DeleteTEX
static void DeleteTEX( GLTexture & ioTEX )
{
  if ( ioTEX._Handle)
    glDeleteTextures(1, &ioTEX._Handle);
  ioTEX._Handle = 0;
}

// DeleteFBO
static void DeleteFBO( GLFrameBuffer & ioFBO )
{
  if ( ioFBO._Handle)
    glDeleteFramebuffers(1, &ioFBO._Handle);
  for ( auto tex : ioFBO._Tex )
    DeleteTEX(tex);
  ioFBO._Handle;
}

// DeleteTBO
static void DeleteTBO( GLTextureBuffer & ioTBO )
{
  if ( ioTBO._Handle)
    glDeleteBuffers(1, &ioTBO._Handle);
  DeleteTEX(ioTBO._Tex);
  ioTBO._Handle = 0;
}

// InitializeTBO
static void InitializeTBO( GLTextureBuffer & ioTBO, GLsizeiptr iSize, const void * iData, GLenum iInternalformat )
{
  glGenBuffers(1, &ioTBO._Handle);
  glBindBuffer(GL_TEXTURE_BUFFER, ioTBO._Handle);
  glBufferData(GL_TEXTURE_BUFFER, iSize, iData, GL_STATIC_DRAW);
  glGenTextures(1, &ioTBO._Tex._Handle);
  glBindTexture(GL_TEXTURE_BUFFER, ioTBO._Tex._Handle);
  glTexBuffer(GL_TEXTURE_BUFFER, iInternalformat, ioTBO._Handle);
}

// ResizeTexture
static void ResizeTexture( GLTexture & ioTex, GLsizei iWidth, GLsizei iHeight )
{
  glBindTexture(GL_TEXTURE_2D, ioTex._Handle);
  glTexImage2D(GL_TEXTURE_2D, 0, ioTex._InternalFormat, iWidth, iHeight, 0, ioTex._DataFormat, ioTex._DataType, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);
}

// ResizeFBO
static void ResizeFBO( GLFrameBuffer & ioFBO, GLsizei iWidth, GLsizei iHeight )
{
  for ( auto tex : ioFBO._Tex )
    ResizeTexture( tex, iWidth, iHeight );

  glBindFramebuffer(GL_FRAMEBUFFER, ioFBO._Handle);
  glClear(GL_COLOR_BUFFER_BIT);
}

// ActivateTexture
static void ActivateTexture( GLTexture & iTex )
{
  glActiveTexture(GL_TEX_UNIT(iTex));
  glBindTexture(iTex._Target, iTex._Handle);
}

// ActivateTexture
static void ActivateTextures( GLFrameBuffer & ioFBO )
{
  for ( auto tex : ioFBO._Tex )
    ActivateTexture(tex);
}

// LoadTexture
static void LoadTexture( GLsizei iWidth, GLsizei iHeight, const void * iData, GLTexture & ioTex )
{
  glBindTexture(ioTex._Target, ioTex._Handle);
  glTexImage2D(ioTex._Target, 0, ioTex._InternalFormat, iWidth, iHeight, 0, ioTex._DataFormat, ioTex._DataType, iData);
  glTexParameteri(ioTex._Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(ioTex._Target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(ioTex._Target, 0);
}

// GenTexture
static void GenTexture( GLenum iTarget, GLint iInternalformat, GLsizei iWidth, GLsizei iHeight, GLenum iFormat, GLenum iType, const void * iData, GLTexture & ioTex )
{
  ioTex._Target         = iTarget;
  ioTex._InternalFormat = iInternalformat;
  ioTex._DataFormat     = iFormat;
  ioTex._DataType       = iType;
  glGenTextures(1, &ioTex._Handle);

  LoadTexture(iWidth, iHeight, iData, ioTex);
}

// UniformArrayElementName
static std::string UniformArrayElementName( const char * iUniformArrayName, int iIndex, const char * iAttributeName )
{
  return std::string(iUniformArrayName).append("[").append(std::to_string(iIndex)).append("].").append(iAttributeName);
}

};

}

#endif /* _GLUtil_ */
