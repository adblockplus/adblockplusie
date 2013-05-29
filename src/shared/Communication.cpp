#include "..\engine\stdafx.h"

#include <Lmcons.h>

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

Communication::InputBuffer Communication::ReadMessage(HANDLE pipe)
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
  return Communication::InputBuffer(stream.str());
}

void Communication::WriteMessage(HANDLE pipe, Communication::OutputBuffer& message)
{
  DWORD bytesWritten;
  std::string data = message.Get();
  if (!WriteFile(pipe, data.c_str(), data.length(), &bytesWritten, 0))
    throw std::runtime_error("Failed to write to pipe");
}
