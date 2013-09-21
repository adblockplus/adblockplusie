#include <Windows.h>
#include <Lmcons.h>
#include <Sddl.h>
#include <aclapi.h>
#include <strsafe.h>

#include "AutoHandle.h"
#include "Communication.h"
#include "Utils.h"

#ifndef SECURITY_APP_PACKAGE_AUTHORITY
#define SECURITY_APP_PACKAGE_AUTHORITY              {0,0,0,0,0,15}
#endif

std::wstring Communication::browserSID;

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

  std::auto_ptr<SID> GetLogonSid(HANDLE token) 
  {
    DWORD tokenGroupsLength = 0;
    if (GetTokenInformation(token, TokenLogonSid, 0, 0, &tokenGroupsLength))
      throw std::runtime_error("Unexpected result from GetTokenInformation");
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
      throw std::runtime_error("Unexpected error from GetTokenInformation");

    std::auto_ptr<TOKEN_GROUPS> tokenGroups(new TOKEN_GROUPS[tokenGroupsLength]);
    if (!GetTokenInformation(token, TokenLogonSid, tokenGroups.get(), tokenGroupsLength, &tokenGroupsLength))
      throw std::runtime_error("GetTokenInformation failed");
    if (tokenGroups->GroupCount != 1) 
      throw std::runtime_error("Unexpected group count");

    DWORD sidLength = GetLengthSid(tokenGroups->Groups[0].Sid);
    std::auto_ptr<SID> sid(new SID[sidLength]);
    if (!CopySid(sidLength, sid.get(), tokenGroups->Groups[0].Sid)) 
      throw std::runtime_error("CopySid failed");
    return sid;
  }

  std::auto_ptr<SECURITY_DESCRIPTOR> CreateObjectSecurityDescriptor(PSID logonSid)
  {
    PSID browserSid = 0;
    std::tr1::shared_ptr<SID> sharedBrowserSid(reinterpret_cast<SID*>(browserSid), FreeSid); // Just to simplify cleanup
    ConvertStringSidToSid(Communication::browserSID.c_str(), &browserSid);

    EXPLICIT_ACCESSW explicitAccess[2] = {};

    explicitAccess[0].grfAccessPermissions = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
    explicitAccess[0].grfAccessMode = SET_ACCESS;
    explicitAccess[0].grfInheritance= NO_INHERITANCE;
    explicitAccess[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    explicitAccess[0].Trustee.TrusteeType = TRUSTEE_IS_USER;
    explicitAccess[0].Trustee.ptstrName  = static_cast<LPWSTR>(logonSid);

    explicitAccess[1].grfAccessPermissions = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
    explicitAccess[1].grfAccessMode = SET_ACCESS;
    explicitAccess[1].grfInheritance= NO_INHERITANCE;
    explicitAccess[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    explicitAccess[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    explicitAccess[1].Trustee.ptstrName = static_cast<LPWSTR>(browserSid);

    PACL acl = 0;
    std::tr1::shared_ptr<ACL> sharedAcl(acl, FreeSid); // Just to simplify cleanup
    if (SetEntriesInAcl(2, explicitAccess, 0, &acl) != ERROR_SUCCESS)
      return std::auto_ptr<SECURITY_DESCRIPTOR>(0);

    std::auto_ptr<SECURITY_DESCRIPTOR> securityDescriptor(new SECURITY_DESCRIPTOR[SECURITY_DESCRIPTOR_MIN_LENGTH]);
    if (!InitializeSecurityDescriptor(securityDescriptor.get(), SECURITY_DESCRIPTOR_REVISION)) 
      return std::auto_ptr<SECURITY_DESCRIPTOR>(0);

    if (!SetSecurityDescriptorDacl(securityDescriptor.get(), TRUE, acl, FALSE))
      return std::auto_ptr<SECURITY_DESCRIPTOR>(0);

    return securityDescriptor;
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
    SECURITY_ATTRIBUTES securityAttributes = {};
    securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    securityAttributes.bInheritHandle = TRUE;

    std::tr1::shared_ptr<SECURITY_DESCRIPTOR> sharedSecurityDescriptor; // Just to simplify cleanup

    const bool inAppContainer = browserSID.empty();
    if (inAppContainer)
    {
      AutoHandle token;
      OpenProcessToken(GetCurrentProcess(), TOKEN_READ, token);
      std::auto_ptr<SID> logonSid = GetLogonSid(token);
      std::auto_ptr<SECURITY_DESCRIPTOR> securityDescriptor = CreateObjectSecurityDescriptor(logonSid.get());
      securityAttributes.lpSecurityDescriptor = securityDescriptor.release();
      sharedSecurityDescriptor.reset(static_cast<SECURITY_DESCRIPTOR*>(securityAttributes.lpSecurityDescriptor));
    }
    else if (IsWindowsVistaOrLater())
    {
      // Low mandatory label. See http://msdn.microsoft.com/en-us/library/bb625958.aspx
      LPCWSTR accessControlEntry = L"S:(ML;;NW;;;LW)";
      ConvertStringSecurityDescriptorToSecurityDescriptorW(accessControlEntry, SDDL_REVISION_1,
        &securityAttributes.lpSecurityDescriptor, 0);
      sharedSecurityDescriptor.reset(static_cast<SECURITY_DESCRIPTOR*>(securityAttributes.lpSecurityDescriptor), LocalFree);
    }

    pipe = CreateNamedPipeW(pipeName.c_str(),  PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES, bufferSize, bufferSize, 0, &securityAttributes);
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
  if (!WriteFile(pipe, data.c_str(), static_cast<DWORD>(data.length()), &bytesWritten, 0))
    throw std::runtime_error("Failed to write to pipe");
}
