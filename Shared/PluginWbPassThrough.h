#pragma once

#include "ProtocolCF.h"
#include "ProtocolImpl.h"

class WBPassthruSink :
	public PassthroughAPP::CInternetProtocolSinkWithSP<WBPassthruSink>,
	public IHttpNegotiate
{
	typedef PassthroughAPP::CInternetProtocolSinkWithSP<WBPassthruSink> BaseClass;

private:

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
