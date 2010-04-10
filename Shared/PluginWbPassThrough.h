#pragma once

#include "ProtocolCF.h"
#include "ProtocolImpl.h"
#define IE_MAX_URL_LENGTH 2048

class WBPassthruSink :
	public PassthroughAPP::CInternetProtocolSinkWithSP<WBPassthruSink>,
	public IHttpNegotiate
{
	typedef PassthroughAPP::CInternetProtocolSinkWithSP<WBPassthruSink> BaseClass;

public:

	//Maximum URL length in IE
	WCHAR m_curUrl[IE_MAX_URL_LENGTH];
	bool m_shouldBlock;
	bool m_lastDataReported;
	IInternetProtocol* m_pTargetProtocol;
	CString m_url;

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
		IInternetBindInfo *pOIBindInfo, DWORD grfPI, DWORD dwReserved,
		IInternetProtocol* pTargetProtocol);
	HRESULT Read(void *pv, ULONG cb, ULONG* pcbRead);

	STDMETHODIMP ReportProgress(
		/* [in] */ ULONG ulStatusCode,
		/* [in] */ LPCWSTR szStatusText);

	STDMETHODIMP Switch(
		/* [in] */ PROTOCOLDATA *pProtocolData);
};

typedef PassthroughAPP::CustomSinkStartPolicy<WBPassthruSink> TestStartPolicy;

class WBPassthru : public PassthroughAPP::CInternetProtocol<TestStartPolicy>
{
};
