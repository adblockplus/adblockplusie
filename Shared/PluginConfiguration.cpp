#include "PluginStdAfx.h"

#include "PluginConfiguration.h"
#include "PluginClient.h"
#include "PluginIniFile.h"
#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginHttpRequest.h"


CPluginConfiguration::CPluginConfiguration() : m_pluginInfoPanel(0)
{
    Invalidate();
}


void CPluginConfiguration::Invalidate()
{
    m_isValid = false;
    m_isValidUserId = false;
    m_isValidPluginActivated = false;
    m_isValidPluginActivateEnabled = false;
    m_isValidPluginExpired = false;
    m_isValidPluginUpdate = false;
	m_isValidPluginInfoPanel = false;
    m_isValidDictionary = false;
#ifdef SUPPORT_FILTER
    m_isValidFilter = false;
#endif
#ifdef SUPPORT_WHITELIST
    m_isValidWhiteList = false;
#endif
#ifdef SUPPORT_CONFIG
    m_isValidConfig = false;
#endif
}


bool CPluginConfiguration::Download()
{
    CPluginSystem* system = CPluginSystem::GetInstance();

    bool isOk = true;

    m_isValid = false;

    CPluginHttpRequest httpRequest(USERS_SCRIPT_SETTINGS);

    CPluginSettings* settings = CPluginSettings::GetInstance();
    
    settings->RefreshTab();

    DEBUG_GENERAL("*** Downloading settings");

    Invalidate();

    httpRequest.AddPluginId();

    httpRequest.Add("enabled", settings->GetPluginEnabled() ? "true":"false");
    httpRequest.Add("lang", settings->GetString(SETTING_LANGUAGE, "err"));
	httpRequest.Add("ie", system->GetBrowserVersion());
	httpRequest.Add("ielang", system->GetBrowserLanguage());

	httpRequest.AddOsInfo();

    httpRequest.Add("pc", system->GetComputerName(), false);
    httpRequest.Add("username", system->GetUserName(), false);

    CString newPluginId = system->GetPluginId();
    if (newPluginId != settings->GetString(SETTING_PLUGIN_ID))
    {
        httpRequest.Add("newplugin", newPluginId);
    }

    httpRequest.Add("errors", settings->GetErrorList());

#ifdef SUPPORT_WHITELIST

    // White list info
    CString whiteListCount;   
    whiteListCount.Format(L"%d", settings->GetWhiteListedDomainCount());

    httpRequest.Add("wcount", whiteListCount);

    TDomainList whiteListToGo = settings->GetWhiteListedDomainList(true);
    TDomainList whiteListToGoSent;

    if (!whiteListToGo.empty())
    {
        CString whiteList;
        int count = 0;

        for (TDomainList::const_iterator it = whiteListToGo.begin(); it != whiteListToGo.end() && count < 5; count++, ++it)
        {            
            CString whiteListReason;
            whiteListReason.Format(L",%d", it->second);

            if (!whiteList.IsEmpty())
            {
                whiteList += ',';
            }
            whiteList += it->first;
            whiteList += whiteListReason;

            whiteListToGoSent.insert(std::make_pair(it->first, it->second));
        }

        httpRequest.Add("wlist", whiteList);   
    }

#endif // SUPPORT_WHITELIST

    httpRequest.Add("dicv", settings->GetValue(SETTING_DICTIONARY_VERSION, 0));

#ifdef SUPPORT_FILTER
    httpRequest.Add("filterv", settings->GetValue(SETTING_FILTER_VERSION, 0));
#endif
#ifdef SUPPORT_CONFIG
    httpRequest.Add("configv", settings->GetValue(SETTING_CONFIG_VERSION, 0));
#endif

	if (!isOk)
	{
	    return false;
    }

	if (!httpRequest.Send(false))
	{
		DEBUG_ERROR("Configuration::Download - Failed downloading settings");
	    return false;
	}

	if (!httpRequest.IsValidResponse())
	{
	    DEBUG_ERROR("Configuration::Download - Invalid settings response");
	    DEBUG_ERROR("Configuration::Download\n\n" + httpRequest.GetResponseText() + "\n");
	    return false;
	}

	const std::auto_ptr<CPluginIniFile>& iniFile = httpRequest.GetResponseFile();

#ifdef SUPPORT_WHITELIST

    // Update whitelists to go
    if (!whiteListToGoSent.empty())
    {
        settings->RemoveWhiteListedDomainsToGo(whiteListToGoSent);
    }

#endif // SUPPORT_WHITELIST

    // Unpack settings
    CPluginIniFile::TSectionData settingsData = iniFile->GetSectionData("Settings");
    CPluginIniFile::TSectionData::iterator it;

    it = settingsData.find("pluginupdate");
    if (it != settingsData.end())
    {
        m_pluginUpdateUrl = it->second;
        DEBUG_SETTINGS("Settings::Configuration plugin update url:" + it->second);
    }

    it = settingsData.find("pluginupdatev");
    if (it != settingsData.end())
    {
        m_pluginUpdateVersion = it->second;
        DEBUG_SETTINGS("Settings::Configuration plugin update version:" + it->second);
    }
    it = settingsData.find("userid");
    if (it != settingsData.end())
    {
        m_userId = it->second;
        DEBUG_SETTINGS("Settings::Configuration user id:" + it->second);
#ifdef CONFIG_IN_REGISTRY
			DWORD dwResult = NULL; 
			HKEY hKey;
			RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\SimpleAdblock", &hKey);
			DWORD type = 0;
			WCHAR pid[250];
			DWORD cbData;
			dwResult = ::RegSetValueEx(hKey, L"UserId", NULL, REG_SZ, (BYTE*)m_userId.GetString(), m_userId.GetLength() * 2);
#endif
	}

    it = settingsData.find("dictionary");
    if (it != settingsData.end())
    {
        m_dictionaryUrl = it->second;
        DEBUG_SETTINGS("Settings::Configuration dictionary url:" + it->second);
    }

    it = settingsData.find("dictionaryv");
    if (it != settingsData.end())
    {
        m_dictionaryVersion = atoi(it->second);
        DEBUG_SETTINGS("Settings::Configuration dictionary version:" + it->second);
    }


    it = settingsData.find("pluginexpired");
    if (it != settingsData.end())
    {
        m_isPluginExpired = it->second == "true";
        m_isValidPluginExpired = true;
        DEBUG_SETTINGS("Settings::Configuration plugin expired:" + it->second);
    }

    m_isValidPluginUpdate = 
        settingsData.find("pluginupdate") != settingsData.end() && 
        settingsData.find("pluginupdatev") != settingsData.end();

    m_isValidUserId =
        settingsData.find("userid") != settingsData.end();

    m_isValidDictionary =
        settingsData.find("dictionary") != settingsData.end() && 
        settingsData.find("dictionaryv") != settingsData.end(); 

    it = settingsData.find("plugininfopanel");
    if (it != settingsData.end())
    {
        m_isValidPluginInfoPanel = true;
        m_pluginInfoPanel = atoi(it->second);
        DEBUG_SETTINGS("Settings::Configuration plugin info panel:" + it->second);
    }

#ifdef SUPPORT_CONFIG

    it = settingsData.find("configurl");
    if (it != settingsData.end())
    {
        m_isValidConfig = true;
        m_configUrl = it->second;

        DEBUG_SETTINGS("Settings::Configuration file url:" + it->second);
    }

    it = settingsData.find("configversion");
    if (it != settingsData.end())
    {
        m_configVersion = atoi(it->second);

        DEBUG_SETTINGS("Settings::Configuration file version:" + it->second);
    }
    else
    {
        m_isValidConfig = false;
    }

#endif // SUPPORT_CONFIG

#ifdef SUPPORT_FILTER

    // Unpack filter URL's
    m_isValidFilter = iniFile->HasSection("Filters");
    if (m_isValidFilter)
    {
        it = settingsData.find(SETTING_FILTER_VERSION);
        if (it != settingsData.end())
        {
            m_filterVersion = atoi(it->second);
        }

        CPluginIniFile::TSectionData filters = iniFile->GetSectionData("Filters");

        int filterCount = 0;
        bool bContinue = true;

        m_filterUrlList.clear();

        do
        {
            CStringA filterCountStr;
            filterCountStr.Format("%d", ++filterCount);
            
            CPluginIniFile::TSectionData::iterator filterIt = filters.find("filter" + filterCountStr);
            CPluginIniFile::TSectionData::iterator versionIt = filters.find("filter" + filterCountStr + "v");
            CPluginIniFile::TSectionData::iterator fileNameIt = filters.find("filter" + filterCountStr + "filename");

            if (bContinue = (filterIt != filters.end() && versionIt != filters.end()))
            {
                m_filterUrlList[CString(filterIt->second)] = atoi(versionIt->second);
				if (fileNameIt != filters.end())
				{
					m_filterFileNameList[CString(filterIt->second)] = fileNameIt->second;
				}
            }

        } while (bContinue);
    }

#endif // SUPPORT_FILTER

#ifdef SUPPORT_WHITELIST

    // Unpack whitelist domains
    m_isValidWhiteList = iniFile->HasSection("Whitelist");
    if (m_isValidWhiteList)
    {
        CPluginIniFile::TSectionData whitelist = iniFile->GetSectionData("Whitelist");        

        int domainCount = 0;
        bool bContinue = true;

        m_whiteList.clear();

        do
        {
            CStringA domainCountStr;
            domainCountStr.Format("%d", ++domainCount);
            
            CPluginIniFile::TSectionData::iterator domainIt = whitelist.find("domain" + domainCountStr);
            CPluginIniFile::TSectionData::iterator reasonIt = whitelist.find("domain" + domainCountStr + "r");

            if (bContinue = (domainIt != whitelist.end() && reasonIt != whitelist.end()))
            {
                m_whiteList[CString(domainIt->second)] = atoi(reasonIt->second);
            }

        } while (bContinue);
    }

#endif // #ifdef SUPPORT_WHITELIST
    it = settingsData.find("registration");
    if (it != settingsData.end())
    {
        m_isPluginRegistered = it->second == "true";
        DEBUG_SETTINGS("Settings::Configuration registration detected:" + it->second);
    }
 
	m_adBlockLimit = -1;
    it = settingsData.find("adblocklimit");
    if (it != settingsData.end())
    {
        m_adBlockLimit = atoi(it->second);
        DEBUG_SETTINGS("Settings::Configuration adblocklimit detected:" + it->second);
    }

	m_downloadLimit = 10;
	it = settingsData.find("downloadlimit");
    if (it != settingsData.end())
    {
        m_downloadLimit = atoi(it->second);
        DEBUG_SETTINGS("Settings::Configuration downloadlimit detected:" + it->second);
    }
    m_isValid = isOk;

	return isOk;
}


bool CPluginConfiguration::IsValid() const
{
   return m_isValid;
}


bool CPluginConfiguration::IsValidUserId() const
{
   return m_isValidUserId;
}



bool CPluginConfiguration::IsValidPluginExpired() const
{
   return m_isValidPluginExpired;
}


bool CPluginConfiguration::IsValidPluginUpdate() const
{
   return m_isValidPluginUpdate;
}


bool CPluginConfiguration::IsValidDictionary() const
{
   return m_isValidDictionary;
}


bool CPluginConfiguration::IsPluginActivated() const
{
    return m_isPluginActivated;
}

bool CPluginConfiguration::IsPluginRegistered() const 
{
	return m_isPluginRegistered;
}

int CPluginConfiguration::GetAdBlockLimit() const 
{
	return m_adBlockLimit;
}
int CPluginConfiguration::GetDownloadLimit() const 
{
	return m_downloadLimit;
}


bool CPluginConfiguration::IsPluginActivateEnabled() const
{
    return m_isPluginActivateEnabled;
}


bool CPluginConfiguration::IsPluginExpired() const
{
    return m_isPluginExpired;
}


bool CPluginConfiguration::IsValidPluginInfoPanel() const
{
   return m_isValidPluginInfoPanel;
}

#ifdef SUPPORT_WHITELIST

bool CPluginConfiguration::IsValidWhiteList() const
{
   return m_isValidWhiteList;
}

#endif // SUPPORT_WHITELIST

#ifdef SUPPORT_FILTER

bool CPluginConfiguration::IsValidFilter() const
{
   return m_isValidFilter;
}

#endif // SUPPORT_FILTER

#ifdef SUPPORT_CONFIG

bool CPluginConfiguration::IsValidConfig() const
{
    return m_isValidConfig;
}

#endif // SUPPORT_CONFIG

CString CPluginConfiguration::GetUserId() const
{
	return m_userId;
}


int CPluginConfiguration::GetPluginInfoPanel() const
{
	return m_pluginInfoPanel;
}


CString CPluginConfiguration::GetPluginUpdateUrl() const
{
	return m_pluginUpdateUrl;
}


CString CPluginConfiguration::GetPluginUpdateVersion() const
{
	return m_pluginUpdateVersion;
}


int CPluginConfiguration::GetDictionaryVersion() const
{
	return m_dictionaryVersion;
}


CString CPluginConfiguration::GetDictionaryUrl() const
{
	return m_dictionaryUrl;
}


#ifdef SUPPORT_FILTER

int CPluginConfiguration::GetFilterVersion() const
{
	return m_filterVersion;
}


TFilterUrlList CPluginConfiguration::GetFilterUrlList() const
{
	return m_filterUrlList;
}

std::map<CString, CString> CPluginConfiguration::GetFilterFileNamesList() const
{
	return m_filterFileNameList;
}
#endif // SUPPORT_FILTER

#ifdef SUPPORT_WHITELIST

TDomainList CPluginConfiguration::GetWhiteList() const
{
	return m_whiteList;
}

#endif // SUPPORT_WHITELIST

#ifdef SUPPORT_CONFIG

CString CPluginConfiguration::GetConfigUrl() const
{
	return m_configUrl;
}

int CPluginConfiguration::GetConfigVersion() const
{
	return m_configVersion;
}

#endif // SUPPORT_CONFIG


#ifdef PRODUCT_AIDOINLINE
CString CPluginConfiguration::GetCollectedStatus() const
{
	return m_collectedStatus;
}
#endif
