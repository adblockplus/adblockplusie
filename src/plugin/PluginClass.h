/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2016 Eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
* http://msdn.microsoft.com/en-us/library/bb250436.aspx
*/

#ifndef _PLUGIN_CLASS_H_
#define _PLUGIN_CLASS_H_


#include "Plugin.h"
#include "PluginTabBase.h"
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

class CPluginMimeFilterClient;
class WebBrowserEventsListener;

class ATL_NO_VTABLE CPluginClass :
  public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>,
  public ATL::CComCoClass<CPluginClass, &CLSID_PluginClass>,
  public ATL::IObjectWithSiteImpl<CPluginClass>,
  public IOleCommandTarget,
  public ATL::IDispEventImpl<1, CPluginClass, &DIID_DWebBrowserEvents2, &LIBID_SHDocVw, 1, 1>
{

  friend class CPluginTab;

public:

  DECLARE_REGISTRY_RESOURCEID(IDR_PLUGIN_CLASS)

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  BEGIN_COM_MAP(CPluginClass)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(IOleCommandTarget)
  END_COM_MAP()

  BEGIN_SINK_MAP(CPluginClass)
    SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2, OnBeforeNavigate2)
    SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_DOWNLOADCOMPLETE, OnDownloadComplete)
    SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_WINDOWSTATECHANGED, OnWindowStateChanged)
    SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_COMMANDSTATECHANGE, OnCommandStateChange)
    SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_ONQUIT, OnOnQuit)
  END_SINK_MAP()

  CPluginClass();
  ~CPluginClass();

  // IObjectWithSite
  STDMETHOD(SetSite)(IUnknown *pUnkSite);

  // IOleCommandTarget
  STDMETHOD(QueryStatus)(const GUID* pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT* pCmdText);
  STDMETHOD(Exec)(const GUID*, DWORD nCmdID, DWORD, VARIANTARG*, VARIANTARG* pvaOut);


  static CPluginTab* GetTab(DWORD dwThreadId);
  CPluginTab* GetTab();

  void UpdateStatusBar();

private:

  bool SetMenuBar(HMENU hMenu, const std::wstring& url);
  HMENU CreatePluginMenu(const std::wstring& url);

  void DisplayPluginMenu(HMENU hMenu, int nToolbarCmdID, POINT pt, UINT nMenuFlags);
  bool CreateStatusBarPane();

public:
  HWND GetBrowserHWND() const;
  bool IsRootBrowser(IWebBrowser2*);

  static CPluginMimeFilterClient* s_mimeFilter;

private:

  std::wstring GetBrowserUrl() const;

  static DWORD WINAPI StartInitObject(LPVOID thisPtr);
  bool InitObject();
  void CloseTheme();
  void UpdateTheme();

  static HICON GetStatusBarIcon(const std::wstring& url);
  static CPluginClass* FindInstance(HWND hStatusBarWnd);
  static LRESULT CALLBACK NewStatusProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK PaneWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static void FirstRunThread();

  void STDMETHODCALLTYPE OnBeforeNavigate2(IDispatch* pDisp /**< [in] */,
                                           VARIANT* URL /**< [in] */,
                                           VARIANT* Flags /**< [in] */,
                                           VARIANT* TargetFrameName /**< [in] */,
                                           VARIANT* PostData /**< [in] */,
                                           VARIANT* Headers /**< [in] */,
                                           VARIANT_BOOL* Cancel /* [in, out] */);
  void STDMETHODCALLTYPE OnDownloadComplete();
  void STDMETHODCALLTYPE OnWindowStateChanged(unsigned long flags, unsigned long validFlagsMask);
  void STDMETHODCALLTYPE OnCommandStateChange(long command, VARIANT_BOOL enable);
  void STDMETHODCALLTYPE OnOnQuit();
  void Unadvise();
  void EnsureWebBrowserConnected(const ATL::CComPtr<IWebBrowser2>& webBrowser);

  void ShowStatusBar();
  bool IsStatusBarEnabled();

  HWND m_hBrowserWnd;
  HWND m_hTabWnd;
  HWND m_hStatusBarWnd;
  HWND m_hPaneWnd;
  
  WNDPROC m_pWndProcStatus;
  int m_nPaneWidth;
  HANDLE m_hTheme;
  struct Data
  {
    std::map<IWebBrowser2*, WebBrowserEventsListener*> connectedWebBrowsersCache;
    std::unique_ptr<CPluginTab> tab;
    ATL::CComPtr<IWebBrowser2> webBrowser2;
  };
  // we need to have it as a shared pointer to get weak pointer to it to avoid
  // wrong usage after destroying of this class.
  std::shared_ptr<Data> m_data;

  CriticalSection m_csStatusBar;

  NotificationMessage notificationMessage;

  bool m_isAdvised;
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

  static HINSTANCE s_hUxtheme;
  static std::set<CPluginClass*> s_instances;
  static std::map<DWORD,CPluginClass*> s_threadInstances;
  static CComAutoCriticalSection s_criticalSectionLocal;
  static CComAutoCriticalSection s_criticalSectionWindow;

  // Async browser
  static CComQIPtr<IWebBrowser2> s_asyncWebBrowser2;
  static CComQIPtr<IWebBrowser2> GetAsyncBrowser();
};

OBJECT_ENTRY_AUTO(__uuidof(PluginClass), CPluginClass)


#endif // _PLUGIN_CLASS_H_
