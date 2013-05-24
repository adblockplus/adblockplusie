#include "stdafx.h"

#include "../shared/AutoHandle.h"
#include "../shared/Communication.h"

namespace
{
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine;

  void Log(const std::string& message)
  {
    // TODO: Log to a log file
    MessageBoxA(0, ("AdblockPlusEngine: " + message).c_str(), "", MB_OK);
  }

  void LogLastError(const std::string& message)
  {
    std::stringstream stream;
    stream << message << " (Error code: " << GetLastError() << ")";
    Log(stream.str());
  }

  void LogException(const std::exception& exception)
  {
    Log(std::string("An exception occurred: ") + exception.what());
  }

  std::string ToUtf8String(std::wstring str)
  {
    size_t length = str.size();
    if (length == 0)
      return std::string();

    DWORD utf8StringLength = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), length, 0, 0, 0, 0);
    if (utf8StringLength == 0)
      throw std::runtime_error("Failed to determine the required buffer size");

    std::string utf8String(utf8StringLength, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), length, &utf8String[0], utf8StringLength, 0, 0);
    return utf8String;
  }

  std::string HandleRequest(const std::vector<std::string>& strings)
  {
    std::string procedureName = strings[0];
    if (procedureName == "Matches")
      return filterEngine->Matches(strings[1], strings[2], strings[3]) ? "1" : "0";
    if (procedureName == "GetElementHidingSelectors")
      return Communication::MarshalStrings(filterEngine->GetElementHidingSelectors(strings[1]));
    return "";
  }

  DWORD WINAPI ClientThread(LPVOID param)
  {
    HANDLE pipe = static_cast<HANDLE>(param);

    try
    {
      std::string message = Communication::ReadMessage(pipe);
      std::vector<std::string> strings = Communication::UnmarshalStrings(message);
      std::string response = HandleRequest(strings);
      Communication::WriteMessage(pipe, response);
    }
    catch (const std::exception& e)
    {
      LogException(e);
    }

    // TODO: Keep the pipe open until the client disconnects
    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
  }

  bool IsWindowsVistaOrLater()
  {
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&osvi));
    return osvi.dwMajorVersion >= 6;
  }
}

std::wstring GetAppDataPath()
{
  std::wstring appDataPath;
  if (IsWindowsVistaOrLater())
  {
    WCHAR* pathBuffer;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, 0, &pathBuffer)))
      throw std::runtime_error("Unable to find app data directory");
    appDataPath.assign(pathBuffer);
    CoTaskMemFree(pathBuffer);
  }
  else
  { 
    std::auto_ptr<wchar_t> pathBuffer(new wchar_t[MAX_PATH]); 
    if (!SHGetSpecialFolderPath(0, pathBuffer.get(), CSIDL_LOCAL_APPDATA, true))
      throw std::runtime_error("Unable to find app data directory");
    appDataPath.assign(pathBuffer.get());
  }
  return appDataPath + L"\\AdblockPlus";
}

std::auto_ptr<AdblockPlus::FilterEngine> CreateFilterEngine()
{
  // TODO: Pass appInfo in, which should be sent by the client
  AdblockPlus::JsEnginePtr jsEngine = AdblockPlus::JsEngine::New();
  std::string dataPath = ToUtf8String(GetAppDataPath());
  dynamic_cast<AdblockPlus::DefaultFileSystem*>(jsEngine->GetFileSystem().get())->SetBasePath(dataPath);
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine(new AdblockPlus::FilterEngine(jsEngine));
  std::vector<AdblockPlus::SubscriptionPtr> subscriptions = filterEngine->FetchAvailableSubscriptions();
  // TODO: Select a subscription based on the language, not just the first one.
  //       This should ideally be done in libadblockplus.
  AdblockPlus::SubscriptionPtr subscription = subscriptions[0];
  subscription->AddToList();
  return filterEngine;
}

HANDLE CreatePipe(const std::wstring& pipeName)
{
  SECURITY_ATTRIBUTES sa;
  memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);

  // Low mandatory label. See http://msdn.microsoft.com/en-us/library/bb625958.aspx
  LPCWSTR accessControlEntry = L"S:(ML;;NW;;;LW)";
  PSECURITY_DESCRIPTOR securitydescriptor;
  ConvertStringSecurityDescriptorToSecurityDescriptor(accessControlEntry, SDDL_REVISION_1, &securitydescriptor, 0);

  sa.lpSecurityDescriptor = securitydescriptor;
  sa.bInheritHandle = TRUE;

  HANDLE pipe = CreateNamedPipe(pipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                PIPE_UNLIMITED_INSTANCES, Communication::bufferSize, Communication::bufferSize, 0, &sa);
  LocalFree(securitydescriptor);
  return pipe;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  // TODO: Attempt to create the pipe first, and exit immediately if this
  //       fails. Since multiple instances of the engine could be running,
  //       this may need named mutices to avoid race conditions.
  //       Note that as soon as the pipe is created first, we can reduce the
  //       client timeout after CreateProcess(), but should increase the one
  //       in WaitNamedPipe().

  filterEngine = CreateFilterEngine();

  for (;;)
  {
    HANDLE pipe = CreatePipe(Communication::pipeName);
    if (pipe == INVALID_HANDLE_VALUE)
    {
      LogLastError("CreateNamedPipe failed");
      return 1;
    }

    if (!ConnectNamedPipe(pipe, 0))
    {
      LogLastError("Client failed to connect");
      CloseHandle(pipe);
      continue;
    }

    // TODO: Count established connections, kill the engine when none are left

    AutoHandle thread(CreateThread(0, 0, ClientThread, static_cast<LPVOID>(pipe), 0, 0));
    if (!thread.get())
    {
      LogLastError("CreateThread failed");
      return 1;
    }
  }

  return 0;
}
