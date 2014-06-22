/*
* http://msdn.microsoft.com/en-us/library/bb250436.aspx
*/

#ifndef _PLUGIN_CLASS_H_
#define _PLUGIN_CLASS_H_


#include "PluginTypedef.h"
#include "Plugin.h"
#include "PluginTab.h"
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <set>

#include "NotificationMessage.h"

#define ICON_PLUGIN_DISABLED 0
#define ICON_PLUGIN_ENABLED 1
#define ICON_PLUGIN_DEACTIVATED 2
#define ICON_MAX 3

#define WM_LAUNCH_INFO					(WM_APP + 10)

#ifdef SUPPORT_WHITELIST
#define WM_WHITELIST_DOMAIN		        (WM_LAUNCH_INFO + 1)
#else
#define WM_GROUP2_START                    (WM_LAUNCH_INFO + 1)
#endif

class CPluginMimeFilterClient;


class ATL_NO_VTABLE CPluginClass :
  public CComObjectRootEx<CComMultiThreadModel>,
  public CComCoClass<CPluginClass, &CLSID_PluginClass>,
  public IObjectWithSiteImpl<CPluginClass>,
  public IDispatchImpl<IIEPlugin, &IID_IIEPlugin, &LIBID_PluginLib>,
  public IOleCommandTarget
{

  friend class CPluginTab;

private:

  CPluginTab* m_tab;

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

  static CPluginTab* GetTab(DWORD dwThreadId);
  CPluginTab* GetTab();

  void UpdateStatusBar();
  static DWORD WINAPI MainThreadProc(LPVOID pParam);

private:

  bool SetMenuBar(HMENU hMenu, const CString& url);
  HMENU CreatePluginMenu(const CString& url);

  void DisplayPluginMenu(HMENU hMenu, int nToolbarCmdID, POINT pt, UINT nMenuFlags);
  bool CreateStatusBarPane();

  CComPtr<IConnectionPoint> GetConnectionPoint();
  CComPtr<IConnectionPoint> GetConnectionPointPropSink();

public:
  HWND GetBrowserHWND() const;
  HWND GetTabHWND() const;
  CComQIPtr<IWebBrowser2> GetBrowser() const;

  STDMETHODIMP OnTabChanged(DISPPARAMS* pDispParams, WORD wFlags);

  static CPluginMimeFilterClient* s_mimeFilter;

private:

  CString GetBrowserUrl() const;


  static DWORD WINAPI StartInitObject(LPVOID thisPtr);
  bool InitObject(bool bBHO);
  void CloseTheme();
  void UpdateTheme();

  static HICON GetStatusBarIcon(const CString& url);
  static CPluginClass* FindInstance(HWND hStatusBarWnd);
  static LRESULT CALLBACK NewStatusProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK PaneWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static void FirstRunThread();
  void BeforeNavigate2(DISPPARAMS* pDispParams);

  void Unadvice();

  void ShowStatusBar();
  bool IsStatusBarEnabled();

public:
  CComQIPtr<IWebBrowser2> m_webBrowser2;
private:
  DWORD m_nConnectionID;
  HWND m_hBrowserWnd;
  HWND m_hTabWnd;
  HWND m_hStatusBarWnd;
  HWND m_hPaneWnd;
  
  WNDPROC m_pWndProcStatus;
  int m_nPaneWidth;
  HANDLE m_hTheme;

  CriticalSection m_csStatusBar;

  NotificationMessage notificationMessage;

  bool m_isAdviced;
  bool m_isInitializedOk;

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


  static HINSTANCE s_hUxtheme;
  static std::set<CPluginClass*> s_instances;
  static std::map<DWORD,CPluginClass*> s_threadInstances;

#ifdef SUPPORT_WHITELIST
  static std::map<UINT, CString> s_menuDomains;
#endif

  static CComAutoCriticalSection s_criticalSectionLocal;
  static CComAutoCriticalSection s_criticalSectionBrowser;
  static CComAutoCriticalSection s_criticalSectionWindow;
#ifdef SUPPORT_WHITELIST
  static CComAutoCriticalSection s_criticalSectionWhiteList;
#endif

  // Async browser
  static CComQIPtr<IWebBrowser2> s_asyncWebBrowser2;

  static CComQIPtr<IWebBrowser2> GetAsyncBrowser();

};

OBJECT_ENTRY_AUTO(__uuidof(PluginClass), CPluginClass)


#endif // _PLUGIN_CLASS_H_
