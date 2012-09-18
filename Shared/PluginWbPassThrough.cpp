#include "PluginStdAfx.h"

#include "PluginWbPassThrough.h"
#include "PluginClient.h"
#include "PluginClientFactory.h"
#ifdef SUPPORT_FILTER
#include "PluginFilter.h"
#endif
#include "PluginSettings.h"
#include "PluginClass.h"
#include "PluginHttpRequest.h"
#include "PluginSystem.h"

#include "wtypes.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;


WBPassthruSink::WBPassthruSink()
{
	m_pTargetProtocol = NULL;
}
////////////////////////////////////////////////////////////////////////////////////////
//WBPassthruSink
//Monitor and/or cancel every request and responde
//WB makes, including images, sounds, scripts, etc
////////////////////////////////////////////////////////////////////////////////////////
HRESULT WBPassthruSink::OnStart(LPCWSTR szUrl, IInternetProtocolSink *pOIProtSink,
		IInternetBindInfo *pOIBindInfo, DWORD grfPI, DWORD dwReserved,
		IInternetProtocol* pTargetProtocol)
{

	m_pTargetProtocol = pTargetProtocol;
    bool isBlocked = false;
	m_shouldBlock = false;
	m_lastDataReported = false;
    CString src;
	src.Append(szUrl);
	DEBUG_GENERAL(src);
    CPluginClient::UnescapeUrl(src);
	m_url = szUrl;
	
	CString cookie;
	ULONG len1 = 2048;
	ULONG len2 = 2048;

#ifdef SUPPORT_FILTER
	int contentType = CFilter::contentTypeAny;

	CPluginTab* tab = CPluginClass::GetTab(::GetCurrentThreadId());
    CPluginClient* client = CPluginClient::GetInstance();

    if (tab && client)
	{
		CString documentUrl = tab->GetDocumentUrl();
		// Page is identical to document => don't block
		if (documentUrl == src)
		{
			// fall through
		}
		else if (CPluginSettings::GetInstance()->IsPluginEnabled() && !client->IsUrlWhiteListed(documentUrl))
		{
		        CString domain = tab->GetDocumentDomain();

			contentType = CFilter::contentTypeAny;

#ifdef SUPPORT_FRAME_CACHING
            if ((tab != 0) && (tab->IsFrameCached(src)))
            {
                contentType = CFilter::contentTypeSubdocument;
            }
            else
#endif // SUPPORT_FRAME_CACHING
			{
                CString srcExt = src;

			    int pos = 0;
			    if ((pos = src.Find('?')) > 0)
			    {
				    srcExt = src.Left(pos);
			    }

			    CString ext = srcExt.Right(4);

			    if (ext == L".jpg" || ext == L".gif" || ext == L".png")
			    {
				    contentType = CFilter::contentTypeImage;
			    }
			    else if (ext == L".css")
			    {
				    contentType = CFilter::contentTypeStyleSheet;
			    }
			    else if (ext.Right(3) == L".js")
			    {
				    contentType = CFilter::contentTypeScript;
			    }
			    else if (ext == L".xml")
			    {
				    contentType = CFilter::contentTypeXmlHttpRequest;
			    }
			    else if (ext == L".swf")
			    {
				    contentType = CFilter::contentTypeObjectSubrequest;
			    }
			    else if (ext == L".jsp" || ext == L".php" || ext == L"html")
			    {
				    contentType = CFilter::contentTypeSubdocument;
			    }
			    else
			    {
				    contentType = CFilter::contentTypeAny & ~CFilter::contentTypeSubdocument;
			    }
	        }
			if (client->ShouldBlock(src, contentType, domain, true))
			{
                isBlocked = true;

				DEBUG_BLOCKER("Blocker::Blocking Http-request:" + src);

				CPluginSettings* settings = CPluginSettings::GetInstance();
				//is plugin registered
				if (!settings->GetBool(SETTING_PLUGIN_REGISTRATION, false))
				{
					//is the limit exceeded?
					if ((settings->GetValue(SETTING_PLUGIN_ADBLOCKCOUNT, 0) >= settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0))
						&& (settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0) > 0))
					{
						return false;
					} 

					else 
					{
						//Increment blocked ads counter if not registered and not yet exceeded the adblocklimit
						settings->SetValue(SETTING_PLUGIN_ADBLOCKCOUNT, settings->GetValue(SETTING_PLUGIN_ADBLOCKCOUNT, 0) + 1);
						settings->Write();
					}
				}
			}
#ifdef ENABLE_DEBUG_RESULT_IGNORED
			else
			{
			    CString type;

        		if (contentType == CFilter::contentTypeDocument) type = "doc";
	            else if (contentType == CFilter::contentTypeObject) type = "object";
	            else if (contentType == CFilter::contentTypeImage) type = "img";
	            else if (contentType == CFilter::contentTypeScript) type = "script";
	            else if (contentType == CFilter::contentTypeOther) type = "other";
	            else if (contentType == CFilter::contentTypeUnknown) type = "?";
	            else if (contentType == CFilter::contentTypeSubdocument) type = "iframe";
	            else if (contentType == CFilter::contentTypeStyleSheet) type = "css";
	            else type = "???";

                CPluginDebug::DebugResultIgnoring(type, src);
            }
#endif // ENABLE_DEBUG_RESULT_IGNORED
		}

        if (!isBlocked)
        {
            DEBUG_BLOCKER("Blocker::Ignoring Http-request:" + src)
        }
	}


	if (tab == NULL)
	{
		if (client->ShouldBlock(src, NULL, L"", true))
		{
			isBlocked = true;
            CString srcExt = src;

		    int pos = 0;
		    if ((pos = src.Find('?')) > 0)
		    {
			    srcExt = src.Left(pos);
		    }

		    CString ext = srcExt.Right(4);

		    if (ext == L".jpg" || ext == L".gif" || ext == L".png")
		    {
			    contentType = CFilter::contentTypeImage;
		    }
		    else if (ext == L".css")
		    {
			    contentType = CFilter::contentTypeStyleSheet;
		    }
		    else if (ext.Right(3) == L".js")
		    {
			    contentType = CFilter::contentTypeScript;
		    }
		    else if (ext == L".xml")
		    {
			    contentType = CFilter::contentTypeXmlHttpRequest;
		    }
		    else if (ext == L".swf")
		    {
			    contentType = CFilter::contentTypeObjectSubrequest;
		    }
		    else if (ext == L".jsp" || ext == L".php" || ext == L"html")
		    {
			    contentType = CFilter::contentTypeSubdocument;
		    }
		    else
		    {
			    contentType = CFilter::contentTypeAny & ~CFilter::contentTypeSubdocument;
		    }

		}
	}

	//Fixes the iframe back button issue
	if (client->GetIEVersion() > 6)
	{
		if ((contentType == CFilter::contentTypeImage) && (isBlocked))
		{
			m_shouldBlock = true;
			BaseClass::OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI, dwReserved, pTargetProtocol);
			
			return INET_E_REDIRECT_FAILED;

		}
		if (((contentType == CFilter::contentTypeSubdocument))&& (isBlocked)) 
		{
			m_shouldBlock = true;
			BaseClass::OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI, dwReserved, pTargetProtocol);

			m_spInternetProtocolSink->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE, L"text/html");

			//Here we check if we are running on Windows 8 Consumer Preview. 
			//For some reason on that environment the next line causes IE to crash
			if (CPluginSettings::GetInstance()->GetWindowsBuildNumber() != 8250)
			{
				m_spInternetProtocolSink->ReportResult(INET_E_REDIRECT_FAILED, 0, L"res://mshtml.dll/blank.htm");
			}
			
			return INET_E_REDIRECT_FAILED;
		} 
	}
#endif // SUPPORT_FILTER

	return isBlocked ? S_FALSE : BaseClass::OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI, dwReserved, pTargetProtocol);
}


HRESULT WBPassthruSink::Read(void *pv, ULONG cb, ULONG* pcbRead)
{
	if (m_shouldBlock)
	{
		*pcbRead = 0;
		if (!m_lastDataReported)
		{
			if (cb <= 1)
			{
				//IE must've gone nuts if this happened, but let's be cool about it and report we have no more data
				m_spInternetProtocolSink->ReportResult(S_FALSE, 0, NULL);
				return S_FALSE;
			}
			*pcbRead = 1;
			memcpy(pv, " ", 1);

			if (m_spInternetProtocolSink != NULL)
			{
				m_spInternetProtocolSink->ReportResult(S_OK, 0, NULL);
			}
			m_lastDataReported = true;
			m_shouldBlock = false;
			return S_OK;
		}
		return S_OK;
	}
	else 
	{

		return m_pTargetProtocol->Read(pv, cb, pcbRead);
	}
	return S_OK;
}
STDMETHODIMP WBPassthruSink::Switch(
	/* [in] */ PROTOCOLDATA *pProtocolData)
{
	ATLASSERT(m_spInternetProtocolSink != 0);

/*
From Igor Tandetnik "itandetnik@mvps.org"
"
Beware multithreading. URLMon has this nasty habit of spinning worker 
threads, not even bothering to initialize COM on them, and calling APP 
methods on those threads. If you try to raise COM events directly from 
such a thread, bad things happen (random crashes, events being lost). 
You are only guaranteed to be on the main STA thread in two cases. 
First, in methods of interfaces that were obtained with 
IServiceProvider, such as IHttpNegotiage::BeginningTransaction or 
IAuthenticate::Authenticate. Second, you can call 
IInternetProtocolSink::Switch with PD_FORCE_SWITCH flag in 
PROTOCOLDATA::grfFlags, eventually URLMon will turn around and call 
IInternetProtocol::Continue on the main thread. 

Or, if you happen to have a window handy that was created on the main 
thread, you can post yourself a message.
"
*/
	return m_spInternetProtocolSink ? m_spInternetProtocolSink->Switch(pProtocolData) : E_UNEXPECTED;
}


STDMETHODIMP WBPassthruSink::BeginningTransaction(LPCWSTR szURL, LPCWSTR szHeaders, DWORD dwReserved, LPWSTR *pszAdditionalHeaders)
{
	if (pszAdditionalHeaders)
	{
		*pszAdditionalHeaders = 0;
	}

	CComPtr<IHttpNegotiate> spHttpNegotiate;
	QueryServiceFromClient(&spHttpNegotiate);
	return spHttpNegotiate ? spHttpNegotiate->BeginningTransaction(szURL, szHeaders,dwReserved, pszAdditionalHeaders) : S_OK;
}

STDMETHODIMP WBPassthruSink::OnResponse(DWORD dwResponseCode, LPCWSTR szResponseHeaders, LPCWSTR szRequestHeaders, LPWSTR *pszAdditionalRequestHeaders)
{
	if (pszAdditionalRequestHeaders)
	{
		*pszAdditionalRequestHeaders = 0;
	}

	CComPtr<IHttpNegotiate> spHttpNegotiate;
	QueryServiceFromClient(&spHttpNegotiate);

	return spHttpNegotiate ? spHttpNegotiate->OnResponse(dwResponseCode, szResponseHeaders, szRequestHeaders, pszAdditionalRequestHeaders) : S_OK;
}

STDMETHODIMP WBPassthruSink::ReportProgress(ULONG ulStatusCode, LPCWSTR szStatusText)
{
	return m_spInternetProtocolSink ? m_spInternetProtocolSink->ReportProgress(ulStatusCode, szStatusText) : S_OK;
}
