#ifndef _RasterData_
#define _RasterData_

#include "MathUtil.h"
#include "RGBA8.h"
#include "Light.h"
#include "Material.h"
#include "Texture.h"
#include <vector>

namespace RTRT
{

namespace RasterData
{
  struct FrameBuffer
  {
    std::vector<RGBA8> _ColorBuffer;
    std::vector<float> _DepthBuffer;
  };

  struct Varying
  {
    Vec3 _WorldPos;
    Vec2 _UV;
    Vec3 _Normal;

    Varying operator*(float t) const
    {
      auto copy = *this;
      copy._WorldPos *= t;
      copy._Normal *= t;
      copy._UV *= t;
      return copy;
    }

    Varying operator+(const Varying& iRhs) const
    {
      Varying copy = *this;

      copy._WorldPos += iRhs._WorldPos;
      copy._Normal += iRhs._Normal;
      copy._UV += iRhs._UV;

      return copy;
    }
  };

  struct Uniform
  {
    const std::vector<Material>* _Materials = nullptr;
    const std::vector<Texture*>* _Textures = nullptr;
    std::vector<Light>           _Lights;
    Vec3                         _CameraPos;
    bool                         _BilinearSampling = true;
  };

  struct Vertex
  {
    Vec3 _WorldPos;
    Vec2 _UV;
    Vec3 _Normal;

    bool operator==(const Vertex& iRhs) const
    {
      return ((_WorldPos == iRhs._WorldPos)
        && (_Normal == iRhs._Normal)
        && (_UV == iRhs._UV));
    }
  };

  struct Triangle
  {
    int   _Indices[3];
    Vec3  _Normal;
    int   _MatID;
  };

  struct ProjectedVertex
  {
    Vec4    _ProjPos;
    Varying _Attrib;
  };

  struct RasterTriangle
  {
    int        _Indices[3];
    Vec3       _V[3];
    float      _EdgeA[3];
    float      _EdgeB[3];
    float      _EdgeC[3];
    float      _InvW[3];
    float      _InvArea;
    AABB<Vec2> _BBox;
    Vec3       _Normal;
    int        _MatID;
  };

  struct Fragment
  {
    Vec3    _FragCoords;
    int     _MatID;
    Varying _Attrib;
  };

  struct Tile
  {
    int                          _X;
    int                          _Y;
    int                          _Width;
    int                          _Height;
    FrameBuffer                  _LocalFB;
    std::vector<RasterTriangle*> _RasterTris;
  };

}

}

#endif /* _RasterData_ */
