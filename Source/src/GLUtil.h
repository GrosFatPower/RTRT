#ifndef _GLUtil_
#define _GLUtil_

#include <string>

namespace RTRT
{

class GLUtil
{
public:

// UniformArrayElementName
static std::string UniformArrayElementName( const char * iUniformArrayName, int iIndex, const char * iAttributeName )
{
  return std::string(iUniformArrayName).append("[").append(std::to_string(iIndex)).append("].").append(iAttributeName);
}

};

}

#endif /* _GLUtil_ */
