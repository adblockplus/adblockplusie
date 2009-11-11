#ifndef _PLUGIN_CLIENT_H_
#define _PLUGIN_CLIENT_H_


#include "AdPluginTypedef.h"


class CPluginClientFactory;
class CPluginFilter;

class CPluginError
{
private:

    int m_errorId;
    int m_errorSubid;
    DWORD m_errorCode;
    CStringA m_errorDescription;
    DWORD m_processId;
    DWORD m_threadId;
    
public:

    CPluginError(int errorId, int errorSubid, DWORD errorCode, const CStringA& errorDesc) : 
        m_errorId(errorId), m_errorSubid(errorSubid), m_errorCode(errorCode), m_errorDescription(errorDesc)
    {
        m_processId = ::GetCurrentProcessId();
        m_threadId = ::GetCurrentThreadId();
    }

    CPluginError() : 
        m_errorId(0), m_errorSubid(0), m_errorCode(0), m_processId(0), m_threadId(0) {}

    CPluginError(const CPluginError& org) : 
        m_errorId(org.m_errorId), m_errorSubid(org.m_errorSubid), m_errorCode(org.m_errorCode), m_errorDescription(org.m_errorDescription), m_processId(org.m_processId), m_threadId(org.m_threadId) {}

    int GetErrorId() const { return m_errorId; }
    int GetErrorSubid() const { return m_errorSubid; }
    DWORD GetErrorCode() const { return m_errorCode; }
    CStringA GetErrorDescription() const { return m_errorDescription; }
    DWORD GetProcessId() const { return m_processId; }
    DWORD GetThreadId() const { return m_threadId; }
};


class CPluginClient
{
	friend class CPluginClientFactory;

private:

	static CComAutoCriticalSection s_criticalSectionLocal;
	static CComAutoCriticalSection s_criticalSectionPluginId;
	static CComAutoCriticalSection s_criticalSectionErrorLog;
#ifdef SUPPORT_FILTER
	static CComAutoCriticalSection s_criticalSectionFilter;
#endif

    static std::vector<CPluginError> s_pluginErrors;

	CStringA m_documentDomain;
	CStringA m_documentUrl;

#ifdef SUPPORT_FILTER
	std::auto_ptr<CPluginFilter> m_filter;
	TFilterFileList m_filterDownloads;
#endif

	static bool s_isErrorLogging;

	static CStringA s_pluginId;

	// Private constructor used by the singleton pattern
	CPluginClient();

public:

	~CPluginClient();

	static void SetLocalization();
	
	// Read the filters from the persistent storage and make them ready for use
#ifdef SUPPORT_FILTER
	void ReadFilters();
    bool IsFilterAlive() const;
	void RequestFilterDownload(const CStringA& filter, const CStringA& filterPath);
    bool DownloadFirstMissingFilter();

	// Removes the url from the list of whitelisted urls if present
	// Only called from ui thread
	bool ShouldBlock(CStringA src, int contentType, const CStringA& domain, bool addDebug=false);

    bool IsElementHidden(const CStringA& tag, IHTMLElement* pEl, const CStringA& domain, const CStringA& indent);
	
#endif // SUPPORT_FILTER

#ifdef SUPPORT_WHITELIST

	// returns true if the list is whitelisted, false otherwise
	// whitelist means that plugin should be disabled on the site
	// param url namespace of the url
	//called from ui thread, called from various threads that handles the mimefilter
	bool IsUrlWhiteListed(const CStringA& url);
	bool IsDocumentWhiteListed();

#endif // SUPPORT_WHITELIST

	static bool IsValidDomain(const CStringA& domain);
	static CStringA ExtractDomain(const CStringA& url);
    static CStringA& UnescapeUrl(CStringA& url);
	
	void SetDocumentDomain(const CStringA& domain);
	CStringA GetDocumentDomain() const;

	void SetDocumentUrl(const CStringA& url);
	CStringA GetDocumentUrl() const;

	// Returns browser info
	static CStringA GetBrowserLanguage();
	static CStringA GetBrowserVersion();

    static CStringA GetUserName();

    static CStringA GetComputerName();

	static CStringA GetPluginId();

    static CStringA GetMacId(bool addSeparator=false);

    static CStringA GeneratePluginId();

	static void LogPluginError(DWORD errorCode, int errorId, int errorSubid, const CStringA& description="", bool isAsync=false, DWORD dwProcessId=0, DWORD dwThreadId=0);

    static bool SendFtpFile(LPCTSTR server, LPCTSTR inputFile, LPCTSTR outputFile);

    static void PostPluginError(int errorId, int errorSubid, DWORD errorCode, const CStringA& errorDescription);
    static bool PopFirstPluginError(CPluginError& pluginError);

    // Cache
	CComAutoCriticalSection m_criticalSectionCache;

#ifdef SUPPORT_WHITELIST
    std::map<CStringA,bool> m_cacheWhitelistedUrls;
    std::set<CStringA> m_cacheFrames;

    void AddCacheFrame(const CStringA& url);
    bool IsFrame(const CStringA& url);
#endif

#ifdef SUPPORT_FILTER
    std::map<CStringA,bool> m_cacheBlockedSources;
#endif

    CStringA m_cacheDomain;

    void ClearCache(const CStringA& domain="");

    // Download files
#ifdef SUPPORT_FILE_DOWNLOAD
    TDownloadFiles m_downloadFiles;
    
    void AddDownloadFile(const CStringA& url, int fileSize, const SDownloadFileProperties& properties);
    TDownloadFiles GetDownloadFiles() const;
    bool HasDownloadFiles() const;
#endif
};

#endif // _PLUGIN_CLIENT_H_
