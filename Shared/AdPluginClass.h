/*
* http://msdn.microsoft.com/en-us/library/bb250436.aspx
*/

#ifndef _PLUGIN_CLASS_H_
#define _PLUGIN_CLASS_H_


#include "AdPluginTypedef.h"


#ifdef PRODUCT_ADBLOCKER
 #include "../AdBlocker/AdBlocker.h"
#endif
#ifdef PRODUCT_DOWNLOADHELPER
 #include "../DownloadHelper/DownloadHelper.h"
#endif

#define ICON_PLUGIN_DISABLED 0
#define ICON_PLUGIN_ENABLED 1
#define ICON_PLUGIN_DEACTIVATED 2
#define ICON_MAX 3

#define WM_LAUNCH_INFO					(WM_APP + 10)

#ifdef SUPPORT_WHITELIST
 #define WM_WHITELIST_DOMAIN		        (WM_LAUNCH_INFO + 1)
 #define WM_WHITELIST_DOMAIN_MAX	        (WM_WHITELIST_DOMAIN + DOMAIN_HISTORY_MAX_COUNT + 1)
 #define WM_WHITELIST_DOMAIN_SUPPORT		(WM_WHITELIST_DOMAIN_MAX + 1)
 #define WM_WHITELIST_DOMAIN_SUPPORT_MAX	(WM_WHITELIST_DOMAIN_SUPPORT + DOMAIN_HISTORY_MAX_COUNT + 1)
 #define WM_WHITELIST_DOMAIN_ERROR		    (WM_WHITELIST_DOMAIN_SUPPORT_MAX + 1)
 #define WM_WHITELIST_DOMAIN_ERROR_MAX	    (WM_WHITELIST_DOMAIN_ERROR + DOMAIN_HISTORY_MAX_COUNT + 1)
 #define WM_GROUP2_START                    (WM_WHITELIST_DOMAIN_ERROR_MAX + 1)
#else
 #define WM_GROUP2_START                    (WM_LAUNCH_INFO + 1)
#endif

#ifdef SUPPORT_FILE_DOWNLOAD
 #define WM_DOWNLOAD_FILE                   (WM_GROUP2_START)
 #define WM_DOWNLOAD_FILE_MAX	            (WM_DOWNLOAD_FILE + DOWNLOAD_FILE_MAX_COUNT + 1)
#endif

#ifdef SUPPORT_FILTER

class CElementHideCache
{
public:

    bool m_isHidden;
    long m_elements;

    CElementHideCache() : m_isHidden(false), m_elements(0) {};
};

#endif


class CPluginMimeFilterClient;

#ifdef PRODUCT_DOWNLOADHELPER
	class CDownloadDomTraverser;
#endif
#ifdef PRODUCT_ADBLOCKER
	class CElementHideDomTraverser;
#endif


// This class implements an object that's created for every browser window. The SetSite
// method is called when the window is created, and the object asks to receive events.
// When an event occurs, the Invoke method is called with details.

class ATL_NO_VTABLE CPluginClass : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CPluginClass, &CLSID_AdPluginClass>,
	public IObjectWithSiteImpl<CPluginClass>,
	public IDispatchImpl<IIEPlugin, &IID_IIEPlugin, &LIBID_AdPluginLib>,
	public IOleCommandTarget
{

public:

	DECLARE_REGISTRY_RESOURCEID(IDR_PLUGIN_CLASS)

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	BEGIN_COM_MAP(CPluginClass)
		COM_INTERFACE_ENTRY(IIEPlugin)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY_IMPL(IObjectWithSite)
		COM_INTERFACE_ENTRY(IOleCommandTarget)
	END_COM_MAP()

	CPluginClass();
	~CPluginClass();
	
	HRESULT FinalConstruct();
	void FinalRelease();

// IObjectWithSite

	STDMETHOD(SetSite)(IUnknown *pUnkSite);

// IOleCommandTarget	

	STDMETHOD(QueryStatus)(const GUID* pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT* pCmdText);
	STDMETHOD(Exec)(const GUID*, DWORD nCmdID, DWORD, VARIANTARG*, VARIANTARG* pvaOut);

// IDispatch

	STDMETHOD(Invoke)(DISPID dispidMember,REFIID riid, LCID lcid, WORD wFlags,
        DISPPARAMS * pdispparams, VARIANT * pvarResult,EXCEPINFO * pexcepinfo, UINT * puArgErr);

private:

	bool SetMenuBar(HMENU hMenu, const CString& url);	
	HMENU CreatePluginMenu(const CString& url);

	void DisplayPluginMenu(HMENU hMenu, int nToolbarCmdID, POINT pt, UINT nMenuFlags);
	bool CreateStatusBarPane();

	CComPtr<IConnectionPoint> GetConnectionPoint();
	CComPtr<IConnectionPoint> GetConnectionPointPropSink();

	HWND GetBrowserHWND() const;
    CComQIPtr<IWebBrowser2> GetBrowser() const;

	CString GetBrowserUrl() const;

	void SetDocumentUrl(const CString& url);
	CString GetDocumentUrl() const;
	CString GetDocumentDomain() const;

	static CPluginMimeFilterClient* s_mimeFilter;

#ifdef SUPPORT_DOM_TRAVERSER
#ifdef PRODUCT_DOWNLOADHELPER
	CDownloadDomTraverser* m_traverser;
#endif
#ifdef PRODUCT_ADBLOCKER
	CElementHideDomTraverser* m_traverser;
#endif
#endif // SUPPORT_DOM_TRAVERSER

	bool InitObject(bool bBHO);
	void CloseTheme();
	void UpdateTheme();

    static void UpdateStatusBar();

	static HICON GetStatusBarButton(const CString& url);	
	static void LaunchUpdater(const CString& path);
	static CPluginClass* FindInstance(HWND hStatusBarWnd);
	static LRESULT CALLBACK NewStatusProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK PaneWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void BeforeNavigate2(DISPPARAMS* pDispParams);

    void Unadvice();

	CComQIPtr<IWebBrowser2> m_webBrowser2;
	DWORD m_nConnectionID;
	HWND m_hBrowserWnd;
	HWND m_hTabWnd;
	HWND m_hStatusBarWnd;
	HWND m_hPaneWnd;
	WNDPROC m_pWndProcStatus;
	int m_nPaneWidth;
	HANDLE m_hTheme;

	CString m_documentUrl;
	CString m_documentDomain;

	bool m_isAdviced;
	bool m_isRefresh;
	
    // Atom pane class  
	static ATOM s_atomPaneClass;

	static ATOM GetAtomPaneClass();

    // Icons
	static HICON s_hIcons[ICON_MAX];
	static DWORD s_hIconTypes[ICON_MAX];

    static HICON GetIcon(int type);

    // Main thread
	static HANDLE s_hMainThread;
	static bool s_isMainThreadDone;

    static HANDLE GetMainThreadHandle();
    static bool IsMainThreadDone(HANDLE mainThread);

	static DWORD WINAPI MainThreadProc(LPVOID pParam);

    // Tab thread
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

	static HANDLE s_hTabThread;
	static bool s_isTabThreadDone;

    static HANDLE GetTabThreadHandle();
    static bool IsTabThreadDone(HANDLE tabThread);

	static DWORD WINAPI TabThreadProc(LPVOID pParam);

	static HINSTANCE s_hUxtheme;
	static CSimpleArray<CPluginClass*> s_instances;

    // Is plugin to be updated?
    static bool s_isPluginToBeUpdated;

    // True when tab is opened or activated (must refresh settings)
    static bool s_isTabActivated;

#ifdef SUPPORT_WHITELIST
	static std::map<UINT, CString> s_menuDomains;
#endif

	static CComAutoCriticalSection s_criticalSectionLocal;
	static CComAutoCriticalSection s_criticalSectionBrowser;
#ifdef SUPPORT_WHITELIST
	static CComAutoCriticalSection s_criticalSectionWhiteList;
#endif

    // Async browser
	static CComQIPtr<IWebBrowser2> s_asyncWebBrowser2;

    static CComQIPtr<IWebBrowser2> GetAsyncBrowser();

#ifdef SUPPORT_FILE_DOWNLOAD
    static TMenuDownloadFiles s_menuDownloadFiles;
#endif

public:

	static void PostMessage(UINT message, WPARAM wParam=0, LPARAM lParam=0);

protected:

#ifdef SUPPORT_CONFIG
	static void UpdateConfig();
#endif

};

OBJECT_ENTRY_AUTO(__uuidof(AdPluginClass), CPluginClass)


#endif // _PLUGIN_CLASS_H_
