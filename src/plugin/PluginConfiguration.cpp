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
  return true;
  // The following code in this method is kepy for reference only. Remove it when the
  // functionality has been implemented elsewhere (if not removed completely)
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

  httpRequest.Add("errors", settings->GetErrorList());

  httpRequest.Add("dicv", settings->GetValue(SETTING_DICTIONARY_VERSION, 0));

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

  m_isValid = isOk;

  return isOk;
}


bool CPluginConfiguration::IsValid() const
{
  // Since we don't need the settings to be of any specific kind, we just assume they are always valid
  // This file will be refactored in future.
  return true;
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
  // We don't use configuration for now, so filters are always valid
  //   return m_isValidFilter;
  return true;
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
  return PLUGIN_UPDATE_URL;
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


#ifdef SUPPORT_WHITELIST


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
