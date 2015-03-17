/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PluginStdAfx.h"
#include "AdblockPlusDomTraverser.h"
#include "AdblockPlusClient.h"
#include "PluginFilter.h"
#include "PluginSettings.h"


CPluginDomTraverser::CPluginDomTraverser(CPluginTab* tab) : CPluginDomTraverserBase(tab)
{
}


bool CPluginDomTraverser::OnIFrame(IHTMLElement* pEl, const std::wstring& url, CString& indent)
{
  CPluginClient* client = CPluginClient::GetInstance();

  // If src should be blocked, set style display:none on iframe
  bool isBlocked = client->ShouldBlock(url,
    AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_SUBDOCUMENT, m_domain);
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

  cache->m_isHidden = client->IsElementHidden(ToWstring(tag), pEl, m_domain, ToWstring(indent), m_tab->m_filter.get());
  if (cache->m_isHidden)
  {
    HideElement(pEl, tag, L"", false, indent);
    return false;
  }

  // Images
  if (tag == "img")
  {
    CComVariant vAttr;

    if (SUCCEEDED(pEl->getAttribute(ATL::CComBSTR(L"src"), 0, &vAttr)) && vAttr.vt == VT_BSTR && ::SysStringLen(vAttr.bstrVal) > 0)
    {
      std::wstring src(vAttr.bstrVal, SysStringLen(vAttr.bstrVal));
      UnescapeUrl(src);

      // If src should be blocked, set style display:none on image
      cache->m_isHidden = client->ShouldBlock(src,
        AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_IMAGE, m_domain);
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
          if (cache->m_isHidden)
          {
            HideElement(pEl, "object", ToWstring(src), true, indent);
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
  return client && CPluginSettings::GetInstance()->IsPluginEnabled() && !client->IsWhitelistedUrl(m_domain);
}


void CPluginDomTraverser::HideElement(IHTMLElement* pEl, const CString& type, const std::wstring& url, bool isDebug, CString& indent)
{
  CComPtr<IHTMLStyle> pStyle;

  if (SUCCEEDED(pEl->get_style(&pStyle)) && pStyle)
  {
    CComBSTR bstrDisplay;

    if (SUCCEEDED(pStyle->get_display(&bstrDisplay)) && bstrDisplay && CString(bstrDisplay) == L"none")
    {
      return;
    }

    static const CComBSTR sbstrNone(L"none");

    if (SUCCEEDED(pStyle->put_display(sbstrNone)))
    {
      DEBUG_HIDE_EL(ToWstring(indent) + L"HideEl::Hiding " + ToWstring(type) + L" url:" + url)

#ifdef ENABLE_DEBUG_RESULT
        if (isDebug)
        {
          CPluginDebug::DebugResultHiding(ToWstring(type), url, L"-");
        }
#endif // ENABLE_DEBUG_RESULT
    }
  }
}
