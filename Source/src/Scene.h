#ifndef _Scene_
#define _Scene_

#include "Light.h"
#include "Camera.h"
#include "Material.h"
#include "MeshInstance.h"
#include "Primitive.h"
#include "PrimitiveInstance.h"
#include "Texture.h"
#include "GpuBvh.h"
#include <vector>
#include <map>
#include <memory>

namespace RTRT
{

class Texture;
class Mesh;
struct Material;

class Scene
{
public:
  Scene();
  virtual ~Scene();

  int AddTexture( const std::string & iFilename, int iNbComponents = 4, TexFormat iFormat = TexFormat::TEX_UNSIGNED_BYTE );
  int AddMaterial( Material & ioMaterial, const std::string & iName );

  int AddMesh( const std::string & iFilename );
  int AddMeshInstance( MeshInstance & iMeshInstance );

  int AddPrimitive( const Primitive & iPrimitive );
  int AddPrimitiveInstance( PrimitiveInstance & iPrimitiveInstance );
  int AddPrimitiveInstance( int iPrimitiveID, int iMaterialID, const Mat4x4 & iTransform );

  void SetCamera( const Camera & iCamera ) { _Camera = iCamera; }
  Camera & GetCamera() { return _Camera; }

  void AddLight( const Light & iLight ) { _Lights.push_back(iLight); }
  Light * GetLight( unsigned int iIndex ) { return ( iIndex < _Lights.size() ) ? &_Lights[iIndex] : nullptr; }

  int FindMaterialID( const std::string & iMateralName ) const;
  std::string FindMaterialName( int iMaterialID ) const;

  std::string FindPrimitiveName( int iPrimitiveInstanceID ) const;

  int GetNbLights()          { return _Lights.size();          }
  int GetNbMaterials()       { return _Materials.size();       }
  int GetNbTextures()        { return _Textures.size();        }
  int GetNbMeshes()          { return _Meshes.size();          }
  int GetNbMeshInstances()   { return _MeshInstances.size();   }
  int GetNbPrimitiveInstances() { return _PrimitiveInstances.size(); }

  std::vector<MeshInstance>      & GetMeshInstances()      { return _MeshInstances;      }
  std::vector<PrimitiveInstance> & GetPrimitiveInstances() { return _PrimitiveInstances; }
  std::vector<Material>          & GetMaterials()          { return _Materials;          }
  std::vector<Texture*>          & GetTextures()           { return _Textures;           }
  std::vector<Mesh*>             & GetMeshes()             { return _Meshes;             }
  std::vector<Primitive*>        & GetPrimitives()         { return _Primitives;         }

  // Compiled data
  void CompileMeshData( Vec2i iTextureArraySize = Vec2i(0), bool iBuildTextureArray = true, bool iBuildBVH = true );
  int GetNbFaces() const { return _NbFaces; }
  int GetNbCompiledTex() const { return _NbCompiledTex; }
  const std::vector<Vec3>          & GetVertices()               const { return _Vertices;                }
  const std::vector<Vec3>          & GetNormals()                const { return _Normals;                 }
  const std::vector<Vec3>          & GetUVMatID()                const { return _UVMatID;                 }
  const std::vector<Vec3i>         & GetIndices()                const { return _Indices;                 }
  const std::vector<int>           & GetTextureArrayIDs()        const { return _TextureArrayIDs;         }
  const std::vector<unsigned char> & GetTextureArray()           const { return _TextureArray;            }
  const std::vector<Vec3>          & GetMeshBBoxes()             const { return _MeshBBoxes;              }
  const std::vector<int>           & GetMeshIdxRange()           const { return _MeshIdxRange;            }
  const std::vector<GpuBvh::Node>  & GetTLASNode()               const { return _TLAS.GetNodes();         }
  const std::vector<Mat4x4>        & GetTLASPackedTransforms()   const { return _TLASPackedTransforms;    }
  const std::vector<Vec2i>         & GetTLASPackedMeshMatID()    const { return _TLASPackedMeshMatID;     }
  const std::vector<GpuBvh::Node>  & GetBLASNode()               const { return _BLASNodes;               }
  const std::vector<Vec2i>         & GetBLASNodeRange()          const { return _BLASNodesRange;          }
  const std::vector<Vec3i>         & GetBLASPackedIndices()      const { return _BLASPackedIndices;       }
  const std::vector<Vec2i>         & GetBLASPackedIndicesRange() const { return _BLASPackedIndicesRange;  }
  const std::vector<Vec3>          & GetBLASPackedVertices()     const { return _BLASPackedVertices;      }
  const std::vector<Vec3>          & GetBLASPackedNormals()      const { return _BLASPackedNormals;       }
  const std::vector<Vec2>          & GetBLASPackedUVs()          const { return _BLASPackedUVs;           }

private:

  Camera                         _Camera;
  std::vector<Light>             _Lights;
  std::vector<Material>          _Materials;
  std::map<std::string,int>      _MaterialIDs;
  std::vector<MeshInstance>      _MeshInstances;
  std::map<std::string,int>      _PrimitiveNames;
  std::vector<PrimitiveInstance> _PrimitiveInstances;

  std::vector<Texture*>          _Textures;
  std::vector<Mesh*>             _Meshes;
  std::vector<Primitive*>        _Primitives;

  // Compiled data
  int                            _NbFaces = 0;
  std::vector<Vec3>              _Vertices;
  std::vector<Vec3>              _Normals;
  std::vector<Vec3>              _UVMatID;
  std::vector<Vec3i>             _Indices;
  int                            _NbCompiledTex = 0;
  std::vector<int>               _TextureArrayIDs;
  std::vector<unsigned char>     _TextureArray;
  std::vector<Vec3>              _MeshBBoxes;
  std::vector<int>               _MeshIdxRange;

  // BVH
  GpuTLAS                        _TLAS;
  std::vector<Mat4x4>            _TLASPackedTransforms;
  std::vector<Vec2i>             _TLASPackedMeshMatID;    // (MeshID, MathID)
  std::vector<GpuBvh::Node>      _BLASNodes;
  std::vector<Vec2i>             _BLASNodesRange;         // (StartIdx, Length)
  std::vector<Vec3i>             _BLASPackedIndices;      // (VertIdx, NormIdx, UVsIdx)
  std::vector<Vec2i>             _BLASPackedIndicesRange; // (StartIdx, Length)
  std::vector<Vec3>              _BLASPackedVertices;
  std::vector<Vec3>              _BLASPackedNormals;
  std::vector<Vec2>              _BLASPackedUVs;
};

}

#endif /* _Scene_ */
