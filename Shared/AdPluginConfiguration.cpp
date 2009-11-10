#include "AdPluginStdAfx.h"

#include "AdPluginConfiguration.h"
#include "AdPluginClient.h"
#include "AdPluginIniFile.h"
#include "AdPluginSettings.h"
#include "AdPluginHttpRequest.h"


CAdPluginConfiguration::CAdPluginConfiguration() : m_pluginInfoPanel(0)
{
    Invalidate();
}


void CAdPluginConfiguration::Invalidate()
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


bool CAdPluginConfiguration::Download()
{
    bool isOk = true;

    m_isValid = false;

    CAdPluginHttpRequest httpRequest(USERS_SCRIPT_SETTINGS);

    CAdPluginSettings* settings = CAdPluginSettings::GetInstance();
    
    settings->RefreshTab();

    DEBUG_GENERAL("*** Downloading settings");

    Invalidate();

    httpRequest.AddPluginId();

    httpRequest.Add("enabled", settings->GetPluginEnabled() ? "true":"false");
    httpRequest.Add("lang", settings->GetString(SETTING_LANGUAGE, "err"));
	httpRequest.Add("ie", LocalClient::GetBrowserVersion());
	httpRequest.Add("ielang", LocalClient::GetBrowserLanguage());

	httpRequest.AddOsInfo();

    httpRequest.Add("pc", LocalClient::GetComputerName(), false);
    httpRequest.Add("username", LocalClient::GetUserName(), false);

    CStringA newPluginId = LocalClient::GetPluginId();
    if (newPluginId != settings->GetString(SETTING_PLUGIN_ID))
    {
        httpRequest.Add("newplugin", newPluginId);
    }

    httpRequest.Add("errors", settings->GetErrorList());

#ifdef SUPPORT_WHITELIST

    // White list info
    CStringA whiteListCount;   
    whiteListCount.Format("%d", settings->GetWhiteListedDomainCount());

    httpRequest.Add("wcount", whiteListCount);

    TDomainList whiteListToGo = settings->GetWhiteListedDomainList(true);
    TDomainList whiteListToGoSent;

    if (!whiteListToGo.empty())
    {
        CStringA whiteList;
        int count = 0;

        for (TDomainList::const_iterator it = whiteListToGo.begin(); it != whiteListToGo.end() && count < 5; count++, ++it)
        {            
            CStringA whiteListReason;
            whiteListReason.Format(",%d", it->second);

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

	const std::auto_ptr<CAdPluginIniFile>& iniFile = httpRequest.GetResponseFile();

#ifdef SUPPORT_WHITELIST

    // Update whitelists to go
    if (!whiteListToGoSent.empty())
    {
        settings->RemoveWhiteListedDomainsToGo(whiteListToGoSent);
    }

#endif // SUPPORT_WHITELIST

    // Unpack settings
    CAdPluginIniFile::TSectionData settingsData = iniFile->GetSectionData("Settings");
    CAdPluginIniFile::TSectionData::iterator it;

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

    it = settingsData.find("pluginactivated");
    if (it != settingsData.end())
    {
        m_isPluginActivated = it->second == "true";
        m_isValidPluginActivated = true;
        DEBUG_SETTINGS("Settings::Configuration plugin activated:" + it->second);
    }

    it = settingsData.find("pluginactivateenabled");
    if (it != settingsData.end())
    {
        m_isPluginActivateEnabled = it->second == "true";
        m_isValidPluginActivateEnabled = true;
        DEBUG_SETTINGS("Settings::Configuration plugin activate enabled:" + it->second);
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

        CAdPluginIniFile::TSectionData filters = iniFile->GetSectionData("Filters");

        int filterCount = 0;
        bool bContinue = true;

        m_filterUrlList.clear();

        do
        {
            CStringA filterCountStr;
            filterCountStr.Format("%d", ++filterCount);
            
            CAdPluginIniFile::TSectionData::iterator filterIt = filters.find("filter" + filterCountStr);
            CAdPluginIniFile::TSectionData::iterator versionIt = filters.find("filter" + filterCountStr + "v");

            if (bContinue = (filterIt != filters.end() && versionIt != filters.end()))
            {
                m_filterUrlList[filterIt->second] = atoi(versionIt->second);
            }

        } while (bContinue);
    }

#endif // SUPPORT_FILTER

#ifdef SUPPORT_WHITELIST

    // Unpack whitelist domains
    m_isValidWhiteList = iniFile->HasSection("Whitelist");
    if (m_isValidWhiteList)
    {
        CAdPluginIniFile::TSectionData whitelist = iniFile->GetSectionData("Whitelist");        

        int domainCount = 0;
        bool bContinue = true;

        m_whiteList.clear();

        do
        {
            CStringA domainCountStr;
            domainCountStr.Format("%d", ++domainCount);
            
            CAdPluginIniFile::TSectionData::iterator domainIt = whitelist.find("domain" + domainCountStr);
            CAdPluginIniFile::TSectionData::iterator reasonIt = whitelist.find("domain" + domainCountStr + "r");

            if (bContinue = (domainIt != whitelist.end() && reasonIt != whitelist.end()))
            {
                m_whiteList[domainIt->second] = atoi(reasonIt->second);
            }

        } while (bContinue);
    }

#endif // #ifdef SUPPORT_WHITELIST

    m_isValid = isOk;

	return isOk;
}


bool CAdPluginConfiguration::IsValid() const
{
   return m_isValid;
}


bool CAdPluginConfiguration::IsValidUserId() const
{
   return m_isValidUserId;
}


bool CAdPluginConfiguration::IsValidPluginActivated() const
{
   return m_isValidPluginActivated;
}


bool CAdPluginConfiguration::IsValidPluginActivateEnabled() const
{
   return m_isValidPluginActivateEnabled;
}


bool CAdPluginConfiguration::IsValidPluginExpired() const
{
   return m_isValidPluginExpired;
}


bool CAdPluginConfiguration::IsValidPluginUpdate() const
{
   return m_isValidPluginUpdate;
}


bool CAdPluginConfiguration::IsValidDictionary() const
{
   return m_isValidDictionary;
}


bool CAdPluginConfiguration::IsPluginActivated() const
{
    return m_isPluginActivated;
}


bool CAdPluginConfiguration::IsPluginActivateEnabled() const
{
    return m_isPluginActivateEnabled;
}


bool CAdPluginConfiguration::IsPluginExpired() const
{
    return m_isPluginExpired;
}


bool CAdPluginConfiguration::IsValidPluginInfoPanel() const
{
   return m_isValidPluginInfoPanel;
}

#ifdef SUPPORT_WHITELIST

bool CAdPluginConfiguration::IsValidWhiteList() const
{
   return m_isValidWhiteList;
}

#endif // SUPPORT_WHITELIST

#ifdef SUPPORT_FILTER

bool CAdPluginConfiguration::IsValidFilter() const
{
   return m_isValidFilter;
}

#endif // SUPPORT_FILTER

#ifdef SUPPORT_CONFIG

bool CAdPluginConfiguration::IsValidConfig() const
{
    return m_isValidConfig;
}

#endif // SUPPORT_CONFIG

CStringA CAdPluginConfiguration::GetUserId() const
{
	return m_userId;
}


int CAdPluginConfiguration::GetPluginInfoPanel() const
{
	return m_pluginInfoPanel;
}


CStringA CAdPluginConfiguration::GetPluginUpdateUrl() const
{
	return m_pluginUpdateUrl;
}


CStringA CAdPluginConfiguration::GetPluginUpdateVersion() const
{
	return m_pluginUpdateVersion;
}


int CAdPluginConfiguration::GetDictionaryVersion() const
{
	return m_dictionaryVersion;
}


CStringA CAdPluginConfiguration::GetDictionaryUrl() const
{
	return m_dictionaryUrl;
}


#ifdef SUPPORT_FILTER

int CAdPluginConfiguration::GetFilterVersion() const
{
	return m_filterVersion;
}


TFilterUrlList CAdPluginConfiguration::GetFilterUrlList() const
{
	return m_filterUrlList;
}

#endif // SUPPORT_FILTER

#ifdef SUPPORT_WHITELIST

TDomainList CAdPluginConfiguration::GetWhiteList() const
{
	return m_whiteList;
}

#endif // SUPPORT_WHITELIST

#ifdef SUPPORT_CONFIG

CStringA CAdPluginConfiguration::GetConfigUrl() const
{
	return m_configUrl;
}

int CAdPluginConfiguration::GetConfigVersion() const
{
	return m_configVersion;
}

#endif // SUPPORT_CONFIG


#ifdef PRODUCT_AIDOINLINE
CStringA CAdPluginConfiguration::GetCollectedStatus() const
{
	return m_collectedStatus;
}
#endif
