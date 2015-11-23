// Copyright 2007 Igor Tandetnik
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PASSTHROUGHAPP_PROTOCOLIMPL_H
#define PASSTHROUGHAPP_PROTOCOLIMPL_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

namespace PassthroughAPP
{

  namespace Detail
  {
    struct PassthroughItfData
    {
      DWORD_PTR offsetItf;
      DWORD_PTR offsetUnk;
      const IID* piidBase;
    };

    template <class itf, class impl, DWORD_PTR offsetUnk, const IID* piidBase>
    struct PassthroughItfHelper
    {
      static PassthroughItfData data;
    };

    template <class itf, class impl, DWORD_PTR offsetUnk, const IID* piidBase>
    PassthroughItfData
      PassthroughItfHelper<itf, impl, offsetUnk, piidBase>::
      data = {offsetofclass(itf, impl), offsetUnk, piidBase};

#define COM_INTERFACE_ENTRY_PASSTHROUGH(itf, punk)\
    {&_ATL_IIDOF(itf),\
    (DWORD_PTR)&::PassthroughAPP::Detail::PassthroughItfHelper<\
    itf, _ComMapClass,\
    (DWORD_PTR)offsetof(_ComMapClass, punk),\
    0\
    >::data,\
    ::PassthroughAPP::Detail::QIPassthrough<_ComMapClass>::\
    QueryInterfacePassthroughT\
    },

#define COM_INTERFACE_ENTRY_PASSTHROUGH2(itf, punk, itfBase)\
    {&_ATL_IIDOF(itf),\
    (DWORD_PTR)&::PassthroughAPP::Detail::PassthroughItfHelper<\
    itf, _ComMapClass,\
    (DWORD_PTR)offsetof(_ComMapClass, punk),\
    &_ATL_IIDOF(itfBase)\
    >::data,\
    ::PassthroughAPP::Detail::QIPassthrough<_ComMapClass>::\
    QueryInterfacePassthroughT\
    },


#ifdef DEBUG

#define COM_INTERFACE_ENTRY_PASSTHROUGH_DEBUG()\
    {0, 0,\
    ::PassthroughAPP::Detail::QIPassthrough<_ComMapClass>::\
    QueryInterfaceDebugT\
    },

#else
#define COM_INTERFACE_ENTRY_PASSTHROUGH_DEBUG()
#endif

#define DECLARE_GET_TARGET_UNKNOWN(x) \
  inline IUnknown* GetTargetUnknown() {return x;}

    // Workaround for VC6's deficiencies in dealing with function templates.
    // We'd use non-member template functions, but VC6 does not handle those well.
    // Static members of class templates work much better, and we don't need
    // parameter deduction here
    template <class T>
    struct QIPassthrough
    {
      static HRESULT WINAPI QueryInterfacePassthroughT(void* pv, REFIID riid,
        LPVOID* ppv, DWORD_PTR dw);
      static HRESULT WINAPI QueryInterfaceDebugT(void* pv, REFIID riid,
        LPVOID* ppv, DWORD_PTR dw);
    };

    HRESULT WINAPI QueryInterfacePassthrough(void* pv, REFIID riid,
      LPVOID* ppv, DWORD_PTR dw, IUnknown* punkTarget, IUnknown* punkWrapper);

    HRESULT WINAPI QueryInterfaceDebug(void* pv, REFIID riid,
      LPVOID* ppv, DWORD_PTR dw, IUnknown* punkTarget);

    HRESULT QueryServicePassthrough(REFGUID guidService,
      IUnknown* punkThis, REFIID riid, void** ppv,
      IServiceProvider* pClientProvider);

  } // end namespace PassthroughAPP::Detail

  class ATL_NO_VTABLE IInternetProtocolImpl :
    public IPassthroughObject,
    public IInternetProtocol,
    public IInternetProtocolInfo,
    public IInternetPriority,
    public IInternetThreadSwitch,
    public IWinInetHttpInfo
  {
  public:
    void ReleaseAll();

    DECLARE_GET_TARGET_UNKNOWN(m_spInternetProtocolUnk)
  public:
    // IPassthroughObject
    STDMETHODIMP SetTargetUnknown(IUnknown* punkTarget);

    // IInternetProtocolRoot
    STDMETHODIMP Start(
      /* [in] */ LPCWSTR szUrl,
      /* [in] */ IInternetProtocolSink *pOIProtSink,
      /* [in] */ IInternetBindInfo *pOIBindInfo,
      /* [in] */ DWORD grfPI,
      /* [in] */ HANDLE_PTR dwReserved);

    STDMETHODIMP Continue(
      /* [in] */ PROTOCOLDATA *pProtocolData);

    STDMETHODIMP Abort(
      /* [in] */ HRESULT hrReason,
      /* [in] */ DWORD dwOptions);

    STDMETHODIMP Terminate(
      /* [in] */ DWORD dwOptions);

    STDMETHODIMP Suspend();

    STDMETHODIMP Resume();

    // IInternetProtocol
    STDMETHODIMP Read(
      /* [in, out] */ void *pv,
      /* [in] */ ULONG cb,
      /* [out] */ ULONG *pcbRead);

    STDMETHODIMP Seek(
      /* [in] */ LARGE_INTEGER dlibMove,
      /* [in] */ DWORD dwOrigin,
      /* [out] */ ULARGE_INTEGER *plibNewPosition);

    STDMETHODIMP LockRequest(
      /* [in] */ DWORD dwOptions);

    STDMETHODIMP UnlockRequest();

    // IInternetProtocolInfo
    STDMETHODIMP ParseUrl(
      /* [in] */ LPCWSTR pwzUrl,
      /* [in] */ PARSEACTION ParseAction,
      /* [in] */ DWORD dwParseFlags,
      /* [out] */ LPWSTR pwzResult,
      /* [in] */ DWORD cchResult,
      /* [out] */ DWORD *pcchResult,
      /* [in] */ DWORD dwReserved);

    STDMETHODIMP CombineUrl(
      /* [in] */ LPCWSTR pwzBaseUrl,
      /* [in] */ LPCWSTR pwzRelativeUrl,
      /* [in] */ DWORD dwCombineFlags,
      /* [out] */ LPWSTR pwzResult,
      /* [in] */ DWORD cchResult,
      /* [out] */ DWORD *pcchResult,
      /* [in] */ DWORD dwReserved);

    STDMETHODIMP CompareUrl(
      /* [in] */ LPCWSTR pwzUrl1,
      /* [in] */ LPCWSTR pwzUrl2,
      /* [in] */ DWORD dwCompareFlags);

    STDMETHODIMP QueryInfo(
      /* [in] */ LPCWSTR pwzUrl,
      /* [in] */ QUERYOPTION QueryOption,
      /* [in] */ DWORD dwQueryFlags,
      /* [in, out] */ LPVOID pBuffer,
      /* [in] */ DWORD cbBuffer,
      /* [in, out] */ DWORD *pcbBuf,
      /* [in] */ DWORD dwReserved);

    // IInternetPriority
    STDMETHODIMP SetPriority(
      /* [in] */ LONG nPriority);

    STDMETHODIMP GetPriority(
      /* [out] */ LONG *pnPriority);

    // IInternetThreadSwitch
    STDMETHODIMP Prepare();

    STDMETHODIMP Continue();

    // IWinInetInfo
    STDMETHODIMP QueryOption(
      /* [in] */ DWORD dwOption,
      /* [in, out] */ LPVOID pBuffer,
      /* [in, out] */ DWORD *pcbBuf);

    // IWinInetHttpInfo
    STDMETHODIMP QueryInfo(
      /* [in] */ DWORD dwOption,
      /* [in, out] */ LPVOID pBuffer,
      /* [in, out] */ DWORD *pcbBuf,
      /* [in, out] */ DWORD *pdwFlags,
      /* [in, out] */ DWORD *pdwReserved);

  public:
    CComPtr<IUnknown> m_spInternetProtocolUnk;
    CComPtr<IInternetProtocol> m_spInternetProtocol;
    CComPtr<IInternetProtocolInfo> m_spInternetProtocolInfo;
    CComPtr<IInternetPriority> m_spInternetPriority;
    CComPtr<IInternetThreadSwitch> m_spInternetThreadSwitch;
    CComPtr<IWinInetInfo> m_spWinInetInfo;
    CComPtr<IWinInetHttpInfo> m_spWinInetHttpInfo;
  };

  class ATL_NO_VTABLE IInternetProtocolSinkImpl :
    public IInternetProtocolSink,
    public IServiceProvider
  {
  public:
    HRESULT OnStart(LPCWSTR szUrl, IInternetProtocolSink *pOIProtSink,
      IInternetBindInfo *pOIBindInfo, DWORD grfPI, HANDLE_PTR dwReserved,
      IInternetProtocol* pTargetProtocol);
    void ReleaseAll();

    DECLARE_GET_TARGET_UNKNOWN(m_spInternetProtocolSink)

    IServiceProvider* GetClientServiceProvider();

    HRESULT QueryServiceFromClient(REFGUID guidService, REFIID riid,
      void** ppvObject);
    template <class Q>
    HRESULT QueryServiceFromClient(REFGUID guidService, Q** pp)
    {
      return QueryServiceFromClient(guidService, _ATL_IIDOF(Q),
        reinterpret_cast<void**>(pp));
    }
    template <class Q>
    HRESULT QueryServiceFromClient(Q** pp)
    {
      return QueryServiceFromClient(_ATL_IIDOF(Q), _ATL_IIDOF(Q),
        reinterpret_cast<void**>(pp));
    }
  public:
    // IInternetProtocolSink
    STDMETHODIMP Switch(
      /* [in] */ PROTOCOLDATA *pProtocolData);

    STDMETHODIMP ReportProgress(
      /* [in] */ ULONG ulStatusCode,
      /* [in] */ LPCWSTR szStatusText);

    STDMETHODIMP ReportData(
      /* [in] */ DWORD grfBSCF,
      /* [in] */ ULONG ulProgress,
      /* [in] */ ULONG ulProgressMax);

    STDMETHODIMP ReportResult(
      /* [in] */ HRESULT hrResult,
      /* [in] */ DWORD dwError,
      /* [in] */ LPCWSTR szResult);

    // IServiceProvider
    STDMETHODIMP QueryService(
      /* [in] */ REFGUID guidService,
      /* [in] */ REFIID riid,
      /* [out] */ void** ppvObject);

  public:
    CComPtr<IInternetProtocolSink> m_spInternetProtocolSink;
    CComPtr<IServiceProvider> m_spServiceProvider;
    CComPtr<IInternetProtocol> m_spTargetProtocol;
  };

  template <class ThreadModel>
  class CInternetProtocolSinkTM :
    public CComObjectRootEx<ThreadModel>,
    public IInternetProtocolSinkImpl
  {
  private:
	static HRESULT WINAPI OnDelegateIID(void* pv, REFIID riid, LPVOID* ppv, DWORD_PTR dw)
	{
		IInternetProtocolSink* pSink = ((CInternetProtocolSinkTM<ThreadModel> *) pv)->m_spInternetProtocolSink;
		ATLASSERT(pSink != 0);
		return pSink ? pSink->QueryInterface(riid, ppv) : E_UNEXPECTED;
	}

  public:
    BEGIN_COM_MAP(CInternetProtocolSinkTM)
      COM_INTERFACE_ENTRY(IInternetProtocolSink)
      COM_INTERFACE_ENTRY_PASSTHROUGH(IServiceProvider,
      m_spServiceProvider.p)
	  COM_INTERFACE_ENTRY_FUNC_BLIND(0, OnDelegateIID)
      COM_INTERFACE_ENTRY_PASSTHROUGH_DEBUG()
    END_COM_MAP()

  };

  typedef CInternetProtocolSinkTM<CComMultiThreadModel> CInternetProtocolSink;

  template <class T, class ThreadModel = CComMultiThreadModel>
  class CInternetProtocolSinkWithSP :
    public CInternetProtocolSinkTM<ThreadModel>
  {
    typedef CInternetProtocolSinkTM<ThreadModel> BaseClass;
  public:
    HRESULT OnStart(LPCWSTR szUrl, IInternetProtocolSink *pOIProtSink,
      IInternetBindInfo *pOIBindInfo, DWORD grfPI, HANDLE_PTR dwReserved,
      IInternetProtocol* pTargetProtocol);

    STDMETHODIMP QueryService(REFGUID guidService, REFIID riid, void** ppv);

    HRESULT _InternalQueryService(REFGUID guidService, REFIID riid,
      void** ppvObject);

    BEGIN_COM_MAP(CInternetProtocolSinkWithSP)
      COM_INTERFACE_ENTRY(IServiceProvider)
      COM_INTERFACE_ENTRY_CHAIN(BaseClass)
    END_COM_MAP()
  };

#define SERVICE_ENTRY_PASSTHROUGH(x) \
  if (InlineIsEqualGUID(guidService, x)) \
  { \
  return ::PassthroughAPP::Detail::QueryServicePassthrough(guidService, \
  GetUnknown(), riid, ppvObject, GetClientServiceProvider()); \
  }

  struct __declspec(uuid("C7A98E66-1010-492c-A1C8-C809E1F75905")) IInternetProtocolEx;

  template <class StartPolicy, class ThreadModel = CComMultiThreadModel>
  class ATL_NO_VTABLE CInternetProtocol :
    public CComObjectRootEx<ThreadModel>,
    public IInternetProtocolImpl,
    public StartPolicy
  {
  private:
    static HRESULT WINAPI OnDelegateIID(void* pv, REFIID riid, LPVOID* ppv, DWORD_PTR dw)
    {
      IInternetProtocol* pProtocol = ((CInternetProtocol<StartPolicy, ThreadModel> *) pv)->m_spInternetProtocol;
      ATLASSERT(pProtocol != 0);
      // TODO: don't need this once we support IInternetProtocolEx
      if (InlineIsEqualGUID(riid, __uuidof(IInternetProtocolEx))) return E_NOINTERFACE;
      return pProtocol ? pProtocol->QueryInterface(riid, ppv) : E_UNEXPECTED;
    }
  public:
    BEGIN_COM_MAP(CInternetProtocol)
      COM_INTERFACE_ENTRY(IPassthroughObject)
      COM_INTERFACE_ENTRY(IInternetProtocolRoot)
      COM_INTERFACE_ENTRY(IInternetProtocol)
      COM_INTERFACE_ENTRY_PASSTHROUGH(IInternetProtocolInfo,
      m_spInternetProtocolInfo.p)
      COM_INTERFACE_ENTRY_PASSTHROUGH(IInternetPriority,
      m_spInternetPriority.p)
      COM_INTERFACE_ENTRY_PASSTHROUGH(IInternetThreadSwitch,
      m_spInternetThreadSwitch.p)
      COM_INTERFACE_ENTRY_PASSTHROUGH(IWinInetInfo, m_spWinInetInfo.p)
      COM_INTERFACE_ENTRY_PASSTHROUGH2(IWinInetHttpInfo,
      m_spWinInetHttpInfo.p, IWinInetInfo)
      COM_INTERFACE_ENTRY_FUNC_BLIND(0, OnDelegateIID)
      COM_INTERFACE_ENTRY_PASSTHROUGH_DEBUG()
    END_COM_MAP()
  };

} // end namespace PassthroughAPP

#include "ProtocolImpl.inl"

#include "SinkPolicy.h"

#endif // PASSTHROUGHAPP_PROTOCOLIMPL_H
