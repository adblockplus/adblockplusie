/**
 * This class contains all client functionality af the IE plugin
 *
 * Exception errors are tested by calls to ExceptionsTest from: Main ...
 */

#ifndef _ADPLUGIN_SETTINGS_H_
#define _ADPLUGIN_SETTINGS_H_


#include "AdPluginTypedef.h"


// Main settings

#define SETTING_USER_ID                 "userid"
#define SETTING_PLUGIN_ID	            "pluginuniqueid"
#define SETTING_PLUGIN_INFO_PANEL	    "plugininfopanel"
#define SETTING_PLUGIN_ACTIVATED        "pluginactivated"
#define SETTING_PLUGIN_ACTIVATE_ENABLED "pluginactivateenabled"
#define SETTING_PLUGIN_EXPIRED          "pluginexpired"
#define SETTING_PLUGIN_PASSWORD         "pluginpassword"
#define SETTING_PLUGIN_VERSION          "pluginversion"
#define SETTING_PLUGIN_UPDATE_URL       "pluginupdateurl"
#define SETTING_PLUGIN_UPDATE_VERSION   "pluginupdateversion"
#define SETTING_PLUGIN_UPDATE_TIME      "pluginupdatetime"
#define SETTING_PLUGIN_SELFTEST         "pluginselftest"
#define SETTING_LANGUAGE                "language"
#ifdef SUPPORT_FILTER
#define SETTING_FILTER_VERSION          "filterversion"
#endif
#ifdef SUPPORT_CONFIG
#define SETTING_CONFIG_VERSION          "configversion"
#endif
#define SETTING_LAST_UPDATE_TIME        "lastupdatetime"
#define SETTING_REG_DATE                "regdate"
#define SETTING_REG_ATTEMPTS            "regattempts"
#define SETTING_REG_SUCCEEDED           "regsucceeded"
#define SETTING_DICTIONARY_VERSION      "dictionaryversion"

// Tab settings

#define SETTING_TAB_PLUGIN_ENABLED          "pluginenabled"
#define SETTING_TAB_COUNT                   "tabcount"
#define SETTING_TAB_START_TIME              "tabstart"
#define SETTING_TAB_UPDATE_ON_START         "updateonstart"
#define SETTING_TAB_UPDATE_ON_START_REMOVE  "updateonstartremove"
#define SETTING_TAB_DICTIONARY_VERSION      "dictionaryversion"
#define SETTING_TAB_SETTINGS_VERSION        "settingsversion"
#ifdef SUPPORT_FILTER
#define SETTING_TAB_FILTER_VERSION          "filterversion"
#endif
#ifdef SUPPORT_WHITELIST
#define SETTING_TAB_WHITELIST_VERSION       "whitelistversion"
#endif
#ifdef SUPPORT_CONFIG
#define SETTING_TAB_CONFIG_VERSION          "configversion"
#endif


class CAdPluginIniFile;


class CAdPluginSettings
{

public:

    typedef std::map<CStringA, CStringA> TProperties;

private:

    bool m_isFirstRun;
    bool m_isFirstRunUpdate;

    DWORD m_dwMainProcessId;
    DWORD m_dwMainThreadId;
    DWORD m_dwWorkingThreadId;
    
    CStringA m_tabNumber;
	
#ifdef SUPPORT_WHITELIST
	TDomainList m_domainList;
	TDomainHistory m_domainHistory;
#endif
	
	CAdPluginSettings::TProperties m_properties;

	bool m_isDirty;

#ifdef SUPPORT_FILTER
	CAdPluginSettings::TFilterUrlList m_filterUrlList;
#endif

	CStringA m_settingsVersion;
    std::auto_ptr<CAdPluginIniFile> m_settingsFile;

	static char* s_dataPath;
	static CAdPluginSettings* s_instance;

	static CComAutoCriticalSection s_criticalSectionLocal;
#ifdef SUPPORT_FILTER
	static CComAutoCriticalSection s_criticalSectionFilters;
#endif
#ifdef SUPPORT_WHITELIST
	static CComAutoCriticalSection s_criticalSectionDomainHistory;
#endif

	bool m_isPluginSelftestEnabled;

    void Clear();

	// Private constructor used by the singleton pattern
	CAdPluginSettings();	

public:

	~CAdPluginSettings();

    static bool HasInstance();
    static CAdPluginSettings* GetInstance();
 
    bool Read(bool bDebug=true);
	bool Write(bool bDebug=true);

	static CStringA GetDataPathParent();
	static CStringA GetDataPath(const CStringA& filename="");

	static CStringA GetTempPath(const CStringA& filename="");
    static CStringA GetTempFile(const CStringA& prefix);

    bool Has(const CStringA& key) const;
    void Remove(const CStringA& key);

    CStringA GetPluginId();

	CStringA GetString(const CStringA& key, const CStringA& defaultValue="") const;
	void SetString(const CStringA& key, const CStringA& value);

	int GetValue(const CStringA& key, int defaultValue=0) const;
	void SetValue(const CStringA& key, int value);

	bool GetBool(const CStringA& key, bool defaultValue) const;
	void SetBool(const CStringA& key, bool value);

    bool IsPluginEnabled() const;
	bool IsPluginUpdateAvailable() const;
	
	bool IsPluginSelftestEnabled();

#ifdef SUPPORT_FILTER

	void SetFilterUrlList(const TFilterUrlList& filters);
	TFilterUrlList GetFilterUrlList() const;

    void AddFilterUrl(const CStringA& url, int version);
#endif // SUPPORT_FILTER

#ifdef SUPPORT_WHITELIST
	
    void AddDomainToHistory(const CStringA& domain);
    TDomainHistory GetDomainHistory() const;

#endif // SUPPORT_WHITELIST

    void SetMainProcessId();
    bool IsMainProcess(DWORD dwProcessId=0) const;

    void SetMainThreadId();
    bool IsMainThread(DWORD dwThread=0) const;

    void SetWorkingThreadId();
    bool IsWorkingThread(DWORD dwThread=0) const;

    void SetFirstRun();
    bool IsFirstRun() const;
    
    void SetFirstRunUpdate();
    bool IsFirstRunUpdate() const;

    bool IsFirstRunAny() const;

    // Settings tab
private:

	bool m_isDirtyTab;
	bool m_isPluginEnabledTab;

	CAdPluginSettings::TProperties m_propertiesTab;
	CAdPluginSettings::TProperties m_errorsTab;

    std::auto_ptr<CAdPluginIniFile> m_settingsFileTab;

    void ClearTab();

    bool ReadTab(bool bDebug=true);
    bool WriteTab(bool bDebug=true);

public:

    void EraseTab();

	CStringA GetTabNumber() const;

    bool IncrementTabCount();
    bool DecrementTabCount();

    void TogglePluginEnabled();
    bool GetPluginEnabled() const;

    void AddError(const CStringA& error, const CStringA& errorCode);
    CStringA GetErrorList() const;
    void RemoveErrors();

    bool GetForceConfigurationUpdateOnStart() const;
    void ForceConfigurationUpdateOnStart(bool isUpdating=true);
    void RemoveForceConfigurationUpdateOnStart();

    void RefreshTab();

    int GetTabVersion(const CStringA& key) const;
    void IncrementTabVersion(const CStringA& key);

    // Settings whitelist
#ifdef SUPPORT_WHITELIST

private:

	bool m_isDirtyWhitelist;

	TDomainList m_whitelist;
	TDomainList m_whitelistToGo;

    std::auto_ptr<CAdPluginIniFile> m_settingsFileWhitelist;
    
    void ClearWhitelist();

    bool ReadWhitelist(bool bDebug=true);
    bool WriteWhitelist(bool bDebug=true);

public:

	void AddWhiteListedDomain(const CStringA& domain, int reason=1, bool isToGo=false);
    void RemoveWhiteListedDomainsToGo(const TDomainList& domains);
    void ReplaceWhiteListedDomains(const TDomainList& domains);
	bool IsWhiteListedDomain(const CStringA& domain) const;
	int GetWhiteListedDomainCount() const;
	TDomainList GetWhiteListedDomainList(bool isToGo=false) const;

    bool RefreshWhitelist();

#endif //SUPPORT_WHITELIST

};


#endif // _ADPLUGIN_SETTINGS_H_
