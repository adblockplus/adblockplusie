#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <string>
#include <vector>
#include <Windows.h>

namespace Communication
{
  extern const std::wstring pipeName;
  const int bufferSize = 1024;

  std::string MarshalStrings(const std::vector<std::string>& strings);
  std::vector<std::string> UnmarshalStrings(const std::string& message);
  std::string ReadMessage(HANDLE pipe);
  void WriteMessage(HANDLE pipe, const std::string& message);
}

#endif
