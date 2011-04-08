#ifndef _SIMPLE_ADBLOCK_CLIENT_H_
#define _SIMPLE_ADBLOCK_CLIENT_H_


#include "PluginTypedef.h"
#include "PluginClientBase.h"


class CPluginFilter;


class CSimpleAdblockClient : public CPluginClientBase
{

private:

	std::auto_ptr<CPluginFilter> m_filter;

	TFilterFileList m_filterDownloads;

	CComAutoCriticalSection m_criticalSectionFilter;
	CComAutoCriticalSection m_criticalSectionCache;

    std::map<CString,bool> m_cacheBlockedSources;


	// Private constructor used by the singleton pattern
	CSimpleAdblockClient();

public:

	static CSimpleAdblockClient* s_instance;
	~CSimpleAdblockClient();

	static CSimpleAdblockClient* GetInstance();

	// Read the filters from the persistent storage and make them ready for use
	void ReadFilters();
	void RequestFilterDownload(const CString& filter, const CString& filterPath);
    bool DownloadFirstMissingFilter();

	// Removes the url from the list of whitelisted urls if present
	// Only called from ui thread
	bool ShouldBlock(CString src, int contentType, const CString& domain, bool addDebug=false);

    bool IsElementHidden(const CString& tag, IHTMLElement* pEl, const CString& domain, const CString& indent);
	bool IsUrlWhiteListed(const CString& url);

	int GetIEVersion();
};

#endif // _SIMPLE_ADBLOCK_CLIENT_H_
