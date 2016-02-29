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

#include "PluginStdAfx.h"

#include "PluginClass.h"
#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginFilter.h"
#include "PluginMimeFilterClient.h"
#include "AdblockPlusClient.h"
#include "PluginClientBase.h"
#include "PluginClientFactory.h"
#include "PluginUtil.h"
#include "../shared/Utils.h"
#include "../shared/Dictionary.h"
#include "IeVersion.h"
#include "../shared/Version.h"
#include <thread>
#include <array>
#include "WebBrowserEventsListener.h"

#ifdef DEBUG_HIDE_EL
DWORD profileTime = 0;
#endif

extern CComModule _Module;

typedef HANDLE (WINAPI *OPENTHEMEDATA)(HWND, LPCWSTR);
typedef HRESULT (WINAPI *DRAWTHEMEBACKGROUND)(HANDLE, HDC, INT, INT, LPRECT, LPRECT);
typedef HRESULT (WINAPI *CLOSETHEMEDATA)(HANDLE);

HICON CPluginClass::s_hIcons[ICON_MAX] = { NULL, NULL, NULL };
DWORD CPluginClass::s_hIconTypes[ICON_MAX] = { IDI_ICON_DISABLED, IDI_ICON_ENABLED, IDI_ICON_DEACTIVATED };
uint32_t iconHeight = 32;
uint32_t iconWidth = 32;

CPluginMimeFilterClient* CPluginClass::s_mimeFilter = NULL;

CLOSETHEMEDATA pfnClose = NULL;
DRAWTHEMEBACKGROUND pfnDrawThemeBackground = NULL;
OPENTHEMEDATA pfnOpenThemeData = NULL;

ATOM CPluginClass::s_atomPaneClass = NULL;
HINSTANCE CPluginClass::s_hUxtheme = NULL;
std::set<CPluginClass*> CPluginClass::s_instances;
std::map<DWORD, CPluginClass*> CPluginClass::s_threadInstances;

CComAutoCriticalSection CPluginClass::s_criticalSectionLocal;
CComAutoCriticalSection CPluginClass::s_criticalSectionWindow;

CComQIPtr<IWebBrowser2> CPluginClass::s_asyncWebBrowser2;

/*
 * Without namespace declaration, the identifier "Rectangle" is ambiguous
 * See http://msdn.microsoft.com/en-us/library/windows/desktop/dd162898(v=vs.85).aspx
 */
namespace AdblockPlus
{
  /**
    * Replacement for ATL type CRect.
    */
  class Rectangle
    : public RECT
  {
  public:
    unsigned long Height() const
    {
      if (bottom < top)
      {
        throw std::runtime_error("invariant violation: rectangle bottom < top");
      }
      return static_cast<unsigned long>(bottom - top);
    }

    unsigned long Width() const
    {
      if (right < left)
      {
        throw std::runtime_error("invariant violation: rectangle right < left");
      }
      return static_cast<unsigned long>(right - left);
    }
  };
}

CPluginClass::CPluginClass()
  : m_data(std::make_shared<Data>())
{
  DEBUG_GENERAL([this]() -> std::wstring
    {
      std::wstring s = L"CPluginClass::<constructor>, this = ";
      s += ToHexLiteral(this);
      return s;
    }());

  //Use this line to debug memory leaks
  //	_CrtDumpMemoryLeaks();

  m_isAdvised = false;
  m_hTabWnd = NULL;
  m_hStatusBarWnd = NULL;
  m_hPaneWnd = NULL;
  m_nPaneWidth = 0;
  m_pWndProcStatus = NULL;
  m_hTheme = NULL;
  m_isInitializedOk = false;


  m_data->tab.reset(new CPluginTab());

  Dictionary::Create(GetBrowserLanguage());
}

CPluginClass::~CPluginClass()
{
  DEBUG_GENERAL([this]() -> std::wstring
    {
      std::wstring s = L"CPluginClass::<destructor>, this = ";
      s += ToHexLiteral(this);
      return s;
    }());

  m_data.reset();
}

HWND CPluginClass::GetBrowserHWND() const
{
  if (!m_data->webBrowser2)
  {
    DEBUG_ERROR_LOG(0, 0, 0, "CPluginClass::GetBrowserHWND - Reached with webBrowser2 == nullptr");
    return nullptr;
  }
  SHANDLE_PTR hBrowserWndHandle = 0;
  HRESULT hr = m_data->webBrowser2->get_HWND(&hBrowserWndHandle);
  if (FAILED(hr))
  {
    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_BROWSER_WINDOW, "Class::GetBrowserHWND - failed");
    return nullptr;
  }
  return (HWND)hBrowserWndHandle;
}

bool CPluginClass::IsRootBrowser(IWebBrowser2* otherBrowser)
{
  return m_data->webBrowser2.IsEqualObject(otherBrowser);
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

std::wstring CPluginClass::GetBrowserUrl() const
{
  std::wstring url;
  if (m_data->webBrowser2)
  {
    CComBSTR bstrURL;
    if (SUCCEEDED(m_data->webBrowser2->get_LocationURL(&bstrURL)))
    {
      url = ToWstring(bstrURL);
    }
  }
  else
  {
    DEBUG_GENERAL(L"CPluginClass::GetBrowserUrl - Reached with webBrowser2 == nullptr (probable invariant violation)");
  }
  if (url.empty())
  {
    url = m_data->tab->GetDocumentUrl();
  }
  return url;
}

DWORD WINAPI CPluginClass::StartInitObject(LPVOID thisPtr)
{
  if (thisPtr == NULL)
    return 0;
  if (!((CPluginClass*)thisPtr)->InitObject())
  {
    ((CPluginClass*)thisPtr)->Unadvise();
  }

  return 0;
}

/*
 * IE calls this when it creates a new browser window or tab, immediately after it also 
 * creates the object. The argument 'unknownSite' in is the OLE "site" of the object, 
 * which is an IWebBrowser2 interface associated with the window/tab. 
 *
 * IE also ordinarily calls this again when its window/tab is closed, in which case
 * 'unknownSite' will be null. Extraordinarily, this is sometimes _not_ called when IE
 * is shutting down. Thus 'SetSite(nullptr)' has some similarities with a destructor,
 * but it is not a proper substitute for one.
 */
STDMETHODIMP CPluginClass::SetSite(IUnknown* unknownSite)
{
  try
  {
    if (unknownSite)
    {
      DEBUG_GENERAL(L"================================================================================\nNEW TAB UI\n================================================================================");

      HRESULT hr = ::CoInitialize(NULL);
      if (FAILED(hr))
      {
        DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_COINIT, "Class::SetSite - CoInitialize");
      }

      /*
       * We were instantiated as a BHO, so our site is always of type IWebBrowser2.
       */
      m_data->webBrowser2 = ATL::CComQIPtr<IWebBrowser2>(unknownSite);
      if (!m_data->webBrowser2)
      {
        throw std::logic_error("CPluginClass::SetSite - Unable to convert site pointer to IWebBrowser2*");
      }
      DEBUG_GENERAL([this]() -> std::wstring
        {
          std::wstringstream ss;
          ss << L"CPluginClass::SetSite, this = " << ToHexLiteral(this);
          ss << L", browser = " << ToHexLiteral(m_data->webBrowser2);
          return ss.str();
        }());

      //register the mimefilter
      //and only mimefilter
      //on some few computers the mimefilter does not get properly registered when it is done on another thread
      s_criticalSectionLocal.Lock();
      {
        // Always register on startup, then check if we need to unregister in a separate thread
        s_mimeFilter = CPluginClientFactory::GetMimeFilterClientInstance();
        s_asyncWebBrowser2 = unknownSite;
        s_instances.insert(this);
      }
      s_criticalSectionLocal.Unlock();

      try
      {
        HRESULT hr = DispEventAdvise(m_data->webBrowser2);
        if (SUCCEEDED(hr))
        {
          m_isAdvised = true;
          try
          {
            std::thread startInitObjectThread(StartInitObject, this);
            startInitObjectThread.detach(); // TODO: but actually we should wait for the thread in the dtr.
          }
          catch (const std::system_error& ex)
          {
            DEBUG_SYSTEM_EXCEPTION(ex, PLUGIN_ERROR_THREAD, PLUGIN_ERROR_MAIN_THREAD_CREATE_PROCESS, 
              "Class::Thread - Failed to create StartInitObject thread");
          }
        }
        else
        {
          DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_ADVICE, "Class::SetSite - Advise");
        }
      }
      catch (const std::runtime_error& ex)
      {
        DEBUG_EXCEPTION(ex);
        Unadvise();
      }
    }
    else
    {
      DEBUG_GENERAL([this]() -> std::wstring
      {
        std::wstringstream ss;
        ss << L"CPluginClass::SetSite, this = " << ToHexLiteral(this);
        ss << L", browser = nullptr";
        return ss.str();
      }());

      Unadvise();

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
        s_instances.erase(this);

        std::map<DWORD,CPluginClass*>::iterator it = s_threadInstances.find(::GetCurrentThreadId());
        if (it != s_threadInstances.end())
        {
          s_threadInstances.erase(it);
        }
        if (s_instances.empty())
        {
          // TODO: Explicitly releasing a resource when a container becomes empty looks like a job better suited for shared_ptr
          CPluginClientFactory::ReleaseMimeFilterClientInstance();
        }
      }
      s_criticalSectionLocal.Unlock();

      m_data->webBrowser2 = nullptr;

      DEBUG_GENERAL("================================================================================\nNEW TAB UI - END\n================================================================================")

      ::CoUninitialize();
    }

  }
  catch (...)
  {
  }
  IObjectWithSiteImpl<CPluginClass>::SetSite(unknownSite);
  return S_OK;
}

bool CPluginClass::IsStatusBarEnabled()
{
  DEBUG_GENERAL("IsStatusBarEnabled start");
  HKEY pHkey;
  HKEY pHkeySub;
  RegOpenCurrentUser(KEY_QUERY_VALUE, &pHkey);
  DWORD truth = 1;
  DWORD truthSize = sizeof(truth);
  RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\Main", &pHkeySub);
  LONG res = RegQueryValueEx(pHkeySub, L"StatusBarWeb", NULL, NULL, (BYTE*)&truth, &truthSize);
  RegCloseKey(pHkey);
  if (res != ERROR_SUCCESS)
  {
    res = RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\MINIE", &pHkeySub);
    if (res == ERROR_SUCCESS)
    {
      LONG res = RegQueryValueEx(pHkeySub, L"ShowStatusBar", NULL, NULL, (BYTE*)&truth, &truthSize);
      if (res == ERROR_SUCCESS)
      {
        RegCloseKey(pHkey);
      }
    }
  }
  DEBUG_GENERAL("IsStatusBarEnabled end");
  return truth == 1;
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
          DWORD truth = 1;
          regRes = RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\MINIE", &pHkeySub);
          regRes = RegSetValueEx(pHkeySub, L"ShowStatusBar", 0, REG_DWORD, (BYTE*)&truth, sizeof(truth));
          regRes = RegCloseKey(pHkeySub);
          regRes = RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\Main", &pHkeySub);
          regRes = RegSetValueEx(pHkeySub, L"StatusBarWeb", 0, REG_DWORD, (BYTE*)&truth, sizeof(truth));
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

// Entry point
void STDMETHODCALLTYPE CPluginClass::OnBeforeNavigate2(
  IDispatch* frameBrowserDisp /**< [in] */,
  VARIANT* urlVariant /**< [in] */,
  VARIANT* /**< [in] Flags*/,
  VARIANT* /**< [in] TargetFrameName*/,
  VARIANT* /**< [in] PostData*/,
  VARIANT* /**< [in] Headers*/,
  VARIANT_BOOL* /**< [in, out] Cancel*/)
{
  try
  {
    ATL::CComQIPtr<IWebBrowser2> webBrowser = frameBrowserDisp;
    if (!webBrowser)
    {
      return;
    }
    if (!urlVariant || urlVariant->vt != VT_BSTR)
    {
      return;
    }
    std::wstring url = ToWstring(urlVariant->bstrVal);
    EnsureWebBrowserConnected(webBrowser);

    // If webbrowser2 is equal to top level browser (as set in SetSite), we are
    // navigating new page
    CPluginClient* client = CPluginClient::GetInstance();
    if (url.find(L"javascript") == 0)
    {
    }
    else if (IsRootBrowser(webBrowser))
    {
      m_data->tab->OnNavigate(url);
      DEBUG_GENERAL(
      L"================================================================================\n"
      L"Begin main navigation url:" + url + L"\n"
      L"================================================================================")

#ifdef ENABLE_DEBUG_RESULT
      CPluginDebug::DebugResultDomain(url);
#endif
      UpdateStatusBar();
    }
    else
    {
      DEBUG_NAVI(L"Navi::Begin navigation url:" + url)
      m_data->tab->CacheFrame(url);
    }
  }
  catch (...)
  {
  }
}

// Entry point
void STDMETHODCALLTYPE CPluginClass::OnDownloadComplete()
{
  try
  {
    if (!m_data->webBrowser2)
    {
      DEBUG_ERROR_LOG(0, 0, 0, "CPluginClass::OnDownloadComplete - Reached with webBrowser2 == nullptr");
      return;
    }
    DEBUG_NAVI(L"Navi::Download Complete")
      m_data->tab->OnDownloadComplete(m_data->webBrowser2);
  }
  catch (...)
  {
  }
}

// Entry point
void STDMETHODCALLTYPE CPluginClass::OnWindowStateChanged(unsigned long flags, unsigned long validFlagsMask)
{
  try
  {
    DEBUG_GENERAL(L"WindowStateChanged (check tab changed)");
    bool newtabshown = validFlagsMask == (OLECMDIDF_WINDOWSTATE_USERVISIBLE | OLECMDIDF_WINDOWSTATE_ENABLED)
      && flags == (OLECMDIDF_WINDOWSTATE_USERVISIBLE | OLECMDIDF_WINDOWSTATE_ENABLED);
    if (newtabshown)
    {
      std::map<DWORD,CPluginClass*>::const_iterator it = s_threadInstances.find(GetCurrentThreadId());
      if (it == s_threadInstances.end())
      {
        s_threadInstances[::GetCurrentThreadId()] = this;
        if (!m_isInitializedOk)
        {
          m_isInitializedOk = true;
          InitObject();
          UpdateStatusBar();
        }
      }
    }
    notificationMessage.Hide();
    DEBUG_GENERAL(L"WindowStateChanged (check tab changed) end");
  }
  catch (...)
  {
  }
}

// Entry point
void STDMETHODCALLTYPE CPluginClass::OnCommandStateChange(long /*command*/, VARIANT_BOOL /*enable*/)
{
  try
  {
    if (m_hPaneWnd == NULL)
    {
      CreateStatusBarPane();
    }
    else
    {
      if (AdblockPlus::IE::InstalledMajorVersion() > 6)
      {
        RECT rect;
        //Get the RECT for the leftmost pane (the status text pane)
        BOOL rectRes = ::SendMessage(m_hStatusBarWnd, SB_GETRECT, 0, (LPARAM)&rect);
        if (rectRes == TRUE)
        {
          MoveWindow(m_hPaneWnd, rect.right - m_nPaneWidth, 0, m_nPaneWidth, rect.bottom - rect.top, TRUE);
        }
      }
    }
  }
  catch (...)
  {
  }
}

// Entry point
void STDMETHODCALLTYPE CPluginClass::OnOnQuit()
{
  try
  {
    Unadvise();
  }
  catch (...)
  {
  }
}

bool CPluginClass::InitObject()
{
  DEBUG_GENERAL("InitObject - begin");
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
      s_hUxtheme = ::GetModuleHandle(L"uxtheme.dll");
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

    wcex.cbSize = sizeof(wcex);
    wcex.style = 0;
    wcex.lpfnWndProc = (WNDPROC)PaneWindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = _Module.m_hInst;
    wcex.hIcon = NULL;
    wcex.hCursor = NULL;
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = STATUSBAR_PANE_NAME;
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

  int ieVersion = AdblockPlus::IE::InstalledMajorVersion();
  // Create status pane
  if (ieVersion > 6 && !CreateStatusBarPane())
  {
    return false;
  }
  
  s_criticalSectionLocal.Lock();
  int versionCompRes = CPluginClient::GetInstance()->CompareVersions(CPluginClient::GetInstance()->GetPref(L"currentVersion", L"0.0"), L"1.2");

  bool isFirstRun = CPluginClient::GetInstance()->IsFirstRun();
  CPluginClient::GetInstance()->SetPref(L"currentVersion", std::wstring(IEPLUGIN_VERSION));
  // This is the first time ABP was installed
  // Or ABP was updated from the version that did not support Acceptable Ads (<1.2)
  if (isFirstRun || versionCompRes < 0)
  {   
    if (!isFirstRun)
    {
      CPluginClient::GetInstance()->SetPref(L"displayUpdatePage", true);
    }

    // IE6 can't be accessed from another thread, execute in current thread
    if (ieVersion < 7)
    {
      FirstRunThread();
    }
    else
    {
      CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)CPluginClass::FirstRunThread, NULL, NULL, NULL);
    }
    if (((m_hPaneWnd == NULL) || !IsStatusBarEnabled()) && isFirstRun)
    {
      ShowStatusBar();
    }

    // Enable acceptable ads by default
    std::wstring aaUrl = CPluginClient::GetInstance()->GetPref(L"subscriptions_exceptionsurl", L"");
    CPluginClient::GetInstance()->AddSubscription(aaUrl);
  }
  s_criticalSectionLocal.Unlock();

  DEBUG_GENERAL("InitObject - end");
  return true;
}

bool CPluginClass::CreateStatusBarPane()
{
  CriticalSection::Lock lock(m_csStatusBar);

  CPluginClient* client = CPluginClient::GetInstance();

  std::array<wchar_t, MAX_PATH> className;
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
    className[0] = L'\0';
    // GetClassNameW returns the number of characters without '\0'
    int classNameLength = GetClassNameW(hTabWnd, className.data(), className.size());

    if (classNameLength && (wcscmp(className.data(), L"TabWindowClass") == 0 || wcscmp(className.data(), L"Frame Tab") == 0))
    {
      // IE8 support
      HWND hTabWnd2 = hTabWnd;
      if (wcscmp(className.data(), L"Frame Tab") == 0)
      {
        hTabWnd2 = ::FindWindowEx(hTabWnd2, NULL, L"TabWindowClass", NULL);
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
            for (auto instance : s_instances)
            {
              if (instance->m_hTabWnd == hTabWnd2)
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
    className[0] = L'\0';
    int classNameLength = GetClassNameW(hWnd, className.data(), className.size());

    if (classNameLength && wcscmp(className.data(), L"msctls_statusbar32") == 0)
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
  AdblockPlus::Rectangle rcStatusBar;
  ::GetClientRect(hWndStatusBar, &rcStatusBar);

  if (rcStatusBar.Height() > 0)
  {
    if (rcStatusBar.Height() < iconWidth)
    { 
      iconWidth = 19;
      iconHeight = 19;
    }

#ifdef _DEBUG
    m_nPaneWidth = 70;
#else
    m_nPaneWidth = min(rcStatusBar.Height(), iconWidth);
#endif
  }
  else
  {
#ifdef _DEBUG
    m_nPaneWidth = 70;
#else
    m_nPaneWidth = iconWidth;
#endif
  }
  // Create pane window
  HWND hWndNewPane = ::CreateWindowEx(
    NULL,
    MAKEINTATOM(GetAtomPaneClass()),
    L"",
    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
    rcStatusBar.Width() - 500, 0, m_nPaneWidth, rcStatusBar.Height(),
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
  // Just return if the First Run Page should be suppressed
  if (CPluginClient::GetInstance()->GetPref(L"suppress_first_run_page", false))
    return;

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
  CPluginClass* result = nullptr;

  s_criticalSectionLocal.Lock();
  {
    for (auto instance : s_instances)
    {
      if (instance->m_hStatusBarWnd == hStatusBarWnd)
      {
        result = instance;
        break;
      }
    }
  }
  s_criticalSectionLocal.Unlock();

  return result;
}

CPluginTab* CPluginClass::GetTab()
{
  return m_data->tab.get();
}

CPluginTab* CPluginClass::GetTab(DWORD dwThreadId)
{
  CPluginTab* tab = NULL;

  s_criticalSectionLocal.Lock();
  {
    std::map<DWORD,CPluginClass*>::const_iterator it = s_threadInstances.find(dwThreadId);
    if (it != s_threadInstances.end())
    {
      tab = it->second->m_data->tab.get();
    }
  }
  s_criticalSectionLocal.Unlock();

  return tab;
}

// Entry point
STDMETHODIMP CPluginClass::QueryStatus(const GUID* pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT* pCmdText)
{
  try
  {
    if (cCmds == 0) return E_INVALIDARG;
    if (prgCmds == 0) return E_POINTER;

    prgCmds[0].cmdf = OLECMDF_ENABLED;
  }
  catch (...)
  {
    DEBUG_GENERAL(L"CPluginClass::QueryStatus - exception");
    return E_FAIL;
  }
  return S_OK;
}

HMENU CPluginClass::CreatePluginMenu(const std::wstring& url)
{
  DEBUG_GENERAL("CreatePluginMenu");
  HINSTANCE hInstance = _AtlBaseModule.GetModuleInstance();

  HMENU hMenu = ::LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1));

  HMENU hMenuTrackPopup = GetSubMenu(hMenu, 0);

  SetMenuBar(hMenuTrackPopup, url);

  return hMenuTrackPopup;
}

void CPluginClass::DisplayPluginMenu(HMENU hMenu, int nToolbarCmdID, POINT pt, UINT nMenuFlags)
{
  CPluginClient* client = CPluginClient::GetInstance();

  // Create menu parent window
  HWND hMenuWnd = ::CreateWindowEx(
    NULL,
    MAKEINTATOM(GetAtomPaneClass()),
    L"",
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

        auto userSettingsFileUrl = UserSettingsFileUrl();
        ATL::CComBSTR urlToNavigate(static_cast<int>(userSettingsFileUrl.length()), userSettingsFileUrl.c_str());
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
      std::wstring urlString = GetTab()->GetDocumentUrl();
      std::string filterText = client->GetWhitelistingFilter(urlString);
      if (!filterText.empty())
      {
        client->RemoveFilter(filterText);
      }
      else
      {
        CPluginSettings::GetInstance()->AddWhiteListedDomain(client->GetHostFromUrl(urlString));
      }
    }
  default:
    break;
  }

  // Invalidate and redraw the control
  UpdateStatusBar();
}


bool CPluginClass::SetMenuBar(HMENU hMenu, const std::wstring& url)
{
  DEBUG_GENERAL("SetMenuBar");

  std::wstring ctext;
  Dictionary* dictionary = Dictionary::GetInstance();

  MENUITEMINFOW fmii = {};
  fmii.cbSize = sizeof(fmii);

  MENUITEMINFOW miiSep = {};
  miiSep.cbSize = sizeof(miiSep);
  miiSep.fMask = MIIM_TYPE | MIIM_FTYPE;
  miiSep.fType = MFT_SEPARATOR;

  CPluginClient* client = CPluginClient::GetInstance();
  CPluginSettings* settings = CPluginSettings::GetInstance();
  {
    ctext = dictionary->Lookup("menu", "menu-disable-on-site");
    ReplaceString(ctext, L"?1?", client->GetHostFromUrl(url));
    /*
     * The display state of the "disable on this site" menu item depends upon tab content
     */
    if (!GetTab()->IsPossibleToDisableOnSite())
    {
      // Since we can't disable the present content,
      // it makes no sense to offer the user an option to block it.
      fmii.fState = MFS_UNCHECKED | MFS_DISABLED;
    }
    else if (client->IsWhitelistedUrl(GetTab()->GetDocumentUrl()))
    {
      // Domain is in white list, indicated by a check mark
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

// Entry point
STDMETHODIMP CPluginClass::Exec(const GUID*, DWORD nCmdID, DWORD, VARIANTARG*, VARIANTARG*)
{
  try
  {
    HWND hBrowserWnd = GetBrowserHWND();
    if (!hBrowserWnd)
    {
      return E_FAIL;
    }

    // Create menu
    HMENU hMenu = CreatePluginMenu(m_data->tab->GetDocumentUrl());
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
        TBBUTTON pTBBtn = {};

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
  }
  catch (...)
  {
    // Suppress exception, log only
    DEBUG_GENERAL(L"CPluginClass::Exec - exception");
    return E_FAIL;
  }

  return S_OK;
}

// Entry point
LRESULT CALLBACK CPluginClass::NewStatusProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CPluginClass *pClass;
  try
  {
    // Find tab
    pClass = FindInstance(hWnd);
    if (!pClass)
    {
      /*
       * Race condition if reached.
       * We did not unhook the window procedure for the status bar when the last BHO instance using it terminated.
       * The next best thing is to call the system default window function.
       */
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

        HLOCAL hLocal = LocalAlloc(LHND, sizeof(int) * (nParts + 1));
        LPINT lpParts = (LPINT)LocalLock(hLocal);
        memcpy(lpParts, (void*)lParam, wParam*sizeof(int));

        for (unsigned i = 0; i < STATUSBAR_PANE_NUMBER; i++)
        {
          lpParts[i] -= pClass->m_nPaneWidth;
        }
        LRESULT hRet = CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, (LPARAM)lpParts);

        AdblockPlus::Rectangle rcPane;
        ::SendMessage(hWnd, SB_GETRECT, STATUSBAR_PANE_NUMBER, (LPARAM)&rcPane);

        AdblockPlus::Rectangle rcClient;
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
  }
  catch (...)
  {
    // Suppress exception. Fall through to default handler.
    DEBUG_GENERAL(L"CPluginClass::NewStatusProc - exception");
  }
  return ::CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, lParam);
}


HICON CPluginClass::GetStatusBarIcon(const std::wstring& url)
{
  // use the disable icon as defualt, if the client doesn't exists
  HICON hIcon = GetIcon(ICON_PLUGIN_DEACTIVATED);

  CPluginTab* tab = GetTab(::GetCurrentThreadId());
  if (tab)
  {
    CPluginClient* client = CPluginClient::GetInstance();
    if (CPluginSettings::GetInstance()->IsPluginEnabled())
    {
      if (client->IsWhitelistedUrl(url))
      {
        hIcon = GetIcon(ICON_PLUGIN_DISABLED);
      }
      else
      {
        CPluginSettings* settings = CPluginSettings::GetInstance();
        hIcon = GetIcon(ICON_PLUGIN_ENABLED);
      }
    }
  }
  return hIcon;
}

// Entry point
LRESULT CALLBACK CPluginClass::PaneWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  try
  {
    // Find tab
    CPluginClass *pClass = FindInstance(GetParent(hWnd));
    if (!pClass)
    {
      return ::DefWindowProc(hWnd, message, wParam, lParam);
    }

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

        AdblockPlus::Rectangle rcClient;
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
            AdblockPlus::Rectangle rc = rcClient;
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

          int offx = nDrawEdge;
          if (hIcon)
          {
            //Get the RECT for the leftmost pane (the status text pane)
            RECT rect;
            BOOL rectRes = ::SendMessage(pClass->m_hStatusBarWnd, SB_GETRECT, 0, (LPARAM)&rect);
            ::DrawIconEx(hDC, 0, rect.bottom - rect.top - iconHeight, hIcon, iconWidth, iconHeight, NULL, NULL, DI_NORMAL);
            offx += iconWidth;
          }
#ifdef _DEBUG
          // Display version
          HFONT hFont = (HFONT)::SendMessage(pClass->m_hStatusBarWnd, WM_GETFONT, 0, 0);
          HGDIOBJ hOldFont = ::SelectObject(hDC, hFont);

          AdblockPlus::Rectangle rcText = rcClient;
          rcText.left += offx;
          ::SetBkMode(hDC, TRANSPARENT);
          ::DrawTextW(hDC, IEPLUGIN_VERSION, -1, &rcText, DT_WORD_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER);

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
        std::wstring url = pClass->GetBrowserUrl();
        if (url != pClass->GetTab()->GetDocumentUrl())
        {
          pClass->GetTab()->SetDocumentUrl(url);
        }

        // Create menu
        HMENU hMenu = pClass->CreatePluginMenu(url);
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

        pClass->DisplayPluginMenu(hMenu, -1, pt, TPM_LEFTALIGN | TPM_BOTTOMALIGN);
        break;
      }
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
          pClass->notificationMessage.MoveToCenter(rect);
        }
        if (LOWORD(wParam) == UIS_CLEAR)
        {
          pClass->notificationMessage.Hide();
        }
        break;
      }
    case WM_WINDOWPOSCHANGING:
      {
        RECT rect;
        GetWindowRect(pClass->m_hPaneWnd, &rect);
        if (pClass->notificationMessage.IsVisible())
        {
          pClass->notificationMessage.MoveToCenter(rect);
        }
        break;
      }
    case WM_WINDOWPOSCHANGED:
      {
        WINDOWPOS* wndPos = reinterpret_cast<WINDOWPOS*>(lParam);
        if (wndPos->flags & SWP_HIDEWINDOW)
        {
          pClass->notificationMessage.Hide();
        }
        break;
      }
    case WM_ALREADY_UP_TO_DATE:
      {
        Dictionary* dictionary = Dictionary::GetInstance();
        std::wstring upToDateText = dictionary->Lookup("updater", "update-already-up-to-date-text");
        std::wstring upToDateTitle = dictionary->Lookup("updater", "update-already-up-to-date-title");
        pClass->notificationMessage.SetTextAndIcon(upToDateText, upToDateTitle, TTI_INFO);
        break;
      }
    case WM_UPDATE_CHECK_ERROR:
      {
        Dictionary* dictionary = Dictionary::GetInstance();
        std::wstring errorText = dictionary->Lookup("updater", "update-error-text");
        std::wstring errorTitle = dictionary->Lookup("updater", "update-error-title");
        pClass->notificationMessage.SetTextAndIcon(errorText, errorTitle, TTI_ERROR);
        break;
      }
    case WM_DOWNLOADING_UPDATE:
      {
        Dictionary* dictionary = Dictionary::GetInstance();
        std::wstring downloadingText = dictionary->Lookup("updater", "downloading-update-text");
        std::wstring downloadingTitle = dictionary->Lookup("updater", "downloading-update-title");
        pClass->notificationMessage.SetTextAndIcon(downloadingText, downloadingTitle, TTI_INFO);
        break;
      }
    }
  }
  catch (...)
  {
    // Suppress exception. Fall through to default handler.
    DEBUG_GENERAL(L"CPluginClass::PaneWindowProc - exception");
  }
  return ::DefWindowProc(hWnd, message, wParam, lParam);
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

void CPluginClass::Unadvise()
{
  if (!m_data->webBrowser2)
  {
    DEBUG_ERROR_LOG(0, 0, 0, "CPluginClass::Unadvise - Reached with webBrowser2 == nullptr");
    return;
  }
  s_criticalSectionLocal.Lock();
  {
    if (m_isAdvised)
    {
      HRESULT hr = DispEventUnadvise(m_data->webBrowser2);
      if (FAILED(hr))
      {
        DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_UNADVISE, "Class::Unadvise - Unadvise");
      }
      m_isAdvised = false;
    }
  }
  s_criticalSectionLocal.Unlock();
}

void CPluginClass::EnsureWebBrowserConnected(const ATL::CComPtr<IWebBrowser2>& webBrowser)
{
  auto it = m_data->connectedWebBrowsersCache.find(webBrowser);
  if (it != m_data->connectedWebBrowsersCache.end())
  {
    return;
  }
  ATL::CComObject<WebBrowserEventsListener>* listenerImpl = nullptr;
  if (FAILED(ATL::CComObject<WebBrowserEventsListener>::CreateInstance(&listenerImpl)))
  {
    return;
  }
  ATL::CComPtr<IUnknown> listenerRefCounterGuard(listenerImpl->GetUnknown());
  std::weak_ptr<Data> dataForCapturing = m_data;
  auto onListenerDestroy = [webBrowser, dataForCapturing]
  {
    if (auto data = dataForCapturing.lock())
    {
      data->connectedWebBrowsersCache.erase(webBrowser);
    }
  };
  auto onReloaded = [webBrowser, dataForCapturing]
  {
    if (auto data = dataForCapturing.lock())
    {
      auto frameSrc = GetLocationUrl(*webBrowser);
      data->tab->OnDocumentComplete(webBrowser, frameSrc, data->webBrowser2.IsEqualObject(webBrowser));
    }
  };
  if (FAILED(listenerImpl->Init(webBrowser, onListenerDestroy, onReloaded)))
  {
    return;
  }
  m_data->connectedWebBrowsersCache.emplace(webBrowser, listenerImpl);
}

HICON CPluginClass::GetIcon(int type)
{
  HICON icon = NULL;

  s_criticalSectionLocal.Lock();
  {
    if (!s_hIcons[type])
    {
      std::wstring imageToLoad = L"#";
      imageToLoad += std::to_wstring(s_hIconTypes[type]);
      s_hIcons[type] = (HICON)::LoadImage(_Module.m_hInst, imageToLoad.c_str(), IMAGE_ICON, iconWidth, iconHeight, LR_SHARED);
      if (!s_hIcons[type])
      {
        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_LOAD_ICON, "Class::GetIcon - LoadIcon");
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

