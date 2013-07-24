#include "PluginStdAfx.h"

#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginFilter.h"
#include "PluginClientFactory.h"
#include "PluginMutex.h"
#include "PluginClass.h"

#include "AdblockPlusClient.h"

#include "../shared/Utils.h"

namespace
{
  void SpawnAdblockPlusEngine()
  {
    std::wstring engineExecutablePath = GetDllDir() + L"AdblockPlusEngine.exe";
    CString params = L"AdblockPlusEngine.exe " + CPluginSystem::GetInstance()->GetBrowserLanguage();

    STARTUPINFO startupInfo = {};
    PROCESS_INFORMATION processInformation = {};

    HANDLE token;
    OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY, &token);
    HANDLE newToken;
    DuplicateTokenEx(token, 0, 0, SecurityImpersonation, TokenPrimary, &newToken);

    if (!CreateProcessAsUserW(newToken, engineExecutablePath.c_str(),
                              params.GetBuffer(params.GetLength() + 1),
                              0, 0, 0, 0, 0, 0, &startupInfo, &processInformation))
    {
      DWORD error = GetLastError();
      throw std::runtime_error("Failed to start Adblock Plus Engine");
    }

    CloseHandle(processInformation.hProcess);
    CloseHandle(processInformation.hThread);
  }

  std::auto_ptr<Communication::Pipe> OpenAdblockPlusEnginePipe()
  {
    try
    {
      return std::auto_ptr<Communication::Pipe>(new Communication::Pipe(Communication::pipeName, Communication::Pipe::MODE_CONNECT));
    }
    catch (Communication::PipeConnectionError e)
    {
      SpawnAdblockPlusEngine();

      const int step = 100;
      for (int timeout = 10000; timeout > 0; timeout -= step)
      {
        Sleep(step);
        try
        {
          return std::auto_ptr<Communication::Pipe>(new Communication::Pipe(Communication::pipeName, Communication::Pipe::MODE_CONNECT));
        }
        catch (Communication::PipeConnectionError e)
        {
        }
      }
      throw std::runtime_error("Unable to open Adblock Plus Engine pipe");
    }
  }

  std::vector<std::wstring> ReadStrings(Communication::InputBuffer& message)
  {
    int32_t count;
    message >> count;

    std::vector<std::wstring> result;
    for (int32_t i = 0; i < count; i++)
    {
      std::string str;
      message >> str;
      result.push_back(ToUtf16String(str));
    }
    return result;
  }

  std::vector<SubscriptionDescription> ReadSubscriptions(Communication::InputBuffer& message)
  {
    int32_t count;
    message >> count;

    std::vector<SubscriptionDescription> result;
    for (int32_t i = 0; i < count; i++)
    {
      SubscriptionDescription description;
      std::string url;
      message >> url;
      description.url = ToUtf16String(url);
      std::string title;
      message >> title;
      description.title = ToUtf16String(title);
      std::string specialization;
      message >> specialization;
      description.specialization = ToUtf16String(specialization);
      message >> description.listed;
      result.push_back(description);
    }
    return result;
  }

  bool CallEngine(Communication::OutputBuffer& message, Communication::InputBuffer* inputBuffer = NULL)
  {
    try
    {
      std::auto_ptr<Communication::Pipe> pipe = OpenAdblockPlusEnginePipe();
      pipe->WriteMessage(message);
      if (inputBuffer != NULL)
      {
        *inputBuffer = pipe->ReadMessage();
      }
      else
      {
        pipe->ReadMessage();
      }
    }
    catch (const std::exception& e)
    {
      DEBUG_GENERAL(e.what());
      return false;
    }
    return true;
  }

  bool CallEngine(Communication::ProcType proc, Communication::InputBuffer* inputBuffer = NULL)
  {
    Communication::OutputBuffer message;
    message << proc;
    return CallEngine(message, inputBuffer);
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

bool CAdblockPlusClient::IsWhitelistedUrl(const std::wstring& url)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_IS_WHITELISTED_URL << ToUtf8String(url);

  Communication::InputBuffer response;
  if (!CallEngine(request, &response)) return false;;

  bool isWhitelisted;
  response >> isWhitelisted;
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

bool CAdblockPlusClient::Matches(const std::wstring& url, const std::wstring& contentType, const std::wstring& domain)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_MATCHES << ToUtf8String(url) << ToUtf8String(contentType) << ToUtf8String(domain);

  Communication::InputBuffer response;
  if (!CallEngine(request, &response)) return false;;

  bool match;
  response >> match;
  return match;
}

std::vector<std::wstring> CAdblockPlusClient::GetElementHidingSelectors(const std::wstring& domain)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_GET_ELEMHIDE_SELECTORS << ToUtf8String(domain);

  Communication::InputBuffer response;
  if (!CallEngine(request, &response)) return std::vector<std::wstring>();
  return ReadStrings(response);
}

std::vector<SubscriptionDescription> CAdblockPlusClient::FetchAvailableSubscriptions()
{
  Communication::InputBuffer response;
  if (!CallEngine(Communication::PROC_AVAILABLE_SUBSCRIPTIONS, &response)) return std::vector<SubscriptionDescription>();
  return ReadSubscriptions(response);
}

std::vector<SubscriptionDescription> CAdblockPlusClient::GetListedSubscriptions()
{
  Communication::InputBuffer response;
  if (!CallEngine(Communication::PROC_LISTED_SUBSCRIPTIONS, &response)) return std::vector<SubscriptionDescription>();
  return ReadSubscriptions(response);
}

void CAdblockPlusClient::SetSubscription(const std::wstring& url)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_SET_SUBSCRIPTION << ToUtf8String(url);
  CallEngine(request);
}

void CAdblockPlusClient::UpdateAllSubscriptions()
{
  CallEngine(Communication::PROC_UPDATE_ALL_SUBSCRIPTIONS);
}

std::vector<std::wstring> CAdblockPlusClient::GetExceptionDomains()
{
  Communication::InputBuffer response;
  if (!CallEngine(Communication::PROC_GET_EXCEPTION_DOMAINS)) return std::vector<std::wstring>();
  return ReadStrings(response);
}

bool CAdblockPlusClient::IsFirstRun()
{
  Communication::InputBuffer response;
  if (!CallEngine(Communication::PROC_IS_FIRST_RUN_ACTION_NEEDED, &response)) return false;
  bool res;
  response >> res;
  return res;
}
void CAdblockPlusClient::AddFilter(const std::wstring& text)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_ADD_FILTER << ToUtf8String(text);
  CallEngine(request);
}

void CAdblockPlusClient::RemoveFilter(const std::wstring& text)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_REMOVE_FILTER << ToUtf8String(text);
  CallEngine(request);
}

void CAdblockPlusClient::SetPref(const std::wstring& name, const std::wstring& value)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_SET_PREF << ToUtf8String(name) << ToUtf8String(value);
  CallEngine(request);
}

void CAdblockPlusClient::SetPref(const std::wstring& name, const int64_t & value)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_SET_PREF << ToUtf8String(name) << value;
  CallEngine(request);
}

void CAdblockPlusClient::SetPref(const std::wstring& name, bool value)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_SET_PREF << ToUtf8String(name) << value;
  CallEngine(request);
}

std::wstring CAdblockPlusClient::GetPref(const std::wstring& name, const wchar_t* defaultValue)
{
  return GetPref(name, std::wstring(defaultValue));
}
std::wstring CAdblockPlusClient::GetPref(const std::wstring& name, const std::wstring& defaultValue)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_GET_PREF << ToUtf8String(name);

  Communication::InputBuffer response;
  if (!CallEngine(request, &response)) return defaultValue;
  bool success;
  response >> success;
  if (success)
  {
    std::string value;
    response >> value;
    return ToUtf16String(value);
  }
  else
    return defaultValue;
}

bool CAdblockPlusClient::GetPref(const std::wstring& name, bool defaultValue)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_GET_PREF << ToUtf8String(name);

  Communication::InputBuffer response;
  if (!CallEngine(request, &response)) return defaultValue;
  bool success;
  response >> success;
  if (success)
  {
    bool value;
    response >> value;
    return value;
  }
  else
    return defaultValue;
}
int64_t CAdblockPlusClient::GetPref(const std::wstring& name, int64_t defaultValue)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_GET_PREF << ToUtf8String(name);

  Communication::InputBuffer response;
  if (!CallEngine(request, &response)) return defaultValue;
    bool success;
  response >> success;
  if (success)
  {
    int64_t value;
    response >> value;
    return value;
  }
  else
    return defaultValue;
}
