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
#include "AdblockPlusClient.h"
#include "PluginFilter.h"
#include "PluginSettings.h"
#include "PluginMutex.h"
#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginClass.h"
#include "mlang.h"
#include "..\shared\CriticalSection.h"
#include "..\shared\Utils.h"

// The filters are described at http://adblockplus.org/en/filters

static CriticalSection s_criticalSectionFilterMap;

namespace
{
  struct GetHtmlElementAttributeResult
  {
    GetHtmlElementAttributeResult() : isAttributeFound(false)
    {
    }
    std::wstring attributeValue;
    bool isAttributeFound;
  };

  GetHtmlElementAttributeResult GetHtmlElementAttribute(IHTMLElement& htmlElement,
    const ATL::CComBSTR& attributeName)
  {
    GetHtmlElementAttributeResult retValue;
    ATL::CComVariant vAttr;
    ATL::CComPtr<IHTMLElement4> htmlElement4;
    if (FAILED(htmlElement.QueryInterface(&htmlElement4)) || !htmlElement4)
    {
      return retValue;
    }
    ATL::CComPtr<IHTMLDOMAttribute> attributeNode;
    if (FAILED(htmlElement4->getAttributeNode(attributeName, &attributeNode)) || !attributeNode)
    {
      return retValue;
    }
    // we set that attribute found but it's not necessary that we can retrieve its value
    retValue.isAttributeFound = true;
    if (FAILED(attributeNode->get_nodeValue(&vAttr)))
    {
      return retValue;
    }
    if (vAttr.vt == VT_BSTR && vAttr.bstrVal)
    {
      retValue.attributeValue = vAttr.bstrVal;
    }
    else if (vAttr.vt == VT_I4)
    {
      retValue.attributeValue = std::to_wstring(vAttr.iVal);
    }
    return retValue;
  }
}

// ============================================================================
// CFilterElementHideAttrSelector
// ============================================================================

CFilterElementHideAttrSelector::CFilterElementHideAttrSelector() : m_type(TYPE_NONE), m_pos(POS_NONE), m_bstrAttr(NULL)
{
}

CFilterElementHideAttrSelector::CFilterElementHideAttrSelector(const CFilterElementHideAttrSelector& filter)
{
  m_type = filter.m_type;
  m_pos = filter.m_pos;
  m_bstrAttr = filter.m_bstrAttr;

  m_value = filter.m_value;
}

CFilterElementHideAttrSelector::~CFilterElementHideAttrSelector()
{
}


// ============================================================================
// CFilterElementHide
// ============================================================================

CFilterElementHide::CFilterElementHide(const CString& filterText) : m_filterText(filterText), m_type(ETraverserComplexType::TRAVERSER_TYPE_ERROR)
{
  // Find tag name, class or any (*)
  CString filterString = filterText;

  wchar_t firstTag = filterString.GetAt(0);
  // Any tag
  if (firstTag == '*')
  {
    filterString = filterString.Mid(1);
  }
  // Any tag (implicitely)
  else if (firstTag == '[' || firstTag == '.' || firstTag == '#')
  {
  }
  // Real tag
  else if (isalnum(firstTag))
  {
    //TODO: Add support for descendant selectors
    int pos = filterString.FindOneOf(L".#[(");

    if (pos < 0)
      pos = filterString.GetLength();
    m_tag = filterString.Left(pos).MakeLower();

    filterString = filterString.Mid(pos);
  }
  // Error
  else
  {
    DEBUG_FILTER("Filter::Error parsing selector:" + filterText + " (invalid tag)");
    throw std::runtime_error(CStringA("Filter::Error parsing selector:" + filterText + " (invalid tag)").GetString());
  }

  // Find Id and class name

  if (!filterString.IsEmpty())
  {
    wchar_t firstId = filterString.GetAt(0);

    // Id
    if (firstId == '#')
    {
      int pos = filterString.Find('[');
      if (pos < 0)
      {
        pos = filterString.GetLength();
      }
      m_tagId = filterString.Mid(1, pos - 1);
      filterString = filterString.Mid(pos);
      pos = m_tagId.Find(L".");
      if (pos > 0)
      {
        m_tagClassName = m_tagId.Mid(pos + 1);
        m_tagId = m_tagId.Left(pos);
      }
    }
    // Class name
    else if (firstId == '.')
    {
      int pos = filterString.Find('[');
      if (pos < 0)
      {
        pos = filterString.GetLength();
      }
      m_tagClassName = filterString.Mid(1, pos - 1);
      filterString = filterString.Mid(pos);
    }
  }

  char chAttrStart = '[';
  char chAttrEnd   = ']';

  while (!filterString.IsEmpty())
  {
    if (filterString.GetAt(0) != chAttrStart)
    {
      DEBUG_FILTER("Filter::Error parsing selector:" + filterText + " (more data)");
      throw std::runtime_error(CStringA("Filter::Error parsing selector:" + filterText + " (more data)").GetString());
    }
    int endPos = filterString.Find(']') ;
    if (endPos < 0)
    {
      DEBUG_FILTER("Filter::Error parsing selector:" + filterText + " (more data)");
      throw std::runtime_error(CStringA("Filter::Error parsing selector:" + filterText + " (more data)").GetString());
    }
        
    CFilterElementHideAttrSelector attrSelector;

    CString arg = filterString.Mid(1, endPos - 1);
    filterString = filterString.Mid(endPos + 1);

    int delimiterPos = arg.Find('=');
    if (delimiterPos > 0)
    {
      attrSelector.m_value = arg.Mid(delimiterPos + 1);
      if (attrSelector.m_value.GetLength() >= 2 && attrSelector.m_value.GetAt(0) == '\"' && attrSelector.m_value.GetAt(attrSelector.m_value.GetLength() - 1) == '\"')
      {
        attrSelector.m_value = attrSelector.m_value.Mid(1, attrSelector.m_value.GetLength() - 2);
      }

      if (arg.GetAt(delimiterPos - 1) == '^')
      {
        attrSelector.m_bstrAttr = arg.Left(delimiterPos - 1);
        attrSelector.m_pos = CFilterElementHideAttrPos::STARTING;
      }
      else if (arg.GetAt(delimiterPos - 1) == '*')
      {
        attrSelector.m_bstrAttr = arg.Left(delimiterPos - 1);
        attrSelector.m_pos = CFilterElementHideAttrPos::ANYWHERE;
      }
      else if (arg.GetAt(delimiterPos - 1) == '$')
      {
        attrSelector.m_bstrAttr = arg.Left(delimiterPos - 1);
        attrSelector.m_pos = CFilterElementHideAttrPos::ENDING;
      }
      else
      {
        attrSelector.m_bstrAttr = arg.Left(delimiterPos);
        attrSelector.m_pos = CFilterElementHideAttrPos::EXACT;
      }
    }
    CString tag = attrSelector.m_bstrAttr;
    if (tag == "style")
    {
      attrSelector.m_type = CFilterElementHideAttrType::STYLE;
      attrSelector.m_value.MakeLower();
    }
    else if (tag == "id")
    {
      attrSelector.m_type = CFilterElementHideAttrType::ID;
    }
    else if (tag == "class")
    {
      attrSelector.m_type = CFilterElementHideAttrType::CLASS;
    }
    m_attributeSelectors.push_back(attrSelector);

  }

  // End check
  if (!filterString.IsEmpty())
  {
    DEBUG_FILTER("Filter::Error parsing selector:" + filterFile + "/" + filterText + " (more data)")
    throw new std::runtime_error(CStringA("Filter::Error parsing selector:"  + filterText + " (more data)").GetString());
  }
}

CFilterElementHide::CFilterElementHide(const CFilterElementHide& filter)
{
  m_filterText = filter.m_filterText;

  m_tagId = filter.m_tagId;
  m_tagClassName = filter.m_tagClassName;

  m_attributeSelectors = filter.m_attributeSelectors;

  m_predecessor = filter.m_predecessor;
}


// ============================================================================
// CFilter
// ============================================================================

CFilter::CFilter(const CFilter& filter)
{
  m_filterType  = filter.m_filterType;

  m_isFirstParty = filter.m_isFirstParty;
  m_isThirdParty = filter.m_isThirdParty;

  m_isMatchCase  = filter.m_isMatchCase;
  m_isFromStart = filter.m_isFromStart;
  m_isFromEnd = filter.m_isFromEnd;

  m_filterText = filter.m_filterText;

  m_hitCount = filter.m_hitCount;
}


CFilter::CFilter() : m_isMatchCase(false), m_isFirstParty(false),
  m_isThirdParty(false),
  m_isFromStart(false), m_isFromEnd(false), m_hitCount(0)
{
}


bool CFilterElementHide::IsMatchFilterElementHide(IHTMLElement* pEl) const
{
  HRESULT hr;

  if (!m_tagId.IsEmpty())
  {
    CComBSTR id;
    hr = pEl->get_id(&id);
    if ((hr != S_OK) || (id != CComBSTR(m_tagId)))
    {
      return false;
    }
  }
  if (!m_tagClassName.IsEmpty())
  {
    CComBSTR classNameBSTR;
    hr = pEl->get_className(&classNameBSTR);
    if (hr == S_OK)
    {
      CString className = classNameBSTR;
      int start = 0;
      CString specificClass;
      bool foundMatch = false;
      while ((specificClass = className.Tokenize(L" ", start)) != L"")
      {
        // TODO: Consider case of multiple classes. (m_tagClassName can be something like "foo.bar")
        if (specificClass == m_tagClassName)
        {
          foundMatch = true;
        }
      }
      if (!foundMatch)
      {
        return false;
      }
    }
  }
  if (!m_tag.IsEmpty())
  {
    CComBSTR tagName;
    hr = pEl->get_tagName(&tagName);
    tagName.ToLower();
    if ((hr != S_OK) || (tagName != CComBSTR(m_tag)))
    {
      return false;
    }
  }

  // Check attributes
  for (std::vector<CFilterElementHideAttrSelector>::const_iterator attrIt = m_attributeSelectors.begin(); 
        attrIt != m_attributeSelectors.end(); ++ attrIt)
  {
    ATL::CString value;
    bool attrFound = false;
    if (attrIt->m_type == CFilterElementHideAttrType::STYLE)
    {
      CComPtr<IHTMLStyle> pStyle;
      if (SUCCEEDED(pEl->get_style(&pStyle)) && pStyle)
      {
        CComBSTR bstrStyle;

        if (SUCCEEDED(pStyle->get_cssText(&bstrStyle)) && bstrStyle)
        {
          value = bstrStyle;
          value.MakeLower();
          attrFound = true;
        }
      }
    }
    else if (attrIt->m_type == CFilterElementHideAttrType::CLASS)
    {
      CComBSTR bstrClassNames;
      if (SUCCEEDED(pEl->get_className(&bstrClassNames)) && bstrClassNames)
      {
        value = bstrClassNames;
        attrFound = true;
      }
    }
    else if (attrIt->m_type == CFilterElementHideAttrType::ID)
    {
      CComBSTR bstrId;
      if (SUCCEEDED(pEl->get_id(&bstrId)) && bstrId)
      {
        value = bstrId;
        attrFound = true;
      }
    }
    else
    {
      auto attributeValue = GetHtmlElementAttribute(*pEl, attrIt->m_bstrAttr);
      if (attrFound = attributeValue.isAttributeFound)
      {
        value = ToCString(attributeValue.attributeValue);
      }
    }

    if (attrFound)
    {
      if (attrIt->m_pos == CFilterElementHideAttrPos::EXACT)
      {
        // TODO: IE rearranges the style attribute completely. Figure out if anything can be done about it.
        if (value != attrIt->m_value)
          return false;
      }
      else if (attrIt->m_pos == CFilterElementHideAttrPos::STARTING)
      {
        if (value.Left(attrIt->m_value.GetLength()) != attrIt->m_value)
          return false;
      }
      else if (attrIt->m_pos == CFilterElementHideAttrPos::ENDING)
      {
        if (value.Right(attrIt->m_value.GetLength()) != attrIt->m_value)
          return false;
      }
      else if (attrIt->m_pos == CFilterElementHideAttrPos::ANYWHERE)
      {
        if (value.Find(attrIt->m_value) < 0)
          return false;
      }
      else if (attrIt->m_value.IsEmpty())
      {
        return true;
      }
    }
    else
    {
      return false;
    }
  }

  if (m_predecessor)
  {
    CComPtr<IHTMLElement> pDomPredecessor;
    HRESULT hr = S_FALSE;
    switch (m_predecessor->m_type)
    {
    case ETraverserComplexType::TRAVERSER_TYPE_PARENT:
      hr = pEl->get_parentElement(&pDomPredecessor);
      break;
    case ETraverserComplexType::TRAVERSER_TYPE_IMMEDIATE:
      hr = S_FALSE;
      CComQIPtr<IHTMLDOMNode> pPrevSiblingNode = pEl;
      long type = 0;
      while (pPrevSiblingNode && type != 1)
      {
        IHTMLDOMNode* tmpNode;
        pPrevSiblingNode->get_previousSibling(&tmpNode);
        pPrevSiblingNode.Attach(tmpNode);
        if (pPrevSiblingNode)
        {
          hr = pPrevSiblingNode->get_nodeType(&type);
          if (hr != S_OK)
            pPrevSiblingNode.Release();
        }
      }

      if (pPrevSiblingNode)
        hr = pPrevSiblingNode.QueryInterface(&pDomPredecessor);
      else
        return false;
      break;
    }
    if (hr != S_OK)
      return false;
    return m_predecessor->IsMatchFilterElementHide(pDomPredecessor);
  }

  return true;
}



// ============================================================================
// CPluginFilter
// ============================================================================

CPluginFilter::CPluginFilter(const CString& dataPath) : m_dataPath(dataPath)
{
  ClearFilters();
}


bool CPluginFilter::AddFilterElementHide(CString filterText)
{
  DEBUG_FILTER("Input: " + filterText + " filterFile" + filterFile);
  CriticalSection::Lock filterEngineLock(s_criticalSectionFilterMap);
  {
    CString filterString  = filterText;
    // Create filter descriptor
    std::auto_ptr<CFilterElementHide> filter;

    CString wholeFilterString = filterString;
    wchar_t separatorChar;
    do
    {
      int chunkEnd = filterText.FindOneOf(L"+>");
      if (chunkEnd > 0)
      {
        separatorChar = filterText.GetAt(chunkEnd);
      }
      else
      {
        chunkEnd = filterText.GetLength();
        separatorChar = L'\0';
      }

      CString filterChunk = filterText.Left(chunkEnd).TrimRight();
      std::auto_ptr<CFilterElementHide> filterParent(filter);

      filter.reset(new CFilterElementHide(filterChunk));

      if (filterParent.get() != 0)
      {
        filter->m_predecessor.reset(filterParent.release());
      }

      if (separatorChar != L'\0') // complex selector
      {
        filterText = filterText.Mid(chunkEnd + 1).TrimLeft();
        if (separatorChar == '+')
          filter->m_type = CFilterElementHide::TRAVERSER_TYPE_IMMEDIATE;
        else if (separatorChar == '>')
          filter->m_type = CFilterElementHide::TRAVERSER_TYPE_PARENT;
      }
      else // Terminating element (simple selector)
      {
        if (!filter->m_tagId.IsEmpty())
        {
          m_elementHideTagsId.insert(std::make_pair(std::make_pair(filter->m_tag, filter->m_tagId), *filter));
        }
        else if (!filter->m_tagClassName.IsEmpty())
        {
          m_elementHideTagsClass.insert(std::make_pair(std::make_pair(filter->m_tag, filter->m_tagClassName), *filter));
        }
        else
        {
          std::pair<CString, CFilterElementHide> pair = std::make_pair(filter->m_tag, *filter);
          m_elementHideTags.insert(pair);
        }
      }
    } while (separatorChar != '\0');
  }

  return true;
}

bool CPluginFilter::IsElementHidden(const std::wstring& tag, IHTMLElement* pEl, const std::wstring& domain, const std::wstring& indent) const
{
  CString tagCString = ToCString(tag);

  CString id;
  CComBSTR bstrId;
  if (SUCCEEDED(pEl->get_id(&bstrId)) && bstrId)
  {
    id = bstrId;
  }

  CString classNames;
  CComBSTR bstrClassNames;
  if (SUCCEEDED(pEl->get_className(&bstrClassNames)) && bstrClassNames)
  {
    classNames = bstrClassNames;
  }

  CriticalSection::Lock filterEngineLock(s_criticalSectionFilterMap);
  {
    // Search tag/id filters
    if (!id.IsEmpty())
    {
      std::pair<TFilterElementHideTagsNamed::const_iterator, TFilterElementHideTagsNamed::const_iterator> idItEnum =
        m_elementHideTagsId.equal_range(std::make_pair(tagCString, id));
      for (TFilterElementHideTagsNamed::const_iterator idIt = idItEnum.first; idIt != idItEnum.second; idIt ++)
      {
        if (idIt->second.IsMatchFilterElementHide(pEl))
        {
#ifdef ENABLE_DEBUG_RESULT
          DEBUG_HIDE_EL(indent + "HideEl::Found (tag/id) filter:" + idIt->second.m_filterText)
            CPluginDebug::DebugResultHiding(tag, L"id:" + ToWstring(id), ToWstring(idIt->second.m_filterText));
#endif
          return true;
        }
      }

      // Search general id
      idItEnum = m_elementHideTagsId.equal_range(std::make_pair("", id));
      for (TFilterElementHideTagsNamed::const_iterator idIt = idItEnum.first; idIt != idItEnum.second; idIt ++)
      {
        if (idIt->second.IsMatchFilterElementHide(pEl))
        {
#ifdef ENABLE_DEBUG_RESULT
          DEBUG_HIDE_EL(indent + "HideEl::Found (?/id) filter:" + idIt->second.m_filterText)
            CPluginDebug::DebugResultHiding(tag, L"id:" + ToWstring(id), ToWstring(idIt->second.m_filterText));
#endif
          return true;
        }
      }
    }

    // Search tag/className filters
    if (!classNames.IsEmpty())
    {
      int pos = 0;
      CString className = classNames.Tokenize(L" \t\n\r", pos);
      while (pos >= 0)
      {
        std::pair<TFilterElementHideTagsNamed::const_iterator, TFilterElementHideTagsNamed::const_iterator> classItEnum = 
          m_elementHideTagsClass.equal_range(std::make_pair(tagCString, className));

        for (TFilterElementHideTagsNamed::const_iterator classIt = classItEnum.first; classIt != classItEnum.second; ++classIt)
        {
          if (classIt->second.IsMatchFilterElementHide(pEl))
          {
#ifdef ENABLE_DEBUG_RESULT
            DEBUG_HIDE_EL(indent + "HideEl::Found (tag/class) filter:" + classIt->second.m_filterText)
              CPluginDebug::DebugResultHiding(tag, L"class:" + ToWstring(className), ToWstring(classIt->second.m_filterText));
#endif
            return true;
          }
        }

        // Search general class name
        classItEnum = m_elementHideTagsClass.equal_range(std::make_pair("", className));
        for (TFilterElementHideTagsNamed::const_iterator classIt = classItEnum.first; classIt != classItEnum.second; ++ classIt)
        {
          if (classIt->second.IsMatchFilterElementHide(pEl))
          {
#ifdef ENABLE_DEBUG_RESULT
            DEBUG_HIDE_EL(indent + L"HideEl::Found (?/class) filter:" + ToWString(classIt->second.m_filterText));
            CPluginDebug::DebugResultHiding(tag, L"class:" + ToWstring(className), ToWstring(classIt->second.m_filterText));
#endif
            return true;
          }
        }

        // Next class name
        className = classNames.Tokenize(L" \t\n\r", pos);
      }
    }

    // Search tag filters
    std::pair<TFilterElementHideTags::const_iterator, TFilterElementHideTags::const_iterator> tagItEnum 
      = m_elementHideTags.equal_range(tagCString);
    for (TFilterElementHideTags::const_iterator tagIt = tagItEnum.first; tagIt != tagItEnum.second; ++ tagIt)
    {
      if (tagIt->second.IsMatchFilterElementHide(pEl))
      {
#ifdef ENABLE_DEBUG_RESULT
        DEBUG_HIDE_EL(indent + "HideEl::Found (tag) filter:" + tagIt->second.m_filterText)
          CPluginDebug::DebugResultHiding(tag, L"-", ToWstring(tagIt->second.m_filterText));
#endif
        return true;
      }
    }
  }

  return false;
}

bool CPluginFilter::LoadHideFilters(std::vector<std::wstring> filters)
{
  ClearFilters();
  bool isRead = false;
  CPluginClient* client = CPluginClient::GetInstance();

  // Parse hide string
  int pos = 0;
  CriticalSection::Lock filterEngineLock(s_criticalSectionFilterMap);
  {
    for (std::vector<std::wstring>::iterator it = filters.begin(); it < filters.end(); ++it)
    {
      CString filter((*it).c_str());
      // If the line is not commented out
      if (!filter.Trim().IsEmpty() && filter.GetAt(0) != '!' && filter.GetAt(0) != '[')
      {
        int filterType = 0;

        // See http://adblockplus.org/en/filters for further documentation

        try
        {
          AddFilterElementHide(filter);
        }
        catch(...)
        {
#ifdef ENABLE_DEBUG_RESULT
          CPluginDebug::DebugResult(L"Error loading hide filter: " + ToWstring(filter));
#endif
        }
      }
    }
  }

  return isRead;
}

void CPluginFilter::ClearFilters()
{
  // Clear filter maps
  CriticalSection::Lock filterEngineLock(s_criticalSectionFilterMap);
  m_elementHideTags.clear();
  m_elementHideTagsId.clear();
  m_elementHideTagsClass.clear();
}

bool CPluginFilter::ShouldBlock(const std::wstring& src, AdblockPlus::FilterEngine::ContentType contentType, const std::wstring& domain, bool addDebug) const
{
  std::wstring srcTrimmed = TrimString(src);

  // We should not block the empty string, so all filtering does not make sense
  // Therefore we just return
  if (srcTrimmed.empty())
  {
    return false;
  }

  CPluginSettings* settings = CPluginSettings::GetInstance();

  CPluginClient* client = CPluginClient::GetInstance();
  bool result = client->Matches(srcTrimmed, contentType, domain);

#ifdef ENABLE_DEBUG_RESULT
  if (addDebug)
  {
    std::wstring type = ToUtf16String(AdblockPlus::FilterEngine::ContentTypeToString(contentType));
    if (result)
    {
      CPluginDebug::DebugResultBlocking(type, srcTrimmed, domain);
    }
    else
    {
      CPluginDebug::DebugResultIgnoring(type, srcTrimmed, domain);
    }
  }
#endif
  return result;
}
