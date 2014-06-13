#pragma once

#include "ProtocolCF.h"
#include "ProtocolImpl.h"
#define IE_MAX_URL_LENGTH 2048

class WBPassthruSink :
	public PassthroughAPP::CInternetProtocolSinkWithSP<WBPassthruSink, CComMultiThreadModel>,
	public IHttpNegotiate
{
	typedef PassthroughAPP::CInternetProtocolSinkWithSP<WBPassthruSink, CComMultiThreadModel> BaseClass;

public:

	bool m_shouldBlock;
	bool m_lastDataReported;
	CComPtr<IInternetProtocol> m_pTargetProtocol;
	CString m_url;

  int GetContentTypeFromMimeType(CString mimeType);
  int GetContentTypeFromURL(CString src);
  int GetContentType(CString mimeType, CString domain, CString src);
public:
	BEGIN_COM_MAP(WBPassthruSink)
		COM_INTERFACE_ENTRY(IHttpNegotiate)
		COM_INTERFACE_ENTRY_CHAIN(BaseClass)
	END_COM_MAP()

	BEGIN_SERVICE_MAP(WBPassthruSink)
		SERVICE_ENTRY(IID_IHttpNegotiate)
	END_SERVICE_MAP()

	STDMETHODIMP BeginningTransaction(
		/* [in] */ LPCWSTR szURL,
		/* [in] */ LPCWSTR szHeaders,
		/* [in] */ DWORD dwReserved,
		/* [out] */ LPWSTR *pszAdditionalHeaders);

	STDMETHODIMP OnResponse(
		/* [in] */ DWORD dwResponseCode,
		/* [in] */ LPCWSTR szResponseHeaders,
		/* [in] */ LPCWSTR szRequestHeaders,
		/* [out] */ LPWSTR *pszAdditionalRequestHeaders);

	HRESULT OnStart(LPCWSTR szUrl, IInternetProtocolSink *pOIProtSink,
		IInternetBindInfo *pOIBindInfo, DWORD grfPI, HANDLE_PTR dwReserved,
		IInternetProtocol* pTargetProtocol);
	HRESULT Read(void *pv, ULONG cb, ULONG* pcbRead);

	STDMETHODIMP ReportProgress(
		/* [in] */ ULONG ulStatusCode,
		/* [in] */ LPCWSTR szStatusText);

	STDMETHODIMP Switch(
		/* [in] */ PROTOCOLDATA *pProtocolData);
};

class WBPassthru;
typedef PassthroughAPP::CustomSinkStartPolicy<WBPassthru, WBPassthruSink> WBStartPolicy;

class WBPassthru : public PassthroughAPP::CInternetProtocol<WBStartPolicy>
{
};
