#ifndef IE_VERSION_H
#define IE_VERSION_H

#include <string>

namespace AdblockPlus
{
  namespace IE
  {
    std::wstring InstalledVersionString();
    int InstalledMajorVersion();
  }
}

#endif