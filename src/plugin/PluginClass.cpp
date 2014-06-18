#include "PluginStdAfx.h"

#include "PluginClass.h"
#include "PluginSettings.h"
#include "PluginSystem.h"
#ifdef SUPPORT_FILTER
#include "PluginFilter.h"
#endif
#include "PluginMimeFilterClient.h"
#include "PluginClient.h"
#include "PluginClientFactory.h"
#include "PluginMutex.h"
#include "sddl.h"
#include "PluginUtil.h"
#include "PluginUserSettings.h"
#include "../shared/Utils.h"
#include "../shared/Dictionary.h"
#include <thread>

#ifdef DEBUG_HIDE_EL
DWORD profileTime = 0;
#endif

typedef HANDLE (WINAPI *OPENTHEMEDATA)(HWND, LPCWSTR);
typedef HRESULT (WINAPI *DRAWTHEMEBACKGROUND)(HANDLE, HDC, INT, INT, LPRECT, LPRECT);
typedef HRESULT (WINAPI *CLOSETHEMEDATA)(HANDLE);

HICON CPluginClass::s_hIcons[ICON_MAX] = { NULL, NULL, NULL };
DWORD CPluginClass::s_hIconTypes[ICON_MAX] = { IDI_ICON_DISABLED, IDI_ICON_ENABLED, IDI_ICON_DEACTIVATED };

CPluginMimeFilterClient* CPluginClass::s_mimeFilter = NULL;

CLOSETHEMEDATA pfnClose = NULL;
DRAWTHEMEBACKGROUND pfnDrawThemeBackground = NULL;
OPENTHEMEDATA pfnOpenThemeData = NULL;

ATOM CPluginClass::s_atomPaneClass = NULL;
HINSTANCE CPluginClass::s_hUxtheme = NULL;
CSimpleArray<CPluginClass*> CPluginClass::s_instances;
std::map<DWORD, CPluginClass*> CPluginClass::s_threadInstances;

CComAutoCriticalSection CPluginClass::s_criticalSectionLocal;
CComAutoCriticalSection CPluginClass::s_criticalSectionBrowser;
CComAutoCriticalSection CPluginClass::s_criticalSectionWindow;

CComQIPtr<IWebBrowser2> CPluginClass::s_asyncWebBrowser2;

#ifdef SUPPORT_WHITELIST
std::map<UINT,CString> CPluginClass::s_menuDomains;
#endif


CPluginClass::CPluginClass()
{
  //Use this line to debug memory leaks
  //	_CrtDumpMemoryLeaks();

  m_isAdviced = false;
  m_nConnectionID = 0;
  m_hTabWnd = NULL;
  m_hStatusBarWnd = NULL;
  m_hPaneWnd = NULL;
  m_nPaneWidth = 0;
  m_pWndProcStatus = NULL;
  m_hTheme = NULL;
  m_isInitializedOk = false;


  m_tab = new CPluginTab(this);

  CPluginSystem* system = CPluginSystem::GetInstance();

  std::wstring locale((LPCWSTR)system->GetBrowserLanguage());
  Dictionary::Create(locale);
}

CPluginClass::~CPluginClass()
{
  delete m_tab;
}


/////////////////////////////////////////////////////////////////////////////
// Initialization

HRESULT CPluginClass::FinalConstruct()
{
  return S_OK;
}

void CPluginClass::FinalRelease()
{
  s_criticalSectionBrowser.Lock();
  {
    m_webBrowser2.Release();
  }
  s_criticalSectionBrowser.Unlock();
}


// This method tries to get a 'connection point' from the stored browser, which can be
// used to attach or detach from the stream of browser events
CComPtr<IConnectionPoint> CPluginClass::GetConnectionPoint()
{
  CComQIPtr<IConnectionPointContainer, &IID_IConnectionPointContainer> pContainer(GetBrowser());
  if (!pContainer)
  {
    return NULL;
  }

  CComPtr<IConnectionPoint> pPoint;
  HRESULT hr = pContainer->FindConnectionPoint(DIID_DWebBrowserEvents2, &pPoint);
  if (FAILED(hr))
  {
    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_FIND_CONNECTION_POINT, "Class::GetConnectionPoint - FindConnectionPoint")
      return NULL;
  }

  return pPoint;
}

// This method tries to get a 'connection point' from the stored browser, which can be
// used to attach or detach from the stream of browser events
CComPtr<IConnectionPoint> CPluginClass::GetConnectionPointPropSink()
{
  CComQIPtr<IConnectionPointContainer, &IID_IConnectionPointContainer> pContainer(GetBrowser());
  if (!pContainer)
  {
    return NULL;
  }

  CComPtr<IConnectionPoint> pPoint;
  HRESULT hr = pContainer->FindConnectionPoint(IID_IPropertyNotifySink, &pPoint);
  if (FAILED(hr))
  {
    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_FIND_CONNECTION_POINT, "Class::GetConnectionPoint - FindConnectionPoint")
      return NULL;
  }

  return pPoint;
}


HWND CPluginClass::GetBrowserHWND() const
{
  SHANDLE_PTR hBrowserWndHandle = NULL;

  CComQIPtr<IWebBrowser2> browser = GetBrowser();
  if (browser)
  {
    HRESULT hr = browser->get_HWND(&hBrowserWndHandle);
    if (FAILED(hr))
    {
      DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_BROWSER_WINDOW, "Class::GetBrowserHWND - failed")
    }
  }

  return (HWND)hBrowserWndHandle;
}


CComQIPtr<IWebBrowser2> CPluginClass::GetBrowser() const
{
  CComQIPtr<IWebBrowser2> browser;

  s_criticalSectionBrowser.Lock();
  {
    browser = m_webBrowser2;
  }
  s_criticalSectionBrowser.Unlock();

  return browser;
}


CComQIPtr<IWebBrowser2> CPluginClass::GetAsyncBrowser()
{
  CComQIPtr<IWebBrowser2> browser;

  s_criticalSectionLocal.Lock();
  {
    browser = s_asyncWebBrowser2;
  }
  s_criticalSectionLocal.Unlock();

  return browser;
}

CString CPluginClass::GetBrowserUrl() const
{
  CString url;

  CComQIPtr<IWebBrowser2> browser = GetBrowser();
  if (browser)
  {
    CComBSTR bstrURL;

    if (SUCCEEDED(browser->get_LocationURL(&bstrURL)))
    {
      url = bstrURL;
      CPluginClient::UnescapeUrl(url);
    }
  }
  else
  {
    url = m_tab->GetDocumentUrl();
  }

  return url;
}

DWORD WINAPI CPluginClass::StartInitObject(LPVOID thisPtr)
{
  if (thisPtr == NULL)
    return 0;
  if (!((CPluginClass*)thisPtr)->InitObject(true))
  {
    ((CPluginClass*)thisPtr)->Unadvice();
  }

  return 0;
}



// This gets called when a new browser window is created (which also triggers the
// creation of this object). The pointer passed in should be to a IWebBrowser2
// interface that represents the browser for the window.
// it is also called when a tab is closed, this unknownSite will be null
// so we should handle that it is called this way several times during a session
STDMETHODIMP CPluginClass::SetSite(IUnknown* unknownSite)
{
  CPluginSettings* settings = CPluginSettings::GetInstance();
  CPluginSystem* system = CPluginSystem::GetInstance();

  MULTIPLE_VERSIONS_CHECK();

  if (unknownSite)
  {

    DEBUG_GENERAL(L"================================================================================\nNEW TAB UI\n================================================================================")

    HRESULT hr = ::CoInitialize(NULL);
    if (FAILED(hr))
    {
      DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_COINIT, "Class::SetSite - CoInitialize");
    }

    s_criticalSectionBrowser.Lock();
    {
      m_webBrowser2 = unknownSite;
    }
    s_criticalSectionBrowser.Unlock();

    //register the mimefilter
    //and only mimefilter
    //on some few computers the mimefilter does not get properly registered when it is done on another thread

    s_criticalSectionLocal.Lock();
    {
      // Always register on startup, then check if we need to unregister in a separate thread
      s_mimeFilter = CPluginClientFactory::GetMimeFilterClientInstance();
      s_asyncWebBrowser2 = unknownSite;
      s_instances.Add(this);
    }
    s_criticalSectionLocal.Unlock();

    try
    {
      // Check if loaded as BHO
      if (GetBrowser())
      {
        DEBUG_GENERAL("Loaded as BHO");
        CComPtr<IConnectionPoint> pPoint = GetConnectionPoint();
        if (pPoint)
        {
          HRESULT hr = pPoint->Advise((IDispatch*)this, &m_nConnectionID);
          if (SUCCEEDED(hr))
          {
            m_isAdviced = true;

            try
            {
              std::thread startInitObjectThread(StartInitObject, this);
              startInitObjectThread.detach(); // TODO: but actually we should wait for the thread in the dtr.
            }
            catch (const std::system_error& ex)
            {
              auto errDescription = std::string("Class::Thread - Failed to create StartInitObject thread, ") +
                ex.code().message() + ex.what();
              DEBUG_ERROR_LOG(ex.code().value(), PLUGIN_ERROR_THREAD, PLUGIN_ERROR_MAIN_THREAD_CREATE_PROCESS, errDescription.c_str());
            }
          }
          else
          {
            DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_ADVICE, "Class::SetSite - Advice");
          }
        }
      }
      else // Check if loaded as toolbar handler
      {
        DEBUG_GENERAL("Loaded as toolbar handler");
        CComPtr<IServiceProvider> pServiceProvider;

        HRESULT hr = unknownSite->QueryInterface(&pServiceProvider);
        if (SUCCEEDED(hr))
        {
          if (pServiceProvider)
          {
            s_criticalSectionBrowser.Lock();
            {
              HRESULT hr = pServiceProvider->QueryService(IID_IWebBrowserApp, &m_webBrowser2);
              if (SUCCEEDED(hr))
              {
                if (m_webBrowser2)
                {
                  InitObject(false);
                }
              }
              else
              {
                DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_QUERY_BROWSER, "Class::SetSite - QueryService (IID_IWebBrowserApp)");
              }
            }
            s_criticalSectionBrowser.Unlock();
          }
        }
        else
        {
          DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_QUERY_SERVICE_PROVIDER, "Class::SetSite - QueryInterface (service provider)");
        }
      }
    }
    catch (std::runtime_error e)
    {
      DEBUG_ERROR(e.what());
      Unadvice();
    }
  }
  else
  {
    // Unadvice
    Unadvice();

    // Destroy window
    if (m_pWndProcStatus)
    {
      ::SetWindowLongPtr(m_hStatusBarWnd, GWLP_WNDPROC, (LPARAM)(WNDPROC)m_pWndProcStatus);

      m_pWndProcStatus = NULL;
    }

    if (m_hPaneWnd)
    {
      DestroyWindow(m_hPaneWnd);
      m_hPaneWnd = NULL;
    }

    m_hTabWnd = NULL;
    m_hStatusBarWnd = NULL;

    // Remove instance from the list, shutdown threads
    HANDLE hMainThread = NULL;
    HANDLE hTabThread = NULL;

    s_criticalSectionLocal.Lock();
    {
      s_instances.Remove(this);

      std::map<DWORD,CPluginClass*>::iterator it = s_threadInstances.find(::GetCurrentThreadId());
      if (it != s_threadInstances.end())
      {
        s_threadInstances.erase(it);
      }
      if (s_instances.GetSize() == 0)
      {
        CPluginClientFactory::ReleaseMimeFilterClientInstance();
      }
    }
    s_criticalSectionLocal.Unlock();

    // Release browser interface
    s_criticalSectionBrowser.Lock();
    {
      m_webBrowser2.Release();
    }
    s_criticalSectionBrowser.Unlock();

    DEBUG_GENERAL("================================================================================\nNEW TAB UI - END\n================================================================================")

    ::CoUninitialize();
  }

  return IObjectWithSiteImpl<CPluginClass>::SetSite(unknownSite);
}

bool CPluginClass::IsStatusBarEnabled()
{
  DEBUG_GENERAL("IsStatusBarEnabled start");
  HKEY pHkey;
  HKEY pHkeySub;
  RegOpenCurrentUser(KEY_QUERY_VALUE, &pHkey);
  DWORD trueth = 1;
  DWORD truethSize = sizeof(DWORD);
  RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\Main", &pHkeySub);
  LONG res = RegQueryValueEx(pHkeySub, L"StatusBarWeb", NULL, NULL, (BYTE*)&trueth, &truethSize);
  RegCloseKey(pHkey);
  if (res != ERROR_SUCCESS)
  {
    res = RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\MINIE", &pHkeySub);
    if (res == ERROR_SUCCESS)
    {
      LONG res = RegQueryValueEx(pHkeySub, L"ShowStatusBar", NULL, NULL, (BYTE*)&trueth, &truethSize);
      if (res == ERROR_SUCCESS)
      {
        RegCloseKey(pHkey);
      }
    }
  }
  DEBUG_GENERAL("IsStatusBarEnabled end");
  return trueth == 1;
}

void CPluginClass::ShowStatusBar()
{
  DEBUG_GENERAL("ShowStatusBar start");

  VARIANT_BOOL isVisible;


  CComQIPtr<IWebBrowser2> browser = GetAsyncBrowser();
  if (browser)
  {
    HRESULT hr = S_OK;
    hr = browser->get_StatusBar(&isVisible);
    if (SUCCEEDED(hr))
    {
      if (!isVisible)
      {
        SHANDLE_PTR pBrowserHWnd;
        browser->get_HWND((SHANDLE_PTR*)&pBrowserHWnd);
        Dictionary* dictionary = Dictionary::GetInstance();

        HKEY pHkey;
        HKEY pHkeySub;
        LSTATUS regRes = 0;
        regRes = RegOpenCurrentUser(KEY_WRITE, &pHkey);

        // Do we have enough rights to enable a status bar?
        if (regRes != 0)
        {
          // We use the tab window here and in the next few calls, since the browser window may still not be available
          LRESULT res = MessageBox((HWND)m_hTabWnd,
              dictionary->Lookup("status-bar", "error-text").c_str(),
              dictionary->Lookup("status-bar", "error-title").c_str(),
              MB_OK);
          return;
        }
        // Ask if a user wants to enable a status bar automatically
        LRESULT res = MessageBox((HWND)m_hTabWnd,
            dictionary->Lookup("status-bar", "question").c_str(),
            dictionary->Lookup("status-bar", "title").c_str(),
            MB_YESNO);
        if (res == IDYES)
        {
          DWORD trueth = 1;
          regRes = RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\MINIE", &pHkeySub);
          regRes = RegSetValueEx(pHkeySub, L"ShowStatusBar", 0, REG_DWORD, (BYTE*)&trueth, sizeof(DWORD));
          regRes = RegCloseKey(pHkeySub);
          regRes = RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\Main", &pHkeySub);
          regRes = RegSetValueEx(pHkeySub, L"StatusBarWeb", 0, REG_DWORD, (BYTE*)&trueth, sizeof(DWORD));
          regRes = RegCloseKey(pHkeySub);
          hr = browser->put_StatusBar(TRUE);
          if (FAILED(hr))
          {
            DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_PUT_STATUSBAR, "Class::Enable statusbar");
          }
          CreateStatusBarPane();

          // We need to restart the tab now, to enable the status bar properly
          VARIANT vFlags;
          vFlags.vt = VT_I4;
          vFlags.intVal = navOpenInNewTab;

          CComBSTR curLoc;
          browser->get_LocationURL(&curLoc);
          HRESULT hr = browser->Navigate(curLoc, &vFlags, NULL, NULL, NULL);
          if (FAILED(hr))
          {
            vFlags.intVal = navOpenInNewWindow;

            hr = browser->Navigate(CComBSTR(curLoc), &vFlags, NULL, NULL, NULL);
            if (FAILED(hr))
            {
              DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_NAVIGATION, PLUGIN_ERROR_NAVIGATION, "Navigation::Failed")
            }
          }
          browser->Quit();
        }
      }
    }
    else
    {
      DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_STATUSBAR, "Class::Get statusbar state");
    }
  }
  DEBUG_GENERAL("ShowStatusBar end");
}

void CPluginClass::BeforeNavigate2(DISPPARAMS* pDispParams)
{

  if (pDispParams->cArgs < 7)
  {
    return;
  }
  //Register a mime filter if it's not registered yet
  if (s_mimeFilter == NULL)
  {
    s_mimeFilter = CPluginClientFactory::GetMimeFilterClientInstance();
  }

  // Get the IWebBrowser2 interface
  CComQIPtr<IWebBrowser2, &IID_IWebBrowser2> WebBrowser2Ptr;
  VARTYPE vt = pDispParams->rgvarg[6].vt;
  if (vt == VT_DISPATCH)
  {
    WebBrowser2Ptr = pDispParams->rgvarg[6].pdispVal;
  }
  else
  {
    // Wrong type, return.
    return;
  }

  // Get the URL
  CString url;
  vt = pDispParams->rgvarg[5].vt;
  if (vt == VT_BYREF + VT_VARIANT)
  {
    url = pDispParams->rgvarg[5].pvarVal->bstrVal;

    CPluginClient::UnescapeUrl(url);
  }
  else
  {
    // Wrong type, return.
    return;
  }

  // If webbrowser2 is equal to top level browser (as set in SetSite), we are navigating new page
  CPluginClient* client = CPluginClient::GetInstance();

  if (url.Find(L"javascript") == 0)
  {
  }
  else if (GetBrowser().IsEqualObject(WebBrowser2Ptr))
  {
    m_tab->OnNavigate(url);

    DEBUG_GENERAL(L"================================================================================\nBegin main navigation url:" + url + "\n================================================================================")

#ifdef ENABLE_DEBUG_RESULT
      CPluginDebug::DebugResultDomain(url);
#endif

    UpdateStatusBar();
  }
  else
  {
    DEBUG_NAVI(L"Navi::Begin navigation url:" + url)

#ifdef SUPPORT_FRAME_CACHING
      m_tab->CacheFrame(url);
#endif
  }
}
STDMETHODIMP CPluginClass::OnTabChanged(DISPPARAMS* pDispParams, WORD wFlags)
{
  DEBUG_GENERAL("Tab changed");
  bool newtabshown = pDispParams->rgvarg[1].intVal==3;
  if (newtabshown)
  {
    std::map<DWORD,CPluginClass*>::const_iterator it = s_threadInstances.find(GetCurrentThreadId());
    if (it == s_threadInstances.end())
    {
      s_threadInstances[::GetCurrentThreadId()] = this;


      if (!m_isInitializedOk)
      {
        m_isInitializedOk = true;
        if (!InitObject(true))
        {
          //					Unadvice();
        }
        UpdateStatusBar();
      }
    }
  }
  notificationMessage.Hide();
  DEBUG_GENERAL("Tab change end");
  return VARIANT_TRUE;
}

// This gets called whenever there's a browser event
STDMETHODIMP CPluginClass::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pvarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
  WCHAR tmp[256];
  wsprintf(tmp, L"Invoke: %d\n", dispidMember);
  DEBUG_GENERAL(tmp);
  switch (dispidMember)
  {

  case DISPID_WINDOWSTATECHANGED:
    return OnTabChanged(pDispParams, wFlags);
    break;
  case DISPID_HTMLDOCUMENTEVENTS2_ONBEFOREUPDATE:
    return VARIANT_TRUE;
    break;

  case DISPID_HTMLDOCUMENTEVENTS2_ONCLICK:
    return VARIANT_TRUE;
    break;

  case DISPID_EVMETH_ONLOAD:
    DEBUG_NAVI("Navi::OnLoad")
      return VARIANT_TRUE;
    break;

  case DISPID_EVMETH_ONCHANGE:
    return VARIANT_TRUE;

  case DISPID_EVMETH_ONMOUSEDOWN:
    return VARIANT_TRUE;

  case DISPID_EVMETH_ONMOUSEENTER:
    return VARIANT_TRUE;

  case DISPID_IHTMLIMGELEMENT_START:
    return VARIANT_TRUE;

  case STDDISPID_XOBJ_ERRORUPDATE:
    return VARIANT_TRUE;

  case STDDISPID_XOBJ_ONPROPERTYCHANGE:
    return VARIANT_TRUE;

  case DISPID_READYSTATECHANGE:
    DEBUG_NAVI("Navi::ReadyStateChange")
      return VARIANT_TRUE;

  case DISPID_BEFORENAVIGATE:
    DEBUG_NAVI("Navi::BeforeNavigate")
      return VARIANT_TRUE;
  case DISPID_COMMANDSTATECHANGE:
    if (m_hPaneWnd == NULL)
    {
      CreateStatusBarPane();
    }
    else
    {
      if (CPluginClient::GetInstance()->GetIEVersion() > 6)
      {
        RECT rect;
        BOOL rectRes = GetClientRect(m_hStatusBarWnd, &rect);
        if (rectRes == TRUE)
        {
          MoveWindow(m_hPaneWnd, rect.right - 200, 0, m_nPaneWidth, rect.bottom - rect.top, TRUE);
        }
      }      
    }
    break;
  case DISPID_STATUSTEXTCHANGE:
    break;

  case DISPID_BEFORENAVIGATE2:
    BeforeNavigate2(pDispParams);
    break;

  case DISPID_DOWNLOADBEGIN:
    {
      DEBUG_NAVI("Navi::Download Begin")
    }
    break;

  case DISPID_DOWNLOADCOMPLETE:
    {
      DEBUG_NAVI("Navi::Download Complete")

        CComQIPtr<IWebBrowser2> browser = GetBrowser();
      if (browser)
      {
        m_tab->OnDownloadComplete(browser);
      }
    }
    break;

  case DISPID_DOCUMENTCOMPLETE:
    {
      DEBUG_NAVI("Navi::Document Complete")

        CComQIPtr<IWebBrowser2> browser = GetBrowser();

      if (browser && pDispParams->cArgs >= 2 && pDispParams->rgvarg[1].vt == VT_DISPATCH)
      {
        CComQIPtr<IWebBrowser2> pBrowser = pDispParams->rgvarg[1].pdispVal;
        if (pBrowser)
        {
          CString url;
          CComBSTR bstrUrl;
          if (SUCCEEDED(pBrowser->get_LocationURL(&bstrUrl)) && ::SysStringLen(bstrUrl) > 0)
          {
            url = bstrUrl;

            CPluginClient::UnescapeUrl(url);

            m_tab->OnDocumentComplete(browser, url, browser.IsEqualObject(pBrowser));
          }
        }
      }
    }
    break;

  case DISPID_ONQUIT:
  case DISPID_QUIT:
    {
      Unadvice();
    }
    break;

  default:
    {
      CString did;
      did.Format(L"DispId:%u", dispidMember);

      DEBUG_NAVI(L"Navi::Default " + did)
    }

    // do nothing
    break;
  }

  return VARIANT_TRUE;
}

bool CPluginClass::InitObject(bool bBHO)
{
  DEBUG_GENERAL("InitObject");
  CPluginSettings* settings = CPluginSettings::GetInstance();

  if (!settings->GetPluginEnabled())
  {
    s_mimeFilter->Unregister();
  }

  // Load theme module
  s_criticalSectionLocal.Lock();
  {
    if (!s_hUxtheme)
    {
      s_hUxtheme = ::GetModuleHandle(_T("uxtheme.dll"));
      if (s_hUxtheme)
      {
        pfnClose = (CLOSETHEMEDATA)::GetProcAddress(s_hUxtheme, "CloseThemeData");
        if (!pfnClose)
        {
          DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_UXTHEME_CLOSE, "Class::InitObject - GetProcAddress(CloseThemeData)");
        }

        pfnDrawThemeBackground = (DRAWTHEMEBACKGROUND)::GetProcAddress(s_hUxtheme, "DrawThemeBackground");
        if (!pfnDrawThemeBackground)
        {
          DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_UXTHEME_DRAW_BACKGROUND, "Class::InitObject - GetProcAddress(DrawThemeBackground)");
        }

        pfnOpenThemeData = (OPENTHEMEDATA)::GetProcAddress(s_hUxtheme, "OpenThemeData");
        if (!pfnOpenThemeData)
        {
          DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_UXTHEME_OPEN, "Class::InitObject - GetProcAddress(pfnOpenThemeData)");
        }
      }
      else
      {
        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_UXTHEME, "Class::InitObject - GetModuleHandle(uxtheme.dll)");
      }
    }
  }
  s_criticalSectionLocal.Unlock();

  // Register pane class
  if (!GetAtomPaneClass())
  {
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = 0;
    wcex.lpfnWndProc = (WNDPROC)PaneWindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = _Module.m_hInst;
    wcex.hIcon = NULL;
    wcex.hCursor = NULL;
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = _T(STATUSBAR_PANE_NAME);
    wcex.hIconSm = NULL;

    s_criticalSectionLocal.Lock();
    {
      s_atomPaneClass = ::RegisterClassEx(&wcex);
    }
    s_criticalSectionLocal.Unlock();

    if (!GetAtomPaneClass())
    {
      DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_REGISTER_PANE_CLASS, "Class::InitObject - RegisterClassEx");
      return false;
    }
  }


  int ieVersion = CPluginClient::GetInstance()->GetIEVersion();
  // Create status pane
  if (bBHO && ieVersion > 6 && !CreateStatusBarPane())
  {
    return false;
  }
  
  if (CPluginClient::GetInstance()->IsFirstRun())
  {   
    // IE6 can't be accessed from another thread, execute in current thread
    if (ieVersion < 7)
    {
      FirstRunThread();
    }
    else
    {
      CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)CPluginClass::FirstRunThread, NULL, NULL, NULL);
    }
    if ((m_hPaneWnd == NULL) || (!IsStatusBarEnabled()))
    {
      ShowStatusBar();
    }

  }
  return true;
}

bool CPluginClass::CreateStatusBarPane()
{
  CriticalSection::Lock lock(m_csStatusBar);

  CPluginClient* client = CPluginClient::GetInstance();

  TCHAR szClassName[MAX_PATH];
  // Get browser window and url
  HWND hBrowserWnd = GetBrowserHWND();
  if (!hBrowserWnd)
  {
    DEBUG_ERROR_LOG(0, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_NO_STATUSBAR_BROWSER, "Class::CreateStatusBarPane - No status bar")
    return false;
  }

  // Looking for a TabWindowClass window in IE7
  // the last one should be parent for statusbar
  HWND hWndStatusBar = NULL;

  HWND hTabWnd = ::GetWindow(hBrowserWnd, GW_CHILD);
  UINT amoundOfNewTabs = 0;
  HWND uniqueNewTab = NULL;
  while (hTabWnd)
  {
    memset(szClassName, 0, MAX_PATH);
    GetClassName(hTabWnd, szClassName, MAX_PATH);

    if (_tcscmp(szClassName, _T("TabWindowClass")) == 0 || _tcscmp(szClassName,_T("Frame Tab")) == 0)
    {
      // IE8 support
      HWND hTabWnd2 = hTabWnd;
      if (_tcscmp(szClassName,_T("Frame Tab")) == 0)
      {
        hTabWnd2 = ::FindWindowEx(hTabWnd2, NULL, _T("TabWindowClass"), NULL);
      }

      if (hTabWnd2)
      {
        DWORD nProcessId;
        ::GetWindowThreadProcessId(hTabWnd2, &nProcessId);
        if (::GetCurrentProcessId() == nProcessId)
        {
          bool bExistingTab = false;

          s_criticalSectionLocal.Lock();
          {
            for (int i = 0; i < s_instances.GetSize(); i++)
            {
              if (s_instances[i]->m_hTabWnd == hTabWnd2)
              {

                bExistingTab = true;
                break;
              }
            }
          }
          s_criticalSectionLocal.Unlock();

          if (!bExistingTab)
          {
            amoundOfNewTabs ++;
            uniqueNewTab = hTabWnd2;
            if (GetCurrentThreadId() == GetWindowThreadProcessId(hTabWnd2, NULL))
            {
              hBrowserWnd = hTabWnd = hTabWnd2;
              break;
            }

          }
        }
      }
    }

    hTabWnd = ::GetWindow(hTabWnd, GW_HWNDNEXT);
  }

  HWND hWnd = ::GetWindow(hBrowserWnd, GW_CHILD);
  while (hWnd)
  {
    memset(szClassName, 0, MAX_PATH);
    ::GetClassName(hWnd, szClassName, MAX_PATH);

    if (_tcscmp(szClassName,_T("msctls_statusbar32")) == 0)
    {
      hWndStatusBar = hWnd;
      break;
    }

    hWnd = ::GetWindow(hWnd, GW_HWNDNEXT);
  }

  if (!hWndStatusBar)
  {
    DEBUG_ERROR_LOG(0, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_NO_STATUSBAR_WIN, "Class::CreateStatusBarPane - No status bar")
    return true;
  }

  // Calculate pane height
  CRect rcStatusBar;
  ::GetClientRect(hWndStatusBar, &rcStatusBar);

  if (rcStatusBar.Height() > 0)
  {
#ifdef _DEBUG
    m_nPaneWidth = 70;
#else
    m_nPaneWidth = min(rcStatusBar.Height(), 22);
#endif
  }
  else
  {
#ifdef _DEBUG
    m_nPaneWidth = 70;
#else
    m_nPaneWidth = 22;
#endif
  }
  // Create pane window
  HWND hWndNewPane = ::CreateWindowEx(
    NULL,
    MAKEINTATOM(GetAtomPaneClass()),
    _T(""),
    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
    rcStatusBar.Width() - 500,0,m_nPaneWidth,rcStatusBar.Height(),
    hWndStatusBar,
    (HMENU)3671,
    _Module.m_hInst,
    NULL);

  if (!hWndNewPane)
  {
    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_CREATE_STATUSBAR_PANE, "Class::CreateStatusBarPane - CreateWindowEx")
    return false;
  }

  DEBUG_GENERAL("ABP window created");
  m_hTabWnd = hTabWnd;
  m_hStatusBarWnd = hWndStatusBar;
  m_hPaneWnd = hWndNewPane;

  UpdateTheme();

  // Subclass status bar
  m_pWndProcStatus = (WNDPROC)SetWindowLongPtr(hWndStatusBar, GWLP_WNDPROC, (LPARAM)(WNDPROC)NewStatusProc);

  // Adjust pane
  LRESULT nPartCount = ::SendMessage(m_hStatusBarWnd, SB_GETPARTS, 0, 0);

  if (nPartCount > 1)
  {
    INT *pData = new INT[nPartCount];

    ::SendMessage(m_hStatusBarWnd, SB_GETPARTS, nPartCount, (LPARAM)pData);
    ::SendMessage(m_hStatusBarWnd, SB_SETPARTS, nPartCount, (LPARAM)pData);

    delete []pData;
  }
    
  HDC hdc = GetWindowDC(m_hStatusBarWnd);
  SendMessage(m_hStatusBarWnd, WM_PAINT, (WPARAM)hdc, 0);
  ReleaseDC(m_hStatusBarWnd, hdc);
  
  return true;
}

void CPluginClass::FirstRunThread()
{
  CoInitialize(NULL);
  VARIANT vFlags;
  vFlags.vt = VT_I4;
  vFlags.intVal = navOpenInNewTab;

  CComBSTR navigatePath = CComBSTR(FirstRunPageFileUrl().c_str());
  
  HRESULT hr = GetAsyncBrowser()->Navigate(navigatePath, &vFlags, NULL, NULL, NULL);
  if (FAILED(hr))
  {
    vFlags.intVal = navOpenInNewWindow;
    hr = GetAsyncBrowser()->Navigate(navigatePath, &vFlags, NULL, NULL, NULL);
  }

  if (FAILED(hr))
  {
    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_NAVIGATION, PLUGIN_ERROR_NAVIGATION_WELCOME, "Navigation::Welcome page failed")
  }
}
void CPluginClass::CloseTheme()
{
  if (m_hTheme)
  {
    if (pfnClose)
    {
      pfnClose(m_hTheme);
    }

    m_hTheme = NULL;
  }
}

void CPluginClass::UpdateTheme()
{
  CloseTheme();

  if (pfnOpenThemeData)
  {
    m_hTheme = pfnOpenThemeData(m_hPaneWnd, L"STATUS");
    if (!m_hTheme)
    {
    }
  }
}


CPluginClass* CPluginClass::FindInstance(HWND hStatusBarWnd)
{
  CPluginClass* instance = NULL;

  s_criticalSectionLocal.Lock();
  {
    for (int i = 0; i < s_instances.GetSize(); i++)
    {
      if (s_instances[i]->m_hStatusBarWnd == hStatusBarWnd)
      {
        instance = s_instances[i];
        break;
      }
    }
  }
  s_criticalSectionLocal.Unlock();

  return instance;
}

CPluginTab* CPluginClass::GetTab()
{
  return m_tab;
}

CPluginTab* CPluginClass::GetTab(DWORD dwThreadId)
{
  CPluginTab* tab = NULL;

  s_criticalSectionLocal.Lock();
  {
    std::map<DWORD,CPluginClass*>::const_iterator it = s_threadInstances.find(dwThreadId);
    if (it != s_threadInstances.end())
    {
      tab = it->second->m_tab;
    }
  }
  s_criticalSectionLocal.Unlock();

  return tab;
}


STDMETHODIMP CPluginClass::QueryStatus(const GUID* pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT* pCmdText)
{
  if (cCmds == 0) return E_INVALIDARG;
  if (prgCmds == 0) return E_POINTER;

  prgCmds[0].cmdf = OLECMDF_ENABLED;

  return S_OK;
}

HMENU CPluginClass::CreatePluginMenu(const CString& url)
{
  DEBUG_GENERAL("CreatePluginMenu");
  HINSTANCE hInstance = _AtlBaseModule.GetModuleInstance();

  HMENU hMenu = ::LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1));

  HMENU hMenuTrackPopup = GetSubMenu(hMenu, 0);

  SetMenuBar(hMenuTrackPopup, url);

  return hMenuTrackPopup;
}
BOOL CreateLowProcess(WCHAR* wszProcessName, WCHAR* cmdLine)
{

  BOOL                  fRet;
  HANDLE                hToken        = NULL;
  HANDLE                hNewToken     = NULL;
  PSID                  pIntegritySid = NULL;
  TOKEN_MANDATORY_LABEL TIL           = {0};
  PROCESS_INFORMATION   ProcInfo      = {0};
  STARTUPINFO           StartupInfo   = {0};



  // Low integrity SID
  WCHAR wszIntegritySid[20] = L"S-1-16-4096";


  fRet = OpenProcessToken(GetCurrentProcess(),
    TOKEN_DUPLICATE |
    TOKEN_ADJUST_DEFAULT |
    TOKEN_QUERY |
    TOKEN_ASSIGN_PRIMARY,
    &hToken);

  if (!fRet)
  {
    goto CleanExit;
  }

  fRet = DuplicateTokenEx(hToken,
    0,
    NULL,
    SecurityImpersonation,
    TokenPrimary,
    &hNewToken);

  if (!fRet)
  {
    goto CleanExit;
  }

  fRet = ConvertStringSidToSid(wszIntegritySid, &pIntegritySid);

  if (!fRet)
  {
    goto CleanExit;
  }


  TIL.Label.Attributes = SE_GROUP_INTEGRITY;
  TIL.Label.Sid        = pIntegritySid;


  //
  // Set the process integrity level
  //

  fRet = SetTokenInformation(hNewToken,
    TokenIntegrityLevel,
    &TIL,
    sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(pIntegritySid));

  if (!fRet)
  {
    goto CleanExit;
  }

  //
  // Create the new process at Low integrity
  //

  fRet  = CreateProcessAsUser(hNewToken,
    wszProcessName,
    cmdLine,
    NULL,
    NULL,
    FALSE,
    0,
    NULL,
    NULL,
    &StartupInfo,
    &ProcInfo);


CleanExit:

  if (ProcInfo.hProcess != NULL)
  {
    CloseHandle(ProcInfo.hProcess);
  }

  if (ProcInfo.hThread != NULL)
  {
    CloseHandle(ProcInfo.hThread);
  }

  LocalFree(pIntegritySid);

  if (hNewToken != NULL)
  {
    CloseHandle(hNewToken);
  }

  if (hToken != NULL)
  {
    CloseHandle(hToken);
  }

  return fRet;
}

void CPluginClass::DisplayPluginMenu(HMENU hMenu, int nToolbarCmdID, POINT pt, UINT nMenuFlags)
{
  CPluginClient* client = CPluginClient::GetInstance();

  CPluginSystem* system = CPluginSystem::GetInstance();

  // Create menu parent window
  HWND hMenuWnd = ::CreateWindowEx(
    NULL,
    MAKEINTATOM(GetAtomPaneClass()),
    _T(""),
    0,
    0,0,0,0,
    NULL,
    NULL,
    _Module.m_hInst,
    NULL);

  if (!hMenuWnd)
  {
    DestroyMenu(hMenu);
    return;
  }

  // Display menu
  nMenuFlags |= TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTBUTTON;

  int nCommand = ::TrackPopupMenu(hMenu, nMenuFlags, pt.x, pt.y, 0, hMenuWnd, 0);

  ::DestroyMenu(hMenu);
  ::DestroyWindow(hMenuWnd);

  switch (nCommand)
  {
  case ID_MENU_UPDATE:
    {
      CPluginClient* client = CPluginClient::GetInstance();
      notificationMessage.SetParent(m_hPaneWnd);
      Dictionary* dictionary = Dictionary::GetInstance();
      std::wstring checkingText = dictionary->Lookup("updater", "checking-for-updates-text");
      std::wstring checkingTitle = dictionary->Lookup("updater", "checking-for-updates-title");
      notificationMessage.Show(checkingText, checkingTitle, TTI_INFO);
      client->CheckForUpdates(m_hPaneWnd);
    }
    break;
  case ID_MENU_DISABLE:
    {
      CPluginSettings* settings = CPluginSettings::GetInstance();

      settings->TogglePluginEnabled();

      // Enable / disable mime filter
      s_criticalSectionLocal.Lock();
      {
        if (settings->GetPluginEnabled())
        {
          s_mimeFilter = CPluginClientFactory::GetMimeFilterClientInstance();
        }
        else
        {
          s_mimeFilter = NULL;

          CPluginClientFactory::ReleaseMimeFilterClientInstance();
        }
      }
      s_criticalSectionLocal.Unlock();
    }
    break;
  case ID_MENU_SETTINGS:
    {
      CComQIPtr<IWebBrowser2> browser = GetAsyncBrowser();
      if (browser)
      {
        VARIANT vFlags;
        vFlags.vt = VT_I4;
        vFlags.intVal = navOpenInNewTab;

        BSTR urlToNavigate = BString(UserSettingsFileUrl());
        HRESULT hr = browser->Navigate(urlToNavigate, &vFlags, NULL, NULL, NULL);
        if (FAILED(hr))
        {
          vFlags.intVal = navOpenInNewWindow;

          hr = browser->Navigate(urlToNavigate, &vFlags, NULL, NULL, NULL);
          if (FAILED(hr))
          {
            DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_NAVIGATION, PLUGIN_ERROR_NAVIGATION_SETTINGS, "Navigation::Failed")
          }
        }
      }
      break;
    }
  case ID_MENU_DISABLE_ON_SITE:
    {
      CPluginSettings* settings = CPluginSettings::GetInstance();
      CString urlString = GetTab()->GetDocumentUrl();
      if (client->IsWhitelistedUrl(std::wstring(urlString)))
      {
        settings->RemoveWhiteListedDomain(client->GetHostFromUrl(urlString.GetString()).c_str());
      }
      else
      {
        settings->AddWhiteListedDomain(client->GetHostFromUrl(urlString.GetString()).c_str());
      }
      GetBrowser()->Refresh();
    }
  default:
    break;
  }

  // Invalidate and redraw the control
  UpdateStatusBar();
}


bool CPluginClass::SetMenuBar(HMENU hMenu, const CString& url)
{
  DEBUG_GENERAL("SetMenuBar");

  std::wstring ctext;

  s_criticalSectionLocal.Lock();
  {
#ifdef SUPPORT_WHITELIST
    s_menuDomains.clear();
#endif
  }
  s_criticalSectionLocal.Unlock();


  Dictionary* dictionary = Dictionary::GetInstance();

  MENUITEMINFOW fmii;
  memset(&fmii, 0, sizeof(MENUITEMINFO));
  fmii.cbSize = sizeof(MENUITEMINFO);

  MENUITEMINFOW miiSep;
  memset(&miiSep, 0, sizeof(MENUITEMINFO));
  miiSep.cbSize = sizeof(MENUITEMINFO);
  miiSep.fMask = MIIM_TYPE | MIIM_FTYPE;
  miiSep.fType = MFT_SEPARATOR;

  CPluginClient* client = CPluginClient::GetInstance();

  CPluginSettings* settings = CPluginSettings::GetInstance();

#ifdef SUPPORT_WHITELIST
  {
    ctext = dictionary->Lookup("menu", "menu-disable-on-site");
    // Is domain in white list?
    ReplaceString(ctext, L"?1?", client->GetHostFromUrl(url.GetString()));
    if (client->IsWhitelistedUrl(std::wstring(GetTab()->GetDocumentUrl())))
    {
      fmii.fState = MFS_CHECKED | MFS_ENABLED;
    }
    else
    {
      fmii.fState = MFS_UNCHECKED | MFS_ENABLED;
    }
    fmii.fMask = MIIM_STRING | MIIM_STATE;
    fmii.dwTypeData = const_cast<LPWSTR>(ctext.c_str());
    fmii.cch = static_cast<UINT>(ctext.size());

    ::SetMenuItemInfoW(hMenu, ID_MENU_DISABLE_ON_SITE, FALSE, &fmii);
  }
#else
  {
    ::DeleteMenu(hMenu, ID_MENU_DISABLE_ON_SITE, FALSE);
  }
#endif // SUPPORT_WHITELIST

  // Plugin update
  ctext = dictionary->Lookup("menu", "menu-update");
  fmii.fMask  = MIIM_STATE | MIIM_STRING;
  fmii.fState = client ? MFS_ENABLED : MFS_DISABLED;
  fmii.dwTypeData = const_cast<LPWSTR>(ctext.c_str());
  fmii.cch = static_cast<UINT>(ctext.size());
  ::SetMenuItemInfoW(hMenu, ID_MENU_UPDATE, FALSE, &fmii);


  // Plugin enable
  ctext = dictionary->Lookup("menu", "menu-disable");
  if (settings->GetPluginEnabled())
  {
    fmii.fState = MFS_UNCHECKED | MFS_ENABLED;
  }
  else
  {
    fmii.fState = MFS_CHECKED | MFS_ENABLED;
  }
  fmii.fMask  = MIIM_STATE | MIIM_STRING;
  fmii.dwTypeData = const_cast<LPWSTR>(ctext.c_str());
  fmii.cch = static_cast<UINT>(ctext.size());
  ::SetMenuItemInfoW(hMenu, ID_MENU_DISABLE, FALSE, &fmii);

  // Settings
  ctext = dictionary->Lookup("menu", "menu-settings");
  fmii.fMask  = MIIM_STATE | MIIM_STRING;
  fmii.fState = MFS_ENABLED;
  fmii.dwTypeData = const_cast<LPWSTR>(ctext.c_str());
  fmii.cch = static_cast<UINT>(ctext.size());
  ::SetMenuItemInfoW(hMenu, ID_MENU_SETTINGS, FALSE, &fmii);

  return true;
}


STDMETHODIMP CPluginClass::Exec(const GUID*, DWORD nCmdID, DWORD, VARIANTARG*, VARIANTARG*)
{
  HWND hBrowserWnd = GetBrowserHWND();
  if (!hBrowserWnd)
  {
    return E_FAIL;
  }

  // Create menu
  HMENU hMenu = CreatePluginMenu(m_tab->GetDocumentUrl());
  if (!hMenu)
  {
    return E_FAIL;
  }

  // Check if button in toolbar was pressed
  int nIDCommand = -1;
  BOOL bRightAlign = FALSE;

  POINT pt;
  GetCursorPos(&pt);

  HWND hWndToolBar = ::WindowFromPoint(pt);

  DWORD nProcessId;
  ::GetWindowThreadProcessId(hWndToolBar, &nProcessId);

  if (hWndToolBar && ::GetCurrentProcessId() == nProcessId)
  {
    ::ScreenToClient(hWndToolBar, &pt);
    int nButton = (int)::SendMessage(hWndToolBar, TB_HITTEST, 0, (LPARAM)&pt);

    if (nButton > 0)
    {
      TBBUTTON pTBBtn;
      memset(&pTBBtn, 0, sizeof(TBBUTTON));

      if (SendMessage(hWndToolBar, TB_GETBUTTON, nButton, (LPARAM)&pTBBtn))
      {
        RECT rcButton;
        nIDCommand = pTBBtn.idCommand;

        if (SendMessage(hWndToolBar, TB_GETRECT, nIDCommand, (LPARAM)&rcButton))
        {
          pt.x = rcButton.left;
          pt.y = rcButton.bottom;
          ClientToScreen(hWndToolBar, &pt);

          RECT rcWorkArea;
          SystemParametersInfo(SPI_GETWORKAREA, 0, (LPVOID)&rcWorkArea, 0);
          if (rcWorkArea.right - pt.x < 150)
          {
            bRightAlign = TRUE;
            pt.x = rcButton.right;
            pt.y = rcButton.bottom;
            ClientToScreen(hWndToolBar, &pt);
          }
        }
      }
    }
    else
    {
      GetCursorPos(&pt);
    }
  }

  // Display menu
  UINT nFlags = 0;
  if (bRightAlign)
  {
    nFlags |= TPM_RIGHTALIGN;
  }
  else
  {
    nFlags |= TPM_LEFTALIGN;
  }

  DisplayPluginMenu(hMenu, nIDCommand, pt, nFlags);

  return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Window procedures

LRESULT CALLBACK CPluginClass::NewStatusProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  // Find tab
  CPluginClass *pClass = FindInstance(hWnd);
  if (!pClass)
  {
    return DefWindowProc(hWnd, message, wParam, lParam);
  }

  // Process message
  switch (message)
  {
  case SB_SIMPLE:
    {
      ShowWindow(pClass->m_hPaneWnd, !wParam);
      break;
    }

  case WM_SYSCOLORCHANGE:
    {
      pClass->UpdateTheme();
      break;
    }

  case SB_SETPARTS:
    {
      if (!lParam || !wParam || wParam > 30 || !IsWindow(pClass->m_hPaneWnd))
      {
        return CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, lParam);
      }

      WPARAM nParts = wParam;
      if (STATUSBAR_PANE_NUMBER >= nParts)
      {
        return CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, lParam);
      }

      HLOCAL hLocal = LocalAlloc(LHND, sizeof(int) * (nParts+1));
      LPINT lpParts = (LPINT)LocalLock(hLocal);
      memcpy(lpParts, (void*)lParam, wParam*sizeof(int));

      for (unsigned i = 0; i < STATUSBAR_PANE_NUMBER; i++)
      {
        lpParts[i] -= pClass->m_nPaneWidth;
      }
      LRESULT hRet = CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, (LPARAM)lpParts);

      CRect rcPane;
      ::SendMessage(hWnd, SB_GETRECT, STATUSBAR_PANE_NUMBER, (LPARAM)&rcPane);

      CRect rcClient;
      ::GetClientRect(hWnd, &rcClient);

      ::MoveWindow(
        pClass->m_hPaneWnd,
        lpParts[STATUSBAR_PANE_NUMBER] - pClass->m_nPaneWidth,
        0,
        pClass->m_nPaneWidth,
        rcClient.Height(),
        TRUE);

      ::LocalFree(hLocal);


      return hRet;
    }

  default:
    break;
  }

  LRESULT result = CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, lParam);


  return result;

}


HICON CPluginClass::GetStatusBarIcon(const CString& url)
{
  // use the disable icon as defualt, if the client doesn't exists
  HICON hIcon = GetIcon(ICON_PLUGIN_DEACTIVATED);

  CPluginTab* tab = GetTab(::GetCurrentThreadId());
  if (tab)
  {
    CPluginClient* client = CPluginClient::GetInstance();

#ifdef PRODUCT_ADBLOCKPLUS
    if (!CPluginSettings::GetInstance()->IsPluginEnabled())
    {
    }
#ifdef SUPPORT_WHITELIST
    else if (client->IsWhitelistedUrl(std::wstring(url)))
    {
      hIcon = GetIcon(ICON_PLUGIN_DISABLED);
    }
#endif // SUPPORT_WHITELIST
    else
    {
      CPluginSettings* settings = CPluginSettings::GetInstance();
      hIcon = GetIcon(ICON_PLUGIN_ENABLED);
    }

#endif // PRODUCT_ADBLOCKPLUS
  }

  return hIcon;
}


LRESULT CALLBACK CPluginClass::PaneWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  // Find tab
  CPluginClass *pClass = FindInstance(GetParent(hWnd));
  if (!pClass)
  {
    return ::DefWindowProc(hWnd, message, wParam, lParam);
  }

  CPluginSystem* system = CPluginSystem::GetInstance();

  // Process message
  switch (message)
  {

  case WM_SETCURSOR:
    {
      ::SetCursor(::LoadCursor(NULL, IDC_ARROW));
      return TRUE;
    }
  case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hDC = ::BeginPaint(hWnd, &ps);

      CRect rcClient;
      ::GetClientRect(hWnd, &rcClient);

      int nDrawEdge = 0;

      // Old Windows background drawing
      if (pClass->m_hTheme == NULL)
      {
        ::FillRect(hDC, &rcClient, (HBRUSH)(COLOR_BTNFACE + 1));
        ::DrawEdge(hDC, &rcClient, BDR_RAISEDINNER, BF_LEFT);

        nDrawEdge = 3;
        rcClient.left += 3;

        ::DrawEdge(hDC, &rcClient, BDR_SUNKENOUTER, BF_RECT);
      }
      // Themed background drawing
      else
      {
        // Draw background
        if (pfnDrawThemeBackground)
        {
          CRect rc = rcClient;
          rc.right -= 2;
          pfnDrawThemeBackground(pClass->m_hTheme, hDC, 0, 0, &rc, NULL);
        }

        // Copy separator picture to left side
        int nHeight = rcClient.Height();
        int nWidth = rcClient.Width() - 2;

        for (int i = 0; i < 2; i++)
        {
          for (int j = 0; j < nHeight; j++)
          {
            COLORREF clr = ::GetPixel(hDC, i + nWidth, j);

            // Ignore black boxes (if source is obscured by other windows)
            if (clr != -1 && (GetRValue(clr) > 8 || GetGValue(clr) > 8 || GetBValue(clr) > 8))
            {
              ::SetPixel(hDC, i, j, clr);
            }
          }
        }
      }

      // Draw icon
      if (CPluginClient::GetInstance())
      {
        HICON hIcon = GetStatusBarIcon(pClass->GetTab()->GetDocumentUrl());

        int offx = (rcClient.Height() - 16)/2 + nDrawEdge;
        if (hIcon)
        {
          ::DrawIconEx(hDC, offx, (rcClient.Height() - 16)/2 + 2, hIcon, 16, 16, NULL, NULL, DI_NORMAL);
          offx += 22;
        }
#ifdef _DEBUG
        // Display version
        HFONT hFont = (HFONT)::SendMessage(pClass->m_hStatusBarWnd, WM_GETFONT, 0, 0);
        HGDIOBJ hOldFont = ::SelectObject(hDC,hFont);

        CRect rcText = rcClient;
        rcText.left += offx;
        ::SetBkMode(hDC, TRANSPARENT);
        ::DrawTextW(hDC, IEPLUGIN_VERSION, -1, &rcText, DT_WORD_ELLIPSIS|DT_LEFT|DT_SINGLELINE|DT_VCENTER);

        ::SelectObject(hDC, hOldFont);
#endif // _DEBUG
      }

      // Done!
      EndPaint(hWnd, &ps);

      return 0;
    }

  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
    {
      CString strURL = pClass->GetBrowserUrl();
      if (strURL != pClass->GetTab()->GetDocumentUrl())
      {
        pClass->GetTab()->SetDocumentUrl(strURL);
      }

      // Create menu
      HMENU hMenu = pClass->CreatePluginMenu(strURL);
      if (!hMenu)
      {
        return 0;
      }

      // Display menu
      POINT pt;
      ::GetCursorPos(&pt);

      RECT rc;
      ::GetWindowRect(hWnd, &rc);

      if (rc.left >= 0 && rc.top >= 0)
      {
        pt.x = rc.left;
        pt.y = rc.top;
      }

      pClass->DisplayPluginMenu(hMenu, -1, pt, TPM_LEFTALIGN|TPM_BOTTOMALIGN);
    }
    break;
  case WM_DESTROY:
    break;
  case SC_CLOSE:
    break;

  case WM_UPDATEUISTATE:
    {
      CPluginTab* tab = GetTab(::GetCurrentThreadId());
      if (tab)
      {
        tab->OnActivate();
        RECT rect;
        GetWindowRect(pClass->m_hPaneWnd, &rect);
        pClass->notificationMessage.Move(rect.left + (rect.right - rect.left) / 2, rect.top + (rect.bottom - rect.top) / 2); 
      }
      if (LOWORD(wParam) == UIS_CLEAR)
      {
        pClass->notificationMessage.Hide();
      }
    }
    break;
  case WM_WINDOWPOSCHANGING:
    {
      RECT rect;
      GetWindowRect(pClass->m_hPaneWnd, &rect);
      if (pClass->notificationMessage.IsVisible())
      {
        pClass->notificationMessage.Move(rect.left + (rect.right - rect.left) / 2, rect.top + (rect.bottom - rect.top) / 2); 
      }
    }
    break;
  case WM_WINDOWPOSCHANGED:
    {
      WINDOWPOS* wndPos = reinterpret_cast<WINDOWPOS*>(lParam);
      if (wndPos->flags & SWP_HIDEWINDOW)
      {
        pClass->notificationMessage.Hide();
      }
    }
    break;
  case WM_ALREADY_UP_TO_DATE:
    {
      Dictionary* dictionary = Dictionary::GetInstance();
      std::wstring upToDateText = dictionary->Lookup("updater", "update-already-up-to-date-text");
      std::wstring upToDateTitle = dictionary->Lookup("updater", "update-already-up-to-date-title");
      pClass->notificationMessage.SetTextAndIcon(upToDateText, upToDateTitle, TTI_INFO);
    }
    break;
  case WM_UPDATE_CHECK_ERROR:
    {
      Dictionary* dictionary = Dictionary::GetInstance();
      std::wstring errorText = dictionary->Lookup("updater", "update-error-text");
      std::wstring errorTitle = dictionary->Lookup("updater", "update-error-title");
      pClass->notificationMessage.SetTextAndIcon(errorText, errorText, TTI_ERROR);
    }
    break;
  case WM_DOWNLOADING_UPDATE:
    {
      Dictionary* dictionary = Dictionary::GetInstance();
      std::wstring downloadingText = dictionary->Lookup("updater", "downloading-update-text");
      std::wstring downloadingTitle = dictionary->Lookup("updater", "downloading-update-title");
      pClass->notificationMessage.SetTextAndIcon(downloadingText, downloadingTitle, TTI_INFO);
    }
    break;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}


void CPluginClass::UpdateStatusBar()
{
  DEBUG_GENERAL("*** Updating statusbar")
  if (m_hPaneWnd == NULL)
  {
    CreateStatusBarPane();
  }
  if ((m_hPaneWnd != NULL) && !::InvalidateRect(m_hPaneWnd, NULL, FALSE))
  {
      DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_INVALIDATE_STATUSBAR, "Class::Invalidate statusbar");
  }
}


void CPluginClass::Unadvice()
{
  s_criticalSectionLocal.Lock();
  {
    if (m_isAdviced)
    {
      CComPtr<IConnectionPoint> pPoint = GetConnectionPoint();
      if (pPoint)
      {
        HRESULT hr = pPoint->Unadvise(m_nConnectionID);
        if (FAILED(hr))
        {
          DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_UNADVICE, "Class::Unadvice - Unadvise");
        }
      }

      m_isAdviced = false;
    }
  }
  s_criticalSectionLocal.Unlock();
}

HICON CPluginClass::GetIcon(int type)
{
  HICON icon = NULL;

  s_criticalSectionLocal.Lock();
  {
    if (!s_hIcons[type])
    {
      s_hIcons[type] = ::LoadIcon(_Module.m_hInst, MAKEINTRESOURCE(s_hIconTypes[type]));
      if (!s_hIcons[type])
      {
        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_LOAD_ICON, "Class::GetIcon - LoadIcon")
      }
    }

    icon = s_hIcons[type];
  }
  s_criticalSectionLocal.Unlock();

  return icon;
}

ATOM CPluginClass::GetAtomPaneClass()
{
  return s_atomPaneClass;
}

HWND CPluginClass::GetTabHWND() const
{

  TCHAR szClassName[MAX_PATH];
  // Get browser window and url
  HWND hBrowserWnd = GetBrowserHWND();
  if (!hBrowserWnd)
  {
    DEBUG_ERROR_LOG(0, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_NO_STATUSBAR_BROWSER, "Class::GetTabWindow - No tab window")
      s_criticalSectionWindow.Unlock();

    return false;
  }

  // Looking for a TabWindowClass window in IE7

  HWND hTabWnd = ::GetWindow(hBrowserWnd, GW_CHILD);
  while (hTabWnd)
  {
    memset(szClassName, 0, MAX_PATH);
    GetClassName(hTabWnd, szClassName, MAX_PATH);

    if (_tcscmp(szClassName, _T("TabWindowClass")) == 0 || _tcscmp(szClassName,_T("Frame Tab")) == 0)
    {
      // IE8 support
      HWND hTabWnd2 = hTabWnd;
      if (_tcscmp(szClassName,_T("Frame Tab")) == 0)
      {
        hTabWnd2 = ::FindWindowEx(hTabWnd2, NULL, _T("TabWindowClass"), NULL);
      }

      if (hTabWnd2)
      {
        DWORD nProcessId;
        ::GetWindowThreadProcessId(hTabWnd2, &nProcessId);
        if (::GetCurrentProcessId() == nProcessId)
        {
          bool bExistingTab = false;
          s_criticalSectionLocal.Lock();

          {
            for (int i = 0; i < s_instances.GetSize(); i++)
            {
              if (s_instances[i]->m_hTabWnd == hTabWnd2)
              {
                bExistingTab = true;
                break;
              }
            }
          }

          if (!bExistingTab)
          {
            hBrowserWnd = hTabWnd2;
            hTabWnd = hTabWnd2;
            s_criticalSectionLocal.Unlock();
            break;
          }
          s_criticalSectionLocal.Unlock();

        }
      }
    }

    hTabWnd = ::GetWindow(hTabWnd, GW_HWNDNEXT);
  }

  return hTabWnd;

}
