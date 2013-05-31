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

#include "../shared/AutoHandle.h"
#include "../shared/Communication.h"

namespace
{
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

  std::auto_ptr<Communication::Pipe> OpenAdblockPlusEnginePipe()
  {
    std::auto_ptr<Communication::Pipe> result;
    try
    {
      try
      {
        result.reset(new Communication::Pipe(Communication::pipeName,
            Communication::Pipe::MODE_CONNECT));
      }
      catch (Communication::PipeConnectionError e)
      {
        SpawnAdblockPlusEngine();

        int timeout = 10000;
        const int step = 10;
        while (!result.get())
        {
          try
          {
            result.reset(new Communication::Pipe(Communication::pipeName,
                  Communication::Pipe::MODE_CONNECT));
          }
          catch (Communication::PipeConnectionError e)
          {
            Sleep(step);
            timeout -= step;
            if (timeout <= 0)
              throw std::runtime_error("Unable to open Adblock Plus Engine pipe");
          }
        }
      }
    }
    catch(std::exception e)
    {
      DEBUG_GENERAL(e.what());
    }
    return result;
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

Communication::InputBuffer CallAdblockPlusEngineProcedure(Communication::OutputBuffer& message)
{
  std::auto_ptr<Communication::Pipe> pipe = OpenAdblockPlusEnginePipe();
  pipe->WriteMessage(message);
  return pipe->ReadMessage();
}

bool CAdblockPlusClient::Matches(const std::string& url, const std::string& contentType, const std::string& domain)
{
  Communication::OutputBuffer request;
  request << std::string("Matches") << url << contentType << domain;

  try
  {
    Communication::InputBuffer response = CallAdblockPlusEngineProcedure(request);

    bool match;
    response >> match;
    return match;
  }
  catch (const std::exception& e)
  {
    DEBUG_GENERAL(e.what());
    return false;
  }
}

std::vector<std::string> CAdblockPlusClient::GetElementHidingSelectors(const std::string& domain)
{
  Communication::OutputBuffer request;
  request << std::string("GetElementHidingSelectors") << domain;

  try
  {
    Communication::InputBuffer response = CallAdblockPlusEngineProcedure(request);

    std::vector<std::string> selectors;
    int32_t length;
    response >> length;
    for (int32_t i = 0; i < length; i++)
    {
      std::string selector;
      response >> selector;
      selectors.push_back(selector);
    }
    return selectors;
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
