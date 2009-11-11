#ifndef _PLUGIN_CONFIGURATION_H_
#define _PLUGIN_CONFIGURATION_H_


#include "AdPluginTypedef.h"


class CPluginConfiguration
{

public:

	// Inject the userid and version id into the class when constructing
	CPluginConfiguration();

    // Refresh (download) configuration
    bool Download();
    void Invalidate();

    // Is configuration valid
    bool IsValid() const;
    bool IsValidUserId() const;
    bool IsValidPluginActivated() const;
    bool IsValidPluginActivateEnabled() const;
    bool IsValidPluginExpired() const;
    bool IsValidPluginUpdate() const;
    bool IsValidPluginInfoPanel() const;
    bool IsValidDictionary() const;
#ifdef SUPPORT_FILTER
    bool IsValidFilter() const;
#endif
#ifdef SUPPORT_WHITELIST
    bool IsValidWhiteList() const;
#endif
#ifdef SUPPORT_CONFIG
    bool IsValidConfig() const;
#endif

    // General plugin status
	bool IsPluginActivated() const;
	bool IsPluginActivateEnabled() const;
	bool IsPluginExpired() const;

	CStringA GetUserId() const;

	// Does there exists a new version, that can be downloaded and installed
	CStringA GetPluginUpdateVersion() const;
	CStringA GetPluginUpdateUrl() const;

	int GetPluginInfoPanel() const;

	// Dictionary information
	int GetDictionaryVersion() const;	
	CStringA GetDictionaryUrl() const;	

#ifdef SUPPORT_FILTER
	int GetFilterVersion() const;	
	TFilterUrlList GetFilterUrlList() const;
#endif // SUPPORT_FILTER

#ifdef SUPPORT_WHITELIST
	TDomainList GetWhiteList() const;
#endif // SUPPORT_WHITELIST

#ifdef SUPPORT_CONFIG
    CStringA GetConfigUrl() const;
    int GetConfigVersion() const;
#endif // SUPPORT_CONFIG

private:

    bool m_isValid;
    bool m_isValidUserId;
    bool m_isValidPluginActivated;
    bool m_isValidPluginActivateEnabled;
    bool m_isValidPluginExpired;
    bool m_isValidPluginUpdate;
    bool m_isValidPluginInfoPanel;
    bool m_isValidDictionary;
#ifdef SUPPORT_WHITELIST
    bool m_isValidWhiteList;
#endif
#ifdef SUPPORT_FILTER
    bool m_isValidFilter;
#endif
#ifdef SUPPORT_CONFIG
    bool m_isValidConfig;
#endif
    
    // General plugin status
	bool m_isPluginActivated;
    bool m_isPluginActivateEnabled;
	bool m_isPluginExpired;

    // User registration
	CStringA m_userId;

	// The version that currently can be downloaded from the server
	CStringA m_pluginUpdateVersion;
	CStringA m_pluginUpdateUrl;

	// Dictionary that currently can be downloaded from the server
	int m_dictionaryVersion;
	CStringA m_dictionaryUrl;

	int m_pluginInfoPanel;

#ifdef SUPPORT_FILTER
	int m_filterVersion;
	TFilterUrlList m_filterUrlList;
#endif

#ifdef SUPPORT_WHITELIST
	TDomainList m_whiteList;
#endif // SUPPORT_WHITELIST

#ifdef SUPPORT_CONFIG
    CStringA m_configUrl;
    int m_configVersion;
#endif // SUPPORT_CONFIG
};


#endif // _PLUGIN_CONFIGURATION_H_
