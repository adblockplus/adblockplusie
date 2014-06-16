#include <Windows.h>
#include <Lmcons.h>
#include <Sddl.h>
#include <aclapi.h>
#include <strsafe.h>

#include "AutoHandle.h"
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

  // Creates a security descriptor: 
  // Allows ALL access to Logon SID and to all app containers in DACL.
  // Sets Low Integrity in SACL.
  std::auto_ptr<SECURITY_DESCRIPTOR> CreateSecurityDescriptor(PSID logonSid)
  {
    std::auto_ptr<SECURITY_DESCRIPTOR> securityDescriptor((SECURITY_DESCRIPTOR*)new char[SECURITY_DESCRIPTOR_MIN_LENGTH]);
    if (!InitializeSecurityDescriptor(securityDescriptor.get(), SECURITY_DESCRIPTOR_REVISION)) 
      return std::auto_ptr<SECURITY_DESCRIPTOR>(0);
    // TODO: Would be better to detect if AppContainers are supported instead of checking the Windows version
    bool isAppContainersSupported = IsWindows8OrLater();
    if (isAppContainersSupported)
    {
      EXPLICIT_ACCESSW explicitAccess[2] = {};

      explicitAccess[0].grfAccessPermissions = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
      explicitAccess[0].grfAccessMode = SET_ACCESS;
      explicitAccess[0].grfInheritance= NO_INHERITANCE;
      explicitAccess[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
      explicitAccess[0].Trustee.TrusteeType = TRUSTEE_IS_USER;
      explicitAccess[0].Trustee.ptstrName  = static_cast<LPWSTR>(logonSid);

      std::tr1::shared_ptr<SID> sharedAllAppContainersSid;

      // Create a well-known SID for the all appcontainers group.
      // We need to allow access to all AppContainers, since, apparently,
      // giving access to specific AppContainer (for example AppContainer of IE) 
      // tricks Windows into thinking that token is IN AppContainer. 
      // Which blocks all the calls from outside, making it impossible to communicate
      // with the engine when IE is launched with different security settings.
      PSID allAppContainersSid = 0;
      SID_IDENTIFIER_AUTHORITY applicationAuthority = SECURITY_APP_PACKAGE_AUTHORITY;

      AllocateAndInitializeSid(&applicationAuthority, 
              SECURITY_BUILTIN_APP_PACKAGE_RID_COUNT,
              SECURITY_APP_PACKAGE_BASE_RID,
              SECURITY_BUILTIN_PACKAGE_ANY_PACKAGE,
              0, 0, 0, 0, 0, 0,
              &allAppContainersSid);
      sharedAllAppContainersSid.reset(static_cast<SID*>(allAppContainersSid), FreeSid); // Just to simplify cleanup

      explicitAccess[1].grfAccessPermissions = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
      explicitAccess[1].grfAccessMode = SET_ACCESS;
      explicitAccess[1].grfInheritance= NO_INHERITANCE;
      explicitAccess[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
      explicitAccess[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
      explicitAccess[1].Trustee.ptstrName = static_cast<LPWSTR>(allAppContainersSid);

      PACL acl = 0;
      if (SetEntriesInAcl(2, explicitAccess, 0, &acl) != ERROR_SUCCESS)
        return std::auto_ptr<SECURITY_DESCRIPTOR>(0);
      std::tr1::shared_ptr<ACL> sharedAcl(static_cast<ACL*>(acl), LocalFree); // Just to simplify cleanup

      if (!SetSecurityDescriptorDacl(securityDescriptor.get(), TRUE, acl, FALSE))
        return std::auto_ptr<SECURITY_DESCRIPTOR>(0);

    }

    // Create a dummy security descriptor with low integrirty preset and copy its SACL into ours
    LPCWSTR accessControlEntry = L"S:(ML;;NW;;;LW)";
    PSECURITY_DESCRIPTOR dummySecurityDescriptorLow;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(accessControlEntry, SDDL_REVISION_1, &dummySecurityDescriptorLow, 0);
    std::tr1::shared_ptr<SECURITY_DESCRIPTOR> sharedDummySecurityDescriptor(static_cast<SECURITY_DESCRIPTOR*>(dummySecurityDescriptorLow), LocalFree); // Just to simplify cleanup
    BOOL saclPresent = FALSE;
    BOOL saclDefaulted = FALSE;
    PACL sacl;
    GetSecurityDescriptorSacl(dummySecurityDescriptorLow, &saclPresent, &sacl, &saclDefaulted);
    if (saclPresent)
    {
      if (!SetSecurityDescriptorSacl(securityDescriptor.get(), TRUE, sacl, FALSE))
      {
        DWORD err = GetLastError();
        return std::auto_ptr<SECURITY_DESCRIPTOR>(0);
      }
    }

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

    AutoHandle token;
    OpenProcessToken(GetCurrentProcess(), TOKEN_READ, token);
    
    if (IsWindowsVistaOrLater())
    {
      std::auto_ptr<SID> logonSid = GetLogonSid(token);
      // Create a SECURITY_DESCRIPTOR that has both Low Integrity and allows access to all AppContainers
      // This is needed since IE likes to jump out of Enhanced Protected Mode for specific pages (bing.com)
      std::auto_ptr<SECURITY_DESCRIPTOR> securityDescriptor = CreateSecurityDescriptor(logonSid.get());
      securityAttributes.lpSecurityDescriptor = securityDescriptor.release();
      sharedSecurityDescriptor.reset(static_cast<SECURITY_DESCRIPTOR*>(securityAttributes.lpSecurityDescriptor));

      pipe = CreateNamedPipeW(pipeName.c_str(),  PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES, bufferSize, bufferSize, 0, &securityAttributes);
    }
    else
    {
      pipe = CreateNamedPipeW(pipeName.c_str(),  PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES, bufferSize, bufferSize, 0, &securityAttributes);
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
  {
    DWORD err = GetLastError();
    throw std::runtime_error("Client failed to connect: error " + GetLastError());
  }
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
