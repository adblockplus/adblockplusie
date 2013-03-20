#include "PluginStdAfx.h"

#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginFilter.h"
#include "PluginClientFactory.h"
#include "PluginDictionary.h"
#include "PluginHttpRequest.h"
#include "PluginMutex.h"
#include "PluginClass.h"

#include "AdblockPlusClient.h"


CAdblockPlusClient* CAdblockPlusClient::s_instance = NULL;


CAdblockPlusClient::CAdblockPlusClient() : CPluginClientBase()
{
    m_filter = std::auto_ptr<CPluginFilter>(new CPluginFilter());
}
CAdblockPlusClient::~CAdblockPlusClient()
{
	s_instance = NULL;
}


CAdblockPlusClient* CAdblockPlusClient::GetInstance()
{
	CAdblockPlusClient* instance = NULL;

    s_criticalSectionLocal.Lock();
    {
	    if (!s_instance)
	    {
		    CAdblockPlusClient* client = new CAdblockPlusClient();

		    s_instance = client;
	    }

	    instance = s_instance;
    }
    s_criticalSectionLocal.Unlock();

	return instance;
}


bool CAdblockPlusClient::ShouldBlock(CString src, int contentType, const CString& domain, bool addDebug)
{
    bool isBlocked = false;

	bool isCached = false;

	CPluginSettings* settings = CPluginSettings::GetInstance();

    m_criticalSectionCache.Lock();
    {
        std::map<CString,bool>::iterator it = m_cacheBlockedSources.find(src);

    	isCached = it != m_cacheBlockedSources.end();
        if (isCached)
        {
            isBlocked = it->second;
        }
    }
    m_criticalSectionCache.Unlock();

    if (!isCached)
    {
        m_criticalSectionFilter.Lock();
        {
	        isBlocked = m_filter->ShouldBlock(src, contentType, domain, addDebug);
        }
        m_criticalSectionFilter.Unlock();


        // Cache result, if content type is defined
        if (contentType != CFilter::contentTypeAny)
        {
            m_criticalSectionCache.Lock();
            {
                m_cacheBlockedSources[src] = isBlocked;
            }
            m_criticalSectionCache.Unlock();
        }
    }


	return isBlocked;
}

void CAdblockPlusClient::RequestFilterDownload(const CString& filter, const CString& filterPath)
{
    DEBUG_GENERAL(L"*** Requesting filter download:" + filter)

    m_criticalSectionFilter.Lock();
    {
	    m_filterDownloads.insert(std::make_pair(filter, filterPath));
    }
    m_criticalSectionFilter.Unlock();
}


bool CAdblockPlusClient::DownloadFirstMissingFilter()
{
    bool isDownloaded = false;

    CString filterFilename;
    CString filterDownloadPath;

    m_criticalSectionFilter.Lock();
    {
        TFilterFileList::iterator it = m_filterDownloads.begin();
        if (it != m_filterDownloads.end())
        {
            filterFilename = it->first;
            filterDownloadPath = it->second;

            m_filterDownloads.erase(it);
        }
    }
    m_criticalSectionFilter.Unlock();

    if (!filterFilename.IsEmpty() && m_filter->DownloadFilterFile(filterDownloadPath, filterFilename))
    {
        isDownloaded = true;

        CPluginSettings* settings = CPluginSettings::GetInstance();

        settings->IncrementTabVersion(SETTING_TAB_FILTER_VERSION);
    }

    return isDownloaded;
}


//in this method we read the filter that are in the persistent storage
//then we read them and use these to create a new filterclass

void CAdblockPlusClient::ReadFilters()
{
    CPluginSettings* settings = CPluginSettings::GetInstance();

    // Check existence of filter file
//    if (settings->IsMainProcess())
//    {
//        CPluginFilter::CreateFilters();
//    }

	TFilterFileList filterFileNames;

	TFilterUrlList filters = settings->GetFilterUrlList();
	std::map<CString, CString> filterFileNameList = settings->GetFilterFileNamesList();

	// Remember first entry in the map, is the filename, second is the version of the filter
	for (TFilterUrlList::iterator it = filters.begin(); it != filters.end(); ++it)
	{
	    DEBUG_FILTER(L"Filter::ReadFilters - adding url:" + it->first)

		CString filename = "";
		if (filterFileNameList.find(it->first) != filterFileNameList.end() )
		{
			filename = filterFileNameList.find(it->first)->second;
		}
		else
		{
			filename = it->first.Right(it->first.GetLength() - it->first.ReverseFind('/') - 1);
		}
		filterFileNames.insert(std::make_pair(filename, it->first));
	}

	// Create our filter class which can be used from now on
    std::auto_ptr<CPluginFilter> filter = std::auto_ptr<CPluginFilter>(new CPluginFilter(filterFileNames, CPluginSettings::GetDataPath()));

    m_criticalSectionFilter.Lock();
    {
	    m_filter = filter;
    }
    m_criticalSectionFilter.Unlock();

	ClearWhiteListCache();
}


bool CAdblockPlusClient::IsElementHidden(const CString& tag, IHTMLElement* pEl, const CString& domain, const CString& indent)
{
    bool isHidden;
	m_criticalSectionFilter.Lock();
	{
		isHidden = m_filter.get() && m_filter->IsElementHidden(tag, pEl, domain, indent);
	}
	m_criticalSectionFilter.Unlock();
    return isHidden;
}

bool CAdblockPlusClient::IsUrlWhiteListed(const CString& url)
{
	bool isWhitelisted = CPluginClientBase::IsUrlWhiteListed(url);
    if (isWhitelisted == false && !url.IsEmpty())
    {
        m_criticalSectionFilter.Lock();
		{
			isWhitelisted = m_filter.get() && m_filter->ShouldWhiteList(url);
		}
        m_criticalSectionFilter.Unlock();

		if (isWhitelisted)
		{
			CacheWhiteListedUrl(url, isWhitelisted);
		}
	}

	return isWhitelisted;
}

int CAdblockPlusClient::GetIEVersion()
{
	//HKEY_LOCAL_MACHINE\Software\Microsoft\Internet Explorer
	HKEY hKey;
	LSTATUS status = RegOpenKey(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Internet Explorer", &hKey);
	if (status != 0)
	{
		return 0;
	}
	DWORD type, cbData;
	BYTE version[50];
	cbData = 50;
	status = RegQueryValueEx(hKey, L"Version", NULL, &type, (BYTE*)version, &cbData);
	if (status != 0)
	{
		return 0;
	}
	RegCloseKey(hKey);
	return (int)(version[0] - 48);
}
