#include "PluginStdAfx.h"

#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginFilter.h"
#include "PluginClientFactory.h"
#include "PluginDictionary.h"
#include "PluginHttpRequest.h"
#include "PluginMutex.h"
#include "PluginClass.h"
#include "PluginUtil.h"

#include "AdblockPlusClient.h"

namespace
{
  // TODO: GetUserName, pipeName, bufferSize, AutoHandle, ReadMessage, WriteMessage, MarshalStrings and UnmarshalStrings are
  //       duplicated in AdblockPlusEngine. We should find a way to reuse them.

  std::wstring GetUserName()
  {
    const DWORD maxLength = UNLEN + 1;
    std::auto_ptr<wchar_t> buffer(new wchar_t[maxLength]);
    DWORD length = maxLength;
    if (!::GetUserName(buffer.get(), &length))
    {
      std::stringstream stream;
      stream << "Failed to get the current user's name (Error code: " << GetLastError() << ")";
      throw std::runtime_error("Failed to get the current user's name");
    }
    return std::wstring(buffer.get(), length);
  }

  const std::wstring pipeName = L"\\\\.\\pipe\\adblockplusengine_" + GetUserName();
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

    AutoHandle(const AutoHandle& autoHandle);
    AutoHandle& operator=(const AutoHandle& autoHandle);
  };

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
    bool doneReading = false;
    while (!doneReading)
    {
      DWORD bytesRead;
      if (ReadFile(pipe, buffer.get(), bufferSize * sizeof(char), &bytesRead, 0))
        doneReading = true;
      else if (GetLastError() != ERROR_MORE_DATA)
      {
        std::stringstream stream;
        stream << "Error reading from pipe: " << GetLastError();
        throw std::runtime_error(stream.str());
      }
      stream << std::string(buffer.get(), bytesRead);
    }
    return stream.str();
  }

  void WriteMessage(HANDLE pipe, const std::string& message)
  {
    DWORD bytesWritten;
    if (!WriteFile(pipe, message.c_str(), message.length(), &bytesWritten, 0)) 
      throw std::runtime_error("Failed to write to pipe");
  }

  HANDLE OpenPipe(const std::wstring& name)
  {
    if (WaitNamedPipe(name.c_str(), 5000))
      return CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    return INVALID_HANDLE_VALUE;
  }

  void SpawnAdblockPlusEngine()
  {
    std::wstring engineExecutablePath = DllDir() + L"AdblockPlusEngine.exe";
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

    CloseHandle(processInformation.hProcess);
    CloseHandle(processInformation.hThread);
  }

  HANDLE OpenAdblockPlusEnginePipe()
  {
    try
    {
      HANDLE pipe = OpenPipe(pipeName);
      if (pipe == INVALID_HANDLE_VALUE)
      {
        SpawnAdblockPlusEngine();

        int timeout = 10000;
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

      return pipe;
    }
    catch(std::exception e)
    {
      DEBUG_GENERAL(e.what());
      return INVALID_HANDLE_VALUE;
    }
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
  return ReadMessage(pipe.get());
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
