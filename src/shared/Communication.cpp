#include PRECOMPILED_HEADER_FILE

#include <Lmcons.h>
#include <sstream>

#include "Communication.h"

namespace
{
  std::wstring GetUserName()
  {
    const DWORD maxLength = UNLEN + 1;
    std::auto_ptr<wchar_t> buffer(new wchar_t[maxLength]);
    DWORD length = maxLength;
    if (!::GetUserName(buffer.get(), &length))
    {
      std::stringstream stream;
      stream << "Failed to get the current user's name (Error code: " << GetLastError() << ")";
      throw std::runtime_error("Failed to get the current user's name");
    }
    return std::wstring(buffer.get(), length);
  }
}

const std::wstring Communication::pipeName = L"\\\\.\\pipe\\adblockplusengine_" + GetUserName();

std::string Communication::MarshalStrings(const std::vector<std::string>& strings)
{
  // TODO: This is some pretty hacky marshalling, replace it with something more robust
  std::string marshalledStrings;
  for (std::vector<std::string>::const_iterator it = strings.begin(); it != strings.end(); it++)
    marshalledStrings += *it + ';';
  return marshalledStrings;
}

std::vector<std::string> Communication::UnmarshalStrings(const std::string& message)
{
  std::stringstream stream(message);
  std::vector<std::string> strings;
  std::string string;
  while (std::getline(stream, string, ';'))
      strings.push_back(string);
  return strings;
}

std::string Communication::ReadMessage(HANDLE pipe)
{
  std::stringstream stream;
  std::auto_ptr<char> buffer(new char[bufferSize]);
  bool doneReading = false;
  while (!doneReading)
  {
    DWORD bytesRead;
    if (ReadFile(pipe, buffer.get(), bufferSize * sizeof(char), &bytesRead, 0))
      doneReading = true;
    else if (GetLastError() != ERROR_MORE_DATA)
    {
      std::stringstream stream;
      stream << "Error reading from pipe: " << GetLastError();
      throw std::runtime_error(stream.str());
    }
    stream << std::string(buffer.get(), bytesRead);
  }
  return stream.str();
}

void Communication::WriteMessage(HANDLE pipe, const std::string& message)
{
  DWORD bytesWritten;
  if (!WriteFile(pipe, message.c_str(), message.length(), &bytesWritten, 0)) 
    throw std::runtime_error("Failed to write to pipe");
}
