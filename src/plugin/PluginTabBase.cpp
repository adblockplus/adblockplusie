/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
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
#include <Mshtmhst.h>

CPluginTabBase::CPluginTabBase(CPluginClass* plugin)
  : m_plugin(plugin)
  , m_isActivated(false)
  , m_continueThreadRunning(true)
{
  m_filter = std::auto_ptr<CPluginFilter>(new CPluginFilter());
  m_filter->hideFiltersLoadedEvent = CreateEvent(NULL, true, false, NULL);

  CPluginClient* client = CPluginClient::GetInstance();
  if (AdblockPlus::IE::InstalledMajorVersion() < 10)
  {
    m_isActivated = true;
  }

  try
  {
    m_thread = std::thread(&CPluginTabBase::ThreadProc, this);
  }
  catch (const std::system_error& ex)
  {
    DEBUG_SYSTEM_EXCEPTION(ex, PLUGIN_ERROR_THREAD, PLUGIN_ERROR_TAB_THREAD_CREATE_PROCESS,
      "Tab::Thread - Failed to create tab thread");
  }
  m_traverser = new CPluginDomTraverser(static_cast<CPluginTab*>(this));
}


CPluginTabBase::~CPluginTabBase()
{
  delete m_traverser;
  m_traverser = NULL;
  m_continueThreadRunning = false;
  if (m_thread.joinable()) {
    m_thread.join();
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

namespace
{
  // Entry Point
  void FilterLoader(CPluginTabBase* tabBase)
  {
    try
    {
      tabBase->m_filter->LoadHideFilters(CPluginClient::GetInstance()->GetElementHidingSelectors(tabBase->GetDocumentDomain()));
      SetEvent(tabBase->m_filter->hideFiltersLoadedEvent);
    }
    catch (...)
    {
      // As a thread-main function, we truncate any C++ exception.
    }
  }
}

void CPluginTabBase::OnNavigate(const std::wstring& url)
{
  SetDocumentUrl(url);
  ClearFrameCache(GetDocumentDomain());
  std::wstring domainString = GetDocumentDomain();
  ResetEvent(m_filter->hideFiltersLoadedEvent);
  try
  {
    std::thread filterLoaderThread(&FilterLoader, this);
    filterLoaderThread.detach(); // TODO: but actually we should wait for the thread in the dtr.
  }
  catch (const std::system_error& ex)
  {
    DEBUG_SYSTEM_EXCEPTION(ex, PLUGIN_ERROR_THREAD, PLUGIN_ERROR_MAIN_THREAD_CREATE_PROCESS,
      "Class::Thread - Failed to start filter loader thread");
  }
  m_traverser->ClearCache();
}

void CPluginTabBase::InjectABP(IWebBrowser2* browser)
{
  CriticalSection::Lock lock(m_csInject);
  auto url = GetDocumentUrl();

  std::wstring log = L"InjectABP. Current URL: ";
  log += url;
  log += L", settings URL: ";
  log += UserSettingsFileUrl();
  DEBUG_GENERAL(log);

  CString urlLegacy = ToCString(url);
  if (!(0 == urlLegacy.CompareNoCase(CString(UserSettingsFileUrl().c_str())) ||
      0 == urlLegacy.CompareNoCase(CString(FirstRunPageFileUrl().c_str()))))
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
  CPluginClient* client = CPluginClient::GetInstance();
  std::wstring url = GetDocumentUrl();
  if (!client->IsWhitelistedUrl(url) && !client->IsElemhideWhitelistedOnDomain(url))
  {
    m_traverser->TraverseDocument(browser, GetDocumentDomain(), GetDocumentUrl());
  }
  InjectABP(browser);
}

void CPluginTabBase::OnDocumentComplete(IWebBrowser2* browser, const std::wstring& url, bool isDocumentBrowser)
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
  CString urlLegacy = ToCString(url);
  if (urlLegacy.Left(6) != "res://")
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
  }
}

std::wstring CPluginTabBase::GetDocumentDomain()
{
  std::wstring domain;

  m_criticalSection.Lock();
  {
    domain = m_documentDomain;
  }
  m_criticalSection.Unlock();

  return domain;
}

void CPluginTabBase::SetDocumentUrl(const std::wstring& url)
{
  m_criticalSection.Lock();
  {
    m_documentUrl = url;
    m_documentDomain = CAdblockPlusClient::GetInstance()->GetHostFromUrl(url);
  }
  m_criticalSection.Unlock();
}

std::wstring CPluginTabBase::GetDocumentUrl()
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
bool CPluginTabBase::IsFrameCached(const std::wstring& url)
{
  bool isFrame;

  m_criticalSectionCache.Lock();
  {
    isFrame = m_cacheFrames.find(url) != m_cacheFrames.end();
  }
  m_criticalSectionCache.Unlock();

  return isFrame;
}

void CPluginTabBase::CacheFrame(const std::wstring& url)
{
  m_criticalSectionCache.Lock();
  {
    m_cacheFrames.insert(url);
  }
  m_criticalSectionCache.Unlock();
}

void CPluginTabBase::ClearFrameCache(const std::wstring& domain)
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

void CPluginTabBase::ThreadProc()
{
  // Force loading/creation of settings
  CPluginSettings::GetInstance()->SetWorkingThreadId();

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
