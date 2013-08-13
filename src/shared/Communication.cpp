#include <Windows.h>
#include <Lmcons.h>
#include <Sddl.h>

#include "Communication.h"
#include "Utils.h"

namespace
{
  const int bufferSize = 1024;

  std::string AppendErrorCode(const std::string& message)
  {
    std::stringstream stream;
    stream << message << " (Error code: " << GetLastError() << ")";
    return stream.str();
  }

  std::wstring GetUserName()
  {
    const DWORD maxLength = UNLEN + 1;
    std::auto_ptr<wchar_t> buffer(new wchar_t[maxLength]);
    DWORD length = maxLength;
    if (!::GetUserNameW(buffer.get(), &length))
      throw std::runtime_error(AppendErrorCode("Failed to get the current user's name"));
    return std::wstring(buffer.get(), length);
  }
}

const std::wstring Communication::pipeName = L"\\\\.\\pipe\\adblockplusengine_" + GetUserName();

void Communication::InputBuffer::CheckType(Communication::ValueType expectedType)
{
  if (!hasType)
    ReadBinary(currentType);

  if (currentType != expectedType)
  {
    // Make sure we don't attempt to read the type again
    hasType = true;
    throw new std::runtime_error("Unexpected type found in input buffer");
  }
  else
    hasType = false;
}

Communication::ValueType Communication::InputBuffer::GetType()
{
  if (!hasType)
    ReadBinary(currentType);

  hasType = true;
  return currentType;
}

Communication::PipeConnectionError::PipeConnectionError()
    : std::runtime_error(AppendErrorCode("Unable to connect to a named pipe"))
{
}

Communication::PipeBusyError::PipeBusyError()
  : std::runtime_error("Timeout while trying to connect to a named pipe, pipe is busy")
{
}

Communication::PipeDisconnectedError::PipeDisconnectedError()
  : std::runtime_error("Pipe disconnected")
{
}

Communication::Pipe::Pipe(const std::wstring& pipeName, Communication::Pipe::Mode mode)
{
  pipe = INVALID_HANDLE_VALUE;
  if (mode == MODE_CREATE)
  {
    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);

    PSECURITY_DESCRIPTOR securitydescriptor;
    if (IsWindowsVistaOrLater())
    {
      // Low mandatory label. See http://msdn.microsoft.com/en-us/library/bb625958.aspx
      LPCWSTR accessControlEntry = L"S:(ML;;NW;;;LW)";
      ConvertStringSecurityDescriptorToSecurityDescriptorW(accessControlEntry, SDDL_REVISION_1, &securitydescriptor, 0);
      sa.lpSecurityDescriptor = securitydescriptor;
    }

    sa.bInheritHandle = TRUE;

    pipe = CreateNamedPipeW (pipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                  PIPE_UNLIMITED_INSTANCES, bufferSize, bufferSize, 0, &sa);
    if (IsWindowsVistaOrLater() && securitydescriptor)
    {
        LocalFree(securitydescriptor);
    }
  }
  else
  {
    pipe = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (pipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY)
    {
      if (!WaitNamedPipeW(pipeName.c_str(), 10000))
        throw PipeBusyError();

      pipe = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    }
  }

  if (pipe == INVALID_HANDLE_VALUE)
    throw PipeConnectionError();

  DWORD pipeMode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
  if (!SetNamedPipeHandleState(pipe, &pipeMode, 0, 0))
    throw std::runtime_error("SetNamedPipeHandleState failed: error " + GetLastError());

  if (mode == MODE_CREATE && !ConnectNamedPipe(pipe, 0))
    throw std::runtime_error("Client failed to connect: error " + GetLastError());
}

Communication::Pipe::~Pipe()
{
  CloseHandle(pipe);
}

Communication::InputBuffer Communication::Pipe::ReadMessage()
{
  std::stringstream stream;
  std::auto_ptr<char> buffer(new char[bufferSize]);
  bool doneReading = false;
  while (!doneReading)
  {
    DWORD bytesRead;
    if (ReadFile(pipe, buffer.get(), bufferSize * sizeof(char), &bytesRead, 0))
      doneReading = true;
    else
    {
      DWORD lastError = GetLastError();
      switch (lastError)
      {
      case ERROR_MORE_DATA:
        break;
      case ERROR_BROKEN_PIPE:
        throw PipeDisconnectedError();
      default:
        std::stringstream stream;
        stream << "Error reading from pipe: " << lastError;
        throw std::runtime_error(stream.str());
      }
    }
    stream << std::string(buffer.get(), bytesRead);
  }
  return Communication::InputBuffer(stream.str());
}

void Communication::Pipe::WriteMessage(Communication::OutputBuffer& message)
{
  DWORD bytesWritten;
  std::string data = message.Get();
  if (!WriteFile(pipe, data.c_str(), data.length(), &bytesWritten, 0))
    throw std::runtime_error("Failed to write to pipe");
}
