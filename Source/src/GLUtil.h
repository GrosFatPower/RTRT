#ifndef _GLUtil_
#define _GLUtil_

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
  ioFBO._Handle = 0;
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
static void LoadTexture( GLsizei iWidth, GLsizei iHeight, const void * iData, GLTexture & ioTex, GLint iTexMinFilter, GLint iTexMagFilter )
{
  glBindTexture(ioTex._Target, ioTex._Handle);
  glTexImage2D(ioTex._Target, 0, ioTex._InternalFormat, iWidth, iHeight, 0, ioTex._DataFormat, ioTex._DataType, iData);
  glTexParameteri(ioTex._Target, GL_TEXTURE_MIN_FILTER, iTexMinFilter);
  glTexParameteri(ioTex._Target, GL_TEXTURE_MAG_FILTER, iTexMagFilter);
  glBindTexture(ioTex._Target, 0);
}

// GenTexture
static void GenTexture( GLenum iTarget, GLint iInternalformat, GLsizei iWidth, GLsizei iHeight, GLenum iFormat, GLenum iType, const void * iData, GLTexture & ioTex, GLint iTexMinFilter = GL_LINEAR, GLint iTexMagFilter = GL_LINEAR )
{
  ioTex._Target         = iTarget;
  ioTex._InternalFormat = iInternalformat;
  ioTex._DataFormat     = iFormat;
  ioTex._DataType       = iType;
  glGenTextures(1, &ioTex._Handle);

  LoadTexture(iWidth, iHeight, iData, ioTex, iTexMinFilter, iTexMagFilter);
}

// EnableAnisotropyIfAvailable
static void EnableAnisotropyIfAvailable(GLTexture & iTex, float iRequestedAniso = 16.0f)
{
  if ( 0 == iTex._Handle )
    return;

  // Check extension (GLEW provides macro/func)
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
static void SetMinFilter(GLTexture iTex, GLint iTexMinFilter)
{
  glBindTexture(GL_TEXTURE_2D_ARRAY, iTex._Handle);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, iTexMinFilter);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

// UniformArrayElementName
static std::string UniformArrayElementName( const char * iUniformArrayName, int iIndex, const char * iAttributeName )
{
  return std::string(iUniformArrayName).append("[").append(std::to_string(iIndex)).append("].").append(iAttributeName);
}

// -----------------------------------------------------------------------------
// Vertex Array / Buffer helpers
// -----------------------------------------------------------------------------
// Attribute tuple layout:
//   (index, size, type, normalized, stride, offset)
// where index is the attribute location, size is number of components (1..4),
// type is GL_FLOAT/GL_INT/etc, normalized is GLboolean, stride is in bytes,
// offset is the byte offset into the vertex struct.
//
// These helpers are convenient wrappers for creating VAO/VBO/EBO and setting
// up attribute pointers. They do not modify ownership semantics of the caller.
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
  glBindBuffer(GL_ARRAY_BUFFER, iVBO);
  glBufferData(GL_ARRAY_BUFFER, iSize, iData, iUsage);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Upload data to an ELEMENT_ARRAY_BUFFER (index buffer)
static void UploadElementArrayBuffer(GLuint iEBO, GLsizeiptr iSize, const void* iData, GLenum iUsage = GL_STATIC_DRAW)
{
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, iSize, iData, iUsage);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// Setup attribute pointers for a VAO given a bound VBO/EBO
static void SetupVertexAttribPointers(GLuint iVAO, GLuint iVBO, GLuint iEBO,
                                      const std::vector<std::tuple<GLuint, GLint, GLenum, GLboolean, GLsizei, std::size_t>> & iAttributes)
{
  glBindVertexArray(iVAO);
  glBindBuffer(GL_ARRAY_BUFFER, iVBO);
  if (iEBO)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iEBO);

  for (const auto & attr : iAttributes)
  {
    GLuint      index      = std::get<0>(attr);
    GLint       size       = std::get<1>(attr);
    GLenum      type       = std::get<2>(attr);
    GLboolean   normalized = std::get<3>(attr);
    GLsizei     stride     = std::get<4>(attr);
    std::size_t offset     = std::get<5>(attr);

    glEnableVertexAttribArray(index);
    glVertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<const void*>(offset));
  }

  // Unbind in safe order
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  if (iEBO)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// Convenience: create VAO/VBO/EBO and upload data, then setup attributes.
// Attributes vector uses the tuple layout described above.
static void CreateMeshBuffers( GLsizeiptr iVertexSize, const void * iVertexData,
                               GLsizeiptr iIndexSize,  const void * iIndexData,
                               const std::vector<std::tuple<GLuint, GLint, GLenum, GLboolean, GLsizei, std::size_t>> & iAttributes,
                               GLuint & oVAO, GLuint & oVBO, GLuint & oEBO )
{
  // Generate objects
  oVAO = GenVertexArray();
  oVBO = GenBuffer();
  oEBO = ( ( iIndexData != nullptr ) && iIndexSize > 0 ) ? GenBuffer() : 0;

  // Upload buffers
  if (oVBO)
    UploadArrayBuffer(oVBO, iVertexSize, iVertexData);

  if (oEBO)
    UploadElementArrayBuffer(oEBO, iIndexSize, iIndexData);

  // Setup VAO attribute pointers
  SetupVertexAttribPointers(oVAO, oVBO, oEBO, iAttributes);
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
