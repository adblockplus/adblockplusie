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

  Communication::OutputBuffer HandleRequest(Communication::InputBuffer& request)
  {
    Communication::OutputBuffer response;

    std::string procedureName;
    request >> procedureName;
    if (procedureName == "Matches")
    {
      std::string url;
      std::string type;
      std::string documentUrl;
      request >> url >> type >> documentUrl;
      response << filterEngine->Matches(url, type, documentUrl);
    }
    if (procedureName == "GetElementHidingSelectors")
    {
      std::string domain;
      request >> domain;

      std::vector<std::string> selectors = filterEngine->GetElementHidingSelectors(domain);

      int32_t length = selectors.size();
      response << length;
      for (int32_t i = 0; i < length; i++)
        response << selectors[i];
    }
    return response;
  }

  DWORD WINAPI ClientThread(LPVOID param)
  {
    std::auto_ptr<Communication::Pipe> pipe(static_cast<Communication::Pipe*>(param));

    try
    {
      Communication::InputBuffer message = pipe->ReadMessage();
      Communication::OutputBuffer response = HandleRequest(message);
      pipe->WriteMessage(response);
    }
    catch (const std::exception& e)
    {
      LogException(e);
    }

    // TODO: Keep the pipe open until the client disconnects

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
  return filterEngine;
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
    try
    {
      Communication::Pipe* pipe = new Communication::Pipe(Communication::pipeName,
            Communication::Pipe::MODE_CREATE);

      // TODO: Count established connections, kill the engine when none are left
      AutoHandle thread(CreateThread(0, 0, ClientThread, static_cast<LPVOID>(pipe), 0, 0));
      if (!thread.get())
      {
        delete pipe;
        LogLastError("CreateThread failed");
        return 1;
      }
    }
    catch (std::runtime_error e)
    {
      LogException(e);
      return 1;
    }
  }

  return 0;
}
