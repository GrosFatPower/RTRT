#ifndef _Mesh_
#define _Mesh_

#include "MathUtil.h"
#include "Primitive.h"
#include "GpuBvh.h"
#include <memory>
#include <vector>
#include <string>

namespace RTRT
{

class Mesh
{
public:
  Mesh();
  Mesh(const std::string        & iName,
       const std::vector<Vec3>  & iVertices,
       const std::vector<Vec3>  & iNormals,
       const std::vector<Vec2>  & iUVs,
       const std::vector<Vec3i> & iIndices);
  virtual ~Mesh();

  bool LoadOBJ( const std::string & iFilename );

  const std::string & Filename() const { return _Filename; }

  int GetMeshID() const { return _MeshID; }
  void SetMeshID( int iMeshID ) { _MeshID = iMeshID; }

  int GetNbFaces() const { return _NbFaces; }

  int BuildBvh();
  int RefitBvh();

  void InitializeSkinningBindPose();
  void ResetToBindPose();
  void ApplySkinning( const std::vector<Mat4x4> & iJointMatrices );

  bool IsSkinned() const { return ( _SkinIndex >= 0 ) && !_Joints.empty() && ( _Joints.size() == _BindVertices.size() ); }

  void SetSkinIndex( int iSkinIndex ) { _SkinIndex = iSkinIndex; }
  int  GetSkinIndex() const { return _SkinIndex; }

  void SetJoints( std::vector<Vec4i> && iJoints ) { _Joints = std::move(iJoints); }
  void SetWeights( std::vector<Vec4> && iWeights ) { _Weights = std::move(iWeights); }
  const std::vector<Vec4i> & GetJoints()  const { return _Joints;  }
  const std::vector<Vec4>  & GetWeights() const { return _Weights; }

  const std::vector<Vec3>  & GetVertices() const { return _Vertices; }
  const std::vector<Vec3>  & GetNormals()  const { return _Normals;  }
  const std::vector<Vec2>  & GetUVs()      const { return _UVs;      }
  const std::vector<Vec3i> & GetIndices()  const { return _Indices;  }

  std::shared_ptr<GpuBLAS> & GetBvh() { return _Bvh; }

  const AABB<Vec3> & GetBoundingBox() const { return _BoundingBox; }

private:

  int                      _MeshID  = -1;
  int                      _NbFaces = 0;

  std::vector<Vec3>        _Vertices;
  std::vector<Vec3>        _Normals;
  std::vector<Vec2>        _UVs;
  std::vector<Vec3i>       _Indices;

  // Skinning data
  int                      _SkinIndex = -1;
  std::vector<Vec3>        _BindVertices;
  std::vector<Vec3>        _BindNormals;
  std::vector<Vec4i>       _Joints;
  std::vector<Vec4>        _Weights;

  AABB<Vec3>               _BoundingBox;

  std::shared_ptr<GpuBLAS> _Bvh = nullptr;

  std::string              _Filename = "";
};

}

#endif /* _Mesh_ */
