#include "QuadMesh.h"
#include "ShaderProgram.h"

const GLfloat g_QuadVtx[] =
{
  -1.0f, -1.0f,  0.0f,
   1.0f, -1.0f,  0.0f,
   1.0f,  1.0f,  0.0f,
   1.0f,  1.0f,  0.0f,
  -1.0f,  1.0f,  0.0f,
  -1.0f, -1.0f,  0.0f
};

const GLfloat g_QuadUVs[] =
{
  0.0f, 0.0f,
  1.0f, 0.0f,
  1.0f, 1.0f,
  1.0f, 1.0f,
  0.0f, 1.0f,
  0.0f, 0.0f,
};

namespace RTRT
{

QuadMesh::QuadMesh()
{
  glGenVertexArrays(1, &_VertexArrayID);
  glBindVertexArray(_VertexArrayID);

  glGenBuffers(1, &_VertexBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, _VertexBufferID);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_QuadVtx), g_QuadVtx, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

  glGenBuffers(1, &_UVBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, _UVBufferID);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_QuadUVs), g_QuadUVs, GL_STATIC_DRAW);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

  glBindVertexArray(0);
}

QuadMesh::~QuadMesh()
{
  glDeleteBuffers(1, &_VertexBufferID);
  glDeleteBuffers(1, &_UVBufferID);
  glDeleteVertexArrays(1, &_VertexArrayID);
}

void QuadMesh::Render(ShaderProgram & iShaderProgram)
{
  iShaderProgram.Use();
  
  glBindVertexArray(_VertexArrayID);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);

  iShaderProgram.StopUsing();
}

}
