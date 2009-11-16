#include "AdPluginStdAfx.h"

#include "AdPluginWbPassThrough.h"
#include "AdPluginClient.h"
#include "AdPluginClientFactory.h"
#ifdef SUPPORT_FILTER
#include "AdPluginFilterClass.h"
#endif
#ifdef PRODUCT_DOWNLOADHELPER
#include "AdPluginConfig.h"
#endif
#include "AdPluginSettings.h"


////////////////////////////////////////////////////////////////////////////////////////
//WBPassthruSink
//Monitor and/or cancel every request and responde
//WB makes, including images, sounds, scripts, etc
////////////////////////////////////////////////////////////////////////////////////////
HRESULT WBPassthruSink::OnStart(LPCWSTR szUrl, IInternetProtocolSink *pOIProtSink,
		IInternetBindInfo *pOIBindInfo, DWORD grfPI, DWORD dwReserved,
		IInternetProtocol* pTargetProtocol)
{
    bool isBlocked = false;

    CString src = szUrl;
    CPluginClient::UnescapeUrl(src);

    m_url = src;

#ifdef SUPPORT_FILTER

	CPluginClient* client = CPluginClientFactory::GetLazyClientInstance();
	if (client)
	{	    
		// Page is identical to document => don't block
		if (client->GetDocumentUrl() == src)
		{
			// fall through
		}
		else if (CPluginSettings::GetInstance()->IsPluginEnabled() && !client->IsDocumentWhiteListed())
		{
	        CStringA domain= client->GetDocumentDomain();

			int contentType = CFilter::contentTypeAny;

            if (client->IsFrame(src))
            {
                contentType = CFilter::contentTypeSubdocument;
            }
            else
            {
                CStringA srcExt = src;

			    int pos = 0;
			    if ((pos = src.Find('?')) > 0)
			    {
				    srcExt = src.Left(pos);
			    }

			    CStringA ext = srcExt.Right(4);

			    if (ext == ".jpg" || ext == ".gif" || ext == ".png")
			    {
				    contentType = CFilter::contentTypeImage;
			    }
			    else if (ext == ".css")
			    {
				    contentType = CFilter::contentTypeStyleSheet;
			    }
			    else if (ext.Right(3) == ".js")
			    {
				    contentType = CFilter::contentTypeScript;
			    }
			    else if (ext == ".xml")
			    {
				    contentType = CFilter::contentTypeXmlHttpRequest;
			    }
			    else if (ext == ".swf")
			    {
				    contentType = CFilter::contentTypeObjectSubrequest;
			    }
			    else if (ext == ".jsp" || ext == ".php")
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
				DEBUG_BLOCKER("Blocker::Blocking Http-request:" + src);

                isBlocked = true;
			}
#ifdef ENABLE_DEBUG_RESULT_IGNORED
			else
			{
			    CStringA type;

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

#endif // SUPPORT_FILTER

	return isBlocked ? S_FALSE : BaseClass::OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI, dwReserved, pTargetProtocol);
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

#ifdef SUPPORT_FILE_DOWNLOAD

    CPluginClient* client = CPluginClientFactory::GetLazyClientInstance();
    if (client)
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
				SDownloadFileProperties downloadFileProperties;

				if (config->GetDownloadProperties(contentType.Left(pos), downloadFileProperties))
                {
					int fileSize = 0;

					// Find length
					CStringA contentLength = szResponseHeaders;

					int posLength = contentLength.Find("Content-Length: ");
					if (posLength > 0)
					{
						contentLength = contentLength.Mid(posLength + 16);
			            
						posLength = contentLength.FindOneOf("; \n\r");
						if (posLength > 0)
						{
							fileSize = atoi(contentLength.Left(posLength).GetBuffer());
							if (fileSize > 0)
							{
::MessageBox(::GetDesktopWindow(), m_url, L"Has file", MB_OK);
								client->AddDownloadFile(m_url, fileSize, downloadFileProperties);
							}
						}
					}
					
                }
            }
        }
    }

#endif // SUPPORT_FILE_DOWNLOAD    

	return spHttpNegotiate ? spHttpNegotiate->OnResponse(dwResponseCode, szResponseHeaders, szRequestHeaders, pszAdditionalRequestHeaders) : S_OK;
}

STDMETHODIMP WBPassthruSink::ReportProgress(ULONG ulStatusCode, LPCWSTR szStatusText)
{
	return m_spInternetProtocolSink ? m_spInternetProtocolSink->ReportProgress(ulStatusCode, szStatusText) : S_OK;
}
