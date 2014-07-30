/**
* This class contains all client functionality of the IE plugin
*
* Exception errors are tested by calls to ExceptionsTest from: Main ...
*/

#ifndef _PLUGIN_SETTINGS_H_
#define _PLUGIN_SETTINGS_H_


#include "PluginTypedef.h"
#include "AdblockPlusClient.h"

// Main settings

#define SETTING_PLUGIN_INFO_PANEL	    L"plugininfopanel"
#define SETTING_PLUGIN_VERSION          L"pluginversion"
#define SETTING_PLUGIN_UPDATE_VERSION   L"pluginupdateversion"
#define SETTING_PLUGIN_SELFTEST         L"pluginselftest"
#define SETTING_LANGUAGE                L"language"

#ifdef SUPPORT_CONFIG
#define SETTING_CONFIG_VERSION          L"configversion"
#endif
#define SETTING_DICTIONARY_VERSION      L"dictionaryversion"

// Tab settings

#define SETTING_TAB_PLUGIN_ENABLED          L"pluginenabled"
#define SETTING_TAB_COUNT                   L"tabcount"
#define SETTING_TAB_START_TIME              L"tabstart"
#define SETTING_TAB_UPDATE_ON_START         L"updateonstart"
#define SETTING_TAB_UPDATE_ON_START_REMOVE  L"updateonstartremove"
#define SETTING_TAB_DICTIONARY_VERSION      L"dictionaryversion"
#define SETTING_TAB_SETTINGS_VERSION        L"settingsversion"
#ifdef SUPPORT_FILTER
#define SETTING_TAB_FILTER_VERSION          L"filterversion"
#endif

#ifdef SUPPORT_WHITELIST
#define SETTING_TAB_WHITELIST_VERSION       L"whitelistversion"
#endif
#ifdef SUPPORT_CONFIG
#define SETTING_TAB_CONFIG_VERSION          L"configversion"
#endif


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

  static CString GetDataPath(const CString& filename=L"");

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
#ifdef SUPPORT_WHITELIST

private:
  std::vector<std::wstring> m_whitelistedDomains;

  void ClearWhitelist();
  bool ReadWhitelist(bool bDebug=true);

public:
  void AddWhiteListedDomain(const CString& domain);
  void RemoveWhiteListedDomain(const CString& domain);
  int GetWhiteListedDomainCount() const;
  std::vector<std::wstring> GetWhiteListedDomainList();
#endif //SUPPORT_WHITELIST

  bool RefreshWhitelist();
  DWORD GetWindowsBuildNumber();

  void SetSubscription(const std::wstring& url);
  void SetDefaultSubscription();
  CString GetSubscription();
  CString GetAppLocale();
  CString GetDocumentationLink();
};


#endif // _PLUGIN_SETTINGS_H_
