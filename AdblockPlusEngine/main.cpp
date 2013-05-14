#include <AdblockPlus.h>
#include <iostream>
#include <ShlObj.h>
#include <sstream>
#include <vector>
#include <Windows.h>
#include <Sddl.h>

namespace
{
  const int bufferSize = 1024;
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine;
  HANDLE filterEngineMutex;

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

  std::vector<std::string> UnmarshalStrings(const std::string& message, int count)
  {
    char* remaining_message = const_cast<char*>(message.c_str());
    std::vector<std::string> strings;
    for (int i = 0; i < count; i++)
    {
      std::string::value_type* part = remaining_message;
      strings.push_back(remaining_message);
      remaining_message += strlen(part) + 1;
    }
    return strings;
  }

  LPCWSTR ToWideString(LPCSTR value)
  {
    int size = MultiByteToWideChar(CP_UTF8, 0, value, -1, 0, 0);
    wchar_t* converted = new wchar_t[size];
    MultiByteToWideChar(CP_UTF8, 0, value, -1, converted, size);
    return converted;
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

  DWORD WINAPI ClientThread(LPVOID param)
  {
    HANDLE pipe = static_cast<HANDLE>(param);

    try
    {
      std::string message = ReadMessage(pipe);
      std::vector<std::string> args = UnmarshalStrings(message, 3);
      WaitForSingleObject(filterEngineMutex, INFINITE);
      bool matches = filterEngine->Matches(args[0], args[1], args[2]);
      ReleaseMutex(filterEngineMutex);
      WriteMessage(pipe, matches ? "1" : "0");
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

std::wstring GetAppDataPath()
{
  wchar_t appDataPath[MAX_PATH];
  // TODO: Doesn't support all Windows versions like this. Also duplicated from CPluginSettings
  if (!SHGetSpecialFolderPath(0, appDataPath, CSIDL_LOCAL_APPDATA, true))
    throw std::runtime_error("Unable to find app data directory");
  return std::wstring(appDataPath) + L"\\AdblockPlusEngine";
}

std::auto_ptr<AdblockPlus::FilterEngine> CreateFilterEngine()
{
    // TODO: Pass the app info in
  AdblockPlus::JsEnginePtr jsEngine = AdblockPlus::JsEngine::New();
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine(new AdblockPlus::FilterEngine(jsEngine));
  std::vector<AdblockPlus::SubscriptionPtr> subscriptions = filterEngine->FetchAvailableSubscriptions();
  AdblockPlus::SubscriptionPtr subscription = subscriptions[0];
  subscription->AddToList();
  return filterEngine;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  SetCurrentDirectory(GetAppDataPath().c_str());
  filterEngine = CreateFilterEngine();
  filterEngineMutex = CreateMutex(0, false, 0);

  for (;;)
  {
    //Load the Low Integrity security attributes
    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));

    //Low mandatory label. See http://msdn.microsoft.com/en-us/library/bb625958.aspx
    LPCWSTR LOW_INTEGRITY_SDDL_SACL_W = L"S:(ML;;NW;;;LW)";
    PSECURITY_DESCRIPTOR securitydescriptor;
    //Yes, that's a function name
    ConvertStringSecurityDescriptorToSecurityDescriptor(
      LOW_INTEGRITY_SDDL_SACL_W, SDDL_REVISION_1, &securitydescriptor, 0);
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = securitydescriptor;
    sa.bInheritHandle = TRUE;

    LPCWSTR pipeName = L"\\\\.\\pipe\\adblockplusengine";
    HANDLE pipe = CreateNamedPipe(pipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                  PIPE_UNLIMITED_INSTANCES, bufferSize, bufferSize, 0, &sa);
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

    // TODO: Count established connections, kill the engine when there are none left

    HANDLE thread = CreateThread(0, 0, ClientThread, static_cast<LPVOID>(pipe), 0, 0);
    if (!thread)
    {
      LogLastError("CreateThread failed");
      return 1;
    }
    CloseHandle(thread);
  }

  return 0;
}
