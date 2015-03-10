/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
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

#include "PluginSettings.h"
#include "AdblockPlusClient.h"
#include "PluginSystem.h"
#include "PluginMutex.h"
#include "../shared/Utils.h"

namespace
{
  std::wstring CreateDomainWhitelistingFilter(const std::wstring& domain)
  {
    return L"@@||" + domain + L"^$document";
  }
}

class CPluginSettingsWhitelistLock : public CPluginMutex
{
public:
  CPluginSettingsWhitelistLock() : CPluginMutex(L"SettingsFileWhitelist", PLUGIN_ERROR_MUTEX_SETTINGS_FILE_WHITELIST) {}
  ~CPluginSettingsWhitelistLock() {}
};

CPluginSettings* CPluginSettings::s_instance = NULL;
CComAutoCriticalSection CPluginSettings::s_criticalSectionLocal;

CPluginSettings::CPluginSettings()
{
  s_instance = NULL;
  ClearWhitelist();
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

bool CPluginSettings::IsPluginEnabled() const
{
  return GetPluginEnabled();
}

std::map<std::wstring, std::wstring> CPluginSettings::GetFilterLanguageTitleList() const
{
  auto subscriptions = CPluginClient::GetInstance()->FetchAvailableSubscriptions();

  std::map<std::wstring, std::wstring> filterList;
  for (size_t i = 0; i < subscriptions.size(); i ++)
  {
    auto it = subscriptions[i];
    filterList.insert(std::make_pair(it.url, it.title));
  }
  return filterList;
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

void CPluginSettings::AddWhiteListedDomain(const std::wstring& domain)
{
  DEBUG_SETTINGS("SettingsWhitelist::AddWhiteListedDomain domain:" + domain)
  CPluginClient::GetInstance()->AddFilter(CreateDomainWhitelistingFilter(domain));
}

void CPluginSettings::RemoveWhiteListedDomain(const std::wstring& domain)
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

void CPluginSettings::SetSubscription(const std::wstring& url)
{
  CPluginClient::GetInstance()->SetSubscription(url);
  RefreshWhitelist();
}

std::wstring CPluginSettings::GetSubscription()
{
  auto subscriptions = CPluginClient::GetInstance()->GetListedSubscriptions();
  std::wstring aaUrl = CPluginClient::GetInstance()->GetPref(L"subscriptions_exceptionsurl", L"");

  for (auto subscription = subscriptions.begin(); subscription != subscriptions.end(); subscription++)
  {
    if (subscription->url != aaUrl)
    {
      return subscription->url;
    }
  }
  return std::wstring();
}
