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

#pragma once
#include <functional>
#include <memory>

class WebBrowserEventsListener;

typedef ATL::IDispEventImpl <1, WebBrowserEventsListener,
  &__uuidof(DWebBrowserEvents2), &LIBID_SHDocVw, 1, 1> WebBrowserEvents2Listener;

typedef ATL::IDispEventImpl <2, WebBrowserEventsListener,
  &__uuidof(HTMLDocumentEvents2), &LIBID_MSHTML, 4, 0> HTMLDocumentEvents2Listener;

class ATL_NO_VTABLE WebBrowserEventsListener :
  public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>,
  public WebBrowserEvents2Listener,
  public HTMLDocumentEvents2Listener,
  public IUnknown
{
  enum class State
  {
    FirstTimeLoading, Loading, Loaded
  };
public:
  typedef std::function<void()> OnDestroy;
  typedef std::function<void()> OnReloaded;

  WebBrowserEventsListener();
  ~WebBrowserEventsListener();
  BEGIN_COM_MAP(WebBrowserEventsListener)
    COM_INTERFACE_ENTRY(IUnknown)
  END_COM_MAP()

  DECLARE_NOT_AGGREGATABLE(WebBrowserEventsListener)
  BEGIN_SINK_MAP(WebBrowserEventsListener)
    SINK_ENTRY_EX(1, __uuidof(DWebBrowserEvents2), DISPID_DOCUMENTCOMPLETE, OnDocumentComplete)
    SINK_ENTRY_EX(2, __uuidof(HTMLDocumentEvents2), DISPID_HTMLDOCUMENTEVENTS2_ONREADYSTATECHANGE, OnReadyStateChange)
  END_SINK_MAP()

  STDMETHOD(OnDocumentComplete)(IDispatch* pDisp, VARIANT* urlVariant);
  STDMETHOD_(void, OnReadyStateChange)(IHTMLEventObj* pEvtObj);

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct(){ return S_OK; }
  void FinalRelease(){}
  HRESULT Init(IWebBrowser2* webBrowser, const OnDestroy& onDestroy, const OnReloaded& onReloaded);

private:
  void emitReloaded();
private:
  ATL::CComPtr<IWebBrowser2> m_browser;
  OnDestroy m_onDestroy;
  OnReloaded m_onReloaded;
  bool m_isDocumentEvents2Connected;
  State m_state;
};