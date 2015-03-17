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

/**
* This class contains all client functionality of the IE plugin
*
* Exception errors are tested by calls to ExceptionsTest from: Main ...
*/

#ifndef _PLUGIN_SETTINGS_H_
#define _PLUGIN_SETTINGS_H_

#include <map>

// Main settings

#define SETTING_PLUGIN_INFO_PANEL	    L"plugininfopanel"
#define SETTING_PLUGIN_VERSION          L"pluginversion"
#define SETTING_PLUGIN_UPDATE_VERSION   L"pluginupdateversion"
#define SETTING_PLUGIN_SELFTEST         L"pluginselftest"
#define SETTING_LANGUAGE                L"language"
#define SETTING_DICTIONARY_VERSION      L"dictionaryversion"

// Tab settings

#define SETTING_TAB_PLUGIN_ENABLED          L"pluginenabled"
#define SETTING_TAB_COUNT                   L"tabcount"
#define SETTING_TAB_START_TIME              L"tabstart"
#define SETTING_TAB_UPDATE_ON_START         L"updateonstart"
#define SETTING_TAB_UPDATE_ON_START_REMOVE  L"updateonstartremove"
#define SETTING_TAB_DICTIONARY_VERSION      L"dictionaryversion"
#define SETTING_TAB_SETTINGS_VERSION        L"settingsversion"
#define SETTING_TAB_FILTER_VERSION          L"filterversion"
#define SETTING_TAB_WHITELIST_VERSION       L"whitelistversion"

class CPluginIniFileW;

class CPluginSettings
{
private:
  DWORD m_dwWorkingThreadId;

  static CComAutoCriticalSection s_criticalSectionLocal;

  void Clear();

  // Private constructor used by the singleton pattern
  CPluginSettings();

public:
  ~CPluginSettings();

  static CPluginSettings* s_instance;

  static bool HasInstance();
  static CPluginSettings* GetInstance();

  bool IsPluginEnabled() const;

  std::map<CString, CString> GetFilterLanguageTitleList() const;

  void SetWorkingThreadId();
  void SetWorkingThreadId(DWORD id);
  bool IsWorkingThread(DWORD dwThread=0) const;

  DWORD m_WindowsBuildNumber;

public:

  void TogglePluginEnabled();
  bool GetPluginEnabled() const;

  void AddError(const CString& error, const CString& errorCode);

  // Settings whitelist
private:
  std::vector<std::wstring> m_whitelistedDomains;
  void ClearWhitelist();
  bool ReadWhitelist(bool bDebug=true);

public:
  void AddWhiteListedDomain(const CString& domain);
  void RemoveWhiteListedDomain(const CString& domain);
  int GetWhiteListedDomainCount() const;
  std::vector<std::wstring> GetWhiteListedDomainList();

  bool RefreshWhitelist();
  DWORD GetWindowsBuildNumber();

  void SetSubscription(const std::wstring& url);
  void SetDefaultSubscription();
  CString GetSubscription();
  CString GetAppLocale();
  CString GetDocumentationLink();
};

std::wstring GetDataPath(const std::wstring& filename=L"");

#endif // _PLUGIN_SETTINGS_H_
