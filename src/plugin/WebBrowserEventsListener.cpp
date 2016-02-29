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
#include "WebBrowserEventsListener.h"

WebBrowserEventsListener::WebBrowserEventsListener()
  : m_isDocumentEvents2Connected(false)
  , m_state(State::FirstTimeLoading)
{
}

WebBrowserEventsListener::~WebBrowserEventsListener()
{
  if (!!m_onDestroy)
  {
    m_onDestroy();
  }
}

HRESULT STDMETHODCALLTYPE WebBrowserEventsListener::OnDocumentComplete(IDispatch* dispFrameBrowser, VARIANT* /*variantUrl*/)
{
  if (!dispFrameBrowser)
  {
    return E_POINTER;
  }

  // if it's a signal from another browser (sub-frame for-example) then ignore it.
  if (!m_browser.IsEqualObject(dispFrameBrowser))
  {
    return S_OK;
  }

  if (!m_isDocumentEvents2Connected)
  {
    ATL::CComPtr<IDispatch> dispDocument;
    ATL::CComQIPtr<IHTMLDocument2> htmlDocument2;
    bool isHtmlDocument2 = SUCCEEDED(m_browser->get_Document(&dispDocument)) && (htmlDocument2 = dispDocument);
    isHtmlDocument2 && (m_isDocumentEvents2Connected = SUCCEEDED(HTMLDocumentEvents2Listener::DispEventAdvise(htmlDocument2)));
  }

  // We can get here when readyStateChanged("complete") is already received,
  // don't emit reloaded, because it's already emitted from OnReadyStateChange.
  if (m_state == State::FirstTimeLoading)
  {
    m_state = State::Loaded;
    emitReloaded();
  }
  return S_OK;
}

void STDMETHODCALLTYPE WebBrowserEventsListener::OnReadyStateChange(IHTMLEventObj* /*pEvtObj*/)
{
  auto documentReadyState = [this]()->std::wstring
  {
    std::wstring notAvailableReadyState;
    ATL::CComPtr<IDispatch> pDocDispatch;
    m_browser->get_Document(&pDocDispatch);
    ATL::CComQIPtr<IHTMLDocument2> htmlDocument2 = pDocDispatch;
    if (!htmlDocument2)
    {
      assert(false && "htmlDocument2 in OnReadyStateChange should not be nullptr");
      return notAvailableReadyState;
    }
    ATL::CComBSTR readyState;
    if (FAILED(htmlDocument2->get_readyState(&readyState)) || !readyState)
    {
      assert(false && "cannot obtain document readyState in OnReadyStateChange");
      return notAvailableReadyState;
    }
    return std::wstring(readyState, readyState.Length());
  }();
  if (documentReadyState == L"loading")
  {
    m_state = State::Loading;
  }
  else if (documentReadyState == L"interactive")
  {
  }
  else if (documentReadyState == L"complete")
  {
    if (m_state == State::Loading)
    {
      m_state = State::Loaded;
      emitReloaded();
    }
    else if (m_state == State::Loaded)
    {
      // It happens but very rearely, most often it appears on gmail.
      // It seems IE prepares the 'browser' and then immediately says
      // "complete" with the new URL. However all cases are related to
      // some redirection technique and I could not reproduce it with local
      // server which redirects, so let's wait for the user response on another
      // web site when an advertisement is not blocked to better investigate
      // when it happens.
    }
    else
    {
      assert(false);
    }
  }
  else if (documentReadyState == L"uninitialized")
  {
  }
  else
  {
    assert(false);
  }
}

HRESULT WebBrowserEventsListener::Init(IWebBrowser2* webBrowser, const OnDestroy& onDestroy, const OnReloaded& onReloaded)
{
  if (!(m_browser = webBrowser))
  {
    return E_POINTER;
  }
  m_onDestroy = onDestroy;
  m_onReloaded = onReloaded;
  if (FAILED(WebBrowserEvents2Listener::DispEventAdvise(m_browser, &DIID_DWebBrowserEvents2)))
  {
    return E_FAIL;
  }
  return S_OK;
}

void WebBrowserEventsListener::emitReloaded()
{
  if (m_onReloaded)
  {
    m_onReloaded();
  }
}
