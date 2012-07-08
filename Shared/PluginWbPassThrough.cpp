#include "PluginStdAfx.h"

#include "PluginWbPassThrough.h"
#include "PluginClient.h"
#include "PluginClientFactory.h"
#ifdef SUPPORT_FILTER
#include "PluginFilter.h"
#endif
#ifdef PRODUCT_DOWNLOADHELPER
#include "PluginConfig.h"
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
			    else if (ext == L".jsp" || ext == L".php")
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
//				m_shouldBlock = true;

				DEBUG_BLOCKER("Blocker::Blocking Http-request:" + src);
#ifndef PRODUCT_DOWNLOADHELPER
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
#endif

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



	//TODO: cleanup here
	//Fixes the iframe back button issue
	if (client->GetIEVersion() > 6)
	{
		if ((contentType == CFilter::contentTypeSubdocument) && (isBlocked)) 
		{
			m_shouldBlock = true;
			BaseClass::OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI, dwReserved, pTargetProtocol);
//			pTargetProtocol->Start(szUrl, pOIProtSink, pOIBindInfo, grfPI, dwReserved);
			m_spInternetProtocolSink->ReportResult(S_FALSE, 0, NULL);
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
//				m_spInternetProtocolSink->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE, L"text/html");
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

#ifdef PRODUCT_DOWNLOADHELPER

	CPluginTab* tab = CPluginClass::GetTab(::GetCurrentThreadId());
    CPluginClient* client = CPluginClient::GetInstance();
    if (tab && client)
    {
		CPluginConfig* config = CPluginConfig::GetInstance();
        
        CString contentType = szResponseHeaders;

        int pos = contentType.Find(L"Content-Type: ");
        if (pos >= 0)
        {
            contentType = contentType.Mid(pos + 14);
            
            pos = contentType.FindOneOf(L"; \n\r");
            if (pos > 0)
            {				
				bool isDownloadFile = false;
				SDownloadFileProperties downloadFileProperties;

				contentType = contentType.Left(pos);
				if (config->GetDownloadProperties(contentType, downloadFileProperties))
                {
					isDownloadFile = true;
                }
				// Special hacks
				else if (contentType == "text/plain")
				{
					CString domain = tab->GetDocumentDomain();
					if (domain == "fragstein.org" && m_url.Find(L".flv") == m_url.GetLength() - 4)
					{
						isDownloadFile = config->GetDownloadProperties("video/x-flv", downloadFileProperties);
					}
					else if (domain == "metacafe.com" && m_url.Find(L"http://akvideos.metacafe.com/ItemFiles/") == 0)
					{
						isDownloadFile = config->GetDownloadProperties("video/x-flv", downloadFileProperties);
					}
				}

				if (isDownloadFile)
				{
					// Find length
					ULONG fetched = 0;
					WCHAR* res[50];
					HRESULT hres;
					CComPtr<IWinInetHttpInfo> pWinInetInfo;
					hres = m_spTargetProtocol.QueryInterface(&pWinInetInfo);
					if (pWinInetInfo != NULL)
					{
						CStringA referer;
						DWORD len = MAX_PATH;
						DWORD flags = 0x80000000;
						//HTTP_QUERY_REFERER = 35; HTTP_QUERY_FLAG_REQUEST_HEADERS = 0x80000000
						hres = pWinInetInfo->QueryInfo(35 | 0x80000000, referer.GetBufferSetLength(MAX_PATH), &len, NULL, NULL);
						downloadFileProperties.properties.referer.Format(L"%S", referer); 
						referer = L"";
					}

					CStringA location;
					DWORD len = 1024;
					//INTERNET_OPTION_URL = 34
					hres = pWinInetInfo->QueryOption(34, location.GetBufferSetLength(1024), &len);
					if (hres == S_OK)
					{
						m_url.Format(L"%S", location);
					}
					CStringA contentLength = szResponseHeaders;

					int posLength = contentLength.Find("Content-Length: ");
					if (posLength > 0)
					{
						contentLength = contentLength.Mid(posLength + 16);
			            
						posLength = contentLength.FindOneOf("; \n\r");
						if (posLength > 0)
						{
							int fileSize = atoi(contentLength.Left(posLength).GetBuffer());
							if (fileSize > 0)
							{
								int rangeStart = m_url.Find(L"&range=");
								if (rangeStart > 0)
								{
									int rangeEnd = m_url.Find(L"&", rangeStart + 1);
									m_url.Delete(rangeStart, rangeEnd - rangeStart);
									fileSize = 0;
								}
								tab->AddDownloadFile(m_url, fileSize, downloadFileProperties);
							}
						}
					}
				}
            }
        }
		else
		{
		}
    }

#endif // PRODUCT_DOWNLOADHELPER

	return spHttpNegotiate ? spHttpNegotiate->OnResponse(dwResponseCode, szResponseHeaders, szRequestHeaders, pszAdditionalRequestHeaders) : S_OK;
}

STDMETHODIMP WBPassthruSink::ReportProgress(ULONG ulStatusCode, LPCWSTR szStatusText)
{
	return m_spInternetProtocolSink ? m_spInternetProtocolSink->ReportProgress(ulStatusCode, szStatusText) : S_OK;
}
