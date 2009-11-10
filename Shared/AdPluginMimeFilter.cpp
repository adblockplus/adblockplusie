#include "AdPluginStdAfx.h"

#include "AdPluginMimeFilter.h"
#include "AdPluginSettings.h"
#include "AdPluginFilterClass.h"
#include "AdPluginClient.h"
#include "AdPluginClientFactory.h"
#include "AdPluginByteBuffer.h"
#include "AdPluginPassthroughObject.h"

#include "ProtocolImpl.h"


#define ADSENSEURL "http://pagead2.googlesyndication.com/pagead/show_ads.js"


// Mime filter contstructor. Is called for every new html page.
CAdPluginMimeFilter::CAdPluginMimeFilter() : m_localClient(CAdPluginClientFactory::GetLazyClientInstance())
{
	m_bFirstRead = true;
	m_pOutgoingProtSink = NULL;
	m_pIncomingProt = NULL;
	m_posPointer = 0;
	m_bParsed = false;
	m_disableFilter = false;
	m_adsReplaced = 0;
	m_pOIBindInfo = NULL;
    m_bIsActive = true;

	m_byteBuffer = new ByteBuffer;
}

CAdPluginMimeFilter::~CAdPluginMimeFilter()
{
	if (m_pIncomingProt)
	{
		m_pIncomingProt->Release();
	}

	delete m_byteBuffer;
}

//-----------------------------------------------------
// IInternetProtocolInfo implementation
//-----------------------------------------------------
HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::CombineUrl(
	/* [in] */ LPCWSTR pwzBaseUrl,
	/* [in] */ LPCWSTR pwzRelativeUrl,
	/* [in] */ DWORD dwCombineFlags,
	/* [out] */ LPWSTR pwzResult,
	/* [in] */ DWORD cchResult,
	/* [out] */ DWORD *pcchResult,
	/* [in] */ DWORD dwReserved)
{
	USES_CONVERSION;	

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::CompareUrl(
	/* [in] */ LPCWSTR pwzUrl1,
	/* [in] */ LPCWSTR pwzUrl2,
	/* [in] */ DWORD dwCompareFlags
	)
{
	USES_CONVERSION;	

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::ParseUrl(
	/* [in] */ LPCWSTR pwzUrl,
	/* [in] */ PARSEACTION ParseAction,
	/* [in] */ DWORD dwParseFlags,
	/* [out] */ LPWSTR pwzResult,
	/* [in] */ DWORD cchResult,
	/* [out] */ DWORD *pcchResult,
	/* [in] */ DWORD dwReserved) 
{
	USES_CONVERSION;	

	switch (ParseAction)
	{
		/* seems to be correct but fails on windows 2000 */
	case PARSE_SECURITY_URL:
		{
			const wchar_t *pch=wcsstr(pwzUrl, L"ws:");

			if (pch == pwzUrl)
			{ 
				int extra = wcslen(L"file") - wcslen(L"ws");
				UINT newlen = wcslen(pwzUrl) + extra + sizeof(wchar_t);

				if (cchResult < newlen)
				{ 
					*pcchResult = newlen;
					return S_FALSE;
				}

				wcscpy_s(pwzResult, wcslen(L"file:"), L"file:");
				pch = wcsstr(pwzUrl, L":");
				pch = CharNextW(pch);
				wcscat_s(pwzResult, wcslen(pch), pch);
				*pcchResult = wcslen(pwzResult) + sizeof(wchar_t);

				return S_OK;
			}
			else 
			{
				return INET_E_DEFAULT_ACTION;
			}
		}

	default:
		{
			return INET_E_DEFAULT_ACTION;
		}
	}
}

HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::QueryInfo(
	/* [in] */ LPCWSTR pwzUrl,
	/* [in] */ QUERYOPTION OueryOption,
	/* [in] */ DWORD dwQueryFlags,
	/* [in] */ LPVOID pBuffer,
	/* [in] */ DWORD cbBuffer,
	/* [in] */ DWORD *pcbBuf,
	/* [in] */ DWORD dwReserved)
{
	USES_CONVERSION;	

	return S_OK;
}


//-------------------------------------
// IInternetProtocolRoot implementation
//-------------------------------------

// Start is called whenever there's a request for a new resource
HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::Start(
	/* [in] */ LPCWSTR szUrl,
	/* [in] */ IInternetProtocolSink __RPC_FAR *pOIProtSink,
	/* [in] */ IInternetBindInfo __RPC_FAR *pOIBindInfo,
	/* [in] */ DWORD grfSTI,
	/* [in] */ DWORD dwReserved)
{
	USES_CONVERSION;

    m_bIsActive = false;

	// In cases where, default implementation of Read(..) returns E_PENDING, we would use
	// m_isPending to save that state
	m_isPending = false;

	// We should be attached as mime filter only
	if (!(grfSTI & PI_FILTER_MODE))
	{
		return E_INVALIDARG;
	}

	// Get the protocol pointer from reserved pointer
	PROTOCOLFILTERDATA* ProtFiltData = (PROTOCOLFILTERDATA*) dwReserved;
	_ASSERTE(NULL == m_pIncomingProt);
	if (NULL == ProtFiltData->pProtocol)
	{
		// !! We can't do anything without an interface to read from
		_ASSERTE(false);
		return E_INVALIDARG;
	}

	// We also get pointers to all the interfaces we might need here in Start,
	// so we save them into class members and AddRef'ing
	CComPtr<IServiceProvider> pServiceProvider2;
	ProtFiltData->pProtocol->QueryInterface(&pServiceProvider2);
	if (pServiceProvider2 != NULL)
	{
		CComPtr<IHttpNegotiate> httpNegotiate2;
		pServiceProvider2->QueryService(IID_IHttpNegotiate, IID_IHttpNegotiate, (void**)&httpNegotiate2);
	}

	m_pIncomingProt = ProtFiltData->pProtocol;
	m_pIncomingInfo = NULL;
	m_pIncomingProt->AddRef();

	// Hold onto the sink as well
	_ASSERTE(NULL == m_pOutgoingProtSink);
	m_pOutgoingProtSink = pOIProtSink;
	m_pOutgoingProtSink->AddRef();

    m_pOIBindInfo = pOIBindInfo;

	// Now we retrieve the current request URL and save it to class member
	// We'll use it later for whitelist check, ad replacement etc.
	LPOLESTR pszURL = NULL;
	DWORD dwSize;
	pOIBindInfo->GetBindString(BINDSTRING_URL, &pszURL, 1, &dwSize);	
	if (pszURL)
	{
		m_curUrl = pszURL;

        m_bIsActive = m_localClient && CAdPluginSettings::GetInstance()->IsPluginEnabled();
        if (m_bIsActive)
        {
            DEBUG_PARSER("MimeFilter Start url:" + m_curUrl);
            m_bIsActive = !m_localClient->IsDocumentWhiteListed();
        }
    }

    // Used in Read(...)
    m_bFirstRead = true;

    // We check if the content is HTML later 
    m_isHTML = true;

    return S_OK;
}	


HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::Continue(
	/* [in] */ PROTOCOLDATA __RPC_FAR *pProtocolData)
{
	if (NULL == m_pIncomingProt)
	{
		return E_FAIL;
	}

	return m_pIncomingProt->Continue(pProtocolData);
}

HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::Abort(
	/* [in] */ HRESULT hrReason,
	/* [in] */ DWORD dwOptions)
{
	if (NULL == m_pIncomingProt)
	{
		return E_FAIL;
	}

	return m_pIncomingProt->Abort(hrReason, dwOptions);
}	

HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::Terminate(
	/* [in] */ DWORD dwOptions)
{
	// Release the sink
	if (m_pOutgoingProtSink != NULL)
	{
		m_pOutgoingProtSink->Release();
		m_pOutgoingProtSink = NULL;
	}
	if (m_pOIBindInfo != NULL)
	{
		m_pOIBindInfo.Release();
		m_pOIBindInfo = NULL;
	}

#ifdef _DUMPMEMORY
	// Dump memory leaks to output window.
	_CrtDumpMemoryLeaks();
#endif
	if (m_pIncomingProt != NULL)
	{
		m_pIncomingProt->Terminate(dwOptions);
	}
	return S_OK;
}	


//This is the method that gets the charset info, based on 
//HTTP headers, <META> HTML tag, or Windows' built in heuristics (MLang)
HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::DetectCharset(CString& input)
{
	HRESULT hr = S_OK;

	//We start by searchin for the body, as that would be the position starting from
	//<META> shouldn't be available. We should look for <META> only in <HEAD>
	int headEnd = input.Find("<body");
	if (headEnd < 0)
	{
		headEnd = input.Find("<BODY");
		if (headEnd < 0)
        {
			headEnd = 0;
        }
	}

	//We create a substring for head only
	CString tempHead = input.Left(headEnd);
	tempHead = tempHead.MakeLower();

	int startIndex = 0;
	int endIndex = 0;

	//First we check if we already have charset info from HTTP headers
	CString httpCharset = "";
	if (m_localClient->charsetsHash.Lookup(m_curUrl, httpCharset) == TRUE)
	{
		m_localClient->charsetsHash.RemoveKey(m_curUrl);
		if (httpCharset != "")
		{
			//We got charset info from HTTP headers,
			//Now we need to convert in to codepage representation.
			//We use MLang for that, and store codepage number in m_bindInfo.dwCodePage
			DWORD maxLCID = 65535;
			CString lcidCP;
			HRESULT hr = S_OK;
			if (m_multiLanguage == NULL)
			{
				CoInitialize(NULL);
				hr = CoCreateInstance(CLSID_CMultiLanguage, NULL, CLSCTX_INPROC, IID_IMultiLanguage, (void**)&m_multiLanguage);
			}
			if (SUCCEEDED(hr))
			{
				CComBSTR charsetBSTR = httpCharset;
				MIMECSETINFO mimeCSetInfo;
				hr = m_multiLanguage->GetCharsetInfo(charsetBSTR, &mimeCSetInfo);
				if (SUCCEEDED(hr))
				{
					m_bindInfo.dwCodePage = mimeCSetInfo.uiInternetEncoding;
				}
			}

			m_detectedCharset = httpCharset;
			return S_OK;
		}
	}

	//We didn't get any charset info from HTTP headers, 
	//so we try to find the meta http-equiv tag with charset info
	if ((startIndex = tempHead.Find("charset=")) > 0) 
	{
		tempHead = tempHead.Right(tempHead.GetLength() - startIndex - strlen("charset="));
		endIndex = tempHead.FindOneOf("\"'; ");
		CString pageMetaCharset = "";
		if (endIndex > 0)
		{
			pageMetaCharset = tempHead.Mid(0, endIndex);
			pageMetaCharset = pageMetaCharset.MakeLower();
			pageMetaCharset.Trim("\"");
			pageMetaCharset.Trim(" ");
			pageMetaCharset.Trim("'");
			m_detectedCharset = pageMetaCharset;
		}

		//We found charset info in HTML's head, now let's convert it into the codepage number
		//and store in m_bindInfo.dwCodePage
        if (m_multiLanguage == NULL)
		{
			CoInitialize(NULL);
			hr = CoCreateInstance(CLSID_CMultiLanguage, NULL, CLSCTX_INPROC, IID_IMultiLanguage, (void**)&m_multiLanguage);
		}
		if (SUCCEEDED(hr))
		{
			CComBSTR charsetBSTR = pageMetaCharset;
			MIMECSETINFO mimeCSetInfo;
			hr = m_multiLanguage->GetCharsetInfo(charsetBSTR, &mimeCSetInfo);
			if (SUCCEEDED(hr))
			{
				m_bindInfo.dwCodePage = mimeCSetInfo.uiInternetEncoding;
			}
		}
	}

	return hr;
}
//Unicode and UTF-8 strings might have specific preamble, which we might treat in a wrong way
//This method deletes that preamble.
HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::DeleteStringPreamble(CString& input)
{
	if ((BYTE)input.GetAt(0) == 0xEF && 
		(BYTE)input.GetAt(1) == 0xBB &&		//utf-8 preamble
		(BYTE)input.GetAt(2) == 0xBF)
	{
		input.Delete(0, 3);
	}
	if (input.GetAt(0) == 0xFF && 
		input.GetAt(1) == 0xFE)			//unicode preamble
	{
		input.Delete(0, 2);
	}
	return S_OK;
}

//Converts input string to unicode, using m_bindInfo.dwCodePage as codepage of input string
HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::ConvertToUnicode(CString& input)
{
	HRESULT hr = S_FALSE;

	if (m_multiLanguage != NULL)
	{
		DWORD context = 0;
		UINT sourceLen = input.GetLength();
		UINT unicodeLength = input.GetLength();
		hr = m_multiLanguage->ConvertStringToUnicode(&context, m_bindInfo.dwCodePage, input.GetBufferSetLength(sourceLen), 
			&sourceLen, m_dataBuffer.GetBufferSetLength(unicodeLength), &unicodeLength);
		m_dataBuffer.GetBufferSetLength(unicodeLength);
	}
	if (FAILED(hr))
	{
		int len = MultiByteToWideChar(m_bindInfo.dwCodePage, 0, input, -1, NULL, 0);
		if (len > 0)
		{
			wchar_t *ptr = m_dataBuffer.GetBufferSetLength(len - 1);
			len = MultiByteToWideChar(m_bindInfo.dwCodePage, 0, input, -1, ptr, len);
		}
	}

    return hr;
}

//------------------------------------------------------
// IInternetProtocol based on IInternetProtocolRoot
//-------------------------------------------------------
HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::Read(
	/* [length_is][size_is][out][in] */ void __RPC_FAR *pv,
	/* [in] */ ULONG cb,
	/* [out] */ ULONG __RPC_FAR *pcbRead)
{
#ifdef CATCHALL
	try {
#endif
		bool bWhiteListDomain = false;
        HRESULT hr;

		// Check if we have a pointer to default implementation. We can't do much if we don't have that.
		if (!m_pIncomingProt)
		{
			return S_FALSE;
        }
        // If we've detected that we should not parse this file, just redirect
        if (!m_localClient || !m_bIsActive || !m_isHTML || m_curUrl.GetLength() > 9 && m_curUrl.Left(10) == "javascript:")
		{
			return m_pIncomingProt->Read((void*)pv, cb, pcbRead);
		}

		// If this is a first buffer to be read, we detect if it's HTML etc
		if (m_bFirstRead)
		{
			CString tmpPage = "";
    		char* pbuff = new char[cb];
			hr = m_pIncomingProt->Read((void*)pbuff, cb, pcbRead);
			if (*pcbRead == 0 && hr == S_FALSE)
			{
				// The Read has finished. But if we were pending before, we should gracefuly finish
				if (!m_isPending)
				{
					return hr;
				}
			}
			else
			{
				// So we got first buffer of data. Let's see if we think it's ok for us to parse it
				CString testString((char*)pbuff, *pcbRead);
				testString = testString.MakeLower();
				m_isHTML = true;

				// If there's no <html tag in first buffer we skip the whole file
				if (testString.Find("<html") < 0)
				{
					if (!m_isPending)
					{
						m_isHTML = false;
						memcpy(pv, pbuff, *pcbRead);

                        return hr;
					}
				}
			}

			UINT downloaded = 0;

			// This is HTML, so we buffer the whole file into m_byteBuffer, for later parsing
			while (*pcbRead > 0 || hr == E_PENDING)
			{
				DWORD written = 0;
				CString buffer(pbuff, *pcbRead);
				m_byteBuffer->Write(pbuff, *pcbRead);
				hr = m_pIncomingProt->Read((void*)pbuff, cb, pcbRead);
				if (hr == E_PENDING)
				{
					m_byteBuffer->Write(pbuff, *pcbRead);
					*pcbRead = 0;
					m_isPending = true;

                    return hr;
				}
				else
				{
					m_isPending = false;
				}
				downloaded += *pcbRead;
			} 
			delete [] pbuff;

			m_bFirstRead = false;
			m_posPointer = 0;

			// We have buffered the whole HTML, not let's parse it
			CComPtr<IHTMLDocument2> parser;
			CComPtr<IAdPluginListener> container;

			// copy the content of the bytebuffer into a streambuffer
			// we use the streambuffer for charset detection and conversion
			// the bytebuffer is kept to use for the bypass function if deemed necessary
			// the string is null terminated in the constructor, so we can just pass the array without any modifications
			CString streamBuffer(m_byteBuffer->m_byteBuffer, m_byteBuffer->m_pos);

            bool bFailedParsing = false;

			if (m_pOIBindInfo != NULL)
			{
				LPOLESTR pszURL = NULL;

				DWORD bindf = 0;
				m_bindInfo.cbSize = sizeof(BINDINFO);
				hr = m_pOIBindInfo->GetBindInfo(&bindf, &m_bindInfo);

				DeleteStringPreamble(streamBuffer);

				hr = DetectCharset(streamBuffer);
                if (SUCCEEDED(hr))
                {
				    // Converts to unicode - output are copied into m_databuffer
				    hr = ConvertToUnicode(streamBuffer);
                    if (FAILED(hr) || hr == S_FALSE)
                    {
                        bFailedParsing = true;
                    }
                }
                else
                {
                    bFailedParsing = true;
                }
			}
			else 
			{
				m_dataBuffer = streamBuffer;
			}

			m_parsedBuffer = m_dataBuffer;
			m_dataBuffer.TrimLeft();

			//commented this section out - the reason for this section is that 
			//we want to avoid that ajax request are searched for ads and ads are replaced purely for performance
			//if (((m_dataBuffer.Find(CStringW("<html")) < 0) && (m_dataBuffer.Find(CStringW("<HTML")) < 0)) || 
			//	((m_dataBuffer.Find(CStringW("</html>")) < 0) && (m_dataBuffer.Find(CStringW("</HTML>")) < 0)))		//skip stuff we don't want to handle
			//{
			//	//this has already been done
			//	//m_dataBuffer.CopyData(m_parsedBuffer);
			//}
			//else
			//{

			//a check to be sure that we are hanling html
			//if not it is undefined what the parser will do
			//int docStartPos = -1;
			//CString loweredSource  = streamBuffer.MakeLower();
			//streamBuffer = streamBuffer.MakeLower();
			//int docTypePos = loweredSource.Find("<!doctype");
			//int htmlPos =  loweredSource.Find("<html");
			//if (docTypePos < htmlPos)
			//	docStartPos = docTypePos;
			//else
			//	docStartPos = htmlPos;
			//if ((docStartPos != textStart) && (docStartPos >= 0))
			//{
			//	m_parsedBuffer = loweredSource;
			//}
			//else
			//{
			//the parsed html are copied into the parsedbuffer
            if (bFailedParsing == false)
            {
				// Make the adrequest container ready
#ifdef PRODUCT_AIDONLINE
				m_adRequests = AdRequests(m_curUrl, m_detectedCharset, CAdPluginSettings::GetInstance()->GetValue(SETTING_PLUGIN_ID), m_localClient->GetDebugMode()); 
#endif // PRODUCT_AIDONLINE
				if (FAILED(ParseHtml()))
			    {
					// Initialize the adRequest
					m_parsedBuffer = m_dataBuffer;	
			    }
#ifdef PRODUCT_AIDONLINE
				else
				{
					// Now we detect the head and insert the global ad script into 
					// We want to insert the script as the first thing in the head section
					int headStart = -1;
					if ((headStart = max(m_parsedBuffer.Find("<head"), m_parsedBuffer.Find("<HEAD"))) >= 0)
					{
						if ((headStart = m_parsedBuffer.Find(">", headStart)) >= 0)
						{
							// Now we are ready to insert the script, we need to move by the >
							m_parsedBuffer.Insert(headStart + 1, m_adRequests.GetScriptSource());   
						}
					}
				}
#endif
			}
			//}		
			//}

			m_dataBuffer = "";
			m_bParsed = true;
		} 

		// If the data in buffer is unicode - the actual char is represented by 2 bytes. 
		// This is just to make sure we get even number of bytes
		if (cb & 0x01)
		{
			cb -= 1;
		}

		int myBufferLength = m_adsReplaced ? m_parsedBuffer.GetLength() : m_byteBuffer->m_pos;

		if (int(m_posPointer + cb) <= myBufferLength)
		{
			if (m_adsReplaced == 0)
			{
				memcpy(pv, m_byteBuffer->m_byteBuffer + m_posPointer, cb);
			}
			else
			{
				memcpy(pv, (void*)(((char*)m_parsedBuffer.GetBuffer()) + m_posPointer), cb);
			}

			*pcbRead = cb;
			m_posPointer += cb;
			return S_OK;
		}
		else
		{
			*pcbRead = m_adsReplaced ? m_parsedBuffer.GetLength() - m_posPointer : m_byteBuffer->m_pos - m_posPointer; 

			if (*pcbRead != 0)
			{
	    		if (m_adsReplaced == 0)
				{
					memcpy(pv, m_byteBuffer->m_byteBuffer + m_posPointer, *pcbRead);
				}
				else
				{
					memcpy(pv, (void*)(((char*)m_parsedBuffer.GetBuffer()) + m_posPointer), *pcbRead);
				}
			}

            m_posPointer = 0;
			if (m_pOIBindInfo != NULL)
			{
				m_pOIBindInfo.Release();
				m_pOIBindInfo = NULL;
			}
			if (m_pIncomingProt != NULL)
			{
				m_pIncomingProt->Release();
				m_pIncomingProt = NULL;
			}

            m_parsedBuffer = "";
			m_byteBuffer->m_pos = 0;
			m_bParsed = false;

			m_bFirstRead = true;

			return S_FALSE;
		}
#ifdef CATCHALL	
	} //end catch
	catch (std::runtime_error e)
	{
		Client::ReportError(__FILE__, __LINE__, e.what());
	}
	catch (...) 
	{
		Client::ReportError(__FILE__, __LINE__, "caught unknown exception in AidOnlineMimeFilter", "unknown url"); 
	}
#endif

	return S_FALSE;
}	


HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::Seek(
	/* [in] */ LARGE_INTEGER dlibMove,
	/* [in] */ DWORD dwOrigin,
	/* [out] */ ULARGE_INTEGER __RPC_FAR *plibNewPosition)
{
	return E_NOTIMPL;
}	


HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::LockRequest(
	/* [in] */ DWORD dwOptions)
{
	if (m_pIncomingProt == NULL)
	{
		return S_OK;
	}

    return m_pIncomingProt->LockRequest(dwOptions);
}	


HRESULT STDMETHODCALLTYPE CAdPluginMimeFilter::UnlockRequest(void)
{
	if (m_pIncomingProt == NULL)
	{
		return S_OK;
	}

    return m_pIncomingProt->UnlockRequest();	
}	


/////////////////////////////////////////////////////////////////////////////
// IInternetProtocolSink interface
STDMETHODIMP CAdPluginMimeFilter::Switch(PROTOCOLDATA __RPC_FAR *pProtocolData)
{
	if (!m_pOutgoingProtSink)
	{
		return E_FAIL;
	}

	return m_pOutgoingProtSink->Switch(pProtocolData);
}

STDMETHODIMP CAdPluginMimeFilter::ReportProgress(ULONG ulStatusCode, LPCWSTR szStatusText)
{
	if (!m_pOutgoingProtSink)
	{
		return E_FAIL;
	}

	return m_pOutgoingProtSink->ReportProgress(ulStatusCode, szStatusText);
}

STDMETHODIMP CAdPluginMimeFilter::ReportData(DWORD grfBSCF, ULONG ulProgress, ULONG ulProgressMax)
{
	if (!m_pOutgoingProtSink)
	{
		return E_FAIL;
	}

	return m_pOutgoingProtSink->ReportData(grfBSCF, ulProgress, ulProgressMax);
}


STDMETHODIMP CAdPluginMimeFilter::ReportResult(HRESULT hrResult, DWORD dwError, LPCWSTR szResult)
{
	if (!m_pOutgoingProtSink)
	{
		return E_FAIL;
	}

	return m_pOutgoingProtSink->ReportResult(hrResult, dwError, szResult);
}


///////////////////////////////////////////////////////////////////////////
// Parsing routin implementation
STDMETHODIMP CAdPluginMimeFilter::CreateParser(IHTMLDocument2** parser, IAdPluginListener** container)
{
	HRESULT hr = S_OK;

	CoInitialize(NULL);
	if (*parser == NULL)
	{
		hr = CoCreateInstance(CLSID_HTMLDocument, NULL, CLSCTX_ALL, IID_IHTMLDocument2, (void**)parser);
		if (FAILED(hr))
        {
			return hr;
        }
	}

	CComPtr<IOleObject> pOleObject;
	
	hr = (*parser)->QueryInterface(IID_IOleObject, (LPVOID*)&pOleObject);
	if (FAILED(hr))  
	{  
		return hr;
	}  
	if (*container == NULL)
	{
		hr = CoCreateInstance(CLSID_AdPluginListener, NULL, CLSCTX_ALL, IID_IAdPluginListener, (void**)container);
		if (FAILED(hr))
        {
			return hr;
        }
	}
	hr = pOleObject->SetClientSite((IOleClientSite*)*container);  	  
	pOleObject.Release();  
	CComPtr<IOleControl> pOleControl;
	hr = (*parser)->QueryInterface(IID_IOleControl, (LPVOID*)&pOleControl);  	  
    if (FAILED(hr))
	{  
		return hr;
	}  		  
	hr = pOleControl->OnAmbientPropertyChange(DISPID_AMBIENT_CHARSET);    

	pOleControl.Release(); 

	return hr;
}

// see http://www.codeproject.com/KB/IP/parse_html.aspx?fid=3219&df=90&mpp=25&noise=3&sort=Position&view=Quick&fr=26&select=1557934
STDMETHODIMP CAdPluginMimeFilter::WriteToParser(CStringW& html, IHTMLDocument2* parser)
{
	USES_CONVERSION;

	CString testHtml = " ";
	CStringW lowerCased = html;
	lowerCased = lowerCased.MakeLower();
	int objectStart = 0;
	while ((objectStart = lowerCased.Find(L"<object ", objectStart)) > 0)
	{
		int objectEnd = lowerCased.Find(L">", objectStart);
		if (objectEnd > 0)
		{
			CStringW objectDeclaration = lowerCased.Mid(objectStart, objectEnd - objectStart);
			int dataStart = 0;
			if ((dataStart = objectDeclaration.Find(L"data=")) > 0)
			{
				int dataEnd = objectDeclaration.Find(objectDeclaration.GetAt(dataStart + 5), dataStart + 6);
				if (dataEnd > 0)
				{
					CStringW dataValue = objectDeclaration.Mid(dataStart + 6, dataEnd - dataStart - 6);
					objectEnd += 1;
					html.Insert(objectEnd, L"<PARAM name=\"data\" value=\"" + dataValue + "\"/>");
					lowerCased.Insert(objectEnd, L"<PARAM name=\"data\" value=\"" + dataValue + "\"/>");
					html.Delete(objectStart + dataStart, dataEnd - dataStart);	
					lowerCased.Delete(objectStart + dataStart, dataEnd - dataStart);
				}
			}
		}
		objectStart ++;
	}

	VARIANT* param;

	// Creates a new one-dimensional array
	SAFEARRAY* sfArray = SafeArrayCreateVector(VT_VARIANT, 0, 1);

	if (sfArray == NULL || parser == NULL) 
    {
		return S_FALSE;
	}
	SafeArrayAccessData(sfArray, (LPVOID*)&param);

	CComBSTR bstr(html);
	VARIANT strVar;
	strVar.bstrVal = bstr.Detach();
	strVar.vt = VT_BSTR;
	*param = strVar;
	SafeArrayUnaccessData(sfArray);
	parser->write(sfArray);

	if (sfArray != NULL) 
	{
		SafeArrayDestroy(sfArray);
	}

	return S_OK;
}

STDMETHODIMP CAdPluginMimeFilter::ParseHtml()
{
	CComPtr<IHTMLDocument2> parser;
	CComPtr<IAdPluginListener> container;	
	if (FAILED(CreateParser(&parser, &container)))
	{
		return S_FALSE;
	}

	if (FAILED(WriteToParser(m_dataBuffer, parser)))
	{
		return S_FALSE;
	}

	if (GetParsedHtml(&m_parsedBuffer, parser) != S_OK)
	{
		return S_FALSE;
	}

    // Add document type to parsed HTML
	int start = m_dataBuffer.Find(L"<html");
	CStringW doctype = "";
	if (start > 0)
	{
		doctype = m_dataBuffer.Left(start);
	}
	m_parsedBuffer = (CString)doctype + m_parsedBuffer;

	parser->clear();
	parser.Release();
	container.Release();

	return S_OK;
}


STDMETHODIMP CAdPluginMimeFilter::ParseHtmlElement(IHTMLElement* pElement, IHTMLElement* pParent, const CString& url, bool isNoScript)
{
    // Handle element
    CComBSTR tagName;
    tagName.Empty();
    HRESULT hr = pElement->get_tagName(&tagName);
    if (FAILED(hr))
    {
        return S_OK;
    }

    tagName.ToLower();

    if (tagName == L"noscript")
    {
        isNoScript = true;
    }

//    DEBUG_PARSER("ParseHtmlElement element " + (CString)tagName);

    // Loop through all children
	CComPtr<IDispatch> pChildrenDispatch;
    hr = pElement->get_children(&pChildrenDispatch);

    if (SUCCEEDED(hr) && pChildrenDispatch != NULL)
    {
    	CComPtr<IHTMLElementCollection> pChildren;
        hr = pChildrenDispatch->QueryInterface(IID_IHTMLElementCollection, (void**)&pChildren);

        if (SUCCEEDED(hr) && pChildren)
        {
            // Get first child
	        CComPtr<IDispatch> pChildDispatch;
	        CComVariant index(0);
	        CComVariant retIndex(0);
            hr = pChildren->item(index, retIndex, &pChildDispatch);

            if (SUCCEEDED(hr) && pChildDispatch != NULL)
	        {
		        CComPtr<IHTMLDOMNode> pChildNode;
		        hr = pChildDispatch->QueryInterface(IID_IHTMLDOMNode, (void**)&pChildNode);

//                DEBUG_PARSER("ParseHtmlElement element children BEGIN");
		        
		        while (pChildNode != NULL)
		        {		            
		            // Prepare next sibling		            
    		        CComPtr<IHTMLDOMNode> pNextChildNode;
		            pChildNode->get_nextSibling(&pNextChildNode);

		            CComPtr<IHTMLElement> pChildElement;
		            if (SUCCEEDED(pChildNode->QueryInterface(IID_IHTMLElement, (void**)&pChildElement)) && pChildElement)
		            {
                        ParseHtmlElement(pChildElement, pElement, url, isNoScript);
                    }

                    // Set next sibling
                    pChildNode = pNextChildNode;
		        }

//                DEBUG_PARSER("ParseHtmlElement element children END");
	        }
/*	        
            long length = 0;
	        pChildren->get_length(&length);

            for (long i = 0; i < length; i++)
	        {
		        CComPtr<IDispatch> pChildDispatch;
		        CComVariant index(i);
		        CComVariant retIndex(0);
                hr = pChildren->item(index, retIndex, &pChildDispatch);

                if (SUCCEEDED(hr) && pChildDispatch != NULL)
		        {
			        CComPtr<IHTMLElement> pChild;
			        hr = pChildDispatch->QueryInterface(IID_IHTMLElement, (void**)&pChild);

                    if (SUCCEEDED(hr) && pChild != NULL)
			        {
                        ParseHtmlElement(pChild, pElement, url, isNoScript);

                        pChild.Release();
                    }
                    else
                    {
                        DEBUG_PARSER("!!! No child - " + (CString)tagName);
                    }
                }
                else
                {
                    DEBUG_PARSER("!!! No child dispatch");
                }
            }
*/
            pChildren.Release();
        }
    }

    if (tagName == L"script" || tagName == L"iframe" || tagName == L"img" || tagName == L"object" || tagName == L"a")
    {
        HandleHtmlElement(pElement, pParent, tagName, url, isNoScript);
    }

    return S_OK;
}


//Loops throught the list of all elements in html and finds urls that match filterlists
STDMETHODIMP CAdPluginMimeFilter::GetParsedHtml(CString *html, IHTMLDocument2* parser)
{
    DEBUG_PARSER("--------------------------------------------------------------------------------");
    DEBUG_PARSER("Parse HTML url:" + m_curUrl);
    DEBUG_PARSER("--------------------------------------------------------------------------------");

    // Iterate through all elements in body
    CComPtr<IHTMLElement> pBody;
    HRESULT hr = parser->get_body(&pBody);

	if (SUCCEEDED(hr) && pBody != NULL)
	{
        ParseHtmlElement(pBody, NULL, m_curUrl, false);

        pBody.Release();
	}

    // Copy HTML model to string
	CComBSTR parsedHtmlBstr = NULL;

    CComPtr<IHTMLDocument3> pHtmlDocument3;
    hr = parser->QueryInterface(IID_IHTMLDocument3, (void**)&pHtmlDocument3);

    if (SUCCEEDED(hr) && pHtmlDocument3 != NULL)
	{
	    CComPtr<IHTMLElement> pDocElement;
		hr = pHtmlDocument3->get_documentElement(&pDocElement); 

        if (SUCCEEDED(hr) && pDocElement != NULL)
		{
			hr = pDocElement->get_outerHTML(&parsedHtmlBstr);
			if (FAILED(hr))
			{
				pDocElement.Release();
				pHtmlDocument3.Release();

				return S_FALSE;
			}
			pDocElement.Release();
		}
		pHtmlDocument3.Release();
	}

	if (parsedHtmlBstr == NULL)
	{
		*html = "";
		return S_FALSE;
	}

    DWORD written = 0;
	DWORD context = 0;
	CStringW tempBuf = parsedHtmlBstr;
	INT sourceLen = wcslen(parsedHtmlBstr);
	INT bufSize = sourceLen + 1000;
	hr = S_OK;
	if (m_multiLanguage == NULL)
	{
		CoInitialize(NULL);
		hr = CoCreateInstance(CLSID_CMultiLanguage, NULL, CLSCTX_INPROC, IID_IMultiLanguage, (void**)&m_multiLanguage);
	}
	if (SUCCEEDED(S_OK))
	{
		CComPtr<IMultiLanguage2> m_multiLanguage2;
		m_multiLanguage.QueryInterface(&m_multiLanguage2);
		if (m_multiLanguage2 != NULL)
		{
			html->Empty();

            bufSize = 0;
			hr = m_multiLanguage2->ConvertStringFromUnicodeEx(&context, m_bindInfo.dwCodePage, parsedHtmlBstr, 
				(UINT*)&sourceLen, NULL, (UINT*)&bufSize, 
				MLCONVCHARF_ENTITIZE | MLCONVCHARF_NCR_ENTITIZE, NULL);

			hr = m_multiLanguage2->ConvertStringFromUnicodeEx(&context, m_bindInfo.dwCodePage, parsedHtmlBstr, 
				(UINT*)&sourceLen, html->GetBufferSetLength(bufSize), (UINT*)&bufSize, 
				MLCONVCHARF_ENTITIZE | MLCONVCHARF_NCR_ENTITIZE, NULL);
		}
	}

    if (FAILED(hr))
	{
		*html = "";
		return S_FALSE;
    }

    return S_OK;
}


///this method removes the element supplied in pElement
///and returns the element we insert instead in elementInstead
///we insert a script element with the id computed as 'm_adRequests.GetKey() + divid'
///the method uses the following algorithm
///elements in head are handled specially, they are only removed, and no element for a new ad is returned
///and no replacement element is returned
///1. are we in head
///		yes: 
///			remove pElement for the htmldocument
///		no: insert a new element 'elementInstead' before 'pElement'
///			set attribute id = 'm_adRequests.GetKey() + divid' on elementInstead
///			remove pElement for the htmldocument
STDMETHODIMP CAdPluginMimeFilter::RemoveElement(IHTMLElement* pElement, IHTMLElement* pParent, IHTMLElement** elementInstead, int divid)
{
	USES_CONVERSION;

	CComPtr<IHTMLDOMNode> node, nodeToRemove;
	CComPtr<IHTMLDOMNode> previousNode;

#ifdef PRODUCT_AIDONLINE
	CComPtr<IHTMLElement> newScriptElement;
	CComPtr<IHTMLDOMNode> newScriptElementDom;
	CComPtr<IHTMLDOMNode> nodeInserted;
#endif // PRODUCT_AIDONLINE

	CComPtr<IDispatch> documentHolder;
	CComPtr<IHTMLDocument2> document; 

	HRESULT hr;

	if (pElement == NULL)
	{
		return E_INVALIDARG;
	}

	// We need to convert the pElement to a domNode to remove it
	hr = pElement->QueryInterface(IID_IHTMLDOMNode, (void**)&nodeToRemove);
	if (FAILED(hr) || nodeToRemove == NULL)
	{
		return hr;
	}

#ifdef PRODUCT_AIDONLINE

	// insert the new element and set the attribute id
	// get the document
	if (FAILED(hr = pElement->get_document(&documentHolder)) || documentHolder == NULL)
	{
		return hr;	
	}
	if (FAILED(hr = documentHolder->QueryInterface(IID_IHTMLDocument, (void**)&document)) || document == NULL)
	{
		return hr;
	}

	// put display:none on the element, if it's inside the <HEAD>
	// here we just disable the element, and does not return an element for insertion later
	bool inHead = false;
	if (pParent != NULL)
	{
		CComBSTR parentTag;
		pParent->get_tagName(&parentTag);
		parentTag.ToLower();
		if (parentTag == "head")
		{
			inHead = true;
		}
	}

	// now create a new element
	if (FAILED(hr = document->createElement(A2BSTR("script"), &newScriptElement)) || newScriptElement == NULL)
	{
		return hr;
	}

	// convert to dom
	if (FAILED(hr = newScriptElement->QueryInterface(IID_IHTMLDOMNode, (void**)&newScriptElementDom)) || newScriptElementDom == NULL)
	{
		return hr;
	}

	// get the parent
	CComPtr<IHTMLDOMNode> pParentNodeDom;
	if (FAILED(hr = nodeToRemove->get_parentNode(&pParentNodeDom)) || pParentNodeDom == NULL)
	{
		return hr;
	}

	// do not insert the new element if we are in head
	// no ads should be inserted here, also no new node should be fetched out
	CComVariant variant(nodeToRemove);
	if (!inHead)
	{
		if (FAILED((hr = pParentNodeDom->insertBefore(newScriptElementDom, variant, &nodeInserted))) || nodeInserted == NULL)
		{
			return hr;
		}
		
		if (FAILED(hr = nodeInserted->QueryInterface(IID_IHTMLElement, (void**)elementInstead)) || *elementInstead == NULL)
		{
			return hr;
		}

		VARIANT stringAttribute;
		char divIDPrinted[200];
		wsprintf(divIDPrinted, "%s%d", m_adRequests.GetKey(), divid);
		stringAttribute.vt = VT_BSTR;
		stringAttribute.bstrVal = A2BSTR(divIDPrinted); 
		BSTR attribute = A2BSTR("id");
		(*elementInstead)->setAttribute(attribute, stringAttribute, 0); 
	}

#endif // PRODUCT_AIDONLINE

	// now we are ready to remove the supplied node
	hr = nodeToRemove->removeNode(VARIANT_TRUE, &node);
	if ((FAILED(hr)))
	{
		return hr ;
	}

	return hr;
}

#ifdef PRODUCT_AIDONLINE

CString CAdPluginMimeFilter::GetZoneID(CString width, CString height)
{
	if (width == "" || height == "")
    {
		return "0";
    }

	CString widthHeight = width + "x" + height;
	//these arrays are based on code of FF plugin from aidonlinedata.js 
	CString widthHeightWhitelist [] = { "88x31", "120x60", "120x90", "120x240", "120x600", "125x125", "160x600", "180x150", "234x60", "240x400", "250x250", "300x250", "336x280", "468x60", "728x90", "930x180", "MAX" };
	CString zoneIDs [] = {"7", "5", "4", "9", "2", "8", "15", "10", "6", "13", "14", "11", "12", "1", "3", "31", "31"};
    int nZoneIDs = sizeof(zoneIDs)/sizeof(CString);
    for (int i = 0; i < nZoneIDs; i++)
	{
		if (widthHeight == widthHeightWhitelist[i])
        {
			return zoneIDs[i];
        }
	}
	return "0";
}

#endif

void CAdPluginMimeFilter::HandleScript(IHTMLElement* pElement, CString script, const CString& url)
{
    script.Replace("/ ", "/");
    script.Replace(" /", "/");
    script.Replace("< ", "<");
    script.Replace(" <", "<");
    script.Replace("> ", ">");
    script.Replace(" >", ">");
    script.Replace(" +", "+");
    script.Replace("+ ", "+");
    script.Replace(" =", "=");
    script.Replace("= ", "=");

#ifdef DEBUG_PARSER
    CString debugScript = script;

    debugScript.Remove('\t');
    debugScript.Replace("  ", " ");
    debugScript.Replace("\n\r", "\n");
    debugScript.Replace("\r\n", "\n");
    debugScript.Replace("\n\n", "\n");
    debugScript.Replace("\n ", "\n");

    debugScript.TrimLeft('\n');
    debugScript.TrimRight('\n');

    debugScript.Replace("\n", "\n  ");

//    DEBUG_PARSER("Parsing script:\n\n  " + debugScript + "\n");
#endif

    std::map<CString, int> contentTypeMap;
    contentTypeMap["script"] = CFilter::contentTypeScript;
    contentTypeMap["img"] = CFilter::contentTypeImage;
    contentTypeMap["object"] = CFilter::contentTypeObject;
    contentTypeMap["iframe"] = CFilter::contentTypeSubdocument;
    contentTypeMap["a"] = CFilter::contentTypeOther;

    bool bIsModifiedScript = false;

    CString srcString[4] = { "src=", "href=", "SRC=", "HREF=" };
    
    for (int type = 0; type < 4; type++)
    {
	    int srcStartPos = 0;

        // Find all elements containing a scr attribute
	    while ((srcStartPos = script.Find(srcString[type], srcStartPos)) >= 0)
	    {
    	    int scriptLength = script.GetLength();

            srcStartPos += srcString[type].GetLength();
            
            // Find src delimiter
            int srcDelimiterPos = srcStartPos;
            while (!isalpha(script.GetAt(srcDelimiterPos)))
            {
                srcDelimiterPos++;
            }
            CString srcDelimiter = script.Mid(srcStartPos, srcDelimiterPos - srcStartPos);
            srcStartPos = srcDelimiterPos;

            // Find src
		    int srcEndPos = 0;
		    if (srcDelimiter.IsEmpty())
		    {
    		    srcEndPos = script.Find(' ', srcStartPos);
		    }
		    else
		    {
    		    srcEndPos = script.Find(srcDelimiter, srcStartPos);
		    }
		    if (srcEndPos > 0)
		    {
			    CString elSrc = script.Right(scriptLength - srcStartPos);
			    elSrc = elSrc.Left(srcEndPos - srcStartPos);

//                DEBUG_PARSER("- Element src:" + elSrc);
                
                // Find start of node
			    CString nodeStart = script.Left(srcStartPos);
			    int startNodeStartPos = nodeStart.ReverseFind('<');
			    if (startNodeStartPos < 0) 
			    {
			        continue;
                }

                // Find node tag
			    int tagEndPos = nodeStart.Find(' ', startNodeStartPos);
			    if (tagEndPos < 0)
			    {
    			    continue;
			    }
			    CString tag = nodeStart.Left(tagEndPos);
			    tag = tag.Right(tag.GetLength() - startNodeStartPos - 1).MakeLower();
			    tag.Replace("'+'", "");

//                DEBUG_PARSER("- Element tag:" + tag);

                // Only continue if the tag is possibly containing an ad.
                if (contentTypeMap.find(tag) == contentTypeMap.end())
                {
                    continue;
                }

                // Should we block?
                if (m_localClient->ShouldBlock(elSrc, contentTypeMap[tag], url))
			    {
			        CString nodeEnd;

			        // Find while start node
			        int startNodeEndPos = script.Find('>', startNodeStartPos);
			        if (startNodeEndPos < 0)
			        {
    			        continue;
			        }			    
			        nodeStart = script.Mid(startNodeStartPos, startNodeEndPos - startNodeStartPos + 1);
    			    
//                    DEBUG_PARSER("- Element start node:" + nodeStart);
                    
                    bool bIsNodeComplete = script.GetAt(startNodeEndPos - 1) == '/';
			        bool bHasEndNode = false;
    			
			        int endNodeStartPos = 0;

                    // Find end node
                    if (!bIsNodeComplete)
                    {
				        CString elEnd = script.Right(scriptLength - tagEndPos);

				        int endNodeEndPos = 0;
        				
				        while (!bHasEndNode && !bIsNodeComplete && (endNodeStartPos = elEnd.Find("</", endNodeStartPos)) >= 0)
				        {
				            endNodeEndPos = endNodeStartPos + 2;

				            bool bKeepTrying = true;
        				    
				            for (int i = 0; i < tag.GetLength() && bKeepTrying; i++)
				            {
                                // Skip non-alpha characters
				                while (!isalpha(elEnd.GetAt(endNodeEndPos)))
				                {
    				                endNodeEndPos++;
				                }

				                if (tag[i] != tolower(elEnd.GetAt(endNodeEndPos)))
				                {
				                    bKeepTrying = false;
				                    break;
				                }
        				        
				                endNodeEndPos++;
				            }

                            // Find end >
                            char ch = elEnd.GetAt(endNodeEndPos);
                            while (bKeepTrying && ch != '>')
                            {
                                if (!isalpha(ch))
                                {
                                    bKeepTrying = false;
                                    break;
                                }
                                
                                ch = elEnd.GetAt(++endNodeEndPos);
                            }

                            if (ch == '>')
                            {
                                // If end node follows start node, we have a complete node
                                if (endNodeStartPos == startNodeStartPos + nodeStart.GetLength())
                                {
                                    bIsNodeComplete = true;
                                    nodeStart = script.Mid(startNodeStartPos, endNodeEndPos - startNodeStartPos + 1);
                                    endNodeStartPos = -1;
                                    
//                                    DEBUG_PARSER("- Element end node is combined with start node");
                                }
                                else
                                {
                                    bHasEndNode = true;

				                    nodeEnd = elEnd.Mid(endNodeStartPos, endNodeEndPos - endNodeStartPos + 1);
				                    // Set position in script
				                    endNodeStartPos = script.Find(nodeEnd, startNodeStartPos);

//                                    DEBUG_PARSER("- Element end node:" + nodeEnd);
                                }
                            }
                            // Skip this tag and try to find another
			                else
			                {
			                    endNodeStartPos += 2;
			                }
				        }

                        // Image is complete, if no end tag is found
                        if (!bHasEndNode && tag == L"img")
                        {
                            bIsNodeComplete = true;
                        }
                    }

                    if (bIsNodeComplete)
                    {
//                        DEBUG_PARSER("- Element complete node:" + nodeStart);
                    }
                    
                    // Delete element
                    if (bIsNodeComplete || bHasEndNode)
                    {
                        // Remove end node
                        if (endNodeStartPos > 0)
                        {
                            script.Delete(endNodeStartPos, nodeEnd.GetLength());
                        }

                        // Remove start node
                        script.Delete(startNodeStartPos, nodeStart.GetLength());

                        bIsModifiedScript = true;
                        
                        // Start searching next src from same position
                        srcStartPos = startNodeStartPos;
                    }
    /*
					    if (removedScript.Find("style=\"display:none\"") > 0)
					    {
						    bRemoveTag = false;
					    }
					    else
					    {
						    script.Delete(insertPos, insertEndPos);

						    // Don't move element and add style=display:none
						    // Not for script elements, as this produces double ads
						    if (tagName != "script")
						    {
							    int styleInsertPos = 0;
							    if ((styleInsertPos = removedScript.Find(">")) > 0)
							    {
								    removedScript.Insert(styleInsertPos, " style=\"display:none\"");
							    } 
							    script.Insert(insertPos, removedScript);
						    }

    #ifdef PRODUCT_AIDONLINE
						    rightStr = rightStr.Left(insertEndPos);
						    CString width;
						    CString height;
						    int widthStart = rightStr.Find("width=");
						    if (widthStart > 0)
						    {
							    widthStart = rightStr.Find("=", widthStart);
							    if (widthStart > 0)
							    {
								    CString widthString = rightStr.Right(rightStr.GetLength() - widthStart);
								    widthString = widthString.TrimLeft(" =");
								    int widthEnd = widthString.Find(" ");
								    if (widthEnd > 0)
								    {
									    widthString = widthString.Left(widthEnd);
									    widthString.Trim("\"' ");
									    width = widthString;
								    }

							    }
						    }
						    int heightStart = rightStr.Find("height");
						    if (heightStart > 0)
						    {
							    heightStart = rightStr.Find("=", heightStart);
							    if (heightStart > 0)
							    {
								    CString heightString = rightStr.Right(rightStr.GetLength() - heightStart);
								    heightString = heightString.TrimLeft(" =");
								    int heightEnd = heightString.Find(" ");
								    if (heightEnd > 0)
								    {
									    heightString = heightString.Left(heightEnd);
									    heightString.Trim("\"' ");
									    height = heightString;
								    }
							    }
						    }

						    if (m_localClient->GetMode() == LocalClient::RUNNING_MODE::MODE_REPLACE)
						    {
							    CString insertedDiv;
							    CString dividString;
							    dividString = itoa(*divid, dividString.GetBufferSetLength(10), 10);
							    dividString = m_adRequests.GetKey() + dividString;

							    //get a pair of scr/content for the script
							    std::pair<CString,CString> adReturned = m_adRequests.AddDetectedAd(GetZoneID(width, height), inscriptUrl, dividString, *divid + 1);
							    insertedDiv += "<script id=\"" + dividString + "\" defer=\"defer\" >" 
								    + adReturned.second
								    + "</scr'+'ipt>";

							    script.Insert(insertPos, insertedDiv);
						        //krak - map fix
						        startPos += insertedDiv.GetLength() + removedScript.GetLength();
						        (*divid) ++;
						    }
    #endif // PRODUCT_AIDONLINE
					    }
				    }
				    else
				    {
					    bRemoveWholeScript= true;
				    }
    */
			    }
		    }
        }
    }

    // Update script
    if (bIsModifiedScript)
	{
		CComPtr<IHTMLScriptElement> scriptEl;
		HRESULT hr = pElement->QueryInterface(&scriptEl);

		if (SUCCEEDED(hr) && scriptEl != NULL)
		{
			scriptEl->put_text((CComBSTR)script);
		}
	}

#ifdef DEBUG_PARSER
    if (bIsModifiedScript)
    {
        CString debugScript = script;

        debugScript.Remove('\t');
        debugScript.Replace("  ", " ");
        debugScript.Replace("\n\r", "\n");
        debugScript.Replace("\r\n", "\n");
        debugScript.Replace("\n\n", "\n");
        debugScript.Replace("\n ", "\n");

        debugScript.TrimLeft('\n');
        debugScript.TrimRight('\n');

        debugScript.Replace("\n", "\n  ");

//        DEBUG_PARSER("Modified script:\n\n  " + debugScript + "\n");
    }
#endif
}

#ifdef PRODUCT_AIDONLINE

void CAdPluginMimeFilter::GetAdSize(IHTMLElement* pElement, CString srcUrl, CString& width, CString& height)
{
    CComBSTR widthAttribute = "width";
	CComBSTR heightAttribute = "height";
	CComVariant widthBSTR;
	CComVariant heightBSTR;

    if (srcUrl == ADSENSEURL)
	{
		CComPtr<IHTMLDOMNode> domNode;
		CComPtr<IHTMLDOMNode> setupNode;
		pElement->QueryInterface(IID_IHTMLDOMNode, (void**)&domNode);
		if (domNode != NULL)
		{
			domNode->get_previousSibling(&setupNode);
			if (setupNode != NULL)
			{
				CComPtr<IHTMLElement> setupElement;
				setupNode.QueryInterface(&setupElement);
				if (setupElement != NULL)
				{
					CComBSTR setupInnerHtml;
					setupElement->get_innerHTML(&setupInnerHtml);
					if (setupInnerHtml != "")
					{
						CString setupScript = setupInnerHtml;
						int startPos = setupScript.Find("google_image_size");
						if (startPos >= 0)
						{
							startPos = setupScript.Find("=", startPos);
							if (startPos > 0)
							{
								setupScript = setupScript.Right(setupScript.GetLength() - startPos - 1);
								setupScript.TrimLeft(" ");
								int endPos = setupScript.Find(setupScript.GetAt(0), 1);
								CString widthAndHeight= setupScript.Left(endPos);
								widthAndHeight = widthAndHeight.MakeLower();
								widthAndHeight = widthAndHeight.TrimLeft("\"'");
								int seperatorPos = widthAndHeight.Find("x");
								width = widthAndHeight.Left(seperatorPos);
								height = widthAndHeight.Right(widthAndHeight.GetLength() - seperatorPos - 1);
								CComPtr<IHTMLDOMNode> node;
								setupNode->removeNode(VARIANT_TRUE, &node);

							}
						}
						else
						{
							int startPos = setupScript.Find("google_ad_width");
							if (startPos > 0)
							{
								startPos = setupScript.Find("=", startPos);
								if (startPos > 0)
								{
									setupScript = setupScript.Right(setupScript.GetLength() - startPos - 1);
									setupScript.TrimLeft(" ");
									int endPos = setupScript.Find(";");
									width = setupScript.Left(endPos);
								}
							}
							startPos = setupScript.Find("google_ad_height");
							if (startPos > 0)
							{
								startPos = setupScript.Find("=", startPos);
								if (startPos > 0)
								{
									setupScript = setupScript.Right(setupScript.GetLength() - startPos - 1);
									setupScript.TrimLeft(" ");
									int endPos = setupScript.Find(";");
									height = setupScript.Left(endPos);
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		HRESULT hr = pElement->getAttribute(widthAttribute, 0, &widthBSTR);

        if (FAILED(hr) || widthBSTR.vt == VT_NULL) 
		{
			width = "";
		}
		else
		{
			width = (CString)widthBSTR;
		}

        hr = pElement->getAttribute(heightAttribute, 0, &heightBSTR);

        if (FAILED(hr) || heightBSTR.vt == VT_NULL)
		{
			height = "";
		}
		else
		{
			height = (CString)heightBSTR;
		}
	}
}

#endif

void CAdPluginMimeFilter::HandleHtmlElement(IHTMLElement* pElement, IHTMLElement* pParent, CComBSTR tag, const CString& url, bool isNoScript)
{
    // Get src attribute
    CComVariant src;
	HRESULT hr = pElement->getAttribute(L"src", 0, &src);		

	if (FAILED(hr) || src.vt == VT_NULL)
	{
        return;
    }
    CString srcUrl = (CString)src;

//    DEBUG_PARSER("Parsing element " + (CString)tag + " - src:" + (CString)src);

	bool bRemoveScript = false;

    std::map<CComBSTR, int> contentTypeMap;
    contentTypeMap["script"] = CFilter::contentTypeScript;
    contentTypeMap["img"] = CFilter::contentTypeImage;
    contentTypeMap["object"] = CFilter::contentTypeObject;
    contentTypeMap["iframe"] = CFilter::contentTypeSubdocument;
    contentTypeMap["a"] = CFilter::contentTypeOther;

    // Try to parse embedded script
    if (tag == L"script")
	{
		CComBSTR scriptSource;
		hr = pElement->get_innerHTML(&scriptSource);

        if (SUCCEEDED(hr) && scriptSource.Length() > 0)
		{
            HandleScript(pElement, (CString)scriptSource, url);
		}
	} 

    // Block element
	if (!srcUrl.IsEmpty() && m_localClient->ShouldBlock(srcUrl, contentTypeMap[tag], url))
	{
		CComPtr<IHTMLElement> replacementElement;

        hr = RemoveElement(pElement, pParent, &replacementElement, m_adsReplaced++);

#ifdef PRODUCT_ADBLOCKER

        return;
#else 

        if (CAdPluginSettings::GetInstance()->GetValue(SETTING_MODE) != "replace")
		{
			return;
		}

		if (replacementElement == NULL)
		{
			return;
		}

		CString width;
		CString height;
        GetAdSize(pElement, srcUrl, width, height);

		CString thisDivId;
		thisDivId = itoa(m_adsReplaced - 1, thisDivId.GetBufferSetLength(10), 10);
		thisDivId = m_adRequests.GetKey() + thisDivId;
		std::pair<CString,CString> replacement = m_adRequests.AddDetectedAd(GetZoneID(width, height), (CString)src.bstrVal, thisDivId, m_adsReplaced);

        // We do not insert the src element of the script
		if (replacementElement != NULL && !replacement.second.IsEmpty())
		{
			CComPtr<IHTMLScriptElement> pScript;
			if (SUCCEEDED(replacementElement.QueryInterface(&pScript)))
			{
				// Set the content of the script
				CComBSTR newSrc = (CComBSTR)replacement.second;			
				pScript->put_text(newSrc);
				
				// Set defer on the script
				pScript->put_defer(TRUE);
			}
		}

#endif // PRODUCT

    }
}

STDMETHODIMP CAdPluginMimeFilter::BeginningTransaction(LPCWSTR szURL, LPCWSTR szHeaders, DWORD dwReserved, LPWSTR *pszAdditionalHeaders)
{
    DEBUG_PARSER("BeginningTransaction url:" + szURL);

	if (pszAdditionalHeaders)
	{
		*pszAdditionalHeaders = 0;
	}

	return S_OK;
}

STDMETHODIMP CAdPluginMimeFilter::OnResponse(DWORD dwResponseCode, LPCWSTR szResponseHeaders, LPCWSTR szRequestHeaders, LPWSTR *pszAdditionalRequestHeaders)
{
	return S_OK;
}


STDMETHODIMP CAdPluginMimeFilter::Cleanup(IHTMLDocument2* parser, IAdPluginListener* container)
{
	if (parser != NULL)
	{
		CComPtr<IOleObject> pOleObject;
		HRESULT hr;
		if (FAILED(hr = parser->QueryInterface(IID_IOleObject, (LPVOID*)&pOleObject)))  
		{  
			return hr;
		}  
		pOleObject->SetClientSite(NULL);  	  
		pOleObject.Release();  
		parser->clear();
		parser->Release();
		parser = NULL;
	}
	if (container != NULL)
	{
		container->Release();
		container = NULL;
	}
	return E_NOTIMPL;
}
