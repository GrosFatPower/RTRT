#ifndef _Loader_
#define _Loader_

#include <string>

namespace RTRT
{

class Scene;
class Shape;

class Loader
{
public:

  static bool LoadScene(const std::string & iFilename, Scene * oScene);

  static bool LoadShape( const std::string & iFilename, Shape * oShape );

private:

  Loader();
  Loader( const Loader &);
  Loader & operator=( const Loader &);
};

}

#endif /* _Loader_ */
