#include "PluginStdAfx.h"

#include "PluginClient.h"
#include "PluginSettings.h"
#ifdef SUPPORT_CONFIG
#include "PluginConfig.h"
#endif
#include "PluginTab.h"
#include "PluginDomTraverser.h"
#include "PluginClass.h"

#include "PluginTabBase.h"
#include "PluginUtil.h"
#include <dispex.h>

int CPluginTabBase::s_dictionaryVersion = 0;
int CPluginTabBase::s_settingsVersion = 1;
#ifdef SUPPORT_FILTER
int CPluginTabBase::s_filterVersion = 0;
#endif
#ifdef SUPPORT_WHITELIST
int CPluginTabBase::s_whitelistVersion = 0;
#endif
#ifdef SUPPORT_CONFIG
int CPluginTabBase::s_configVersion = 0;
#endif


CPluginTabBase::CPluginTabBase(CPluginClass* plugin) : m_plugin(plugin), m_isActivated(false)
{
  m_filter = std::auto_ptr<CPluginFilter>(new CPluginFilter());
  m_filter->hideFiltersLoadedEvent = CreateEvent(NULL, true, false, NULL);

  CPluginClient* client = CPluginClient::GetInstance();
  if (client->GetIEVersion() < 10)
  {
    m_isActivated = true;
  }

  DWORD id;
  m_hThread = ::CreateThread(NULL, 0, ThreadProc, (LPVOID)this, CREATE_SUSPENDED, &id);
  if (m_hThread)
  {
    m_isThreadDone = false;
    ::ResumeThread(m_hThread);
  }
  else
  {
    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_THREAD, PLUGIN_ERROR_TAB_THREAD_CREATE_PROCESS, "Tab::Thread - Failed to create tab thread");
  }

#ifdef SUPPORT_DOM_TRAVERSER
  m_traverser = new CPluginDomTraverser(static_cast<CPluginTab*>(this));
#endif // SUPPORT_DOM_TRAVERSER
}


CPluginTabBase::~CPluginTabBase()
{
#ifdef SUPPORT_DOM_TRAVERSER
  delete m_traverser;
  m_traverser = NULL;
#endif // SUPPORT_DOM_TRAVERSER

  // Close down thread
  if (m_hThread != NULL)
  {
    m_isThreadDone = true;

    ::WaitForSingleObject(m_hThread, INFINITE);
    ::CloseHandle(m_hThread);
  }
}

void CPluginTabBase::OnActivate()
{
  m_isActivated = true;
}


void CPluginTabBase::OnUpdate()
{
  m_isActivated = true;
}

DWORD WINAPI FilterLoader(LPVOID threadParam)
{
  CPluginTabBase* tabBase = (CPluginTabBase*)threadParam;
  tabBase->m_filter->LoadHideFilters(CPluginClient::GetInstance()->GetElementHidingSelectors(tabBase->GetDocumentDomain().GetString()));
  SetEvent(tabBase->m_filter->hideFiltersLoadedEvent);
  return 0;
}

void CPluginTabBase::OnNavigate(const CString& url)
{
  SetDocumentUrl(url);


#ifdef SUPPORT_FRAME_CACHING
  ClearFrameCache(GetDocumentDomain());
#endif

  std::wstring domainString = GetDocumentDomain();
  ResetEvent(m_filter->hideFiltersLoadedEvent);
  CreateThread(NULL, NULL, &FilterLoader, this, NULL, NULL);

#ifdef SUPPORT_DOM_TRAVERSER
  m_traverser->ClearCache();
#endif
}

void CPluginTabBase::InjectABP(IWebBrowser2* browser)
{
  CriticalSection::Lock lock(m_csInject);
  CString url = GetDocumentUrl();
  CString log;
  log.Format(L"InjectABP. Current URL: %s, settings URL: %s", url, UserSettingsFileUrl().c_str());
  DEBUG_GENERAL(log);
  if (!(0 == url.CompareNoCase(CString(UserSettingsFileUrl().c_str())) ||
      0 == url.CompareNoCase(CString(FirstRunPageFileUrl().c_str()))))
  {
    DEBUG_GENERAL(L"Not injecting");
    return;
  }
  DEBUG_GENERAL(L"Going to inject");
  CComPtr<IDispatch> pDocDispatch;
  browser->get_Document(&pDocDispatch);
  CComQIPtr<IHTMLDocument2> pDoc2 = pDocDispatch;
  if (!pDoc2)
  {
    DEBUG_ERROR_LOG(0, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT_INVOKE, "CPluginTabBase::InjectABP - Failed to QI document");
    return;
  }
  CComPtr<IHTMLWindow2> pWnd2;
  pDoc2->get_parentWindow(&pWnd2);
  if (!pWnd2)
  {
    DEBUG_ERROR_LOG(0, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT_INVOKE, "CPluginTabBase::InjectABP - Failed to get parent window");
    return;
  }
  CComQIPtr<IDispatchEx> pWndEx = pWnd2;
  if (!pWndEx)
  {
    DEBUG_ERROR_LOG(0, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT_INVOKE, "CPluginTabBase::InjectABP - Failed to QI dispatch");
    return;
  }
  // Create "Settings" object in JavaScript.
  // A method call of "Settings" in JavaScript, transfered to "Invoke" of m_pluginUserSettings
  DISPID dispid;
  HRESULT hr = pWndEx->GetDispID(L"Settings", fdexNameEnsure, &dispid);
  if (FAILED(hr))
  {
    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT_INVOKE, "CPluginTabBase::InjectABP - Failed to get dispatch");
    return;
  }
  CComVariant var((IDispatch*)&m_pluginUserSettings);

  DEBUG_GENERAL("Injecting");

  DISPPARAMS params;
  params.cArgs = 1;
  params.cNamedArgs = 0;
  params.rgvarg = &var;
  params.rgdispidNamedArgs = 0;
  hr = pWndEx->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF, &params, 0, 0, 0);
  DEBUG_GENERAL("Invoke");
  if (FAILED(hr))
  {
    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT_INVOKE, "CPluginTabBase::InjectABP - Failed to create Settings in JavaScript");
  }
}

void CPluginTabBase::OnDownloadComplete(IWebBrowser2* browser)
{
#ifdef SUPPORT_DOM_TRAVERSER
  if (!CPluginClient::GetInstance()->IsWhitelistedUrl(std::wstring(GetDocumentUrl())))
  {
    m_traverser->TraverseDocument(browser, GetDocumentDomain(), GetDocumentUrl());
  }
#endif // SUPPORT_DOM_TRAVERSER

  InjectABP(browser);
}

void CPluginTabBase::OnDocumentComplete(IWebBrowser2* browser, const CString& url, bool isDocumentBrowser)
{
  CString documentUrl = GetDocumentUrl();

  if (isDocumentBrowser)
  {
    if (url != documentUrl)
    {
      SetDocumentUrl(url);
    }
    InjectABP(browser);
  }

#ifdef SUPPORT_DOM_TRAVERSER
  if (url.Left(6) != "res://")
  {
    // Get document
    CComPtr<IDispatch> pDocDispatch;
    HRESULT hr = browser->get_Document(&pDocDispatch);
    if (FAILED(hr) || !pDocDispatch)
    {
      return;
    }

    CComQIPtr<IHTMLDocument2> pDoc = pDocDispatch;
    if (!pDoc)
    {
      return;
    }
    CComPtr<IOleObject> pOleObj;

    pDocDispatch->QueryInterface(IID_IOleObject, (void**)&pOleObj);


    CComPtr<IOleClientSite> pClientSite;
    pOleObj->GetClientSite(&pClientSite);
    if (pClientSite != NULL)
    {
      CComPtr<IDocHostUIHandler> docHostUIHandler;
      pClientSite->QueryInterface(IID_IDocHostUIHandler, (void**)&docHostUIHandler);
      if (docHostUIHandler != NULL)
      {
        docHostUIHandler->UpdateUI();
      }
    }

    pDoc.Release();
    pDocDispatch.Release();
  }
#endif
}

CString CPluginTabBase::GetDocumentDomain()
{
  CString domain;

  m_criticalSection.Lock();
  {
    domain = m_documentDomain;
  }
  m_criticalSection.Unlock();

  return domain;
}

void CPluginTabBase::SetDocumentUrl(const CString& url)
{
  m_criticalSection.Lock();
  {
    m_documentUrl = url;
    m_documentDomain = CString(CAdblockPlusClient::GetInstance()->GetHostFromUrl(url.GetString()).c_str());
  }
  m_criticalSection.Unlock();
}

CString CPluginTabBase::GetDocumentUrl()
{
  CString url;

  m_criticalSection.Lock();
  {
    url = m_documentUrl;
  }
  m_criticalSection.Unlock();

  return url;
}


// ============================================================================
// Frame caching
// ============================================================================

#ifdef SUPPORT_FRAME_CACHING

bool CPluginTabBase::IsFrameCached(const CString& url)
{
  bool isFrame;

  m_criticalSectionCache.Lock();
  {
    isFrame = m_cacheFrames.find(url) != m_cacheFrames.end();
  }
  m_criticalSectionCache.Unlock();

  return isFrame;
}

void CPluginTabBase::CacheFrame(const CString& url)
{
  m_criticalSectionCache.Lock();
  {
    m_cacheFrames.insert(url);
  }
  m_criticalSectionCache.Unlock();
}

void CPluginTabBase::ClearFrameCache(const CString& domain)
{
  m_criticalSectionCache.Lock();
  {
    if (domain.IsEmpty() || domain != m_cacheDomain)
    {
      m_cacheFrames.clear();
      m_cacheDomain = domain;
    }
  }
  m_criticalSectionCache.Unlock();
}

#endif // SUPPORT_FRAME_CACHING


DWORD WINAPI CPluginTabBase::ThreadProc(LPVOID pParam)
{
  CPluginTab* tab = static_cast<CPluginTab*>(pParam);

  // Force loading/creation of settings
  CPluginSettings* settings = CPluginSettings::GetInstance();

  settings->SetWorkingThreadId();

  CString threadInfo;
  threadInfo.Format(L"%d.%d", ::GetCurrentProcessId(), ::GetCurrentThreadId());

  CString debugText;

  debugText += L"================================================================================";
  debugText += L"\nTAB THREAD " + threadInfo;
  debugText += L"\n================================================================================";

  DEBUG_GENERAL(debugText)

  // --------------------------------------------------------------------
  // Tab loop
  // --------------------------------------------------------------------

  DWORD loopCount = 0;
  DWORD tabLoopIteration = 1;

  while (!tab->m_isThreadDone)
  {
#ifdef ENABLE_DEBUG_THREAD
    CStringA sTabLoopIteration;
    sTabLoopIteration.Format("%u", tabLoopIteration);

    DEBUG_THREAD("--------------------------------------------------------------------------------")
      DEBUG_THREAD("Loop iteration " + sTabLoopIteration);
    DEBUG_THREAD("--------------------------------------------------------------------------------")
#endif
      if (tab->m_isActivated)
      {
        bool isChanged = false;

        if (isChanged)
        {
          tab->m_plugin->UpdateStatusBar();
        }

        tab->m_isActivated = false;
      }

      // --------------------------------------------------------------------
      // End loop
      // --------------------------------------------------------------------

      // Sleep loop
      while (!tab->m_isThreadDone && !tab->m_isActivated && (++loopCount % (TIMER_THREAD_SLEEP_TAB_LOOP / 50)) != 0)
      {
        // Post async plugin error
        CPluginError pluginError;
        if (CPluginClient::PopFirstPluginError(pluginError))
        {
          CPluginClient::LogPluginError(pluginError.GetErrorCode(), pluginError.GetErrorId(), pluginError.GetErrorSubid(), pluginError.GetErrorDescription(), true, pluginError.GetProcessId(), pluginError.GetThreadId());
        }

        // Non-hanging sleep
        Sleep(50);
      }

      tabLoopIteration++;
  }

  return 0;
}
