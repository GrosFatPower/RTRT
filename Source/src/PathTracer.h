#ifndef _PathTracer_
#define _PathTracer_

#include "Renderer.h"

namespace RTRT
{

class Scene;
class RenderSettings;

class PathTracer : public Renderer
{
public:
  PathTracer( Scene & iScene, RenderSettings & iSettings );
  virtual ~PathTracer();

  virtual int Render();

private:


};

}

#endif /* _PathTracer_ */
