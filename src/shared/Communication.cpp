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

  // See http://msdn.microsoft.com/en-us/library/windows/desktop/hh448493(v=vs.85).aspx
  bool GetLogonSid(HANDLE hToken, PSID *ppsid) 
  {
    if (ppsid == NULL)
      return false;

    // Get required buffer size and allocate the TOKEN_GROUPS buffer.
    DWORD dwLength = 0;
    PTOKEN_GROUPS ptg = NULL;
    if (!GetTokenInformation(hToken, TokenLogonSid, ptg, 0, &dwLength)) 
    {
      if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) 
        return false;

      ptg = (PTOKEN_GROUPS) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwLength);

      if (ptg == NULL)
        return false;
    }

    // Get the token group information from the access token.
    if (!GetTokenInformation(hToken, TokenLogonSid, ptg, dwLength, &dwLength) || ptg->GroupCount != 1) 
    {
      HeapFree(GetProcessHeap(), 0, ptg);
      return false;
    }

    // Found the logon SID; make a copy of it.
    dwLength = GetLengthSid(ptg->Groups[0].Sid);
    *ppsid = (PSID) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwLength);
    if (*ppsid == NULL)
    {
      HeapFree(GetProcessHeap(), 0, ptg);
      return false;
    }
    if (!CopySid(dwLength, *ppsid, ptg->Groups[0].Sid)) 
    {
      HeapFree(GetProcessHeap(), 0, ptg);
      HeapFree(GetProcessHeap(), 0, *ppsid);
      return false;
    }

    HeapFree(GetProcessHeap(), 0, ptg);
    return true;
  }

  bool CreateObjectSecurityDescriptor(PSID pLogonSid, PSECURITY_DESCRIPTOR* ppSD)
  {
    BOOL bSuccess = FALSE;
    DWORD dwRes;
    PSID pBrowserSID = NULL;
    PACL pACL = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    EXPLICIT_ACCESS ea[2];
    SID_IDENTIFIER_AUTHORITY ApplicationAuthority = SECURITY_APP_PACKAGE_AUTHORITY;

    ConvertStringSidToSid(Communication::browserSID.c_str(), &pBrowserSID);

    // Initialize an EXPLICIT_ACCESS structure for an ACE.
    // The ACE will allow LogonSid generic all access
    ZeroMemory(&ea, 2 * sizeof(EXPLICIT_ACCESS));
    ea[0].grfAccessPermissions = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
    ea[0].grfAccessMode = SET_ACCESS;
    ea[0].grfInheritance= NO_INHERITANCE;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.TrusteeType = TRUSTEE_IS_USER;
    ea[0].Trustee.ptstrName  = (LPTSTR) pLogonSid;

    // Initialize an EXPLICIT_ACCESS structure for an ACE.
    // The ACE will give the browser SID all permissions
    ea[1].grfAccessPermissions = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
    ea[1].grfAccessMode = SET_ACCESS;
    ea[1].grfInheritance= NO_INHERITANCE;
    ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    ea[1].Trustee.ptstrName  = (LPTSTR) pBrowserSID;

    // Create a new ACL that contains the new ACEs.
    dwRes = SetEntriesInAcl(2, ea, NULL, &pACL);
    if (ERROR_SUCCESS != dwRes) 
    {
      FreeSid(pBrowserSID);
      return false;
    }

    // Initialize a security descriptor.  
    pSD = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH); 
    if (NULL == pSD) 
    { 
      FreeSid(pBrowserSID);
      LocalFree(pACL);
      return false;; 
    } 

    if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION)) 
    {  
      FreeSid(pBrowserSID);
      LocalFree(pACL);
      LocalFree((LPVOID)pSD);
      return false;
    } 

    // Add the ACL to the security descriptor. 
    if (!SetSecurityDescriptorDacl(pSD, TRUE, pACL, FALSE))   // not a default DACL 
    {  
      FreeSid(pBrowserSID);
      LocalFree(pACL);
      LocalFree((LPVOID)pSD);
      return false;
    } 

    *ppSD = pSD;
    pSD = NULL;

    if (pBrowserSID) 
      FreeSid(pBrowserSID);
    if (pACL) 
      LocalFree(pACL);

    return true;
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

    const bool inAppContainer = browserSID.empty();
    if (inAppContainer)
    {
      AutoHandle token;
      OpenProcessToken(GetCurrentProcess(), TOKEN_READ, token);
      PSID logonSid = NULL;
      if (GetLogonSid(token, &logonSid))
        CreateObjectSecurityDescriptor(logonSid, &securityAttributes.lpSecurityDescriptor);
    }
    else if (IsWindowsVistaOrLater())
    {
      // Low mandatory label. See http://msdn.microsoft.com/en-us/library/bb625958.aspx
      LPCWSTR accessControlEntry = L"S:(ML;;NW;;;LW)";
      ConvertStringSecurityDescriptorToSecurityDescriptorW(accessControlEntry, SDDL_REVISION_1,
        &securityAttributes.lpSecurityDescriptor, 0);
    }
    pipe = CreateNamedPipeW(pipeName.c_str(),  PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES, bufferSize, bufferSize, 0, &securityAttributes);
    if (securityAttributes.lpSecurityDescriptor)
      LocalFree(securityAttributes.lpSecurityDescriptor);
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
