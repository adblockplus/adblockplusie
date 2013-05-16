#include <AdblockPlus.h>
#include <iostream>
#include <ShlObj.h>
#include <sstream>
#include <vector>
#include <Windows.h>
#include <Sddl.h>

namespace
{
  const std::wstring pipeName = L"\\\\.\\pipe\\adblockplusengine";
  const int bufferSize = 1024;
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine;
  HANDLE filterEngineMutex;

  class AutoHandle
  {
  public:
    AutoHandle()
    {
    }

    AutoHandle(HANDLE handle) : handle(handle)
    {
    }

    ~AutoHandle()
    {
      CloseHandle(handle);
    }

    HANDLE get()
    {
      return handle;
    }

  private:
    HANDLE handle;
    static int references;

    AutoHandle(const AutoHandle& autoHandle);
    AutoHandle& operator=(const AutoHandle& autoHandle);
  };

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

  std::string MarshalStrings(const std::vector<std::string>& strings)
  {
    // TODO: This is some pretty hacky marshalling, replace it with something more robust
    std::string marshalledStrings;
    for (std::vector<std::string>::const_iterator it = strings.begin(); it != strings.end(); it++)
      marshalledStrings += *it + ';';
    return marshalledStrings;
  }

  std::vector<std::string> UnmarshalStrings(const std::string& message)
  {
    std::stringstream stream(message);
    std::vector<std::string> strings;
    std::string string;
    while (std::getline(stream, string, ';'))
        strings.push_back(string);
    return strings;
  }

  std::string ToString(std::wstring value)
  {
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, 0, 0, 0, 0);
    char* converted = new char[size];
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, converted, size, 0, 0);
    std::string string(converted);
    delete converted;
    return string;
  }

  std::string ReadMessage(HANDLE pipe)
  {
    std::stringstream stream;
    std::auto_ptr<char> buffer(new char[bufferSize]);
    bool hasError;
    do
    {
      DWORD bytesRead;
      hasError = !ReadFile(pipe, buffer.get(), bufferSize * sizeof(char), &bytesRead, 0);
      if (hasError && GetLastError() != ERROR_MORE_DATA)
      {
        std::stringstream stream;
        stream << "Error reading from pipe: " << GetLastError();
        throw std::runtime_error(stream.str());
      }
      stream << std::string(buffer.get(), bytesRead);
    } while (hasError);
    return stream.str();
  }

  void WriteMessage(HANDLE pipe, const std::string& message)
  {
    DWORD bytesWritten;
    if (!WriteFile(pipe, message.c_str(), message.length(), &bytesWritten, 0)) 
      throw std::runtime_error("Failed to write to pipe");
  }

  std::string HandleRequest(const std::vector<std::string>& strings)
  {
    std::string procedureName = strings[0];
    if (procedureName == "Matches")
      return filterEngine->Matches(strings[1], strings[2], strings[3]) ? "1" : "0";
    if (procedureName == "GetElementHidingSelectors")
      return MarshalStrings(filterEngine->GetElementHidingSelectors(strings[1]));
    return "";
  }

  DWORD WINAPI ClientThread(LPVOID param)
  {
    HANDLE pipe = static_cast<HANDLE>(param);

    try
    {
      std::string message = ReadMessage(pipe);
      std::vector<std::string> strings = UnmarshalStrings(message);
      WaitForSingleObject(filterEngineMutex, INFINITE);
      std::string response = HandleRequest(strings);
      ReleaseMutex(filterEngineMutex);
      WriteMessage(pipe, response);
    }
    catch (const std::exception& e)
    {
      LogException(e);
    }

    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
  }
}

bool IsWindowsVistaOrLater()
{
  OSVERSIONINFOEX osvi;
  ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&osvi));
  return osvi.dwMajorVersion >= 6;
}

std::wstring GetAppDataPath()
{
  wchar_t appDataPath[MAX_PATH];
  if (IsWindowsVistaOrLater())
  {
    WCHAR* dataPath;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, 0, &dataPath)))
      throw std::runtime_error("Unable to find app data directory");
    wcscpy_s(appDataPath, dataPath);
    CoTaskMemFree(dataPath);
  }
  else
  { 
    if (!SHGetSpecialFolderPath(0, appDataPath, CSIDL_LOCAL_APPDATA, true))
      throw std::runtime_error("Unable to find app data directory");
  }
  return std::wstring(appDataPath) + L"\\AdblockPlus";
}

std::auto_ptr<AdblockPlus::FilterEngine> CreateFilterEngine()
{
  // TODO: Pass appInfo in, which should be sent by the client
  AdblockPlus::JsEnginePtr jsEngine = AdblockPlus::JsEngine::New();
  std::string dataPath = ToString(GetAppDataPath());
  dynamic_cast<AdblockPlus::DefaultFileSystem*>(jsEngine->GetFileSystem().get())->SetBasePath(dataPath);
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine(new AdblockPlus::FilterEngine(jsEngine));
  int timeout = 5000;
  while (!filterEngine->IsInitialized())
  {
    const int step = 10;
    Sleep(step);
    timeout -= step;
    if (timeout <= 0)
      throw std::runtime_error("Timeout while waiting for FilterEngine initialization");
  }
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
  LPCWSTR LOW_INTEGRITY_SDDL_SACL_W = L"S:(ML;;NW;;;LW)";
  PSECURITY_DESCRIPTOR securitydescriptor;
  ConvertStringSecurityDescriptorToSecurityDescriptor(
    LOW_INTEGRITY_SDDL_SACL_W, SDDL_REVISION_1, &securitydescriptor, 0);

  sa.lpSecurityDescriptor = securitydescriptor;
  sa.bInheritHandle = TRUE;

  return CreateNamedPipe(pipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                         PIPE_UNLIMITED_INSTANCES, bufferSize, bufferSize, 0, &sa);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  filterEngine = CreateFilterEngine();
  filterEngineMutex = CreateMutex(0, false, 0);

  for (;;)
  {
    HANDLE pipe = CreatePipe(pipeName);
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
