#include "PluginStdAfx.h"

#include "PluginClient.h"
#include "PluginFilter.h"
#include "PluginSettings.h"

#include "AdblockPlusDomTraverser.h"


CPluginDomTraverser::CPluginDomTraverser(CPluginTab* tab) : CPluginDomTraverserBase(tab)
{
}


bool CPluginDomTraverser::OnIFrame(IHTMLElement* pEl, const CString& url, CString& indent)
{
  CPluginClient* client = CPluginClient::GetInstance();

  // If src should be blocked, set style display:none on iframe
  bool isBlocked = client->ShouldBlock(url, CFilter::contentTypeSubdocument, m_domain);
  if (isBlocked)
  {
    HideElement(pEl, "iframe", url, true, indent);
  }

  return !isBlocked;
}


bool CPluginDomTraverser::OnElement(IHTMLElement* pEl, const CString& tag, CPluginDomTraverserCache* cache, bool isDebug, CString& indent)
{
  if (cache->m_isHidden)
  {
    return false;
  }

  // Check if element is hidden
  CPluginClient* client = CPluginClient::GetInstance();

  cache->m_isHidden = client->IsElementHidden(tag, pEl, m_domain, indent);
  if (cache->m_isHidden)
  {
    HideElement(pEl, tag, "", false, indent);
    return false;
  }

  // Images
  if (tag == "img")
  {
    CComVariant vAttr;

    if (SUCCEEDED(pEl->getAttribute(L"src", 0, &vAttr)) && vAttr.vt == VT_BSTR && ::SysStringLen(vAttr.bstrVal) > 0)
    {
      CString src = vAttr.bstrVal;
      CPluginClient::UnescapeUrl(src);

      // If src should be blocked, set style display:none on image
      cache->m_isHidden = client->ShouldBlock(src, CFilter::contentTypeImage, m_domain);
      if (cache->m_isHidden)
      {
        HideElement(pEl, "image", src, true, indent);
        return false;
      }
    }
  }
  // Objects
  else if (tag == "object")
  {
    CComBSTR bstrInnerHtml;

    if (SUCCEEDED(pEl->get_innerHTML(&bstrInnerHtml)) && bstrInnerHtml)
    {
      CString sObjectHtml = bstrInnerHtml;
      CString src;

      int posBegin = sObjectHtml.Find(L"VALUE=\"");
      int posEnd = posBegin >= 0 ? sObjectHtml.Find('\"', posBegin + 7) : -1;

      while (posBegin >= 0 && posEnd >= 0)
      {
        posBegin += 7;

        src = sObjectHtml.Mid(posBegin, posEnd - posBegin);

        // eg. http://w3schools.com/html/html_examples.asp
        if (src.Left(2) == "//")
        {
          src = "http:" + src;
        }

        if (!src.IsEmpty())
        {
          //			        cache->m_isHidden = client->ShouldBlock(src, CFilter::contentTypeObject, m_domain);
          if (cache->m_isHidden)
          {
            HideElement(pEl, "object", src, true, indent);
            return false;
          }
        }

        posBegin = sObjectHtml.Find(L"VALUE=\"", posBegin);
        posEnd = posBegin >= 0 ? sObjectHtml.Find(L"\"", posBegin + 7) : -1;
      }
    }
  }

  return true;
}


bool CPluginDomTraverser::IsEnabled()
{ 
  CPluginClient* client = CPluginClient::GetInstance();

  return client && CPluginSettings::GetInstance()->IsPluginEnabled() && !client->IsUrlWhiteListed(m_domain);
}


void CPluginDomTraverser::HideElement(IHTMLElement* pEl, const CString& type, const CString& url, bool isDebug, CString& indent)
{
  CComPtr<IHTMLStyle> pStyle;

  if (SUCCEEDED(pEl->get_style(&pStyle)) && pStyle)
  {
#ifdef ENABLE_DEBUG_RESULT
    CComBSTR bstrDisplay;

    if (SUCCEEDED(pStyle->get_display(&bstrDisplay)) && bstrDisplay && CString(bstrDisplay) == L"none")
    {
      return;
    }
#endif // ENABLE_DEBUG_RESULT

    static const CComBSTR sbstrNone(L"none");

    if (SUCCEEDED(pStyle->put_display(sbstrNone)))
    {
      DEBUG_HIDE_EL(indent + L"HideEl::Hiding " + type + L" url:" + url)

#ifdef ENABLE_DEBUG_RESULT
        if (isDebug)
        {
          CPluginDebug::DebugResultHiding(type, url, "-");
        }
#endif // ENABLE_DEBUG_RESULT
    }
  }
}
