#include "PluginStdAfx.h"

#include <Wbemidl.h>
#include <time.h>
#include "PluginSettings.h"
#include "PluginClient.h"
#include "PluginSystem.h"
#ifdef SUPPORT_FILTER
#include "PluginFilter.h"
#endif
#include "PluginMutex.h"
#include "../shared/Utils.h"
#include <memory>


// IE functions
#pragma comment(lib, "iepmapi.lib")

#include <knownfolders.h>

namespace
{
  std::wstring CreateDomainWhitelistingFilter(const CString domain)
  {
    return std::wstring(L"@@||") + domain.GetString() + std::wstring(L"^$document");
  }
}

class TSettings
{
  DWORD processorId;

  char sPluginId[44];
};


class CPluginSettingsLock : public CPluginMutex
{
public:
  CPluginSettingsLock() : CPluginMutex("SettingsFile", PLUGIN_ERROR_MUTEX_SETTINGS_FILE) {}
  ~CPluginSettingsLock() {}

};


class CPluginSettingsTabLock : public CPluginMutex
{
public:
  CPluginSettingsTabLock() : CPluginMutex("SettingsFileTab", PLUGIN_ERROR_MUTEX_SETTINGS_FILE_TAB) {}
  ~CPluginSettingsTabLock() {}
};

#ifdef SUPPORT_WHITELIST

class CPluginSettingsWhitelistLock : public CPluginMutex
{
public:
  CPluginSettingsWhitelistLock() : CPluginMutex("SettingsFileWhitelist", PLUGIN_ERROR_MUTEX_SETTINGS_FILE_WHITELIST) {}
  ~CPluginSettingsWhitelistLock() {}
};

#endif

CPluginSettings* CPluginSettings::s_instance = NULL;

CComAutoCriticalSection CPluginSettings::s_criticalSectionLocal;


CPluginSettings::CPluginSettings() : m_dwWorkingThreadId(0)
{
  s_instance = NULL;

  m_WindowsBuildNumber = 0;

#ifdef SUPPORT_WHITELIST
  ClearWhitelist();
#endif
}


CPluginSettings::~CPluginSettings()
{
  s_instance = NULL;
}


CPluginSettings* CPluginSettings::GetInstance()
{
  CPluginSettings* instance = NULL;

  s_criticalSectionLocal.Lock();
  {
    if (!s_instance)
    {
      s_instance = new CPluginSettings();
    }

    instance = s_instance;
  }
  s_criticalSectionLocal.Unlock();

  return instance;
}


bool CPluginSettings::HasInstance()
{
  bool hasInstance = true;

  s_criticalSectionLocal.Lock();
  {
    hasInstance = s_instance != NULL;
  }
  s_criticalSectionLocal.Unlock();

  return hasInstance;
}



CString CPluginSettings::GetDataPath(const CString& filename)
{
  std::wstring path = ::GetAppDataPath() + L"\\" + static_cast<LPCWSTR>(filename);
  return CString(path.c_str());
}

CString CPluginSettings::GetSystemLanguage()
{
  CString language;
  CString country;

  DWORD bufSize = 256;
  int ccBuf = GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SISO639LANGNAME, language.GetBufferSetLength(bufSize), bufSize);
  ccBuf = GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SISO3166CTRYNAME, country.GetBufferSetLength(bufSize), bufSize);

  if ((country.IsEmpty()) || (language.IsEmpty()))
  {
    return CString();
  }
  CString lang;
  lang.Append(language);
  lang.Append(L"-");
  lang.Append(country);

  return lang;

}


bool CPluginSettings::IsPluginEnabled() const
{
  return GetPluginEnabled();
}


std::map<CString, CString> CPluginSettings::GetFilterLanguageTitleList()
{
  m_subscriptions = CPluginClient::GetInstance()->FetchAvailableSubscriptions();

  std::map<CString, CString> filterList;
  for (size_t i = 0; i < m_subscriptions.size(); i ++)
  {
    SubscriptionDescription it = m_subscriptions[i];
    filterList.insert(std::make_pair(CString(it.url.c_str()), CString(it.title.c_str())));
  }
  return filterList;
}

bool CPluginSettings::IsWorkingThread(DWORD dwThreadId) const
{
  if (dwThreadId == 0)
  {
    dwThreadId = ::GetCurrentThreadId();
  }
  return m_dwWorkingThreadId == dwThreadId;
}

void CPluginSettings::SetWorkingThreadId()
{
  m_dwWorkingThreadId = ::GetCurrentThreadId();
}

void CPluginSettings::SetWorkingThreadId(DWORD id)
{
  m_dwWorkingThreadId = id;
}

void CPluginSettings::TogglePluginEnabled()
{
  CPluginClient::GetInstance()->TogglePluginEnabled();  
}
bool CPluginSettings::GetPluginEnabled() const
{
  return CPluginClient::GetInstance()->GetPref(L"enabled", true);
}


void CPluginSettings::AddError(const CString& error, const CString& errorCode)
{
  DEBUG_SETTINGS(L"SettingsTab::AddError error:" + error + " code:" + errorCode)
}


// ============================================================================
// Whitelist settings
// ============================================================================

#ifdef SUPPORT_WHITELIST

void CPluginSettings::ClearWhitelist()
{
  s_criticalSectionLocal.Lock();
  {
    m_whitelistedDomains.clear();
  }
  s_criticalSectionLocal.Unlock();
}


bool CPluginSettings::ReadWhitelist(bool isDebug)
{
  bool isRead = true;

  DEBUG_SETTINGS("SettingsWhitelist::Read")

    if (isDebug)
    {
      DEBUG_GENERAL("*** Loading whitelist settings");
    }

    CPluginSettingsWhitelistLock lock;
    if (lock.IsLocked())
    {
      ClearWhitelist();

      s_criticalSectionLocal.Lock();
      m_whitelistedDomains = CPluginClient::GetInstance()->GetExceptionDomains();
      s_criticalSectionLocal.Unlock();
    }
    else
    {
      isRead = false;
    }

    return isRead;
}


void CPluginSettings::AddWhiteListedDomain(const CString& domain)
{
  DEBUG_SETTINGS("SettingsWhitelist::AddWhiteListedDomain domain:" + domain)
  CPluginClient::GetInstance()->AddFilter(CreateDomainWhitelistingFilter(domain));
}

void CPluginSettings::RemoveWhiteListedDomain(const CString& domain)
{
  DEBUG_SETTINGS("SettingsWhitelist::RemoveWhiteListedDomain domain:" + domain)
  CPluginClient::GetInstance()->RemoveFilter(CreateDomainWhitelistingFilter(domain));
}

int CPluginSettings::GetWhiteListedDomainCount() const
{
  int count = 0;

  s_criticalSectionLocal.Lock();
  {
    count = (int)m_whitelistedDomains.size();
  }
  s_criticalSectionLocal.Unlock();

  return count;
}


std::vector<std::wstring> CPluginSettings::GetWhiteListedDomainList()
{
  bool r = ReadWhitelist(false);
  return m_whitelistedDomains;
}


bool CPluginSettings::RefreshWhitelist()
{
  CPluginSettingsWhitelistLock lock;
  if (lock.IsLocked())
  {
    ReadWhitelist(true);
  }

  return true;
}

DWORD CPluginSettings::GetWindowsBuildNumber()
{
  if (m_WindowsBuildNumber == 0)
  {
    OSVERSIONINFOEX osvi;
    SYSTEM_INFO si;
    BOOL bOsVersionInfoEx;

    ZeroMemory(&si, sizeof(SYSTEM_INFO));
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO*) &osvi);

    m_WindowsBuildNumber = osvi.dwBuildNumber;
  }

  return m_WindowsBuildNumber;
}

void CPluginSettings::SetSubscription(const std::wstring& url)
{
  CPluginClient::GetInstance()->SetSubscription(url);
  RefreshWhitelist();
}

CString CPluginSettings::GetSubscription()
{
  std::vector<SubscriptionDescription> subscriptions = CPluginClient::GetInstance()->GetListedSubscriptions();
  if (subscriptions.size() > 0)
    return CString(subscriptions.front().url.c_str());
  else
    return CString(L"");
}

CString CPluginSettings::GetAppLocale()
{
  return CPluginSystem::GetInstance()->GetBrowserLanguage();
}

CString CPluginSettings::GetDocumentationLink()
{
  return CString(CPluginClient::GetInstance()->GetDocumentationLink().c_str());
}



#endif // SUPPORT_WHITELIST
