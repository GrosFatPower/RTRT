#ifndef _GLUtil_
#define _GLUtil_

#include <algorithm>
#include <string>
#include <vector>
#include <tuple>
#include <cstddef>

#include <GL/glew.h>


#define GL_TEX_UNIT(x) ( GL_TEXTURE0 + (unsigned int)x._Slot )

namespace RTRT
{

using TextureSlot = unsigned int;

struct GLTexture
{
  GLuint      _Handle         = 0;
  GLenum      _Target         = GL_TEXTURE_2D;
  TextureSlot _Slot           = 0;
  GLint       _InternalFormat = GL_RGBA32F;
  GLenum      _DataFormat     = GL_RGBA;
  GLenum      _DataType       = GL_FLOAT;
};

struct GLFrameBuffer
{
  GLuint                  _Handle = 0;
  std::vector<GLTexture*> _Tex;
};

struct GLTextureBuffer
{
  GLuint    _Handle = 0;
  GLTexture _Tex;
};

struct GLTextureDesc
{
  GLenum      _Target         = GL_TEXTURE_2D;
  TextureSlot _Slot           = 0;
  GLsizei     _Width          = 0;
  GLsizei     _Height         = 0;
  GLsizei     _Depth          = 1;
  GLint       _InternalFormat = GL_RGBA32F;
  GLenum      _DataFormat     = GL_RGBA;
  GLenum      _DataType       = GL_FLOAT;
  const void* _Data           = nullptr;
  GLint       _MinFilter      = GL_LINEAR;
  GLint       _MagFilter      = GL_LINEAR;
  GLint       _WrapS          = GL_REPEAT;
  GLint       _WrapT          = GL_REPEAT;
  GLint       _WrapR          = GL_REPEAT;
  bool        _GenerateMipMap = false;
};

struct GLBufferDesc
{
  GLenum       _Target = GL_ARRAY_BUFFER;
  GLsizeiptr   _Size   = 0;
  const void * _Data   = nullptr;
  GLenum       _Usage  = GL_STATIC_DRAW;
};

struct GLTextureBufferDesc
{
  TextureSlot  _Slot           = 0;
  GLsizeiptr   _Size           = 0;
  const void * _Data           = nullptr;
  GLenum       _Usage          = GL_STATIC_DRAW;
  GLenum       _InternalFormat = GL_RGBA32F;
};

struct GLVertexAttribDesc
{
  GLuint      _Index      = 0;
  GLint       _Size       = 0;
  GLenum      _Type       = GL_FLOAT;
  GLboolean   _Normalized = GL_FALSE;
  GLsizei     _Stride     = 0;
  std::size_t _Offset     = 0;
  bool        _Integer    = false;
};

struct GLFrameBufferAttachmentDesc
{
  GLenum      _Attachment = GL_COLOR_ATTACHMENT0;
  GLTexture * _Tex        = nullptr;
  GLenum      _Target     = 0;
  GLint       _Level      = 0;
  bool        _Activate   = true;
};

struct GLFrameBufferDesc
{
  std::vector<GLFrameBufferAttachmentDesc> _Attachments;
  std::vector<GLenum>                      _DrawBuffers;
};

class GLUtil
{
public:

// DeleteTEX
static void DeleteTEX( GLTexture & ioTEX )
{
  if ( ioTEX._Handle )
    glDeleteTextures(1, &ioTEX._Handle);
  ioTEX._Handle = 0;
}

// DeleteFBO
static void DeleteFBO( GLFrameBuffer & ioFBO )
{
  if ( ioFBO._Handle )
    glDeleteFramebuffers(1, &ioFBO._Handle);
  ioFBO._Tex.clear();
  ioFBO._Handle = 0;
}

// DeleteTBO
static void DeleteTBO( GLTextureBuffer & ioTBO )
{
  if ( ioTBO._Handle )
    glDeleteBuffers(1, &ioTBO._Handle);
  DeleteTEX(ioTBO._Tex);
  ioTBO._Handle = 0;
}

// CreateBuffer
static void CreateBuffer( const GLBufferDesc & iDesc, GLuint & oBuffer )
{
  if ( !oBuffer )
    glGenBuffers(1, &oBuffer);

  glBindBuffer(iDesc._Target, oBuffer);
  glBufferData(iDesc._Target, iDesc._Size, iDesc._Data, iDesc._Usage);
  glBindBuffer(iDesc._Target, 0);
}

// UpdateBuffer
static bool UpdateBuffer( GLuint iBuffer, const GLBufferDesc & iDesc )
{
  if ( !iBuffer )
    return false;

  glBindBuffer(iDesc._Target, iBuffer);

  GLint curSize = 0;
  glGetBufferParameteriv(iDesc._Target, GL_BUFFER_SIZE, &curSize);
  if ( curSize != iDesc._Size )
  {
    glBindBuffer(iDesc._Target, 0);
    return false;
  }

  glBufferSubData(iDesc._Target, 0, iDesc._Size, iDesc._Data);
  glBindBuffer(iDesc._Target, 0);
  return true;
}

// CreateTextureBuffer
static void CreateTextureBuffer( const GLTextureBufferDesc & iDesc, GLTextureBuffer & ioTBO )
{
  GLBufferDesc bufferDesc;
  bufferDesc._Target = GL_TEXTURE_BUFFER;
  bufferDesc._Size   = iDesc._Size;
  bufferDesc._Data   = iDesc._Data;
  bufferDesc._Usage  = iDesc._Usage;

  CreateBuffer(bufferDesc, ioTBO._Handle);

  ioTBO._Tex._Target = GL_TEXTURE_BUFFER;
  ioTBO._Tex._Slot   = iDesc._Slot;
  if ( !ioTBO._Tex._Handle )
    glGenTextures(1, &ioTBO._Tex._Handle);

  glBindTexture(GL_TEXTURE_BUFFER, ioTBO._Tex._Handle);
  glTexBuffer(GL_TEXTURE_BUFFER, iDesc._InternalFormat, ioTBO._Handle);
  glBindTexture(GL_TEXTURE_BUFFER, 0);
}

// InitializeTBO
static void InitializeTBO( GLTextureBuffer & ioTBO, GLsizeiptr iSize, const void * iData, GLenum iInternalformat )
{
  GLTextureBufferDesc desc;
  desc._Slot           = ioTBO._Tex._Slot;
  desc._Size           = iSize;
  desc._Data           = iData;
  desc._Usage          = GL_STATIC_DRAW;
  desc._InternalFormat = iInternalformat;
  CreateTextureBuffer(desc, ioTBO);
}

// UpdateTBO
static bool UpdateTBO( GLTextureBuffer & ioTBO, GLsizeiptr iSize, const void * iData )
{
  GLBufferDesc desc;
  desc._Target = GL_TEXTURE_BUFFER;
  desc._Size   = iSize;
  desc._Data   = iData;
  return UpdateBuffer(ioTBO._Handle, desc);
}

// UploadTexture
static void UploadTexture( const GLTextureDesc & iDesc, GLTexture & ioTex )
{
  ioTex._Target         = iDesc._Target;
  ioTex._Slot           = iDesc._Slot;
  ioTex._InternalFormat = iDesc._InternalFormat;
  ioTex._DataFormat     = iDesc._DataFormat;
  ioTex._DataType       = iDesc._DataType;

  glBindTexture(ioTex._Target, ioTex._Handle);

  if ( ( GL_TEXTURE_2D_ARRAY == ioTex._Target ) || ( GL_TEXTURE_CUBE_MAP_ARRAY == ioTex._Target ) )
    glTexImage3D(ioTex._Target, 0, ioTex._InternalFormat, iDesc._Width, iDesc._Height, iDesc._Depth, 0, ioTex._DataFormat, ioTex._DataType, iDesc._Data);
  else if ( GL_TEXTURE_CUBE_MAP == ioTex._Target )
  {
    for ( int face = 0; face < 6; ++face )
      glTexImage2D(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face), 0, ioTex._InternalFormat, iDesc._Width, iDesc._Height, 0, ioTex._DataFormat, ioTex._DataType, iDesc._Data);
  }
  else
    glTexImage2D(ioTex._Target, 0, ioTex._InternalFormat, iDesc._Width, iDesc._Height, 0, ioTex._DataFormat, ioTex._DataType, iDesc._Data);

  glTexParameteri(ioTex._Target, GL_TEXTURE_MIN_FILTER, iDesc._MinFilter);
  glTexParameteri(ioTex._Target, GL_TEXTURE_MAG_FILTER, iDesc._MagFilter);
  glTexParameteri(ioTex._Target, GL_TEXTURE_WRAP_S, iDesc._WrapS);
  glTexParameteri(ioTex._Target, GL_TEXTURE_WRAP_T, iDesc._WrapT);
  if ( ( GL_TEXTURE_2D_ARRAY == ioTex._Target ) || ( GL_TEXTURE_CUBE_MAP_ARRAY == ioTex._Target ) || ( GL_TEXTURE_CUBE_MAP == ioTex._Target ) )
    glTexParameteri(ioTex._Target, GL_TEXTURE_WRAP_R, iDesc._WrapR);

  if ( iDesc._GenerateMipMap )
    glGenerateMipmap(ioTex._Target);
  else if ( ( GL_TEXTURE_2D == ioTex._Target ) || ( GL_TEXTURE_2D_ARRAY == ioTex._Target ) || ( GL_TEXTURE_CUBE_MAP_ARRAY == ioTex._Target ) || ( GL_TEXTURE_CUBE_MAP == ioTex._Target ) )
    glTexParameteri(ioTex._Target, GL_TEXTURE_MAX_LEVEL, 0);

  glBindTexture(ioTex._Target, 0);
}

// CreateTexture
static void CreateTexture( const GLTextureDesc & iDesc, GLTexture & ioTex )
{
  if ( !ioTex._Handle )
    glGenTextures(1, &ioTex._Handle);

  UploadTexture(iDesc, ioTex);
}

// ResizeTexture
static void ResizeTexture( GLTexture & ioTex, GLsizei iWidth, GLsizei iHeight )
{
  glBindTexture(ioTex._Target, ioTex._Handle);

  if ( GL_TEXTURE_CUBE_MAP == ioTex._Target )
  {
    for ( int face = 0; face < 6; ++face )
      glTexImage2D(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face), 0, ioTex._InternalFormat, iWidth, iHeight, 0, ioTex._DataFormat, ioTex._DataType, nullptr);
  }
  else
    glTexImage2D(ioTex._Target, 0, ioTex._InternalFormat, iWidth, iHeight, 0, ioTex._DataFormat, ioTex._DataType, nullptr);

  glBindTexture(ioTex._Target, 0);
}

// CreateFrameBuffer
static bool CreateFrameBuffer( const GLFrameBufferDesc & iDesc, GLFrameBuffer & ioFBO )
{
  DeleteFBO(ioFBO);

  glGenFramebuffers(1, &ioFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, ioFBO._Handle);

  std::vector<GLenum> colorAttachments;
  ioFBO._Tex.clear();
  for ( const GLFrameBufferAttachmentDesc & attachment : iDesc._Attachments )
  {
    if ( !attachment._Tex )
      continue;

    GLenum target = attachment._Target ? attachment._Target : attachment._Tex->_Target;
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment._Attachment, target, attachment._Tex->_Handle, attachment._Level);

    if ( attachment._Activate )
      ioFBO._Tex.push_back(attachment._Tex);

    if ( ( attachment._Attachment >= GL_COLOR_ATTACHMENT0 ) && ( attachment._Attachment <= static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + 31) ) )
      colorAttachments.push_back(attachment._Attachment);
  }

  if ( !iDesc._DrawBuffers.empty() )
    glDrawBuffers(static_cast<GLsizei>(iDesc._DrawBuffers.size()), iDesc._DrawBuffers.data());
  else if ( !colorAttachments.empty() )
    glDrawBuffers(static_cast<GLsizei>(colorAttachments.size()), colorAttachments.data());
  else
  {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  }

  bool complete = ( glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE );
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if ( !complete )
    DeleteFBO(ioFBO);
  return complete;
}

// ResizeFBO
static void ResizeFBO( GLFrameBuffer & ioFBO, GLsizei iWidth, GLsizei iHeight )
{
  for ( GLTexture * tex : ioFBO._Tex )
  {
    if ( tex )
      ResizeTexture(*tex, iWidth, iHeight);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, ioFBO._Handle);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ActivateTexture
static void ActivateTexture( const GLTexture & iTex )
{
  glActiveTexture(GL_TEX_UNIT(iTex));
  glBindTexture(iTex._Target, iTex._Handle);
}

// ActivateTextures
static void ActivateTextures( const GLFrameBuffer & iFBO )
{
  for ( const GLTexture * tex : iFBO._Tex )
  {
    if ( tex )
      ActivateTexture(*tex);
  }
}

// LoadTexture
static void LoadTexture( GLsizei iWidth, GLsizei iHeight, const void * iData, GLTexture & ioTex, GLint iTexMinFilter, GLint iTexMagFilter )
{
  GLTextureDesc desc;
  desc._Target         = ioTex._Target;
  desc._Slot           = ioTex._Slot;
  desc._Width          = iWidth;
  desc._Height         = iHeight;
  desc._InternalFormat = ioTex._InternalFormat;
  desc._DataFormat     = ioTex._DataFormat;
  desc._DataType       = ioTex._DataType;
  desc._Data           = iData;
  desc._MinFilter      = iTexMinFilter;
  desc._MagFilter      = iTexMagFilter;
  UploadTexture(desc, ioTex);
}

// GenTexture
static void GenTexture( GLenum iTarget, GLint iInternalformat, GLsizei iWidth, GLsizei iHeight, GLenum iFormat, GLenum iType, const void * iData, GLTexture & ioTex, GLint iTexMinFilter = GL_LINEAR, GLint iTexMagFilter = GL_LINEAR )
{
  GLTextureDesc desc;
  desc._Target         = iTarget;
  desc._Slot           = ioTex._Slot;
  desc._Width          = iWidth;
  desc._Height         = iHeight;
  desc._InternalFormat = iInternalformat;
  desc._DataFormat     = iFormat;
  desc._DataType       = iType;
  desc._Data           = iData;
  desc._MinFilter      = iTexMinFilter;
  desc._MagFilter      = iTexMagFilter;
  CreateTexture(desc, ioTex);
}

// EnableAnisotropyIfAvailable
static void EnableAnisotropyIfAvailable( const GLTexture & iTex, float iRequestedAniso = 16.0f )
{
  if ( 0 == iTex._Handle )
    return;

  if ( glewIsSupported("GL_EXT_texture_filter_anisotropic") || GLEW_EXT_texture_filter_anisotropic )
  {
    GLfloat maxAniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    GLfloat aniso = std::min<GLfloat>(iRequestedAniso, maxAniso);

    glBindTexture(iTex._Target, iTex._Handle);
    glTexParameterf(iTex._Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
    glBindTexture(iTex._Target, 0);
  }
}

// SetMinFilter
static void SetMinFilter( const GLTexture & iTex, GLint iTexMinFilter )
{
  glBindTexture(iTex._Target, iTex._Handle);
  glTexParameteri(iTex._Target, GL_TEXTURE_MIN_FILTER, iTexMinFilter);
  glBindTexture(iTex._Target, 0);
}

// UniformArrayElementName
static std::string UniformArrayElementName( const std::string & iUniformArrayName, int iIndex, const std::string & iAttributeName )
{
  return std::string(iUniformArrayName).append("[").append(std::to_string(iIndex)).append("].").append(iAttributeName);
}

// -----------------------------------------------------------------------------
// Vertex Array / Buffer helpers
// -----------------------------------------------------------------------------

// Generate / delete VAO
static GLuint GenVertexArray()
{
  GLuint vao = 0;
  glGenVertexArrays(1, &vao);
  return vao;
}

static void DeleteVertexArray(GLuint & ioVAO)
{
  if (ioVAO)
    glDeleteVertexArrays(1, &ioVAO);
  ioVAO = 0;
}

// Generate / delete generic buffer
static GLuint GenBuffer()
{
  GLuint buf = 0;
  glGenBuffers(1, &buf);
  return buf;
}

static void DeleteBuffer(GLuint & ioBuf)
{
  if (ioBuf)
    glDeleteBuffers(1, &ioBuf);
  ioBuf = 0;
}

// Upload data to an ARRAY_BUFFER (vertex buffer)
static void UploadArrayBuffer(GLuint iVBO, GLsizeiptr iSize, const void* iData, GLenum iUsage = GL_STATIC_DRAW)
{
  GLBufferDesc desc;
  desc._Target = GL_ARRAY_BUFFER;
  desc._Size   = iSize;
  desc._Data   = iData;
  desc._Usage  = iUsage;
  CreateBuffer(desc, iVBO);
}

// Upload data to an ELEMENT_ARRAY_BUFFER (index buffer)
static void UploadElementArrayBuffer(GLuint iEBO, GLsizeiptr iSize, const void* iData, GLenum iUsage = GL_STATIC_DRAW)
{
  GLBufferDesc desc;
  desc._Target = GL_ELEMENT_ARRAY_BUFFER;
  desc._Size   = iSize;
  desc._Data   = iData;
  desc._Usage  = iUsage;
  CreateBuffer(desc, iEBO);
}

// Setup attribute pointers for a VAO given a bound VBO/EBO
static void SetupVertexAttribPointers(GLuint iVAO, GLuint iVBO, GLuint iEBO,
                                      const std::vector<GLVertexAttribDesc> & iAttributes)
{
  glBindVertexArray(iVAO);
  glBindBuffer(GL_ARRAY_BUFFER, iVBO);
  if (iEBO)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iEBO);

  for (const GLVertexAttribDesc & attr : iAttributes)
  {
    glEnableVertexAttribArray(attr._Index);
    if ( attr._Integer )
      glVertexAttribIPointer(attr._Index, attr._Size, attr._Type, attr._Stride, reinterpret_cast<const void*>(attr._Offset));
    else
      glVertexAttribPointer(attr._Index, attr._Size, attr._Type, attr._Normalized, attr._Stride, reinterpret_cast<const void*>(attr._Offset));
  }

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  if (iEBO)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// Setup attribute pointers for a VAO given a bound VBO/EBO
static void SetupVertexAttribPointers(GLuint iVAO, GLuint iVBO, GLuint iEBO,
                                      const std::vector<std::tuple<GLuint, GLint, GLenum, GLboolean, GLsizei, std::size_t>> & iAttributes)
{
  std::vector<GLVertexAttribDesc> attrs;
  attrs.reserve(iAttributes.size());
  for ( const auto & attr : iAttributes )
  {
    GLVertexAttribDesc desc;
    desc._Index      = std::get<0>(attr);
    desc._Size       = std::get<1>(attr);
    desc._Type       = std::get<2>(attr);
    desc._Normalized = std::get<3>(attr);
    desc._Stride     = std::get<4>(attr);
    desc._Offset     = std::get<5>(attr);
    attrs.push_back(desc);
  }

  SetupVertexAttribPointers(iVAO, iVBO, iEBO, attrs);
}

// Convenience: create VAO/VBO/EBO and upload data, then setup attributes.
static void CreateMeshBuffers( GLsizeiptr iVertexSize, const void * iVertexData,
                               GLsizeiptr iIndexSize,  const void * iIndexData,
                               const std::vector<GLVertexAttribDesc> & iAttributes,
                               GLuint & oVAO, GLuint & oVBO, GLuint & oEBO,
                               GLenum iVertexUsage = GL_STATIC_DRAW, GLenum iIndexUsage = GL_STATIC_DRAW )
{
  oVAO = GenVertexArray();
  oVBO = GenBuffer();
  oEBO = ( ( iIndexData != nullptr ) && iIndexSize > 0 ) ? GenBuffer() : 0;

  if (oVBO)
    UploadArrayBuffer(oVBO, iVertexSize, iVertexData, iVertexUsage);

  if (oEBO)
    UploadElementArrayBuffer(oEBO, iIndexSize, iIndexData, iIndexUsage);

  SetupVertexAttribPointers(oVAO, oVBO, oEBO, iAttributes);
}

// Convenience: create VAO/VBO/EBO and upload data, then setup attributes.
static void CreateMeshBuffers( GLsizeiptr iVertexSize, const void * iVertexData,
                               GLsizeiptr iIndexSize,  const void * iIndexData,
                               const std::vector<std::tuple<GLuint, GLint, GLenum, GLboolean, GLsizei, std::size_t>> & iAttributes,
                               GLuint & oVAO, GLuint & oVBO, GLuint & oEBO )
{
  std::vector<GLVertexAttribDesc> attrs;
  attrs.reserve(iAttributes.size());
  for ( const auto & attr : iAttributes )
  {
    GLVertexAttribDesc desc;
    desc._Index      = std::get<0>(attr);
    desc._Size       = std::get<1>(attr);
    desc._Type       = std::get<2>(attr);
    desc._Normalized = std::get<3>(attr);
    desc._Stride     = std::get<4>(attr);
    desc._Offset     = std::get<5>(attr);
    attrs.push_back(desc);
  }

  CreateMeshBuffers(iVertexSize, iVertexData, iIndexSize, iIndexData, attrs, oVAO, oVBO, oEBO);
}

// Convenience: delete mesh buffers
static void DeleteMeshBuffers(GLuint & ioVAO, GLuint & ioVBO, GLuint & ioEBO)
{
  DeleteVertexArray(ioVAO);
  DeleteBuffer(ioVBO);
  DeleteBuffer(ioEBO);
  ioVAO = ioVBO = ioEBO = 0;
}

};

}

#endif /* _GLUtil_ */
