#include "PluginStdAfx.h"

#include <Wbemidl.h>

#include "PluginIniFileW.h"
#include "PluginSettings.h"
#include "PluginDictionary.h"
#include "PluginClient.h"
#include "PluginChecksum.h"
#ifdef SUPPORT_FILTER
#include "PluginFilter.h"
#endif
#include "PluginMutex.h"

// IE functions
#pragma comment(lib, "iepmapi.lib")

#include <knownfolders.h>

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

WCHAR* CPluginSettings::s_dataPath;
WCHAR* CPluginSettings::s_dataPathParent;

CPluginSettings* CPluginSettings::s_instance = NULL;
bool CPluginSettings::s_isLightOnly = false;

CComAutoCriticalSection CPluginSettings::s_criticalSectionLocal;
#ifdef SUPPORT_FILTER
CComAutoCriticalSection CPluginSettings::s_criticalSectionFilters;
#endif
#ifdef SUPPORT_WHITELIST
CComAutoCriticalSection CPluginSettings::s_criticalSectionDomainHistory;
#endif


CPluginSettings::CPluginSettings() : 
    m_settingsVersion("1"), m_isDirty(false), m_isFirstRun(false), m_isFirstRunUpdate(false), m_dwMainProcessId(0), m_dwMainThreadId(0), m_dwWorkingThreadId(0), 
    m_isDirtyTab(false), m_isPluginEnabledTab(true), m_tabNumber("1")
{


	CPluginSettings *lightInstance = s_instance;
	s_instance = NULL;

#ifdef SUPPORT_WHITELIST
    m_isDirtyWhitelist = false;
#endif

    m_settingsFile = std::auto_ptr<CPluginIniFileW>(new CPluginIniFileW(GetDataPath(SETTINGS_INI_FILE), true));
    m_settingsFileTab = std::auto_ptr<CPluginIniFileW>(new CPluginIniFileW(GetDataPath(SETTINGS_INI_FILE_TAB), true));
#ifdef SUPPORT_WHITELIST
    m_settingsFileWhitelist = std::auto_ptr<CPluginIniFileW>(new CPluginIniFileW(GetDataPath(SETTINGS_INI_FILE_WHITELIST), true));
#endif

	m_WindowsBuildNumber = 0;

    Clear();
    ClearTab();
#ifdef SUPPORT_WHITELIST
    ClearWhitelist();
#endif

    // Check existence of settings file
    bool isFileExisting = false;
    {
        CPluginSettingsLock lock;
        if (lock.IsLocked())
        {
            std::ifstream is;
	        is.open(GetDataPath(SETTINGS_INI_FILE), std::ios_base::in);
	        if (!is.is_open())
	        {
                m_isDirty = true;
	        }
	        else
	        {
		        is.close();

	            isFileExisting = true;
	        }
        }
    }

    // Read or convert file
    if (isFileExisting)
    {
        Read(false);
    }
    else
    {
        m_isDirty = true;
    }

	if (s_isLightOnly)
	{
		this->SetMainProcessId(lightInstance->m_dwMainProcessId);
		this->SetMainThreadId(lightInstance->m_dwMainThreadId);
		this->SetMainUiThreadId(lightInstance->m_dwMainUiThreadId);
		this->SetWorkingThreadId(lightInstance->m_dwWorkingThreadId);
	}
    Write();
}

CPluginSettings::CPluginSettings(bool isLight) : 
m_settingsVersion("1"), m_isDirty(false), m_isFirstRun(false), m_isFirstRunUpdate(false), m_dwMainProcessId(0), m_dwMainThreadId(0), m_dwWorkingThreadId(0), 
m_isDirtyTab(false), m_isPluginEnabledTab(true), m_tabNumber("1")
{

	s_instance = NULL;
#ifdef SUPPORT_WHITELIST
	m_isDirtyWhitelist = false;
#endif

	Clear();
	ClearTab();
}


CPluginSettings::~CPluginSettings()
{

	if (s_dataPathParent != NULL)
	{
		delete s_dataPathParent;
	}
	s_instance = NULL;
}


CPluginSettings* CPluginSettings::GetInstance() 
{
	CPluginSettings* instance = NULL;

	s_criticalSectionLocal.Lock();
	{
		if ((!s_instance) || (s_isLightOnly))
		{
			s_instance = new CPluginSettings();
			s_isLightOnly = false;
		}

		instance = s_instance;
	}
	s_criticalSectionLocal.Unlock();

	return instance;
}

CPluginSettings* CPluginSettings::GetInstanceLight() 
{
	CPluginSettings* instance = NULL;

	s_criticalSectionLocal.Lock();
	{
		if (!s_instance)
		{
			s_instance = new CPluginSettings(true);
			s_isLightOnly = true;
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


bool CPluginSettings::Read(bool bDebug)
{
    bool isRead = true;

    DEBUG_SETTINGS(L"Settings::Read")
    {
        if (bDebug)
        {
            DEBUG_GENERAL(L"*** Loading settings:" + m_settingsFile->GetFilePath());
        }

        CPluginSettingsLock lock;
        if (lock.IsLocked())
        {
            isRead = m_settingsFile->Read();        
            if (isRead)
            {
                if (m_settingsFile->IsValidChecksum())
                {
	                s_criticalSectionLocal.Lock();
		            {
			            m_properties = m_settingsFile->GetSectionData("Settings");

			            // Delete obsolete properties
			            TProperties::iterator it = m_properties.find("pluginupdate");
			            if (it != m_properties.end())
			            {
				            m_properties.erase(it);
				            m_isDirty = true;
			            }

			            it = m_properties.find("pluginerrors");
			            if (it != m_properties.end())
			            {
				            m_properties.erase(it);
				            m_isDirty = true;
			            }

			            it = m_properties.find("pluginerrorcodes");
			            if (it != m_properties.end())
			            {
				            m_properties.erase(it);
				            m_isDirty = true;
			            }

			            it = m_properties.find("pluginenabled");
			            if (it != m_properties.end())
			            {
				            m_properties.erase(it);
				            m_isDirty = true;
			            }

			            // Convert property 'pluginid' to 'userid'
			            if (m_properties.find(SETTING_USER_ID) == m_properties.end())
			            {
				            it = m_properties.find("pluginid");
				            if (it != m_properties.end())
				            {
					            m_properties[SETTING_USER_ID] = it->second;

					            m_properties.erase(it);
					            m_isDirty = true;
				            }
			            }
		            }
		            s_criticalSectionLocal.Unlock();

#ifdef SUPPORT_FILTER            	    
                    // Unpack filter URLs
                    CPluginIniFileW::TSectionData filters = m_settingsFile->GetSectionData("Filters");
                    int filterCount = 0;
                    bool bContinue = true;

    	            s_criticalSectionFilters.Lock();
		            {
			            m_filterUrlList.clear();

			            do
			            {
				            CString filterCountStr;
				            filterCountStr.Format(L"%d", ++filterCount);
            	            
				            CPluginIniFileW::TSectionData::iterator filterIt = filters.find(L"filter" + filterCountStr);
				            CPluginIniFileW::TSectionData::iterator versionIt = filters.find(L"filter" + filterCountStr + "v");
				            CPluginIniFileW::TSectionData::iterator fileNameIt = filters.find(L"filter" + filterCountStr + "fileName");

				            if (bContinue = (filterIt != filters.end() && versionIt != filters.end()))
				            {
					            m_filterUrlList[filterIt->second] = _wtoi(versionIt->second);
				            }

				            if (filterIt != filters.end() && fileNameIt != filters.end())
				            {
								m_filterFileNameList[filterIt->second] = fileNameIt->second;
				            }

			            } while (bContinue);
		            }
                    s_criticalSectionFilters.Unlock();

#endif // SUPPORT_FILTER
	            }
	            else
	            {
                    DEBUG_SETTINGS("Settings:Invalid checksum - Deleting file")

                    Clear();

                    DEBUG_ERROR_LOG(m_settingsFile->GetLastError(), PLUGIN_ERROR_SETTINGS, PLUGIN_ERROR_SETTINGS_FILE_READ_CHECKSUM, "Settings::Read - Checksum")
                    isRead = false;
                    m_isDirty = true;
	            }
            }
            else if (m_settingsFile->GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                DEBUG_ERROR_LOG(m_settingsFile->GetLastError(), PLUGIN_ERROR_SETTINGS, PLUGIN_ERROR_SETTINGS_FILE_READ, "Settings::Read")
                m_isDirty = true;
            }
            else
            {
                DEBUG_ERROR_LOG(m_settingsFile->GetLastError(), PLUGIN_ERROR_SETTINGS, PLUGIN_ERROR_SETTINGS_FILE_READ, "Settings::Read")
            }
        }
        else
        {
            isRead = false;
        }
    }

	// Write file in case it is dirty
    if (isRead)
    {
        isRead = Write();
    }

    return isRead;
}


void CPluginSettings::Clear()
{
	// Default settings
	s_criticalSectionLocal.Lock();
	{
		m_properties.clear();

		m_properties[SETTING_PLUGIN_VERSION] = IEPLUGIN_VERSION;
		m_properties[SETTING_LANGUAGE] = "en";
		m_properties[SETTING_DICTIONARY_VERSION] = "1";
	}
	s_criticalSectionLocal.Unlock();

	// Default filters
#ifdef SUPPORT_FILTER

	s_criticalSectionFilters.Lock();
	{
	    m_filterUrlList.clear();
		m_filterUrlList[CString(FILTERS_PROTOCOL) + CString(FILTERS_HOST) + "/easylist.txt"] = 1;

		m_filterFileNameList.clear();
		m_filterFileNameList[CString(FILTERS_PROTOCOL) + CString(FILTERS_HOST) + "/easylist.txt"] = "filter1.txt";
	}
	s_criticalSectionFilters.Unlock();

#endif // SUPPORT_FILTER
}


CString CPluginSettings::GetDataPathParent()
{
	if (s_dataPathParent == NULL) 
	{
		WCHAR* lpData = new WCHAR[MAX_PATH];

		OSVERSIONINFO osVersionInfo;
		::ZeroMemory(&osVersionInfo, sizeof(OSVERSIONINFO));

		osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

		::GetVersionEx(&osVersionInfo);

		//Windows Vista				- 6.0 
		//Windows Server 2003 R2	- 5.2 
		//Windows Server 2003		- 5.2 
		//Windows XP				- 5.1 
		if (osVersionInfo.dwMajorVersion >= 6)
		{
			if (::SHGetSpecialFolderPath(NULL, lpData, CSIDL_LOCAL_APPDATA, TRUE))
			{
				wcscat(lpData, L"Low");
			}
			else
			{
				DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_GET_SPECIAL_FOLDER_LOCAL, "Settings::GetDataPath failed");
			}
		}
		else
		{
			if (!SHGetSpecialFolderPath(NULL, lpData, CSIDL_APPDATA, TRUE))
			{
				DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_GET_SPECIAL_FOLDER, "Settings::GetDataPath failed");
			}
		}

	    ::PathAddBackslash(lpData);

	    s_dataPathParent = lpData;

    	if (!::CreateDirectory(s_dataPathParent, NULL))
		{
			DWORD errorCode = ::GetLastError();
			if (errorCode != ERROR_ALREADY_EXISTS)
			{
				DEBUG_ERROR_LOG(errorCode, PLUGIN_ERROR_SETTINGS, PLUGIN_ERROR_SETTINGS_CREATE_FOLDER, "Settings::CreateDirectory failed");
			}
		}
	}

    return s_dataPathParent;
}

CString CPluginSettings::GetDataPath(const CString& filename)
{
	if (s_dataPath == NULL) 
	{
		WCHAR* lpData = new WCHAR[MAX_PATH];

		OSVERSIONINFO osVersionInfo;
		::ZeroMemory(&osVersionInfo, sizeof(OSVERSIONINFO));

		osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

		::GetVersionEx(&osVersionInfo);

		//Windows Vista				- 6.0 
		//Windows Server 2003 R2	- 5.2 
		//Windows Server 2003		- 5.2 
		//Windows XP				- 5.1 
		if (osVersionInfo.dwMajorVersion >= 6)
		{
			if (::SHGetSpecialFolderPath(NULL, lpData, CSIDL_LOCAL_APPDATA, TRUE))
			{
				wcscat(lpData, L"Low");
			}
			else
			{
				DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_GET_SPECIAL_FOLDER_LOCAL, "Settings::GetDataPath failed");
			}
		}
		else
		{
			if (!SHGetSpecialFolderPath(NULL, lpData, CSIDL_APPDATA, TRUE))
			{
				DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_GET_SPECIAL_FOLDER, "Settings::GetDataPath failed");
			}
		}

	    ::PathAddBackslash(lpData);

	    s_dataPath = lpData;

    	if (!::CreateDirectory(s_dataPath + CString(USER_DIR), NULL))
		{
			DWORD errorCode = ::GetLastError();
			if (errorCode != ERROR_ALREADY_EXISTS)
			{
				DEBUG_ERROR_LOG(errorCode, PLUGIN_ERROR_SETTINGS, PLUGIN_ERROR_SETTINGS_CREATE_FOLDER, "Settings::CreateDirectory failed");
			}
		}
	}

    return s_dataPath + CString(USER_DIR) + filename;
}


CString CPluginSettings::GetTempPath(const CString& filename)
{
	CString tempPath;

	LPWSTR pwszCacheDir = NULL;
 
	HRESULT hr = ::IEGetWriteableFolderPath(FOLDERID_InternetCache, &pwszCacheDir); 
	if (SUCCEEDED(hr))
    {
		tempPath = pwszCacheDir;
    }
	// Not implemented in IE6
	else if (hr == E_NOTIMPL)
	{
        TCHAR path[MAX_PATH] = _T("");

		if (::SHGetSpecialFolderPath(NULL, path, CSIDL_INTERNET_CACHE, TRUE))
        {
			tempPath = path;
        }
		else
		{
			DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_GET_SPECIAL_FOLDER_TEMP, "Settings::GetTempPath failed");
		}
	}
	// Other error
	else
	{
		DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_TEMP_PATH, "Settings::GetTempPath failed");
	}

	::CoTaskMemFree(pwszCacheDir);

	return tempPath + "\\" + filename;
}

CString CPluginSettings::GetTempFile(const CString& prefix, const CString& extension)
{
    TCHAR nameBuffer[MAX_PATH] = _T("");

	CString tempPath;
 
	DWORD dwRetVal = ::GetTempFileName(GetTempPath(), prefix, 0, nameBuffer);
    if (dwRetVal == 0)
    {
	    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_TEMP_FILE, "Settings::GetTempFileName failed");

        tempPath = GetDataPath();
    }
    else
    {
        tempPath = nameBuffer;
		if (!extension.IsEmpty())
		{
			int pos = tempPath.ReverseFind(_T('.'));
			if (pos >= 0)
			{
				tempPath = tempPath.Left(pos+1) + extension;
			}
		}
    }

    return tempPath;
}


bool CPluginSettings::Has(const CString& key) const
{
	bool hasKey;

    s_criticalSectionLocal.Lock();
	{
		hasKey = m_properties.find(key) != m_properties.end();
	}
    s_criticalSectionLocal.Unlock();
    
    return hasKey;
}


void CPluginSettings::Remove(const CString& key)
{
    s_criticalSectionLocal.Lock();
	{    
		TProperties::iterator it = m_properties.find(key);
		if (it != m_properties.end())
		{
			m_properties.erase(it);
			m_isDirty = true;
		}
	}
    s_criticalSectionLocal.Unlock();
}


CString CPluginSettings::GetString(const CString& key, const CString& defaultValue) const
{
	CString val = defaultValue;

    s_criticalSectionLocal.Lock();
	{

		if (key == SETTING_PLUGIN_ID)
		{
#ifdef CONFIG_IN_REGISTRY
			DWORD dwResult = NULL; 
			HKEY hKey;
			RegOpenKey(HKEY_CURRENT_USER, L"SOFTWARE\\SimpleAdblock", &hKey);
			DWORD type = 0;
			WCHAR pid[250];
			DWORD cbData;
			dwResult = ::RegQueryValueEx(hKey, L"PluginId", NULL, &type, (BYTE*)pid, &cbData);
			if (dwResult == ERROR_SUCCESS)
			{
				CString pluginId = pid;
				::RegCloseKey(hKey);
				s_criticalSectionLocal.Unlock();
				return pluginId;
			}
#endif
		}

		if (key == SETTING_USER_ID)
		{
#ifdef CONFIG_IN_REGISTRY
			DWORD dwResult = NULL; 
			HKEY hKey;
			RegOpenKey(HKEY_CURRENT_USER, L"SOFTWARE\\SimpleAdblock", &hKey);
			DWORD type = 0;
			WCHAR pid[250];
			DWORD cbData;
			dwResult = ::RegQueryValueEx(hKey, L"UserId", NULL, &type, (BYTE*)pid, &cbData);
			if (dwResult == ERROR_SUCCESS)
			{
				CString userId = pid;
				::RegCloseKey(hKey);
				s_criticalSectionLocal.Unlock();
				return userId;
			}
#endif
		}

		TProperties::const_iterator it = m_properties.find(key);
		if (it != m_properties.end())
		{
			val = it->second;
		}
	}
    s_criticalSectionLocal.Unlock();

    DEBUG_SETTINGS("Settings::GetString key:" + key + " value:" + val)

	return val;
}


void CPluginSettings::SetString(const CString& key, const CString& value)
{
    if (value.IsEmpty()) return;

    DEBUG_SETTINGS("Settings::SetString key:" + key + " value:" + value)

    s_criticalSectionLocal.Lock();
	{
		TProperties::iterator it = m_properties.find(key);
		if (it != m_properties.end() && it->second != value)
		{
			it->second = value;
			m_isDirty = true;
		}
		else if (it == m_properties.end())
		{
			m_properties[key] = value; 
			m_isDirty = true;
		}
	}
    s_criticalSectionLocal.Unlock();
}


int CPluginSettings::GetValue(const CString& key, int defaultValue) const
{
	int val = defaultValue;

    CString sValue;
    sValue.Format(L"%d", defaultValue);

    s_criticalSectionLocal.Lock();
	{
		TProperties::const_iterator it = m_properties.find(key);
		if (it != m_properties.end())
		{
		    sValue = it->second;
			val = _wtoi(it->second);
		}
	}
    s_criticalSectionLocal.Unlock();

    DEBUG_SETTINGS("Settings::GetValue key:" + key + " value:" + sValue)

	return val;
}


void CPluginSettings::SetValue(const CString& key, int value)
{
    CString sValue;
    sValue.Format(L"%d", value);

    DEBUG_SETTINGS("Settings::SetValue key:" + key + " value:" + sValue)

    s_criticalSectionLocal.Lock();
	{
		TProperties::iterator it = m_properties.find(key);
		if (it != m_properties.end() && it->second != sValue)
		{
			it->second = sValue;
			m_isDirty = true;
		}
		else if (it == m_properties.end())
		{
			m_properties[key] = sValue; 
			m_isDirty = true;
		}
	}
    s_criticalSectionLocal.Unlock();
}


bool CPluginSettings::GetBool(const CString& key, bool defaultValue) const
{
	bool value = defaultValue;

    s_criticalSectionLocal.Lock();
    {
		TProperties::const_iterator it = m_properties.find(key);
		if (it != m_properties.end())
		{
			if (it->second == "true") value = true;
			if (it->second == "false") value = false;
		}
	}
    s_criticalSectionLocal.Unlock();

	DEBUG_SETTINGS("Settings::GetBool key:" + key + " value:" + (value ? "true":"false"))

 	return value;
}


void CPluginSettings::SetBool(const CString& key, bool value)
{
    SetString(key, value ? "true":"false");
}


bool CPluginSettings::IsPluginEnabled() const
{
	return m_isPluginEnabledTab;
}


#ifdef SUPPORT_FILTER

void CPluginSettings::SetFilterUrlList(const TFilterUrlList& filters) 
{
    DEBUG_SETTINGS(L"Settings::SetFilterUrlList")

	s_criticalSectionFilters.Lock();
	{
		if (m_filterUrlList != filters)
		{
    		m_filterUrlList = filters;
    		m_isDirty = true;
		}
	}
	s_criticalSectionFilters.Unlock();
}

void CPluginSettings::SetFilterFileNamesList(const std::map<CString, CString>& filters) 
{
    DEBUG_SETTINGS(L"Settings::SetFilterUrlList")

	s_criticalSectionFilters.Lock();
	{
		if (m_filterFileNameList != filters)
		{
    		m_filterFileNameList = filters;
    		m_isDirty = true;
		}
	}
	s_criticalSectionFilters.Unlock();
}

TFilterUrlList CPluginSettings::GetFilterUrlList() const
{
	TFilterUrlList filterUrlList;

	s_criticalSectionFilters.Lock();
	{
		filterUrlList = m_filterUrlList;
	}
	s_criticalSectionFilters.Unlock();

	return filterUrlList;
}

std::map<CString, CString> CPluginSettings::GetFilterFileNamesList() const
{
	std::map<CString, CString> filterFileNamesList;

	s_criticalSectionFilters.Lock();
	{
		filterFileNamesList = m_filterFileNameList;
	}
	s_criticalSectionFilters.Unlock();

	return filterFileNamesList;
}
void CPluginSettings::AddFilterUrl(const CString& url, int version) 
{
	s_criticalSectionFilters.Lock();
	{
		TFilterUrlList::iterator it = m_filterUrlList.find(url);
		if (it == m_filterUrlList.end() || it->second != version)
		{
            m_filterUrlList[url] = version;
		    m_isDirty = true;
	    }
    }
	s_criticalSectionFilters.Unlock();
}

void CPluginSettings::AddFilterFileName(const CString& url, const CString& fileName) 
{
	s_criticalSectionFilters.Lock();
	{
		std::map<CString, CString>::iterator it = m_filterFileNameList.find(url);
		if (it == m_filterFileNameList.end() || it->second != fileName)
		{
            m_filterFileNameList[url] = fileName;
		    m_isDirty = true;
	    }
    }
	s_criticalSectionFilters.Unlock();
}
#endif // SUPPORT_FILTER

bool CPluginSettings::Write(bool isDebug)
{
	bool isWritten = true;

    if (!m_isDirty)
    {
        return isWritten;
    }

    if (isDebug)
    {
		DEBUG_GENERAL(L"*** Writing changed settings")
	}

    CPluginSettingsLock lock;
    if (lock.IsLocked())
    {
        m_settingsFile->Clear();

        // Properties
        CPluginIniFileW::TSectionData settings;        

        s_criticalSectionLocal.Lock();
        {
		    for (TProperties::iterator it = m_properties.begin(); it != m_properties.end(); ++it)
		    {
			    settings[it->first] = it->second;
		    }
	    }
        s_criticalSectionLocal.Unlock();

        m_settingsFile->UpdateSection("Settings", settings);

        // Filter URL's
#ifdef SUPPORT_FILTER

        int filterCount = 0;
        CPluginIniFileW::TSectionData filters;        

        s_criticalSectionFilters.Lock();
	    {
		    for (TFilterUrlList::iterator it = m_filterUrlList.begin(); it != m_filterUrlList.end(); ++it)
		    {
			    CString filterCountStr;
			    filterCountStr.Format(L"%d", ++filterCount);

			    CString filterVersion;
			    filterVersion.Format(L"%d", it->second);

			    filters[L"filter" + filterCountStr] = it->first;
			    filters[L"filter" + filterCountStr + L"v"] = filterVersion;
				if (m_filterFileNameList.size() > 0)
				{
					std::map<CString, CString>::iterator fni = m_filterFileNameList.find(it->first);
					if (fni != m_filterFileNameList.end())
					{
						CString fileName = fni->second;
						filters[L"filter" + filterCountStr + "fileName"] = fileName;
					}
				}
		    }
	    }
        s_criticalSectionFilters.Unlock();

        m_settingsFile->UpdateSection("Filters", filters);

#endif // SUPPORT_FILTER

        // Write file
        isWritten = m_settingsFile->Write();
        if (!isWritten)
        {
            DEBUG_ERROR_LOG(m_settingsFile->GetLastError(), PLUGIN_ERROR_SETTINGS, PLUGIN_ERROR_SETTINGS_FILE_WRITE, "Settings::Write")
        }
        
        m_isDirty = false;

        IncrementTabVersion(SETTING_TAB_SETTINGS_VERSION);
    }
    else
    {
        isWritten = false;
    }

    return isWritten;
}

#ifdef SUPPORT_WHITELIST

void CPluginSettings::AddDomainToHistory(const CString& domain)
{
	if (!CPluginClient::IsValidDomain(domain))
    {
	    return;
    }

    // Delete domain
	s_criticalSectionDomainHistory.Lock();
	{
		for (TDomainHistory::iterator it = m_domainHistory.begin(); it != m_domainHistory.end(); ++it)
		{
			if (it->first == domain)
			{
				m_domainHistory.erase(it);
				break;
			}
		}

		// Get whitelist reason
		int reason = 0;

		s_criticalSectionLocal.Lock();
		{
			TDomainList::iterator it = m_whitelist.find(domain);
			if (it != m_whitelist.end())
			{
				reason = it->second;
			}
			else
			{
				reason = 3;
			}
		}
		s_criticalSectionLocal.Unlock();

		// Delete domain, if history is too long
		if (m_domainHistory.size() >= DOMAIN_HISTORY_MAX_COUNT)
		{
			m_domainHistory.erase(m_domainHistory.begin());
		}

		m_domainHistory.push_back(std::make_pair(domain, reason));
	}
	s_criticalSectionDomainHistory.Unlock();
}


TDomainHistory CPluginSettings::GetDomainHistory() const
{
	TDomainHistory domainHistory;

	s_criticalSectionDomainHistory.Lock();
	{
		domainHistory = m_domainHistory;
	}
	s_criticalSectionDomainHistory.Unlock();

    return domainHistory;
}

#endif // SUPPORT_WHITELIST


bool CPluginSettings::IsPluginUpdateAvailable() const
{
	bool isAvailable = Has(SETTING_PLUGIN_UPDATE_VERSION);
	if (isAvailable)
	{
		CString newVersion = GetString(SETTING_PLUGIN_UPDATE_VERSION);
	    CString curVersion = IEPLUGIN_VERSION;

		isAvailable = newVersion != curVersion;
		if (isAvailable)
		{
			int curPos = 0;
			int curMajor = _wtoi(curVersion.Tokenize(L".", curPos));
			int curMinor = _wtoi(curVersion.Tokenize(L".", curPos));
			int curDev   = _wtoi(curVersion.Tokenize(L".", curPos));

			int newPos = 0;
			int newMajor = _wtoi(newVersion.Tokenize(L".", newPos));
			int newMinor = newPos > 0 ? _wtoi(newVersion.Tokenize(L".", newPos)) : 0;
			int newDev   = newPos > 0 ? _wtoi(newVersion.Tokenize(L".", newPos)) : 0;

			isAvailable = newMajor > curMajor || newMajor == curMajor && newMinor > curMinor || newMajor == curMajor && newMinor == curMinor && newDev > curDev;
		}
	}

	return isAvailable;
}

bool CPluginSettings::IsMainProcess(DWORD dwProcessId) const
{
    if (dwProcessId == 0)
    {
        dwProcessId = ::GetCurrentProcessId();
    }
    return m_dwMainProcessId == dwProcessId;
}

void CPluginSettings::SetMainProcessId()
{
    m_dwMainProcessId = ::GetCurrentProcessId();
}

void CPluginSettings::SetMainProcessId(DWORD id)
{
	m_dwMainProcessId = id;
}


bool CPluginSettings::IsMainUiThread(DWORD dwThreadId) const
{
    if (dwThreadId == 0)
    {
        dwThreadId = ::GetCurrentThreadId();
    }
    return m_dwMainUiThreadId == dwThreadId;
}

void CPluginSettings::SetMainUiThreadId()
{
    m_dwMainUiThreadId = ::GetCurrentThreadId();
}

void CPluginSettings::SetMainUiThreadId(DWORD id)
{
	m_dwMainUiThreadId = id;
}
bool CPluginSettings::IsMainThread(DWORD dwThreadId) const
{
    if (dwThreadId == 0)
    {
        dwThreadId = ::GetCurrentThreadId();
    }
    return m_dwMainThreadId == dwThreadId;
}

void CPluginSettings::SetMainThreadId()
{
    m_dwMainThreadId = ::GetCurrentThreadId();
}

void CPluginSettings::SetMainThreadId(DWORD id)
{
	m_dwMainThreadId = id;
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

void CPluginSettings::SetFirstRun()
{
    m_isFirstRun = true;
}

bool CPluginSettings::IsFirstRun() const
{
    return m_isFirstRun;
}

void CPluginSettings::SetFirstRunUpdate()
{
    m_isFirstRunUpdate = true;
}

bool CPluginSettings::IsFirstRunUpdate() const
{
    return m_isFirstRunUpdate;
}

bool CPluginSettings::IsFirstRunAny() const
{
    return m_isFirstRun || m_isFirstRunUpdate;
}

// ============================================================================
// Tab settings
// ============================================================================

void CPluginSettings::ClearTab()
{
    s_criticalSectionLocal.Lock();
	{
	    m_isPluginEnabledTab = true;

	    m_errorsTab.clear();

	    m_propertiesTab.clear();

	    m_propertiesTab[SETTING_TAB_PLUGIN_ENABLED] = "true";
    }
    s_criticalSectionLocal.Unlock();
}


bool CPluginSettings::ReadTab(bool bDebug)
{
    bool isRead = true;

    DEBUG_SETTINGS(L"SettingsTab::Read tab")

    if (bDebug)
    {
        DEBUG_GENERAL(L"*** Loading tab settings:" + m_settingsFileTab->GetFilePath());
    }

    isRead = m_settingsFileTab->Read();        
    if (isRead)
    {
        ClearTab();

        if (m_settingsFileTab->IsValidChecksum())
        {
            s_criticalSectionLocal.Lock();
            {
                m_propertiesTab = m_settingsFileTab->GetSectionData("Settings");

                m_errorsTab = m_settingsFileTab->GetSectionData("Errors");

                TProperties::iterator it = m_propertiesTab.find(SETTING_TAB_PLUGIN_ENABLED);
                if (it != m_propertiesTab.end())
                {
                    m_isPluginEnabledTab = it->second != "false";
                }
            }
            s_criticalSectionLocal.Unlock();
        }
        else
        {
            DEBUG_SETTINGS("SettingsTab:Invalid checksum - Deleting file")

            DEBUG_ERROR_LOG(m_settingsFileTab->GetLastError(), PLUGIN_ERROR_SETTINGS_TAB, PLUGIN_ERROR_SETTINGS_FILE_READ_CHECKSUM, "SettingsTab::Read - Checksum")
            isRead = false;
            m_isDirtyTab = true;
        }
    }
    else if (m_settingsFileTab->GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        m_isDirtyTab = true;
    }
    else
    {
        DEBUG_ERROR_LOG(m_settingsFileTab->GetLastError(), PLUGIN_ERROR_SETTINGS_TAB, PLUGIN_ERROR_SETTINGS_FILE_READ, "SettingsTab::Read")
    }


	// Write file in case it is dirty or does not exist
    WriteTab();

    return isRead;
}

bool CPluginSettings::WriteTab(bool isDebug)
{
	bool isWritten = true;

    if (!m_isDirtyTab)
    {
        return isWritten;
    }

    if (isDebug)
    {
		DEBUG_GENERAL(L"*** Writing changed tab settings")
	}

    m_settingsFileTab->Clear();

    // Properties & errors
    CPluginIniFileW::TSectionData settings;        
    CPluginIniFileW::TSectionData errors;        

    s_criticalSectionLocal.Lock();
    {
        for (TProperties::iterator it = m_propertiesTab.begin(); it != m_propertiesTab.end(); ++it)
        {
	        settings[it->first] = it->second;
        }

        for (TProperties::iterator it = m_errorsTab.begin(); it != m_errorsTab.end(); ++it)
        {
	        errors[it->first] = it->second;
        }
    }
    s_criticalSectionLocal.Unlock();

    m_settingsFileTab->UpdateSection("Settings", settings);
    m_settingsFileTab->UpdateSection("Errors", errors);

    // Write file
    isWritten = m_settingsFileTab->Write();
    if (!isWritten)
    {
        DEBUG_ERROR_LOG(m_settingsFileTab->GetLastError(), PLUGIN_ERROR_SETTINGS_TAB, PLUGIN_ERROR_SETTINGS_FILE_WRITE, "SettingsTab::Write")
    }
    
    m_isDirtyTab = !isWritten;

    return isWritten;
}


void CPluginSettings::EraseTab()
{
    ClearTab();
    
    m_isDirtyTab = true;

    WriteTab();
}


bool CPluginSettings::IncrementTabCount()
{
    int tabCount = 1;


	if (s_isLightOnly)
	{
		return false;
	}

    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        SYSTEMTIME systemTime;
        ::GetSystemTime(&systemTime);

        CString today;
        today.Format(L"%d-%d-%d", systemTime.wYear, systemTime.wMonth, systemTime.wDay);

        ReadTab(false);
        
        s_criticalSectionLocal.Lock();
        {
            TProperties::iterator it = m_propertiesTab.find(SETTING_TAB_COUNT);
            if (it != m_propertiesTab.end())
            {        
                tabCount = _wtoi(it->second) + 1;
            }

            it = m_propertiesTab.find(SETTING_TAB_START_TIME);

			//Is this a first IE instance?
			HWND ieWnd = FindWindow(L"IEFrame", NULL);
			if (ieWnd != NULL)
			{
				ieWnd = FindWindowEx(NULL, ieWnd, L"IEFrame", NULL);

			}
            if ((it != m_propertiesTab.end() && it->second != today))
            {
                tabCount = 1;        
            }
            m_tabNumber.Format(L"%d", tabCount);
        
            m_propertiesTab[SETTING_TAB_COUNT] = m_tabNumber;
            m_propertiesTab[SETTING_TAB_START_TIME] = today;
            
            // Main tab?
            if (tabCount == 1)
            {
                m_propertiesTab[SETTING_TAB_DICTIONARY_VERSION] = "1";
                m_propertiesTab[SETTING_TAB_SETTINGS_VERSION] = "1";
#ifdef SUPPORT_WHITELIST
                m_propertiesTab[SETTING_TAB_WHITELIST_VERSION] = "1";
#endif
#ifdef SUPPORT_FILTER
                m_propertiesTab[SETTING_TAB_FILTER_VERSION] = "1";
#endif
#ifdef SUPPORT_CONFIG
                m_propertiesTab[SETTING_TAB_CONFIG_VERSION] = "1";
#endif
            }
        }
        s_criticalSectionLocal.Unlock();

        m_isDirtyTab = true;

        WriteTab(false);        
    }

    return tabCount == 1;
}


CString CPluginSettings::GetTabNumber() const
{
    CString tabNumber;
    
    s_criticalSectionLocal.Lock();
    {
        tabNumber = m_tabNumber;
    }
    s_criticalSectionLocal.Unlock();
    
    return tabNumber;
}


bool CPluginSettings::DecrementTabCount()
{
    int tabCount = 0;

    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        ReadTab(false);
        
        s_criticalSectionLocal.Lock();
        {
            TProperties::iterator it = m_propertiesTab.find(SETTING_TAB_COUNT);
            if (it != m_propertiesTab.end())
            {
                tabCount = max(_wtoi(it->second) - 1, 0);

                if (tabCount > 0)
                {
                    m_tabNumber.Format(L"%d", tabCount);
                
                    m_propertiesTab[SETTING_TAB_COUNT] = m_tabNumber;
                }
                else
                {
                    it = m_propertiesTab.find(SETTING_TAB_START_TIME);
                    if (it != m_propertiesTab.end())
                    {
                        m_propertiesTab.erase(it);
                    }

                    it = m_propertiesTab.find(SETTING_TAB_COUNT);
                    if (it != m_propertiesTab.end())
                    {
                        m_propertiesTab.erase(it);
                    }
                }

                m_isDirtyTab = true;               
            }
        }
        s_criticalSectionLocal.Unlock();

        WriteTab(false);
    }

    return tabCount == 0;
}


void CPluginSettings::TogglePluginEnabled()
{
    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        ReadTab(false);

        s_criticalSectionLocal.Lock();
        {
            m_isPluginEnabledTab = m_isPluginEnabledTab ? false : true;
            m_propertiesTab[SETTING_TAB_PLUGIN_ENABLED] = m_isPluginEnabledTab ? "true" : "false";
            m_isDirtyTab = true;
        }
        s_criticalSectionLocal.Unlock();
        
        WriteTab(false);
    }
}
void CPluginSettings::SetPluginDisabled()
{
    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        ReadTab(false);

        s_criticalSectionLocal.Lock();
        {
            m_isPluginEnabledTab = false;
            m_propertiesTab[SETTING_TAB_PLUGIN_ENABLED] = "false";
            m_isDirtyTab = true;
        }
        s_criticalSectionLocal.Unlock();
        
        WriteTab(false);
    }
}
void CPluginSettings::SetPluginEnabled()
{
    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        ReadTab(false);

        s_criticalSectionLocal.Lock();
        {
            m_isPluginEnabledTab = true;
            m_propertiesTab[SETTING_TAB_PLUGIN_ENABLED] = "true";
            m_isDirtyTab = true;
        }
        s_criticalSectionLocal.Unlock();
        
        WriteTab(false);
    }
}
bool CPluginSettings::GetPluginEnabled() const
{
	return m_isPluginEnabledTab;
}


void CPluginSettings::AddError(const CString& error, const CString& errorCode)
{
    DEBUG_SETTINGS(L"SettingsTab::AddError error:" + error + " code:" + errorCode)

    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        ReadTab(false);

        s_criticalSectionLocal.Lock();
        {
		    if (m_errorsTab.find(error) == m_errorsTab.end())
		    {
			    m_errorsTab[error] = errorCode; 
			    m_isDirtyTab = true;
		    }
		}
        s_criticalSectionLocal.Unlock();

		WriteTab(false);
	}
}


CString CPluginSettings::GetErrorList() const
{
    CString errors;

    s_criticalSectionLocal.Lock();
    {
        for (TProperties::const_iterator it = m_errorsTab.begin(); it != m_errorsTab.end(); ++it)
        {
            if (!errors.IsEmpty())
            {
                errors += ',';
            }

            errors += it->first + '.' + it->second;
        }
	}
    s_criticalSectionLocal.Unlock();

    return errors;
}


void CPluginSettings::RemoveErrors()
{
    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        ReadTab(false);

        s_criticalSectionLocal.Lock();
        {
	        if (m_errorsTab.size() > 0)
	        {
	            m_isDirtyTab = true;
	        }
            m_errorsTab.clear();
        }
        s_criticalSectionLocal.Unlock();

        WriteTab(false);
	}
}


bool CPluginSettings::GetForceConfigurationUpdateOnStart() const
{
    bool isUpdating = false;

    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        s_criticalSectionLocal.Lock();
        {
            isUpdating = m_propertiesTab.find(SETTING_TAB_UPDATE_ON_START) != m_propertiesTab.end();
        }
        s_criticalSectionLocal.Unlock();
    }

    return isUpdating;
}


void CPluginSettings::ForceConfigurationUpdateOnStart(bool isUpdating)
{
    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        ReadTab(false);

        s_criticalSectionLocal.Lock();
        {
            TProperties::iterator it = m_propertiesTab.find(SETTING_TAB_UPDATE_ON_START);
            
            if (isUpdating && it == m_propertiesTab.end())
            {
                m_propertiesTab[SETTING_TAB_UPDATE_ON_START] = "true";
                m_propertiesTab[SETTING_TAB_UPDATE_ON_START_REMOVE] = "false";
                
                m_isDirtyTab = true;
            }
            else if (!isUpdating)
            {
                // OK to remove?
                TProperties::iterator itRemove = m_propertiesTab.find(SETTING_TAB_UPDATE_ON_START_REMOVE);

                if (itRemove == m_propertiesTab.end() || itRemove->second == "true")
                {
                    if (it != m_propertiesTab.end())
                    {
                        m_propertiesTab.erase(it);
                    }

                    if (itRemove != m_propertiesTab.end())
                    {
                        m_propertiesTab.erase(itRemove);
                    }

                    m_isDirtyTab = true;
                }
            }
        }
        s_criticalSectionLocal.Unlock();

        WriteTab(false);
    }
}

void CPluginSettings::RemoveForceConfigurationUpdateOnStart()
{
    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        ReadTab(false);

        s_criticalSectionLocal.Lock();
        {
            // OK to remove?
            TProperties::iterator itRemove = m_propertiesTab.find(SETTING_TAB_UPDATE_ON_START_REMOVE);

            if (itRemove != m_propertiesTab.end())
            {
                m_propertiesTab.erase(itRemove);
                m_isDirtyTab = true;
            }
        }
        s_criticalSectionLocal.Unlock();

        WriteTab(false);
    }
}

void CPluginSettings::RefreshTab()
{
    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        ReadTab();
    }
}


int CPluginSettings::GetTabVersion(const CString& key) const
{
    int version = 0;

    s_criticalSectionLocal.Lock();
    {
        TProperties::const_iterator it = m_propertiesTab.find(key);
        if (it != m_propertiesTab.end())
        {
            version = _wtoi(it->second);
        }
    }
    s_criticalSectionLocal.Unlock();

    return version;
}

void CPluginSettings::IncrementTabVersion(const CString& key)
{
    CPluginSettingsTabLock lock;
    if (lock.IsLocked())
    {
        ReadTab(false);

        s_criticalSectionLocal.Lock();
        {
            int version = 1;

            TProperties::iterator it = m_propertiesTab.find(key);
            if (it != m_propertiesTab.end())
            {
                version = _wtoi(it->second) + 1;
            }

            CString versionString;
            versionString.Format(L"%d", version);
        
            m_propertiesTab[key] = versionString;
        }
        s_criticalSectionLocal.Unlock();

        m_isDirtyTab = true;

        WriteTab(false);        
    }
}


// ============================================================================
// Whitelist settings
// ============================================================================

#ifdef SUPPORT_WHITELIST

void CPluginSettings::ClearWhitelist()
{
    s_criticalSectionLocal.Lock();
	{
	    m_whitelist.clear();
	    m_whitelistToGo.clear();
    }
    s_criticalSectionLocal.Unlock();
}


bool CPluginSettings::ReadWhitelist(bool isDebug)
{
    bool isRead = true;

    DEBUG_SETTINGS("SettingsWhitelist::Read")

    if (isDebug)
    {
        DEBUG_GENERAL("*** Loading whitelist settings:" + m_settingsFileWhitelist->GetFilePath());
    }

    CPluginSettingsWhitelistLock lock;
    if (lock.IsLocked())
    {
        isRead = m_settingsFileWhitelist->Read();        
        if (isRead)
        {
            if (m_settingsFileWhitelist->IsValidChecksum())
            {
                ClearWhitelist();

                s_criticalSectionLocal.Lock();
	            {
                    // Unpack white list
                    CPluginIniFileW::TSectionData whitelist = m_settingsFileWhitelist->GetSectionData("Whitelist");
                    int domainCount = 0;
                    bool bContinue = true;

		            do
		            {
			            CString domainCountStr;
			            domainCountStr.Format(L"%d", ++domainCount);
        	            
			            CPluginIniFileW::TSectionData::iterator domainIt = whitelist.find(L"domain" + domainCountStr);
			            CPluginIniFileW::TSectionData::iterator reasonIt = whitelist.find(L"domain" + domainCountStr + L"r");

			            if (bContinue = (domainIt != whitelist.end() && reasonIt != whitelist.end()))
			            {
				            m_whitelist[domainIt->second] = _wtoi(reasonIt->second);
			            }

		            } while (bContinue);

                    // Unpack white list
                    whitelist = m_settingsFileWhitelist->GetSectionData("Whitelist togo");
                    domainCount = 0;
                    bContinue = true;

		            do
		            {
			            CString domainCountStr;
			            domainCountStr.Format(L"%d", ++domainCount);
        	            
			            CPluginIniFileW::TSectionData::iterator domainIt = whitelist.find(L"domain" + domainCountStr);
			            CPluginIniFileW::TSectionData::iterator reasonIt = whitelist.find(L"domain" + domainCountStr + L"r");

			            if (bContinue = (domainIt != whitelist.end() && reasonIt != whitelist.end()))
			            {
				            m_whitelistToGo[domainIt->second] = _wtoi(reasonIt->second);
			            }

		            } while (bContinue);
	            }
	            s_criticalSectionLocal.Unlock();
            }
            else
            {
                DEBUG_SETTINGS("SettingsWhitelist:Invalid checksum - Deleting file")

                DEBUG_ERROR_LOG(m_settingsFileWhitelist->GetLastError(), PLUGIN_ERROR_SETTINGS_WHITELIST, PLUGIN_ERROR_SETTINGS_FILE_READ_CHECKSUM, "SettingsWhitelist::Read - Checksum")
                isRead = false;
                m_isDirtyWhitelist = true;
            }
        }
        else if (m_settingsFileWhitelist->GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            m_isDirtyWhitelist = true;
        }
        else
        {
            DEBUG_ERROR_LOG(m_settingsFileWhitelist->GetLastError(), PLUGIN_ERROR_SETTINGS_WHITELIST, PLUGIN_ERROR_SETTINGS_FILE_READ, "SettingsWhitelist::Read")
        }
    }
    else
    {
        isRead = false;
    }

	// Write file in case it is dirty
    WriteWhitelist(isDebug);

    return isRead;
}


bool CPluginSettings::WriteWhitelist(bool isDebug)
{
	bool isWritten = true;

    if (!m_isDirtyWhitelist)
    {
        return isWritten;
    }

    if (isDebug)
    {
		DEBUG_GENERAL("*** Writing changed whitelist settings")
	}

    CPluginSettingsWhitelistLock lock;
    if (lock.IsLocked())
    {
        m_settingsFileWhitelist->Clear();

        s_criticalSectionLocal.Lock();
	    {
            // White list
            int whitelistCount = 0;
            CPluginIniFileW::TSectionData whitelist;

		    for (TDomainList::iterator it = m_whitelist.begin(); it != m_whitelist.end(); ++it)
		    {
			    CString whitelistCountStr;
			    whitelistCountStr.Format(L"%d", ++whitelistCount);

			    CString reason;
			    reason.Format(L"%d", it->second);

			    whitelist[L"domain" + whitelistCountStr] = it->first;
			    whitelist[L"domain" + whitelistCountStr + L"r"] = reason;
		    }

            m_settingsFileWhitelist->UpdateSection("Whitelist", whitelist);

            // White list (not yet committed)
            whitelistCount = 0;
            whitelist.clear();

            for (TDomainList::iterator it = m_whitelistToGo.begin(); it != m_whitelistToGo.end(); ++it)
            {
	            CString whitelistCountStr;
	            whitelistCountStr.Format(L"%d", ++whitelistCount);

	            CString reason;
	            reason.Format(L"%d", it->second);

	            whitelist[L"domain" + whitelistCountStr] = it->first;
	            whitelist[L"domain" + whitelistCountStr + L"r"] = reason;
            }

            m_settingsFileWhitelist->UpdateSection("Whitelist togo", whitelist);
	    }
        s_criticalSectionLocal.Unlock();

        // Write file
        isWritten = m_settingsFileWhitelist->Write();
        if (!isWritten)
        {
            DEBUG_ERROR_LOG(m_settingsFileWhitelist->GetLastError(), PLUGIN_ERROR_SETTINGS_WHITELIST, PLUGIN_ERROR_SETTINGS_FILE_WRITE, "SettingsWhitelist::Write")
        }
        
        m_isDirty = false;
    }
    else
    {
        isWritten = false;
    }

    if (isWritten)
    {
        DEBUG_WHITELIST("Whitelist::Icrement version")

        IncrementTabVersion(SETTING_TAB_WHITELIST_VERSION);
    }

    return isWritten;
}


void CPluginSettings::AddWhiteListedDomain(const CString& domain, int reason, bool isToGo)
{
    DEBUG_SETTINGS("SettingsWhitelist::AddWhiteListedDomain domain:" + domain)

    bool isNewVersion = false;
    bool isForcingUpdateOnStart = false;

    CPluginSettingsWhitelistLock lock;
    if (lock.IsLocked())
    {
        ReadWhitelist(false);

        s_criticalSectionLocal.Lock();
        {
            bool isToGoMatcingReason = false;
            bool isToGoMatcingDomain = false;

	        TDomainList::iterator itToGo = m_whitelistToGo.find(domain);
	        TDomainList::iterator it = m_whitelist.find(domain);
	        if (isToGo)
	        {
		        if (itToGo != m_whitelistToGo.end())  
		        {
    		        isToGoMatcingDomain = true;
    		        isToGoMatcingReason = itToGo->second == reason;

                    if (reason == 3)
                    {
                        m_whitelistToGo.erase(itToGo);
				        m_isDirtyWhitelist = true;                        
                    }
                    else if (!isToGoMatcingReason)
			        {
				        itToGo->second = reason;
				        m_isDirtyWhitelist = true;
			        }
		        }
		        else 
		        {
			        m_whitelistToGo[domain] = reason;
			        m_isDirtyWhitelist = true;

                    // Delete new togo item from saved white list
                    if (it != m_whitelist.end())
                    {
                        m_whitelist.erase(it);
                    }
		        }
	        }
	        else
	        {
	            if (isToGoMatcingDomain)
	            {
                    m_whitelistToGo.erase(itToGo);
			        m_isDirtyWhitelist = true;
	            }

		        if (it != m_whitelist.end())  
		        {
			        if (it->second != reason)
			        {
				        it->second = reason;
				        m_isDirtyWhitelist = true;
			        }
		        }
		        else 
		        {
			        m_whitelist[domain] = reason; 
			        m_isDirtyWhitelist = true;
		        }
	        }

            isForcingUpdateOnStart = m_whitelistToGo.size() > 0;
        }
	    s_criticalSectionLocal.Unlock();

	    WriteWhitelist(false);
	}

    if (isForcingUpdateOnStart)
    {
        ForceConfigurationUpdateOnStart();
    }
}


bool CPluginSettings::IsWhiteListedDomain(const CString& domain) const
{
	bool bIsWhiteListed;

	s_criticalSectionLocal.Lock();
	{
		bIsWhiteListed = m_whitelist.find(domain) != m_whitelist.end();
		if (!bIsWhiteListed)
		{
		    TDomainList::const_iterator it = m_whitelistToGo.find(domain);
		    bIsWhiteListed = it != m_whitelistToGo.end() && it->second != 3;
		}
	}
	s_criticalSectionLocal.Unlock();

    return bIsWhiteListed;
}

int CPluginSettings::GetWhiteListedDomainCount() const
{
	int count = 0;

	s_criticalSectionLocal.Lock();
	{
		count = m_whitelist.size();
	}
	s_criticalSectionLocal.Unlock();

    return count;
}


TDomainList CPluginSettings::GetWhiteListedDomainList(bool isToGo) const
{
	TDomainList domainList;

	s_criticalSectionLocal.Lock();
	{
	    if (isToGo)
	    {
	        domainList = m_whitelistToGo;
	    }
	    else
	    {
	        domainList = m_whitelist;
	    }
	}
	s_criticalSectionLocal.Unlock();

    return domainList;
}


void CPluginSettings::ReplaceWhiteListedDomains(const TDomainList& domains)
{
    CPluginSettingsWhitelistLock lock;
    if (lock.IsLocked())
    {
        ReadWhitelist(false);

        s_criticalSectionLocal.Lock();
        {
            if (m_whitelist != domains)
            {
                m_whitelist = domains;
                m_isDirtyWhitelist = true;
            }

            // Delete entries in togo list
            bool isDeleted = true;

            while (isDeleted)
            {
                isDeleted = false;

                for (TDomainList::iterator it = m_whitelistToGo.begin(); it != m_whitelistToGo.end(); ++it)
                {
	                if (m_whitelist.find(it->first) != m_whitelist.end() || it->second == 3)
	                {
    	                m_whitelistToGo.erase(it);

                        // Force another round...
    	                isDeleted = true;
    	                break;
	                }
                }
            }
        }
        s_criticalSectionLocal.Unlock();

        WriteWhitelist(false);
    }
}


void CPluginSettings::RemoveWhiteListedDomainsToGo(const TDomainList& domains)
{
    CPluginSettingsWhitelistLock lock;
    if (lock.IsLocked())
    {
        ReadWhitelist(false);

        s_criticalSectionLocal.Lock();
        {
            for (TDomainList::const_iterator it = domains.begin(); it != domains.end(); ++it)
            {
                for (TDomainList::iterator itToGo = m_whitelistToGo.begin(); itToGo != m_whitelistToGo.end(); ++ itToGo)
                {
                    if (it->first == itToGo->first)
                    {
                        m_whitelistToGo.erase(itToGo);
                        m_isDirtyWhitelist = true;
                        break;
                    }
                }
            }
        }
        s_criticalSectionLocal.Unlock();

        WriteWhitelist(false);
    }
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
	   DWORD dwType;

	   ZeroMemory(&si, sizeof(SYSTEM_INFO));
	   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

	   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	   bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO*) &osvi);

	   m_WindowsBuildNumber = osvi.dwBuildNumber;
	}

   return m_WindowsBuildNumber;
}

#endif // SUPPORT_WHITELIST
