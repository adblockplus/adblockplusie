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
  // TODO: bufferSize, AutoHandle, ReadMessage, WriteMessage, MarshalStrings and UnmarshalStrings are duplicated in AdblockPlusEngine

  const int bufferSize = 1024;

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
    if (WaitNamedPipe(name.c_str(), 5000))
      return CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    return INVALID_HANDLE_VALUE;
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

  void SpawnAdblockPlusEngine()
  {
    std::wstring engineExecutablePath = GetDllDirectory() + L"\\AdblockPlusEngine.exe";
    STARTUPINFO startupInfo = {};
    PROCESS_INFORMATION processInformation = {};

    HANDLE token;
    OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY, &token);
    HANDLE newToken;
    DuplicateTokenEx(token, 0, 0, SecurityImpersonation, TokenPrimary, &newToken);

    if (!CreateProcessAsUser(newToken, 0, const_cast<wchar_t*>(engineExecutablePath.c_str()), 0, 0, 0, 0, 0, 0,
                              &startupInfo, &processInformation))
    {
      DWORD error = GetLastError();
      throw std::runtime_error("Failed to start Adblock Plus Engine");
    }
  }

  HANDLE OpenAdblockPlusEnginePipe()
  {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    try
    {
      LPCWSTR pipeName = L"\\\\.\\pipe\\adblockplusengine";
      pipe = OpenPipe(pipeName);
      if (pipe == INVALID_HANDLE_VALUE)
      {
        SpawnAdblockPlusEngine();

        int timeout = 5000;
        while ((pipe = OpenPipe(pipeName)) == INVALID_HANDLE_VALUE)
        {
          const int step = 10;
          Sleep(step);
          timeout -= step;
          if (timeout <= 0)
            throw std::runtime_error("Unable to open Adblock Plus Engine pipe");
        }
      }

      DWORD mode = PIPE_READMODE_MESSAGE; 
      if (!SetNamedPipeHandleState(pipe, &mode, 0, 0)) 
         throw std::runtime_error("SetNamedPipeHandleState failed");
    }
    catch(std::exception e)
    {
      MessageBoxA(NULL, e.what(), "Exception", MB_OK);
    }
    return pipe;
  }
}

CAdblockPlusClient* CAdblockPlusClient::s_instance = NULL;

CAdblockPlusClient::CAdblockPlusClient() : CPluginClientBase()
{
  m_filter = std::auto_ptr<CPluginFilter>(new CPluginFilter());
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

std::string CallAdblockPlusEngineProcedure(const std::string& name, const std::vector<std::string>& args)
{
  AutoHandle pipe(OpenAdblockPlusEnginePipe());
  std::vector<std::string> strings;
  strings.push_back(name);
  for (std::vector<std::string>::const_iterator it = args.begin(); it != args.end(); it++)
    strings.push_back(*it);
  WriteMessage(pipe.get(), MarshalStrings(strings));
  std::string response = ReadMessage(pipe.get());
  return response;
}

bool CAdblockPlusClient::Matches(const std::string& url, const std::string& contentType, const std::string& domain)
{
  std::vector<std::string> args;
  args.push_back(url);
  args.push_back(contentType);
  args.push_back(domain);

  try
  {
    std::string response = CallAdblockPlusEngineProcedure("Matches", args);
    return response == "1";
  }
  catch (const std::exception& e)
  {
    DEBUG_GENERAL(e.what());
    return false;
  }
}

std::vector<std::string> CAdblockPlusClient::GetElementHidingSelectors(std::string domain)
{
  std::vector<std::string> args;
  args.push_back(domain);

  try
  {
    std::string response = CallAdblockPlusEngineProcedure("GetElementHidingSelectors", args);
    return UnmarshalStrings(response);
  }
  catch (const std::exception& e)
  {
    DEBUG_GENERAL(e.what());
    return std::vector<std::string>();
  }
}

std::vector<AdblockPlus::SubscriptionPtr> CAdblockPlusClient::FetchAvailableSubscriptions()
{
  //TODO: implement this
  return std::vector<AdblockPlus::SubscriptionPtr>();
}

std::vector<AdblockPlus::FilterPtr> CAdblockPlusClient::GetListedFilters()
{
  //TODO: implement this
  return std::vector<AdblockPlus::FilterPtr>();
}

AdblockPlus::FilterPtr CAdblockPlusClient::GetFilter(std::string text)
{
  //TODO: implement this
  return AdblockPlus::FilterPtr();
}

std::vector<AdblockPlus::SubscriptionPtr> CAdblockPlusClient::GetListedSubscriptions()
{
  //TODO: implement this
  return std::vector<AdblockPlus::SubscriptionPtr>();
}

AdblockPlus::SubscriptionPtr CAdblockPlusClient::GetSubscription(std::string url)
{
  //TODO: imlement this
  return AdblockPlus::SubscriptionPtr();
}
