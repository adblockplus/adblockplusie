#include "PluginStdAfx.h"

#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginFilter.h"
#include "PluginClientFactory.h"
#include "PluginDictionary.h"
#include "PluginHttpRequest.h"
#include "PluginMutex.h"
#include "PluginClass.h"

#include "AdblockPlusClient.h"

namespace
{
  // TODO: bufferSize, ToWideString, ReadMessage and WriteMessage are duplicated in AdblockPlusEngine

  const int bufferSize = 1024;

  std::auto_ptr<AdblockPlus::FilterEngine> CreateFilterEngine()
  {
    try
    {
      DEBUG_GENERAL("Building client");
      AdblockPlus::AppInfo appInfo;
      appInfo.name = "adblockplusie";
      appInfo.version = CT2CA(_T(IEPLUGIN_VERSION), CP_UTF8);
      appInfo.platform = "msie";
  
      DEBUG_GENERAL(L"Building engine");

      JsEnginePtr jsEngine(AdblockPlus::JsEngine::New(appInfo));
      return std::auto_ptr<AdblockPlus::FilterEngine>(new AdblockPlus::FilterEngine(jsEngine));
    }
    catch(std::exception ex)
    {
      DEBUG_GENERAL(ex.what());
    }
  }

  LPCWSTR ToWideString(LPCSTR value)
  {
    int size = MultiByteToWideChar(CP_UTF8, 0, value, -1, 0, 0);
    wchar_t* converted = new wchar_t[size];
    MultiByteToWideChar(CP_UTF8, 0, value, -1, converted, size);
    return converted;
  }

  std::wstring GetDllDirectory()
  {
    wchar_t buffer[MAX_PATH];
    GetModuleFileName(reinterpret_cast<HINSTANCE>(&__ImageBase), buffer, MAX_PATH);
    std::wstring modulePath(buffer);
    int lastSeparator = modulePath.find_last_of(L"\\");
    return modulePath.substr(0, lastSeparator);
  }

  HANDLE OpenPipe(const std::wstring& name)
  {
    HANDLE pipe = CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    // TODO: Use WaitNamedPipe before giving up if this is INVALID_HANDLE
    return pipe;
  }

  HANDLE OpenAdblockPlusEnginePipe()
  {
    LPCWSTR pipeName = L"\\\\.\\pipe\\adblockplusengine";
    HANDLE pipe = OpenPipe(pipeName);
    if (pipe == INVALID_HANDLE_VALUE)
    {
      std::wstring engineExecutablePath = GetDllDirectory() + L"\\AdblockPlusEngine.exe";
      STARTUPINFO startupInfo = {};
      PROCESS_INFORMATION processInformation = {};
      if (!CreateProcess(engineExecutablePath.c_str(), 0, 0, 0, false, 0, 0, 0, &startupInfo, &processInformation))
        throw std::runtime_error("Failed to start Adblock Plus Engine");
      // TODO: The engine needs some time to update its filters and create the pipe, but there should be a better way than Sleep()
      Sleep(1000);

      pipe = OpenPipe(pipeName);
      if (pipe == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Unable to open Adblock Plus Engine pipe");
    }

    DWORD mode = PIPE_READMODE_MESSAGE; 
    if (!SetNamedPipeHandleState(pipe, &mode, 0, 0)) 
       throw std::runtime_error("SetNamedPipeHandleState failed");

    return pipe;
  }

  std::string MarshalStrings(const std::vector<std::string>& strings)
  {
    std::string marshalledStrings;
    for (std::vector<std::string>::const_iterator it = strings.begin(); it != strings.end(); it++)
      marshalledStrings += *it + '\0';
    return marshalledStrings;
  }

  std::string ReadMessage(HANDLE pipe)
  {
    // TODO: Read messages larger than the bufferSize
    char* buffer = new char[bufferSize];
    DWORD bytesRead;
    if (!ReadFile(pipe, buffer, bufferSize * sizeof(char), &bytesRead, 0) || !bytesRead)
    {
      delete buffer;
      std::stringstream stream;
      stream << "Error reading from pipe: " << GetLastError();
      throw std::runtime_error(stream.str());
    }
    std::string message(buffer, bytesRead);
    delete buffer;
    return message;
  }

  void WriteMessage(HANDLE pipe, const std::string& message)
  {
    // TODO: Make sure messages with >bufferSize chars work
    DWORD bytesWritten;
    if (!WriteFile(pipe, message.c_str(), message.length(), &bytesWritten, 0)) 
      throw std::runtime_error("Failed to write to pipe");
  }
}

CAdblockPlusClient* CAdblockPlusClient::s_instance = NULL;

CAdblockPlusClient::CAdblockPlusClient() : CPluginClientBase()
{
  m_filter = std::auto_ptr<CPluginFilter>(new CPluginFilter());

  // TODO: Don't create a filter engine here. For this we need to invoke all
  //       filter engine methods via IPC. This is already done for
  //       FilterEngine::Matches().
  filterEngine = CreateFilterEngine();
}

CAdblockPlusClient::~CAdblockPlusClient()
{
  s_instance = NULL;
}


CAdblockPlusClient* CAdblockPlusClient::GetInstance()
{
  CAdblockPlusClient* instance = NULL;

  s_criticalSectionLocal.Lock();
  {
    if (!s_instance)
    {
      CAdblockPlusClient* client = new CAdblockPlusClient();

      s_instance = client;
    }

    instance = s_instance;
  }
  s_criticalSectionLocal.Unlock();

  return instance;
}

AdblockPlus::FilterEngine* CAdblockPlusClient::GetFilterEngine()
{
  return filterEngine.get();
}

bool CAdblockPlusClient::ShouldBlock(CString src, int contentType, const CString& domain, bool addDebug)
{
  bool isBlocked = false;

  bool isCached = false;

  CPluginSettings* settings = CPluginSettings::GetInstance();

  m_criticalSectionCache.Lock();
  {
    std::map<CString,bool>::iterator it = m_cacheBlockedSources.find(src);

    isCached = it != m_cacheBlockedSources.end();
    if (isCached)
    {
      isBlocked = it->second;
    }
  }
  m_criticalSectionCache.Unlock();

  if (!isCached)
  {
    m_criticalSectionFilter.Lock();
    {
      isBlocked = m_filter->ShouldBlock(src, contentType, domain, addDebug);
    }
    m_criticalSectionFilter.Unlock();


    // Cache result, if content type is defined
    if (contentType != CFilter::contentTypeAny)
    {
      m_criticalSectionCache.Lock();
      {
        m_cacheBlockedSources[src] = isBlocked;
      }
      m_criticalSectionCache.Unlock();
    }
  }


  return isBlocked;
}

bool CAdblockPlusClient::IsElementHidden(const CString& tag, IHTMLElement* pEl, const CString& domain, const CString& indent, CPluginFilter* filter)
{
  bool isHidden;
  m_criticalSectionFilter.Lock();
  {
    isHidden = filter && filter->IsElementHidden(tag, pEl, domain, indent);
  }
  m_criticalSectionFilter.Unlock();
  return isHidden;
}

bool CAdblockPlusClient::IsUrlWhiteListed(const CString& url)
{
  bool isWhitelisted = CPluginClientBase::IsUrlWhiteListed(url);
  if (isWhitelisted == false && !url.IsEmpty())
  {
    m_criticalSectionFilter.Lock();
    {
      isWhitelisted = m_filter.get() && m_filter->ShouldWhiteList(url);
    }
    m_criticalSectionFilter.Unlock();

    if (isWhitelisted)
    {
      CacheWhiteListedUrl(url, isWhitelisted);
    }
  }

  return isWhitelisted;
}

int CAdblockPlusClient::GetIEVersion()
{
  //HKEY_LOCAL_MACHINE\Software\Microsoft\Internet Explorer
  HKEY hKey;
  LSTATUS status = RegOpenKey(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Internet Explorer", &hKey);
  if (status != 0)
  {
    return 0;
  }
  DWORD type, cbData;
  BYTE version[50];
  cbData = 50;
  status = RegQueryValueEx(hKey, L"Version", NULL, &type, (BYTE*)version, &cbData);
  if (status != 0)
  {
    return 0;
  }
  RegCloseKey(hKey);
  return (int)(version[0] - 48);
}

bool CAdblockPlusClient::Matches(const std::string& url, const std::string& contentType, const std::string& domain)
{
  HANDLE pipe;
  try
  {
    pipe = OpenAdblockPlusEnginePipe();
  }
  catch (const std::exception& e)
  {
    DEBUG_GENERAL(ToWideString(e.what()));
    MessageBoxA(0, e.what(), "Exception", MB_OK);
    return false;
  }

  std::wstringstream stream;
  stream << "Sending request for " << ToWideString(url.c_str()) << " in process " << GetCurrentProcessId() << ", thread " << GetCurrentThreadId();
  DEBUG_GENERAL(stream.str().c_str());

  std::vector<std::string> args;
  args.push_back(url);
  args.push_back(contentType);
  args.push_back(domain);

  boolean matches = false;

  try
  {
    WriteMessage(pipe, MarshalStrings(args));
    std::string message = ReadMessage(pipe);
    matches = message == "1";

    stream.str(std::wstring());
    stream << "Got response for " << ToWideString(url.c_str()) << " in process " << GetCurrentProcessId() << ", thread " << GetCurrentThreadId();
    DEBUG_GENERAL(stream.str().c_str());
  }
  catch (const std::exception& e)
  {
    DEBUG_GENERAL(ToWideString(e.what()));
    MessageBoxA(0, e.what(), "Exception", MB_OK);
  }

  CloseHandle(pipe);

  return matches;
}
