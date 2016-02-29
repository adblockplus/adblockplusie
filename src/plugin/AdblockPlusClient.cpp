/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2016 Eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PluginStdAfx.h"
#include "AdblockPlusClient.h"
#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginFilter.h"
#include "PluginMutex.h"
#include "PluginClass.h"
#include "../shared/Utils.h"

namespace
{
  class ScopedProcessInformation : public PROCESS_INFORMATION {
  public:
    ScopedProcessInformation()
    {
      hProcess = hThread = 0;
      dwProcessId = dwThreadId = 0;
    }
    ~ScopedProcessInformation()
    {
      if (hThread != nullptr)
      {
        CloseHandle(hThread);
      }
      if (hProcess != nullptr)
      {
        CloseHandle(hProcess);
      }
    }
  };

  void SpawnAdblockPlusEngine()
  {
    std::wstring engineExecutablePath = GetDllDir() + L"AdblockPlusEngine.exe";
    std::wstring params = L"AdblockPlusEngine.exe " + GetBrowserLanguage();

    STARTUPINFO startupInfo = {};
    ScopedProcessInformation processInformation;

    // We need to break out from AppContainer. Launch with default security - registry entry will eat the user prompt
    // See http://msdn.microsoft.com/en-us/library/bb250462(v=vs.85).aspx#wpm_elebp
    BOOL createProcRes = CreateProcessW(engineExecutablePath.c_str(), &params[0],
      0, 0, false, 0, 0, 0, &startupInfo, &processInformation);
    if (!createProcRes)
    {
      throw std::runtime_error("Failed to start Adblock Plus Engine");
    }
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
CComAutoCriticalSection CAdblockPlusClient::s_criticalSectionLocal;

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
  catch (const std::exception& ex)
  {
    DEBUG_EXCEPTION(ex);
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

namespace
{
  bool ShouldBlockLocal(const std::wstring& src, AdblockPlus::FilterEngine::ContentType contentType, const std::wstring& domain, bool addDebug)
  {
    std::wstring srcTrimmed = TrimString(src);

    // We should not block the empty string, so all filtering does not make sense
    // Therefore we just return
    if (srcTrimmed.empty())
    {
      return false;
    }

    CPluginSettings* settings = CPluginSettings::GetInstance();

    CPluginClient* client = CPluginClient::GetInstance();
    bool result = client->Matches(srcTrimmed, contentType, domain);

#ifdef ENABLE_DEBUG_RESULT
    if (addDebug)
    {
      std::wstring type = ToUtf16String(AdblockPlus::FilterEngine::ContentTypeToString(contentType));
      if (result)
      {
        CPluginDebug::DebugResultBlocking(type, srcTrimmed, domain);
      }
      else
      {
        CPluginDebug::DebugResultIgnoring(type, srcTrimmed, domain);
      }
    }
#endif
    return result;
  }
}

bool CAdblockPlusClient::ShouldBlock(const std::wstring& src, AdblockPlus::FilterEngine::ContentType contentType, const std::wstring& domain, bool addDebug)
{
  bool isBlocked = false;
  bool isCached = false;
  m_criticalSectionCache.Lock();
  {
    auto it = m_cacheBlockedSources.find(src);

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
      isBlocked = ShouldBlockLocal(src, contentType, domain, addDebug);
    }
    m_criticalSectionFilter.Unlock();

    // Cache result, if content type is defined
    if (contentType != AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_OTHER)
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

bool CAdblockPlusClient::IsWhitelistedUrl(const std::wstring& url, const std::vector<std::string>& frameHierarchy)
{
  return !GetWhitelistingFilter(url, frameHierarchy).empty();
}

std::string CAdblockPlusClient::GetWhitelistingFilter(const std::wstring& url, const std::vector<std::string>& frameHierarchy)
{
  DEBUG_GENERAL((L"IsWhitelistedUrl: " + url + L" start").c_str());
  Communication::OutputBuffer request;
  request << Communication::PROC_GET_WHITELISTING_FITER << ToUtf8String(url) << frameHierarchy;

  Communication::InputBuffer response;
  if (!CallEngine(request, response)) 
    return "";

  std::string filterText;
  response >> filterText;

  DEBUG_GENERAL((L"IsWhitelistedUrl: " + url + L" end").c_str());
  return filterText;
}

bool CAdblockPlusClient::IsElemhideWhitelistedOnDomain(const std::wstring& url, const std::vector<std::string>& frameHierarchy)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_IS_ELEMHIDE_WHITELISTED_ON_URL << ToUtf8String(url) << frameHierarchy;

  Communication::InputBuffer response;
  if (!CallEngine(request, response)) 
    return false;

  bool isWhitelisted;
  response >> isWhitelisted;
  return isWhitelisted;
}

bool CAdblockPlusClient::Matches(const std::wstring& url, AdblockPlus::FilterEngine::ContentType contentType, const std::wstring& domain)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_MATCHES << ToUtf8String(url) << static_cast<int32_t>(contentType) << ToUtf8String(domain);

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

  std::vector<std::string> selectors;
  response >> selectors;
  return ToUtf16Strings(selectors);
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

// Returns true if Acceptable Ads are enabled, false otherwise.
bool CAdblockPlusClient::IsAcceptableAdsEnabled()
{
  std::vector<SubscriptionDescription> subscriptions = GetListedSubscriptions();
  std::wstring aaUrl = GetPref(L"subscriptions_exceptionsurl", L"");
  for (std::vector<SubscriptionDescription>::iterator subscription = subscriptions.begin(); subscription != subscriptions.end(); subscription++)
  {
    if (subscription->url == aaUrl)
    {
      return true;
    }
  }
  return false;
}

void CAdblockPlusClient::SetSubscription(const std::wstring& url)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_SET_SUBSCRIPTION << ToUtf8String(url);
  CallEngine(request);
}

void CAdblockPlusClient::AddSubscription(const std::wstring& url)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_ADD_SUBSCRIPTION << ToUtf8String(url);
  CallEngine(request);
}

void CAdblockPlusClient::RemoveSubscription(const std::wstring& url)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_REMOVE_SUBSCRIPTION << ToUtf8String(url);
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

  std::vector<std::string> domains;
  response >> domains;
  return ToUtf16Strings(domains);
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
  RemoveFilter(ToUtf8String(text));
}

void CAdblockPlusClient::RemoveFilter(const std::string& text)
{
  Communication::OutputBuffer request;
  request << Communication::PROC_REMOVE_FILTER << text;
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
  std::string docLink;
  response >> docLink;
  return ToUtf16String(docLink);
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

int CAdblockPlusClient::CompareVersions(const std::wstring& v1, const std::wstring& v2)
{
  DEBUG_GENERAL("CompareVersions");
  Communication::OutputBuffer request;
  request << Communication::PROC_COMPARE_VERSIONS << ToUtf8String(v1) << ToUtf8String(v2);
  Communication::InputBuffer response;
  if (!CallEngine(request, response))
    return 0;
  int result;
  response >> result;
  return result;
}