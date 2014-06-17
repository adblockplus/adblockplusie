#include "PluginStdAfx.h"

#include "PluginWbPassThrough.h"
#include "PluginClient.h"
#include "PluginClientFactory.h"
#ifdef SUPPORT_FILTER
#include "PluginFilter.h"
#endif
#include "PluginSettings.h"
#include "PluginClass.h"
#include "PluginSystem.h"

#include "wtypes.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;



int WBPassthruSink::GetContentTypeFromMimeType(CString mimeType)
{
  if (mimeType.Find(L"image/") >= 0)
  {
    return CFilter::contentTypeImage;
  }
  if (mimeType.Find(L"text/css") >= 0)
  {
    return CFilter::contentTypeStyleSheet;
  }
  if ((mimeType.Find(L"application/javascript") >= 0) || (mimeType.Find(L"application/json") >= 0))
  {
    return CFilter::contentTypeScript;
  }
  if (mimeType.Find(L"application/x-shockwave-flash") >= 0)
  {
    return CFilter::contentTypeObject;
  }
  if (mimeType.Find(L"text/html") >= 0)
  {
    return CFilter::contentTypeSubdocument;
  }
  // It is important to have this check last, since it is rather generic, and might overlay text/html, for example
  if (mimeType.Find(L"xml") >= 0)
  {
    return CFilter::contentTypeXmlHttpRequest;
  }

  return CFilter::contentTypeAny;
}

int WBPassthruSink::GetContentTypeFromURL(CString src)
{
  CString srcExt = src;

  int pos = 0;
  if ((pos = src.Find('?')) > 0)
  {
    srcExt = src.Left(pos);
  }

  int lastDotIndex = srcExt.ReverseFind('.');
  if (lastDotIndex < 0)
    return CFilter::contentTypeAny;
  CString ext = srcExt.Mid(lastDotIndex);
  if (ext == L".jpg" || ext == L".gif" || ext == L".png" || ext == L".jpeg")
  {
    return CFilter::contentTypeImage;
  }
  else if (ext == L".css")
  {
    return CFilter::contentTypeStyleSheet;
  }
  else if (ext.Right(3) == L".js")
  {
    return CFilter::contentTypeScript;
  }
  else if (ext == L".xml")
  {
    return CFilter::contentTypeXmlHttpRequest;
  }
  else if (ext == L".swf")
  {
    return CFilter::contentTypeObject;
  }
  else if (ext == L".jsp" || ext == L".php" || ext == L".html")
  {
    return CFilter::contentTypeSubdocument;
  }
  else
  {
    return CFilter::contentTypeAny & ~CFilter::contentTypeSubdocument;
  }

}

int WBPassthruSink::GetContentType(CString mimeType, CString domain, CString src)
{
  // No referer or mime type
  // BINDSTRING_XDR_ORIGIN works only for IE v8+
  if (mimeType.IsEmpty() && domain.IsEmpty() && CPluginClient::GetInstance()->GetIEVersion() >= 8)
  {
    return CFilter::contentTypeXmlHttpRequest;
  }
  int contentType = GetContentTypeFromMimeType(mimeType);
  if (contentType == CFilter::contentTypeAny)
  {
    contentType = GetContentTypeFromURL(src);
  }
  return contentType;
}

////////////////////////////////////////////////////////////////////////////////////////
//WBPassthruSink
//Monitor and/or cancel every request and responde
//WB makes, including images, sounds, scripts, etc
////////////////////////////////////////////////////////////////////////////////////////
HRESULT WBPassthruSink::OnStart(LPCWSTR szUrl, IInternetProtocolSink *pOIProtSink,
                                IInternetBindInfo *pOIBindInfo, DWORD grfPI, HANDLE_PTR dwReserved,
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

  CString boundDomain;
  CString mimeType;
  LPOLESTR mime[10];
  if (pOIBindInfo)
  {
    ULONG resLen = 0;
    pOIBindInfo->GetBindString(BINDSTRING_ACCEPT_MIMES, mime, 10, &resLen);
    if (mime && resLen > 0)
    {
      mimeType.SetString(mime[0]);
    }
    LPOLESTR bindToObject = 0;
    pOIBindInfo->GetBindString(BINDSTRING_FLAG_BIND_TO_OBJECT, &bindToObject, 1, &resLen);
    LPOLESTR domainRetrieved = 0;
    if (resLen == 0 || wcscmp(bindToObject, L"FALSE") == 0)
    {   
      HRESULT hr = pOIBindInfo->GetBindString(BINDSTRING_XDR_ORIGIN, &domainRetrieved, 1, &resLen);
      
      if ((hr == S_OK) && domainRetrieved && (resLen > 0))
      {
        boundDomain.SetString(domainRetrieved);
      }
    }
  }

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
    else if (CPluginSettings::GetInstance()->IsPluginEnabled() && !client->IsWhitelistedUrl(std::wstring(documentUrl)))
    {
      boundDomain = tab->GetDocumentUrl();

      contentType = CFilter::contentTypeAny;

#ifdef SUPPORT_FRAME_CACHING
      if ((tab != 0) && (tab->IsFrameCached(src)))
      {
        contentType = CFilter::contentTypeSubdocument;
      }
      else
#endif // SUPPORT_FRAME_CACHING
      contentType = GetContentType(mimeType, boundDomain, src);
      if (client->ShouldBlock(src, contentType, boundDomain, true))
      {
        isBlocked = true;

        DEBUG_BLOCKER("Blocker::Blocking Http-request:" + src);
      }
    }
    if (!isBlocked)
    {
      DEBUG_BLOCKER("Blocker::Ignoring Http-request:" + src)
    }
  }


  if (tab == NULL)
  {
    contentType = GetContentType(mimeType, boundDomain, src);
    if (client->ShouldBlock(src, contentType, boundDomain, true))
    {
      isBlocked = true;
    }
  }

#ifdef _DEBUG
  CString type;

  if (contentType == CFilter::contentTypeDocument) type = "DOCUMENT";
  else if (contentType == CFilter::contentTypeObject) type = "OBJECT";
  else if (contentType == CFilter::contentTypeImage) type = "IMAGE";
  else if (contentType == CFilter::contentTypeScript) type = "SCRIPT";
  else if (contentType == CFilter::contentTypeOther) type = "OTHER";
  else if (contentType == CFilter::contentTypeUnknown) type = "OTHER";
  else if (contentType == CFilter::contentTypeSubdocument) type = "SUBDOCUMENT";
  else if (contentType == CFilter::contentTypeStyleSheet) type = "STYLESHEET";
  else type = "OTHER";

  if (isBlocked)
  {
    CPluginDebug::DebugResultBlocking(type, src, boundDomain);
  }
  else
  {
    CPluginDebug::DebugResultIgnoring(type, src, boundDomain);
  }
#endif

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
        m_spInternetProtocolSink->ReportResult(INET_E_REDIRECTING, 301, L"res://mshtml.dll/blank.htm");
      }

      return INET_E_REDIRECT_FAILED;
    } 
    if (((contentType == CFilter::contentTypeScript))&& (isBlocked)) 
    {
      m_shouldBlock = true;
      BaseClass::OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI, dwReserved, pTargetProtocol);
      m_spInternetProtocolSink->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE, L"text/javascript");
      m_spInternetProtocolSink->ReportResult(INET_E_REDIRECTING, 301, L"data:");
      return INET_E_REDIRECT_FAILED;
    }
    if ((isBlocked)) 
    {
/*      WCHAR tmp[256];
      wsprintf(tmp, L"URL: %s, domain: %s, mime: %s, type: %d", szUrl, boundDomain, mimeType, contentType); 
      MessageBox(NULL, tmp, L"", MB_OK);
      contentType = GetContentType(mimeType, boundDomain, src);
*/
      m_shouldBlock = true;
      BaseClass::OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI, dwReserved, pTargetProtocol);
      m_spInternetProtocolSink->ReportResult(S_FALSE, 0, L"");

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


STDMETHODIMP WBPassthru::Start(LPCWSTR szUrl, IInternetProtocolSink *pOIProtSink,
    IInternetBindInfo *pOIBindInfo, DWORD grfPI, HANDLE_PTR dwReserved)
{
  ATLASSERT(m_spInternetProtocol != 0);
  if (!m_spInternetProtocol)
  {
    return E_UNEXPECTED;
  }

  return OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI,
    dwReserved, m_spInternetProtocol);
}

 STDMETHODIMP WBPassthru::Read(	/* [in, out] */ void *pv,/* [in] */ ULONG cb,/* [out] */ ULONG *pcbRead)
 {
   
   WBPassthruSink* pSink = GetSink();
   return pSink->Read(pv, cb, pcbRead);
 }