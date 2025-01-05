#ifndef _Renderer_
#define _Renderer_

namespace RTRT
{

class Scene;
class RenderSettings;

class Renderer
{
public:
  Renderer( Scene & iScene, RenderSettings & iSettings );
  virtual ~Renderer();

  virtual int Initialize() = 0;
  virtual int Update() = 0;

  virtual int RenderToTexture() = 0;
  virtual int RenderToScreen() = 0;

protected:
  
  Scene          & _Scene;
  RenderSettings & _Settings;
};

}

#endif /* _Renderer_ */
