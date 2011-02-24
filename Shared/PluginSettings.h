/**
 * This class contains all client functionality af the IE plugin
 *
 * Exception errors are tested by calls to ExceptionsTest from: Main ...
 */

#ifndef _PLUGIN_SETTINGS_H_
#define _PLUGIN_SETTINGS_H_


#include "PluginTypedef.h"


// Main settings

#define SETTING_USER_ID                 L"userid"
#define SETTING_PLUGIN_ID	            L"pluginuniqueid"
#define SETTING_PLUGIN_INFO_PANEL	    L"plugininfopanel"
#define SETTING_PLUGIN_EXPIRED          L"pluginexpired"
#define SETTING_PLUGIN_PASSWORD         L"pluginpassword"
#define SETTING_PLUGIN_VERSION          L"pluginversion"
#define SETTING_PLUGIN_UPDATE_URL       L"pluginupdateurl"
#define SETTING_PLUGIN_UPDATE_VERSION   L"pluginupdateversion"
#define SETTING_PLUGIN_UPDATE_TIME      L"pluginupdatetime"
#define SETTING_PLUGIN_SELFTEST         L"pluginselftest"
#define SETTING_LANGUAGE                L"language"
#define SETTING_PLUGIN_REGISTRATION     L"registration"
#define SETTING_PLUGIN_ADBLOCKLIMIT		L"adblocklimit"
#define SETTING_PLUGIN_ADBLOCKCOUNT		L"adblockcount"
#define SETTING_PLUGIN_LIMITDAY			L"limitday"
#ifdef SUPPORT_FILTER
#define SETTING_FILTER_VERSION          L"filterversion"
#endif
#ifdef SUPPORT_CONFIG
#define SETTING_CONFIG_VERSION          L"configversion"
#endif
#define SETTING_LAST_UPDATE_TIME        L"lastupdatetime"
#define SETTING_REG_DATE                L"regdate"
#define SETTING_REG_ATTEMPTS            L"regattempts"
#define SETTING_REG_SUCCEEDED           L"regsucceeded"
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

public:

    typedef std::map<CString, CString> TProperties;

private:

    bool m_isFirstRun;
    bool m_isFirstRunUpdate;

    DWORD m_dwMainProcessId;
    DWORD m_dwMainThreadId;
    DWORD m_dwMainUiThreadId;
    DWORD m_dwWorkingThreadId;
    
    CString m_tabNumber;
	
#ifdef SUPPORT_WHITELIST
	TDomainList m_domainList;
	TDomainHistory m_domainHistory;
#endif
	
	CPluginSettings::TProperties m_properties;

	bool m_isDirty;

#ifdef SUPPORT_FILTER
	CPluginSettings::TFilterUrlList m_filterUrlList;
	std::map<CString, CString> m_filterFileNameList;
#endif

	CString m_settingsVersion;
    std::auto_ptr<CPluginIniFileW> m_settingsFile;

	static WCHAR* s_dataPath;
	static WCHAR* s_dataPathParent;

	static CPluginSettings* s_instance;

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
	CPluginSettings();	

public:

	~CPluginSettings();

    static bool HasInstance();
    static CPluginSettings* GetInstance();
 
    bool Read(bool bDebug=true);
	bool Write(bool bDebug=true);

	static CString GetDataPathParent();
	static CString GetDataPath(const CString& filename=L"");

	static CString GetTempPath(const CString& filename=L"");
    static CString GetTempFile(const CString& prefix, const CString& extension=L"");

    bool Has(const CString& key) const;
    void Remove(const CString& key);

    CString GetPluginId();

	CString GetString(const CString& key, const CString& defaultValue=L"") const;
	void SetString(const CString& key, const CString& value);

	int GetValue(const CString& key, int defaultValue=0) const;
	void SetValue(const CString& key, int value);

	bool GetBool(const CString& key, bool defaultValue) const;
	void SetBool(const CString& key, bool value);

    bool IsPluginEnabled() const;
	bool IsPluginUpdateAvailable() const;
	
	bool IsPluginSelftestEnabled();

#ifdef SUPPORT_FILTER

	void SetFilterUrlList(const TFilterUrlList& filters);
	void SetFilterFileNamesList(const std::map<CString, CString>& filters);
	TFilterUrlList GetFilterUrlList() const;
	std::map<CString, CString> GetFilterFileNamesList() const;

    void AddFilterUrl(const CString& url, int version);
    void AddFilterFileName(const CString& url, const CString& fileName);
#endif // SUPPORT_FILTER

#ifdef SUPPORT_WHITELIST
	
    void AddDomainToHistory(const CString& domain);
    TDomainHistory GetDomainHistory() const;

#endif // SUPPORT_WHITELIST

    void SetMainProcessId();
    bool IsMainProcess(DWORD dwProcessId=0) const;

    void SetMainUiThreadId();
    bool IsMainUiThread(DWORD dwThread=0) const;

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

	CPluginSettings::TProperties m_propertiesTab;
	CPluginSettings::TProperties m_errorsTab;

    std::auto_ptr<CPluginIniFileW> m_settingsFileTab;

    void ClearTab();

    bool ReadTab(bool bDebug=true);
    bool WriteTab(bool bDebug=true);

public:

    void EraseTab();

	CString GetTabNumber() const;

    bool IncrementTabCount();
    bool DecrementTabCount();

    void TogglePluginEnabled();
	void SetPluginDisabled();
	void SetPluginEnabled();
    bool GetPluginEnabled() const;

    void AddError(const CString& error, const CString& errorCode);
    CString GetErrorList() const;
    void RemoveErrors();

    bool GetForceConfigurationUpdateOnStart() const;
    void ForceConfigurationUpdateOnStart(bool isUpdating=true);
    void RemoveForceConfigurationUpdateOnStart();

    void RefreshTab();

    int GetTabVersion(const CString& key) const;
    void IncrementTabVersion(const CString& key);

    // Settings whitelist
#ifdef SUPPORT_WHITELIST

private:

	bool m_isDirtyWhitelist;

	TDomainList m_whitelist;
	TDomainList m_whitelistToGo;

    std::auto_ptr<CPluginIniFileW> m_settingsFileWhitelist;
    
    void ClearWhitelist();

    bool ReadWhitelist(bool bDebug=true);
    bool WriteWhitelist(bool bDebug=true);

public:

	void AddWhiteListedDomain(const CString& domain, int reason=1, bool isToGo=false);
    void RemoveWhiteListedDomainsToGo(const TDomainList& domains);
    void ReplaceWhiteListedDomains(const TDomainList& domains);
	bool IsWhiteListedDomain(const CString& domain) const;
	int GetWhiteListedDomainCount() const;
	TDomainList GetWhiteListedDomainList(bool isToGo=false) const;

    bool RefreshWhitelist();

#endif //SUPPORT_WHITELIST

};


#endif // _PLUGIN_SETTINGS_H_
