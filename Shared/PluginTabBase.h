#ifndef _PLUGIN_TAB_BASE_H_
#define _PLUGIN_TAB_BASE_H_


#ifdef SUPPORT_DOM_TRAVERSER
 class CPluginDomTraverser;
#endif


class CPluginClass;


class CPluginTabBase
{

	friend class CPluginClass;

protected:

	CComAutoCriticalSection m_criticalSection;

	CString m_documentDomain;
	CString m_documentUrl;
public:
	CPluginClass* m_plugin;
protected:
	bool m_isActivated;

	HANDLE m_hThread;
	bool m_isThreadDone;

#ifdef SUPPORT_DOM_TRAVERSER
	CPluginDomTraverser* m_traverser;
#endif

    static int s_dictionaryVersion;
    static int s_settingsVersion;
#ifdef SUPPORT_FILTER
    static int s_filterVersion;
#endif
#ifdef SUPPORT_WHITELIST
    static int s_whitelistVersion;
#endif
#ifdef SUPPORT_CONFIG
    static int s_configVersion;
#endif

	static DWORD WINAPI ThreadProc(LPVOID pParam);

#ifdef SUPPORT_FRAME_CACHING
	CComAutoCriticalSection m_criticalSectionCache;
    std::set<CString> m_cacheFrames;
	CString m_cacheDomain;
#endif

	void SetDocumentUrl(const CString& url);

public:

	CPluginTabBase(CPluginClass* plugin);
	~CPluginTabBase();

	CString GetDocumentDomain();
	CString GetDocumentUrl();

	virtual void OnActivate();
	virtual void OnUpdate();
	virtual bool OnUpdateSettings(bool forceUpdate);
	virtual bool OnUpdateConfig();
	virtual void OnNavigate(const CString& url);
	virtual void OnDownloadComplete(IWebBrowser2* browser);
	virtual void OnDocumentComplete(IWebBrowser2* browser, const CString& url, bool isDocumentBrowser);

	static DWORD WINAPI TabThreadProc(LPVOID pParam);

#ifdef SUPPORT_FRAME_CACHING
    void CacheFrame(const CString& url);
    bool IsFrameCached(const CString& url);
    void ClearFrameCache(const CString& domain="");
#endif

};


#endif // _PLUGIN_TAB_BASE_H_
