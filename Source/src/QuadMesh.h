#ifndef _QuadMesh_
#define _QuadMesh_

#include <GL/glew.h>

namespace RTRT
{

class ShaderProgram;

class QuadMesh
{
public:
  QuadMesh();
  virtual ~QuadMesh();

  void Render(ShaderProgram & iShaderProgram);

private:

  GLuint _VertexArrayID;
  GLuint _VertexBufferID;
  GLuint _UVBufferID;
};

}

#endif /* _QuadMesh_ */
