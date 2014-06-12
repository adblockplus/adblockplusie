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

    TOKEN_APPCONTAINER_INFORMATION *acs = NULL;
    DWORD length = 0;

    // Get AppContainer SID
    if (!GetTokenInformation(token, TokenAppContainerSid, acs, 0, &length) && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        acs = (TOKEN_APPCONTAINER_INFORMATION*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, length);
        if (acs != NULL)
        {
          GetTokenInformation(token, TokenAppContainerSid, acs, length, &length);
        }
        else
        {
          throw std::runtime_error("Out of memory");
        }
    }

    BOOL createProcRes = 0;
    // Running inside AppContainer?
    if (acs != NULL && acs->TokenAppContainer != NULL)
    {
      // We need to break out from AppContainer. Launch with default security - registry entry will eat the user prompt
      // See http://msdn.microsoft.com/en-us/library/bb250462(v=vs.85).aspx#wpm_elebp
      createProcRes = CreateProcessW(engineExecutablePath.c_str(), params.GetBuffer(params.GetLength() + 1),
                              0, 0, false, 0, 0, 0, (STARTUPINFOW*)&startupInfo, &processInformation);
    }
    else
    {
      // Launch with Low Integrity explicitly
      HANDLE newToken;
      DuplicateTokenEx(token, 0, 0, SecurityImpersonation, TokenPrimary, &newToken);

      PSID integritySid = 0;
      ConvertStringSidToSid(L"S-1-16-4096", &integritySid);
      std::tr1::shared_ptr<SID> sharedIntegritySid(static_cast<SID*>(integritySid), FreeSid); // Just to simplify cleanup

      TOKEN_MANDATORY_LABEL tml = {};
      tml.Label.Attributes = SE_GROUP_INTEGRITY;
      tml.Label.Sid = integritySid;

      // Set the process integrity level
      SetTokenInformation(newToken, TokenIntegrityLevel, &tml, sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(integritySid));

      STARTUPINFO startupInfo = {};
      PROCESS_INFORMATION processInformation = {};

      createProcRes = CreateProcessAsUserW(newToken, engineExecutablePath.c_str(), params.GetBuffer(params.GetLength() + 1),
                              0, 0, false, 0, 0, 0, (STARTUPINFOW*)&startupInfo, &processInformation);
    }

    if (!createProcRes)
    {
      throw std::runtime_error("Failed to start Adblock Plus Engine");
    }

    CloseHandle(processInformation.hProcess);
    CloseHandle(processInformation.hThread);
  }

  Communication::Pipe* OpenEnginePipe()
  {
    try
    {
      return new Communication::Pipe(Communication::pipeName, Communication::Pipe::MODE_CONNECT);
    }
    catch (Communication::PipeConnectionError e)
    {
      SpawnAdblockPlusEngine();

      const int step = 100;
      for (int timeout = ENGINE_STARTUP_TIMEOUT; timeout > 0; timeout -= step)
      {
        Sleep(step);
        try
        {
          return new Communication::Pipe(Communication::pipeName, Communication::Pipe::MODE_CONNECT);
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
}

CAdblockPlusClient* CAdblockPlusClient::s_instance = NULL;

CAdblockPlusClient::CAdblockPlusClient() : CPluginClientBase()
{
  m_filter = std::auto_ptr<CPluginFilter>(new CPluginFilter());
}

bool CAdblockPlusClient::CallEngine(Communication::OutputBuffer& message, Communication::InputBuffer& inputBuffer)
{
  DEBUG_GENERAL("CallEngine start");
  CriticalSection::Lock lock(enginePipeLock);
  try
  {
    if (!enginePipe)
      enginePipe.reset(OpenEnginePipe());
    enginePipe->WriteMessage(message);
    inputBuffer = enginePipe->ReadMessage();
  }
  catch (const std::exception& e)
  {
    DEBUG_GENERAL(e.what());
    return false;
  }
  DEBUG_GENERAL("CallEngine end");
  return true;
}

bool CAdblockPlusClient::CallEngine(Communication::ProcType proc, Communication::InputBuffer& inputBuffer)
{
  Communication::OutputBuffer message;
  message << proc;
  return CallEngine(message, inputBuffer);
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
  DEBUG_GENERAL((L"IsWhitelistedUrl: " + url + L" start").c_str());
  Communication::OutputBuffer request;
  request << Communication::PROC_IS_WHITELISTED_URL << ToUtf8String(url);

  Communication::InputBuffer response;
  if (!CallEngine(request, response)) 
    return false;

  bool isWhitelisted;
  response >> isWhitelisted;

  DEBUG_GENERAL((L"IsWhitelistedUrl: " + url + L" end").c_str());
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
  if (!CallEngine(request, response)) 
    return false;

  bool match;
  response >> match;
  return match;
}

std::vector<std::wstring> CAdblockPlusClient::GetElementHidingSelectors(const std::wstring& domain)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_GET_ELEMHIDE_SELECTORS << ToUtf8String(domain);

  Communication::InputBuffer response;
  if (!CallEngine(request, response)) 
    return std::vector<std::wstring>();
  return ReadStrings(response);
}

std::vector<SubscriptionDescription> CAdblockPlusClient::FetchAvailableSubscriptions()
{
  Communication::InputBuffer response;
  if (!CallEngine(Communication::PROC_AVAILABLE_SUBSCRIPTIONS, response)) 
    return std::vector<SubscriptionDescription>();
  return ReadSubscriptions(response);
}

std::vector<SubscriptionDescription> CAdblockPlusClient::GetListedSubscriptions()
{
  Communication::InputBuffer response;
  if (!CallEngine(Communication::PROC_LISTED_SUBSCRIPTIONS, response)) 
    return std::vector<SubscriptionDescription>();
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
  if (!CallEngine(Communication::PROC_GET_EXCEPTION_DOMAINS, response)) 
    return std::vector<std::wstring>();
  return ReadStrings(response);
}

bool CAdblockPlusClient::IsFirstRun()
{
  DEBUG_GENERAL("IsFirstRun");
  Communication::InputBuffer response;
  if (!CallEngine(Communication::PROC_IS_FIRST_RUN_ACTION_NEEDED, response)) return false;
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
  DEBUG_GENERAL((L"GetPref: " + name + L" start").c_str());
  Communication::OutputBuffer request;
  request << Communication::PROC_GET_PREF << ToUtf8String(name);

  Communication::InputBuffer response;
  if (!CallEngine(request, response)) 
    return defaultValue;
  bool success;
  response >> success;
  if (success)
  {
    std::string value;
    response >> value;
    DEBUG_GENERAL((L"GetPref: " + name + L" end").c_str());
    return ToUtf16String(value);
  }
  else
  {
    DEBUG_GENERAL((L"GetPref: " + name + L" end").c_str());
    return defaultValue;
  }
}

bool CAdblockPlusClient::GetPref(const std::wstring& name, bool defaultValue)
{
  DEBUG_GENERAL((L"GetPref: " + name + L" start").c_str());
  Communication::OutputBuffer request;
  request << Communication::PROC_GET_PREF << ToUtf8String(name);

  Communication::InputBuffer response;
  if (!CallEngine(request, response)) 
    return defaultValue;
  bool success;
  response >> success;
  if (success)
  {
    bool value;
    response >> value;
    DEBUG_GENERAL((L"GetPref: " + name + L" end").c_str());
    return value;
  }
  else
  {
    DEBUG_GENERAL((L"GetPref: " + name + L" end").c_str());
    return defaultValue;
  }
}
int64_t CAdblockPlusClient::GetPref(const std::wstring& name, int64_t defaultValue)
{
  DEBUG_GENERAL((L"GetPref: " + name + L" start").c_str());
  Communication::OutputBuffer request;
  request << Communication::PROC_GET_PREF << ToUtf8String(name);

  Communication::InputBuffer response;
  if (!CallEngine(request, response)) 
    return defaultValue;
  bool success;
  response >> success;
  if (success)
  {
    int64_t value;
    response >> value;
    DEBUG_GENERAL((L"GetPref: " + name + L" end").c_str());
    return value;
  }
  else
  {
    DEBUG_GENERAL((L"GetPref: " + name + L" end").c_str());
    return defaultValue;
  }
}

void CAdblockPlusClient::CheckForUpdates(HWND callbackWindow)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_CHECK_FOR_UPDATES << reinterpret_cast<int32_t>(callbackWindow);
  CallEngine(request);
}

std::wstring CAdblockPlusClient::GetDocumentationLink()
{
  DEBUG_GENERAL("GetDocumentationLink");
  Communication::InputBuffer response;
  if (!CallEngine(Communication::PROC_GET_DOCUMENTATION_LINK, response)) 
    return L"";
  std::wstring docLink;
  response >> docLink;
  return docLink;
}

bool CAdblockPlusClient::TogglePluginEnabled()
{
  DEBUG_GENERAL("TogglePluginEnabled");
  Communication::InputBuffer response;
  if (!CallEngine(Communication::PROC_TOGGLE_PLUGIN_ENABLED, response)) 
    return false;
  bool currentEnabledState;
  response >> currentEnabledState;
  return currentEnabledState;
}
std::wstring CAdblockPlusClient::GetHostFromUrl(const std::wstring& url)
{
  DEBUG_GENERAL("GetHostFromUrl");
  Communication::OutputBuffer request;
  request << Communication::PROC_GET_HOST << ToUtf8String(url);

  Communication::InputBuffer response;
  if (!CallEngine(request, response)) 
    return L"";
  std::string host;
  response >> host;
  return ToUtf16String(host);
}
