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
#include "AdblockPlusClient.h"
#include "PluginClientBase.h"
#include "PluginSettings.h"
#include "AdblockPlusDomTraverser.h"
#include "PluginTabBase.h"
#include "IeVersion.h"
#include "../shared/Utils.h"
#include <Mshtmhst.h>

CPluginTab::CPluginTab()
  : m_isActivated(false)
  , m_continueThreadRunning(true)
{
  m_filter.hideFiltersLoadedEvent = CreateEvent(NULL, true, false, NULL);

  CPluginClient* client = CPluginClient::GetInstance();
  if (AdblockPlus::IE::InstalledMajorVersion() < 10)
  {
    m_isActivated = true;
  }

  try
  {
    m_thread = std::thread(&CPluginTab::ThreadProc, this);
  }
  catch (const std::system_error& ex)
  {
    DEBUG_SYSTEM_EXCEPTION(ex, PLUGIN_ERROR_THREAD, PLUGIN_ERROR_TAB_THREAD_CREATE_PROCESS,
      "Tab::Thread - Failed to create tab thread");
  }
}


CPluginTab::~CPluginTab()
{
  m_continueThreadRunning = false;
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

/**
 * ABP only intercepts protocols "http:" and "https:".
 * We can disable any domain used in those protocol with an appropriate whitelist filter.
 * Thus, the possibility to disable on a particular site depends only on the protocol.
 */
bool CPluginTab::IsPossibleToDisableOnSite()
{
  auto url = GetDocumentUrl();
  return BeginsWith(url, L"http:") || BeginsWith(url, L"https:");
}

void CPluginTab::OnActivate()
{
  m_isActivated = true;
}


void CPluginTab::OnUpdate()
{
  m_isActivated = true;
}

namespace
{
  // Entry Point
  void FilterLoader(CPluginFilter* filter, const std::wstring& domain)
  {
    try
    {
      filter->LoadHideFilters(CPluginClient::GetInstance()->GetElementHidingSelectors(domain));
      SetEvent(filter->hideFiltersLoadedEvent);
    }
    catch (...)
    {
      // As a thread-main function, we truncate any C++ exception.
    }
  }
}

void CPluginTab::OnNavigate(const std::wstring& url)
{
  SetDocumentUrl(url);
  ClearFrameCache(GetDocumentDomain());
  std::wstring domainString = GetDocumentDomain();
  ResetEvent(m_filter.hideFiltersLoadedEvent);
  try
  {
    std::thread filterLoaderThread(&FilterLoader, &m_filter, GetDocumentDomain());
    filterLoaderThread.detach(); // TODO: but actually we should wait for the thread in the dtr.
  }
  catch (const std::system_error& ex)
  {
    DEBUG_SYSTEM_EXCEPTION(ex, PLUGIN_ERROR_THREAD, PLUGIN_ERROR_MAIN_THREAD_CREATE_PROCESS,
      "Class::Thread - Failed to start filter loader thread");
  }
  m_traverser.reset();
}

namespace
{
  /**
   * Determine if the HTML file is one of ours.
   * The criterion is that it appear in the "html/templates" folder within our installation.
   *
   * Warning: This function may fail if the argument is not a "file://" URL.
   * This is occasionally the case in circumstances yet to be characterized.
   */
  bool IsOurHtmlFile(const std::wstring& url)
  {
    // Declared static because the value is derived from an installation directory, which won't change during run-time.
    static auto dir = FileUrl(HtmlFolderPath());

    DEBUG_GENERAL([&]() -> std::wstring {
      std::wstring log = L"InjectABP. Current URL: ";
      log += url;
      log += L", template directory URL: ";
      log += dir;
      return log;
    }());

    /*
     * The length check here is defensive, in case the document URL is truncated for some reason.
     */
    if (url.length() < 5)
    {
      // We can't match ".html" at the end of the URL if it's too short.
      return false;
    }
    auto urlCstr = url.c_str();
    // Check the prefix to match our directory
    // Check the suffix to be an HTML file
    return (_wcsnicmp(urlCstr, dir.c_str(), dir.length()) == 0) &&
      (_wcsnicmp(urlCstr + url.length() - 5, L".html", 5) == 0);
  }
}

void CPluginTab::InjectABP(IWebBrowser2* browser)
{
  CriticalSection::Lock lock(m_csInject);
  auto url = GetDocumentUrl();
  if (!IsOurHtmlFile(url))
  {
    DEBUG_GENERAL(L"InjectABP. Not injecting");
    return;
  }
  DEBUG_GENERAL(L"InjectABP. Injecting");
  CComPtr<IDispatch> pDocDispatch;
  browser->get_Document(&pDocDispatch);
  CComQIPtr<IHTMLDocument2> pDoc2(pDocDispatch);
  if (!pDoc2)
  {
    DEBUG_ERROR_LOG(0, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT_INVOKE, "CPluginTab::InjectABP - Failed to QI document");
    return;
  }
  CComPtr<IHTMLWindow2> pWnd2;
  pDoc2->get_parentWindow(&pWnd2);
  if (!pWnd2)
  {
    DEBUG_ERROR_LOG(0, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT_INVOKE, "CPluginTab::InjectABP - Failed to get parent window");
    return;
  }
  CComQIPtr<IDispatchEx> pWndEx(pWnd2);
  if (!pWndEx)
  {
    DEBUG_ERROR_LOG(0, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT_INVOKE, "CPluginTab::InjectABP - Failed to QI dispatch");
    return;
  }
  // Create "Settings" object in JavaScript.
  // A method call of "Settings" in JavaScript, transfered to "Invoke" of m_pluginUserSettings
  DISPID dispid;
  HRESULT hr = pWndEx->GetDispID(L"Settings", fdexNameEnsure, &dispid);
  if (FAILED(hr))
  {
    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT_INVOKE, "CPluginTab::InjectABP - Failed to get dispatch");
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
    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT, PLUGIN_ERROR_CREATE_SETTINGS_JAVASCRIPT_INVOKE, "CPluginTab::InjectABP - Failed to create Settings in JavaScript");
  }
}

namespace
{
  ATL::CComPtr<IWebBrowser2> GetParent(IWebBrowser2& browser)
  {
    ATL::CComPtr<IDispatch> parentDispatch;
    if (FAILED(browser.get_Parent(&parentDispatch)) || !parentDispatch)
    {
      return nullptr;
    }
    // The InternetExplorer application always returns a pointer to itself.
    // https://msdn.microsoft.com/en-us/library/aa752136(v=vs.85).aspx
    if (parentDispatch.IsEqualObject(&browser))
    {
      return nullptr;
    }
    ATL::CComQIPtr<IServiceProvider> parentDocumentServiceProvider = parentDispatch;
    if (!parentDocumentServiceProvider)
    {
      return nullptr;
    }
    ATL::CComPtr<IWebBrowser2> parentBrowser;
    if (FAILED(parentDocumentServiceProvider->QueryService(SID_SWebBrowserApp, &parentBrowser)))
    {
      return nullptr;
    }
    return parentBrowser;
  }

  bool IsFrameWhiteListed(ATL::CComPtr<IWebBrowser2> frame)
  {
    if (!frame)
    {
      return false;
    }
    auto url = GetLocationUrl(*frame);
    std::vector<std::string> frameHierarchy;
    while(frame = GetParent(*frame))
    {
      frameHierarchy.push_back(ToUtf8String(GetLocationUrl(*frame)));
    }
    CPluginClient* client = CPluginClient::GetInstance();
    return client->IsWhitelistedUrl(url, frameHierarchy)
        || client->IsElemhideWhitelistedOnDomain(url, frameHierarchy);
  }
}

void CPluginTab::OnDownloadComplete(IWebBrowser2* browser)
{
  CPluginClient* client = CPluginClient::GetInstance();
  std::wstring url = GetDocumentUrl();
  if (!client->IsWhitelistedUrl(url) && !client->IsElemhideWhitelistedOnDomain(url))
  {
    if (!m_traverser)
      m_traverser.reset(new CPluginDomTraverser(static_cast<CPluginTab*>(this)));
    m_traverser->TraverseDocument(browser, GetDocumentDomain(), GetDocumentUrl());
  }
  InjectABP(browser);
}

void CPluginTab::OnDocumentComplete(IWebBrowser2* browser, const std::wstring& url, bool isDocumentBrowser)
{
  std::wstring documentUrl = GetDocumentUrl();

  if (isDocumentBrowser)
  {
    if (url != documentUrl)
    {
      SetDocumentUrl(url);
    }
    InjectABP(browser);
  }
  if (BeginsWith(url, L"res://"))
  {
    return;
  }
  // Get document
  CComPtr<IDispatch> pDocDispatch;
  HRESULT hr = browser->get_Document(&pDocDispatch);
  if (FAILED(hr) || !pDocDispatch)
  {
    return;
  }

  CComQIPtr<IHTMLDocument2> pDoc(pDocDispatch);
  if (!pDoc)
  {
    return;
  }

  CComPtr<IOleObject> pOleObj;
  pDocDispatch->QueryInterface(&pOleObj);
  if (!pOleObj)
  {
    return;
  }
  CComPtr<IOleClientSite> pClientSite;
  pOleObj->GetClientSite(&pClientSite);
  if (pClientSite != NULL)
  {
    CComPtr<IDocHostUIHandler> docHostUIHandler;
    pClientSite->QueryInterface(&docHostUIHandler);
    if (docHostUIHandler != NULL)
    {
      docHostUIHandler->UpdateUI();
    }
  }
}

std::wstring CPluginTab::GetDocumentDomain()
{
  std::wstring domain;

  m_criticalSection.Lock();
  {
    domain = m_documentDomain;
  }
  m_criticalSection.Unlock();

  return domain;
}

void CPluginTab::SetDocumentUrl(const std::wstring& url)
{
  m_criticalSection.Lock();
  {
    m_documentUrl = url;
    m_documentDomain = CAdblockPlusClient::GetInstance()->GetHostFromUrl(url);
  }
  m_criticalSection.Unlock();
}

std::wstring CPluginTab::GetDocumentUrl()
{
  std::wstring url;

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
bool CPluginTab::IsFrameCached(const std::wstring& url)
{
  bool isFrame;

  m_criticalSectionCache.Lock();
  {
    isFrame = m_cacheFrames.find(url) != m_cacheFrames.end();
  }
  m_criticalSectionCache.Unlock();

  return isFrame;
}

void CPluginTab::CacheFrame(const std::wstring& url)
{
  m_criticalSectionCache.Lock();
  {
    m_cacheFrames.insert(url);
  }
  m_criticalSectionCache.Unlock();
}

void CPluginTab::ClearFrameCache(const std::wstring& domain)
{
  m_criticalSectionCache.Lock();
  {
    if (domain.empty() || domain != m_cacheDomain)
    {
      m_cacheFrames.clear();
      m_cacheDomain = domain;
    }
  }
  m_criticalSectionCache.Unlock();
}

void CPluginTab::ThreadProc()
{
  // Force loading/creation of settings
  CPluginSettings::GetInstance();

  std::string message =
    "================================================================================\n"
    "TAB THREAD process=";
  message += std::to_string(::GetCurrentProcessId());
  message + " thread=";
  message += std::to_string(::GetCurrentThreadId());
  message +=
    "\n"
    "================================================================================";
  DEBUG_GENERAL(message);

  // --------------------------------------------------------------------
  // Tab loop
  // --------------------------------------------------------------------

  DWORD loopCount = 0;
  DWORD tabLoopIteration = 1;

  while (this->m_continueThreadRunning)
  {
#ifdef ENABLE_DEBUG_THREAD
    CStringA sTabLoopIteration;
    sTabLoopIteration.Format("%u", tabLoopIteration);

    DEBUG_THREAD("--------------------------------------------------------------------------------")
      DEBUG_THREAD("Loop iteration " + sTabLoopIteration);
    DEBUG_THREAD("--------------------------------------------------------------------------------")
#endif
      this->m_isActivated = false;

      // --------------------------------------------------------------------
      // End loop
      // --------------------------------------------------------------------

      // Sleep loop
      while (this->m_continueThreadRunning && !this->m_isActivated && (++loopCount % (TIMER_THREAD_SLEEP_TAB_LOOP / 50)) != 0)
      {
        // Post async plugin error
        CPluginError pluginError;
        if (LogQueue::PopFirstPluginError(pluginError))
        {
          LogQueue::LogPluginError(pluginError.GetErrorCode(), pluginError.GetErrorId(), pluginError.GetErrorSubid(), pluginError.GetErrorDescription(), true, pluginError.GetProcessId(), pluginError.GetThreadId());
        }

        // Non-hanging sleep
        Sleep(50);
      }

      tabLoopIteration++;
  }
}
