#include "AdPluginStdAfx.h"

#include "AdPluginServerClient.h"
#include "AdPluginClient.h"
#include "AdPluginDictionary.h"
#include "AdPluginConfiguration.h"
#include "AdPluginSettings.h"


CAdPluginServerClient::CAdPluginServerClient(LocalClient* localClient) : m_localClient(localClient)
{
	// Create and download the configuration
    m_pluginConfiguration = std::auto_ptr<CAdPluginConfiguration>(new CAdPluginConfiguration(CAdPluginSettings::GetInstance()->GetValue(SETTING_PLUGIN_ID), CAdPluginSettings::GetInstance()->GetValue(SETTING_PLUGIN_PASSWORD)));

	// Check and update filters and dictionary
	UpdateFilterUrlList();
}


void CAdPluginServerClient::UpdateFilterUrlList()
{
	TFilterUrlList filterUrlList = CAdPluginSettings::GetInstance()->GetFilterUrlList();

	// Call to server - fetch the configuration
	TFilterUrlList downloadedFilterUrllist = m_pluginConfiguration->GetFilterUrlList();
	TFilterUrlList newFilterUrlList;

	// Compare downloaded URL string with persistent URLs
	for (TFilterUrlList::iterator it = downloadedFilterUrllist.begin(); it != downloadedFilterUrllist.end(); ++it) 
	{
		CString downloadFilterName = it->first;

		CString filename = downloadFilterName.Trim().Right(downloadFilterName.GetLength() - downloadFilterName.ReverseFind('/') - 1).Trim();
		CString version = it->second;

		TFilterUrlList::const_iterator fi = filterUrlList.find(downloadFilterName);
		if (fi == filterUrlList.end() || fi->second != version)
		{
		    if (CAdPlutinFilter::DownloadFilterFile(downloadFilterName, filename))
		    {
			    newFilterUrlList[downloadFilterName] = version;
		    }
		}
	}

	// Save and update the filters
	if (!newFilterUrlList.empty())
	{
		m_localClient->UpdateFilters(newFilterUrlList);
	}
}
