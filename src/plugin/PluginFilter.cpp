/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2017 eyeo GmbH
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
#include "PluginUtil.h"
#include "mlang.h"
#include "..\shared\CriticalSection.h"
#include "..\shared\Utils.h"
#include "..\shared\MsHTMLUtils.h"

// The filters are described at http://adblockplus.org/en/filters

static CriticalSection s_criticalSectionFilterMap;

// ============================================================================
// CFilterElementHide
// ============================================================================
namespace
{
  class ElementHideParseException
    : public std::runtime_error
  {
    std::string message(const std::wstring& filterText, const std::string& reason)
    {
      std::string s(
        "CFilterElementHide::CFilterElementHide, error parsing selector \""
        + ToUtf8String(filterText) + "\" (" + reason + ")"
        );
      DEBUG_FILTER(ToUtf16String(s));
      return s;
    }
  public:
    ElementHideParseException(const std::wstring& filterText, const std::string& reason)
      : std::runtime_error(message(filterText, reason))
    {}
  };
}

CFilterElementHide::CFilterElementHide(const std::wstring& filterText)
  : m_filterText(filterText), m_type(ETraverserComplexType::TRAVERSER_TYPE_ERROR)
{
  std::wstring filterSuffix(filterText); // The unparsed remainder of the input filter text

  // Find tag name, class or any (*)
  wchar_t firstTag = filterText[0];
  if (firstTag == '*')
  {
    // Any tag
    filterSuffix = filterSuffix.substr(1);
  }
  else if (firstTag == '[' || firstTag == '.' || firstTag == '#')
  {
    // Any tag (implicitly)
  }
  else if (isalnum(firstTag))
  {
    // Real tag
    // TODO: Add support for descendant selectors
    auto pos = filterSuffix.find_first_of(L".#[(");
    if (pos == std::wstring::npos)
    {
      pos = filterSuffix.length();
    }
    m_tag = ToLowerString(filterSuffix.substr(0, pos));
    filterSuffix = filterSuffix.substr(pos);
  }
  else
  {
    // Error
    throw ElementHideParseException(filterText, "invalid tag");
  }

  // Find Id and class name
  if (!filterSuffix.empty())
  {
    wchar_t firstId = filterSuffix[0];

    // Id
    if (firstId == '#')
    {
      auto pos = filterSuffix.find('[');
      if (pos == std::wstring::npos)
      {
        pos = filterSuffix.length();
      }
      m_tagId = filterSuffix.substr(1, pos - 1);
      filterSuffix = filterSuffix.substr(pos);
      pos = m_tagId.find(L".");
      if (pos != std::wstring::npos)
      {
        if (pos == 0)
        {
          throw ElementHideParseException(filterText, "empty tag id");
        }
        m_tagClassName = m_tagId.substr(pos + 1);
        m_tagId = m_tagId.substr(0, pos);
      }
    }
    // Class name
    else if (firstId == '.')
    {
      auto pos = filterSuffix.find('[');
      if (pos == std::wstring::npos)
      {
        pos = filterSuffix.length();
      }
      m_tagClassName = filterSuffix.substr(1, pos - 1);
      filterSuffix = filterSuffix.substr(pos);
    }
  }

  while (!filterSuffix.empty())
  {
    if (filterSuffix[0] != '[')
    {
      throw ElementHideParseException(filterText, "expected '['");
    }
    auto endPos = filterSuffix.find(']') ;
    if (endPos == std::wstring::npos)
    {
      throw ElementHideParseException(filterText, "expected ']'");
    }
    std::wstring arg = filterSuffix.substr(1, endPos - 1);
    filterSuffix = filterSuffix.substr(endPos + 1);

    CFilterElementHideAttrSelector attrSelector;
    auto posEquals = arg.find('=');
    if (posEquals != std::wstring::npos)
    {
      if (posEquals == 0)
      {
        throw ElementHideParseException(filterText, "empty attribute name before '='");
      }
      attrSelector.m_value = arg.substr(posEquals + 1);
      if (attrSelector.m_value.length() >= 2 && attrSelector.m_value[0] == '\"' && attrSelector.m_value[attrSelector.m_value.length() - 1] == '\"')
      {
        attrSelector.m_value = attrSelector.m_value.substr(1, attrSelector.m_value.length() - 2);
      }

      if (arg[posEquals - 1] == '^')
      {
        attrSelector.m_pos = CFilterElementHideAttrPos::STARTING;
      }
      else if (arg[posEquals - 1] == '*')
      {
        attrSelector.m_pos = CFilterElementHideAttrPos::ANYWHERE;
      }
      else if (arg[posEquals - 1] == '$')
      {
        attrSelector.m_pos = CFilterElementHideAttrPos::ENDING;
      }
      if (attrSelector.m_pos != CFilterElementHideAttrPos::POS_NONE)
      {
        if (posEquals == 1)
        {
          throw ElementHideParseException(filterText, "empty attribute name before " + std::string(1, static_cast<char>(arg[0])) + "'='");
        }
        attrSelector.m_attr = arg.substr(0, posEquals - 1);
      }
      else
      {
        attrSelector.m_pos = CFilterElementHideAttrPos::EXACT;
        attrSelector.m_attr = arg.substr(0, posEquals);
      }
    }

    if (attrSelector.m_attr == L"style")
    {
      attrSelector.m_type = CFilterElementHideAttrType::STYLE;
      attrSelector.m_value = ToLowerString(attrSelector.m_value);
    }
    else if (attrSelector.m_attr == L"id")
    {
      attrSelector.m_type = CFilterElementHideAttrType::ID;
    }
    else if (attrSelector.m_attr == L"class")
    {
      attrSelector.m_type = CFilterElementHideAttrType::CLASS;
    }
    m_attributeSelectors.push_back(attrSelector);
  }

  // End check
  if (!filterSuffix.empty())
  {
    throw ElementHideParseException(filterText, "extra characters at end");
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
  /*
   * If a tag id is specified, it must match
   */
  if (!m_tagId.empty())
  {
    CComBSTR idBstr;
    if (FAILED(pEl->get_id(&idBstr)) || !idBstr || m_tagId != ToWstring(idBstr))
    {
      return false;
    }
  }
  /*
   * If a class name is specified, it must match
   */
  if (!m_tagClassName.empty())
  {
    CComBSTR classNameListBstr;
    hr = pEl->get_className(&classNameListBstr);
    if (FAILED(hr) || !classNameListBstr)
    {
      return false; // We can't match a class name if there's no class name
    }
    std::wstring classNameList(ToWstring(classNameListBstr));
    if (classNameList.empty())
    {
      return false;
    }
    // TODO: Consider case of multiple classes. (m_tagClassName can be something like "foo.bar")
    /*
     * Match when 'm_tagClassName' appears as a token within classNameList
     */
    bool foundMatch = false;
    wchar_t* nextToken = nullptr;
    const wchar_t* token = wcstok_s(&classNameList[0], L" ", &nextToken);
    while (token != nullptr)
    {
      if (std::wstring(token) == m_tagClassName)
      {
        foundMatch = true;
        break;
      }
      token = wcstok_s(nullptr, L" ", &nextToken);
    }
    if (!foundMatch)
    {
      return false;
    }
  }
  /*
   * If a tag name is specified, it must match
   */
  if (!m_tag.empty())
  {
    CComBSTR tagNameBstr;
    if (FAILED(pEl->get_tagName(&tagNameBstr)) || !tagNameBstr)
    {
      return false;
    }
    if (m_tag != ToLowerString(ToWstring(tagNameBstr)))
    {
      return false;
    }
  }
  /*
   * Match each attribute
   */
  for (auto attrIt = m_attributeSelectors.begin(); attrIt != m_attributeSelectors.end(); ++attrIt)
  {
    std::wstring value;
    bool attrFound = false;
    if (attrIt->m_type == CFilterElementHideAttrType::STYLE)
    {
      CComPtr<IHTMLStyle> pStyle;
      if (SUCCEEDED(pEl->get_style(&pStyle)) && pStyle)
      {
        CComBSTR styleBstr;
        if (SUCCEEDED(pStyle->get_cssText(&styleBstr)) && styleBstr)
        {
          value = ToLowerString(ToWstring(styleBstr));
          attrFound = true;
        }
      }
    }
    else if (attrIt->m_type == CFilterElementHideAttrType::CLASS)
    {
      CComBSTR classNamesBstr;
      if (SUCCEEDED(pEl->get_className(&classNamesBstr)) && classNamesBstr)
      {
        value = ToWstring(classNamesBstr);
        attrFound = true;
      }
    }
    else if (attrIt->m_type == CFilterElementHideAttrType::ID)
    {
      CComBSTR idBstr;
      if (SUCCEEDED(pEl->get_id(&idBstr)) && idBstr)
      {
        value = ToWstring(idBstr);
        attrFound = true;
      }
    }
    else
    {
      CComBSTR attrArgument(attrIt->m_attr.length(), attrIt->m_attr.c_str());
      auto x = GetHtmlElementAttribute(*pEl, attrArgument);
      attrFound = x.isAttributeFound;
      if (attrFound)
      {
        value = x.attributeValue;
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
        if (value.compare(0, attrIt->m_value.length(), attrIt->m_value) != 0)
          return false;
      }
      else if (attrIt->m_pos == CFilterElementHideAttrPos::ENDING)
      {
        size_t valueLength = value.length();
        size_t attrLength = attrIt->m_value.length();
        if (valueLength < attrLength)
          return false;
        if (value.compare(valueLength - attrLength, attrLength, attrIt->m_value) != 0)
          return false;
      }
      else if (attrIt->m_pos == CFilterElementHideAttrPos::ANYWHERE)
      {
        if (value.find(attrIt->m_value) == std::wstring::npos)
          return false;
      }
      else if (attrIt->m_value.empty())
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


bool CPluginFilter::AddFilterElementHide(std::wstring filterText)
{
  DEBUG_FILTER(L"Input: " + filterText + L" filterFile" + filterFile);
  CriticalSection::Lock filterEngineLock(s_criticalSectionFilterMap);
  {
    // Create filter descriptor
    std::auto_ptr<CFilterElementHide> filter;
    wchar_t separatorChar;
    do
    {
      auto chunkEnd = filterText.find_first_of(L"+>");
      if (chunkEnd != std::wstring::npos && chunkEnd > 0)
      {
        separatorChar = filterText[chunkEnd];
      }
      else
      {
        chunkEnd = filterText.length();
        separatorChar = L'\0';
      }

      std::auto_ptr<CFilterElementHide> filterParent(filter);
      filter.reset(new CFilterElementHide(TrimStringRight(filterText.substr(0, chunkEnd))));
      if (filterParent.get() != 0)
      {
        filter->m_predecessor.reset(filterParent.release());
      }

      if (separatorChar != L'\0') // complex selector
      {
        filterText = TrimStringLeft(filterText.substr(chunkEnd + 1));
        if (separatorChar == '+')
          filter->m_type = CFilterElementHide::TRAVERSER_TYPE_IMMEDIATE;
        else if (separatorChar == '>')
          filter->m_type = CFilterElementHide::TRAVERSER_TYPE_PARENT;
      }
      else // Terminating element (simple selector)
      {
        if (!filter->m_tagId.empty())
        {
          m_elementHideTagsId.insert(std::make_pair(std::make_pair(filter->m_tag, filter->m_tagId), *filter));
        }
        else if (!filter->m_tagClassName.empty())
        {
          m_elementHideTagsClass.insert(std::make_pair(std::make_pair(filter->m_tag, filter->m_tagClassName), *filter));
        }
        else
        {
          m_elementHideTags.insert(std::make_pair(filter->m_tag, *filter));
        }
      }
    } while (separatorChar != '\0');
  }

  return true;
}

bool CPluginFilter::IsElementHidden(const std::wstring& tag, IHTMLElement* pEl, const std::wstring& domain, const std::wstring& indent) const
{
  std::wstring id;
  CComBSTR idBstr;
  if (SUCCEEDED(pEl->get_id(&idBstr)) && idBstr)
  {
    id = ToWstring(idBstr);
  }
  std::wstring classNames;
  CComBSTR classNamesBstr;
  if (SUCCEEDED(pEl->get_className(&classNamesBstr)) && classNamesBstr)
  {
    classNames = ToWstring(classNamesBstr);
  }

  CriticalSection::Lock filterEngineLock(s_criticalSectionFilterMap);
  {
    // Search tag/id filters
    if (!id.empty())
    {
      auto idItEnum = m_elementHideTagsId.equal_range(std::make_pair(tag, id));
      for (auto idIt = idItEnum.first; idIt != idItEnum.second; ++idIt)
      {
        if (idIt->second.IsMatchFilterElementHide(pEl))
        {
#ifdef ENABLE_DEBUG_RESULT
          DEBUG_HIDE_EL(indent + L"HideEl::Found (tag/id) filter:" + idIt->second.m_filterText);
          CPluginDebug::DebugResultHiding(tag, L"id:" + id, idIt->second.m_filterText);
#endif
          return true;
        }
      }

      // Search general id
      idItEnum = m_elementHideTagsId.equal_range(std::make_pair(L"", id));
      for (auto idIt = idItEnum.first; idIt != idItEnum.second; ++idIt)
      {
        if (idIt->second.IsMatchFilterElementHide(pEl))
        {
#ifdef ENABLE_DEBUG_RESULT
          DEBUG_HIDE_EL(indent + L"HideEl::Found (?/id) filter:" + idIt->second.m_filterText);
          CPluginDebug::DebugResultHiding(tag, L"id:" + id, idIt->second.m_filterText);
#endif
          return true;
        }
      }
    }

    // Search tag/className filters
    if (!classNames.empty())
    {
      wchar_t* nextToken = nullptr;
      const wchar_t* token = wcstok_s(&classNames[0], L" \t\n\r", &nextToken);
      while (token != nullptr)
      {
        std::wstring className(token);
        auto classItEnum = m_elementHideTagsClass.equal_range(std::make_pair(tag, className));
        for (auto classIt = classItEnum.first; classIt != classItEnum.second; ++classIt)
        {
          if (classIt->second.IsMatchFilterElementHide(pEl))
          {
#ifdef ENABLE_DEBUG_RESULT
            DEBUG_HIDE_EL(indent + L"HideEl::Found (tag/class) filter:" + classIt->second.m_filterText);
            CPluginDebug::DebugResultHiding(tag, L"class:" + className, classIt->second.m_filterText);
#endif
            return true;
          }
        }

        // Search general class name
        classItEnum = m_elementHideTagsClass.equal_range(std::make_pair(L"", className));
        for (auto classIt = classItEnum.first; classIt != classItEnum.second; ++ classIt)
        {
          if (classIt->second.IsMatchFilterElementHide(pEl))
          {
#ifdef ENABLE_DEBUG_RESULT
            DEBUG_HIDE_EL(indent + L"HideEl::Found (?/class) filter:" + classIt->second.m_filterText);
            CPluginDebug::DebugResultHiding(tag, L"class:" + className, classIt->second.m_filterText);
#endif
            return true;
          }
        }
        token = wcstok_s(nullptr, L" \t\n\r", &nextToken);
      }
    }

    // Search tag filters
    auto tagItEnum = m_elementHideTags.equal_range(tag);
    for (auto tagIt = tagItEnum.first; tagIt != tagItEnum.second; ++tagIt)
    {
      if (tagIt->second.IsMatchFilterElementHide(pEl))
      {
#ifdef ENABLE_DEBUG_RESULT
        DEBUG_HIDE_EL(indent + L"HideEl::Found (tag) filter:" + tagIt->second.m_filterText);
        CPluginDebug::DebugResultHiding(tag, L"-", tagIt->second.m_filterText);
#endif
        return true;
      }
    }
  }

  return false;
}

CPluginFilter::CPluginFilter(const std::vector<std::wstring>& filters)
{
  m_hideFilters = filters;
  CPluginClient* client = CPluginClient::GetInstance();

  // Parse hide string
  int pos = 0;
  CriticalSection::Lock filterEngineLock(s_criticalSectionFilterMap);
  {
    for (auto it = filters.begin(); it < filters.end(); ++it)
    {
      std::wstring filter(TrimString(*it));
      // If the line is not commented out
      if (!filter.empty() && filter[0] != '!' && filter[0] != '[')
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
          CPluginDebug::DebugResult(L"Error loading hide filter: " + filter);
#endif
        }
      }
    }
  }
}
