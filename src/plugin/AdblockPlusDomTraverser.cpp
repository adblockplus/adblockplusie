/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2016 Eyeo GmbH
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
#include "..\shared\Utils.h"


CPluginDomTraverser::CPluginDomTraverser(const PluginFilterPtr& pluginFilter) : CPluginDomTraverserBase(pluginFilter)
{
}


bool CPluginDomTraverser::OnIFrame(IHTMLElement* pEl, const std::wstring& url, const std::wstring& indent)
{
  CPluginClient* client = CPluginClient::GetInstance();

  // If src should be blocked, set style display:none on iframe
  bool isBlocked = client->ShouldBlock(url,
    AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_SUBDOCUMENT, m_documentUrl);
  if (isBlocked)
  {
    HideElement(pEl, L"iframe", url, true, indent);
  }

  return !isBlocked;
}


bool CPluginDomTraverser::OnElement(IHTMLElement* pEl, const std::wstring& tag, CPluginDomTraverserCache* cache, bool isDebug, const std::wstring& indent)
{
  if (cache->m_isHidden)
  {
    return false;
  }

  cache->m_isHidden = m_pluginFilter->IsElementHidden(tag, pEl, m_domain, indent);
  if (cache->m_isHidden)
  {
    HideElement(pEl, tag, L"", false, indent);
    return false;
  }

  // Images
  if (tag == L"img")
  {
    CComVariant vAttr;
    if (SUCCEEDED(pEl->getAttribute(ATL::CComBSTR(L"src"), 0, &vAttr)) && vAttr.vt == VT_BSTR)
    {
      std::wstring src = ToWstring(vAttr.bstrVal);
      if (!src.empty())
      {
        // If src should be blocked, set style display:none on image
        cache->m_isHidden = CPluginClient::GetInstance()->ShouldBlock(src,
          AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_IMAGE, m_documentUrl);
        if (cache->m_isHidden)
        {
          HideElement(pEl, L"image", src, true, indent);
          return false;
        }
      }
    }
  }
  // Objects
  else if (tag == L"object")
  {
    CComBSTR bstrInnerHtml;
    if (SUCCEEDED(pEl->get_innerHTML(&bstrInnerHtml)) && bstrInnerHtml)
    {
      const std::wstring objectInnerHtml(ToWstring(bstrInnerHtml));
      std::wstring::size_type posBegin = 0;
      while (true)
      {
        posBegin = objectInnerHtml.find(L"VALUE=\"", posBegin);
        if (posBegin == std::wstring::npos)
        {
          // No more "value" attributes to scan
          break;
        }
        auto posPostInitialQuote = posBegin + 7;
        auto posFinalQuote = objectInnerHtml.find(L'\"', posPostInitialQuote);
        if (posFinalQuote == std::wstring::npos)
        {
          // We have an initial quotation mark but no final one.
          // Ignore this tag because it has an HTML syntax error.
          break;
        }
        auto src = objectInnerHtml.substr(posPostInitialQuote, posFinalQuote - posPostInitialQuote);
        // eg. http://w3schools.com/html/html_examples.asp
        if (BeginsWith(src, L"//"))
        {
          src = L"http:" + src;
        }
        if (!src.empty() && cache->m_isHidden)
        {
          HideElement(pEl, L"object", src, true, indent);
          return false;
        }
        posBegin = posFinalQuote; // Don't scan content of quoted string
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


void CPluginDomTraverser::HideElement(IHTMLElement* pEl, const std::wstring& type, const std::wstring& url, bool isDebug, const std::wstring& indent)
{
  CComPtr<IHTMLStyle> pStyle;

  if (SUCCEEDED(pEl->get_style(&pStyle)) && pStyle)
  {
    CComBSTR bstrDisplay;
    if (SUCCEEDED(pStyle->get_display(&bstrDisplay)) && ToWstring(bstrDisplay) == L"none")
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
          CPluginDebug::DebugResultHiding(type, url, L"-");
        }
#endif // ENABLE_DEBUG_RESULT
    }
  }
}
