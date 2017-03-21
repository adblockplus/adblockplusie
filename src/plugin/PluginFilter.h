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

#ifndef _PLUGIN_FILTER_H_
#define _PLUGIN_FILTER_H_

#include <memory>
#include <AdblockPlus/FilterEngine.h>

enum CFilterElementHideAttrPos
{
  POS_NONE = 0, STARTING, ENDING, ANYWHERE, EXACT
};

enum CFilterElementHideAttrType
{
  TYPE_NONE = 0, STYLE, ID, CLASS
};
// ============================================================================
// CFilterElementHideAttrSelector
// ============================================================================

struct CFilterElementHideAttrSelector
{
  CFilterElementHideAttrPos m_pos;
  CFilterElementHideAttrType m_type;
  std::wstring m_attr;
  std::wstring m_value;

  CFilterElementHideAttrSelector()
    : m_type(TYPE_NONE), m_pos(POS_NONE)
  {
  }
};



// ============================================================================
// CFilterElementHide
// ============================================================================
class CFilterElementHide
{

public:

  enum ETraverserComplexType
  {
    TRAVERSER_TYPE_PARENT,
    TRAVERSER_TYPE_IMMEDIATE,
    TRAVERSER_TYPE_ERROR
  };

  std::wstring m_filterText;

  // For domain specific filters only
  std::wstring m_tagId;
  std::wstring m_tagClassName;
  std::wstring m_tag;

  std::vector<CFilterElementHideAttrSelector> m_attributeSelectors;
  std::shared_ptr<CFilterElementHide> m_predecessor;

  CFilterElementHide(const std::wstring& filterText = L"");
  CFilterElementHide(const CFilterElementHide& filter);
  ETraverserComplexType m_type;

  bool IsMatchFilterElementHide(IHTMLElement* pEl) const;

};

// ============================================================================
// CFilter
// ============================================================================

class CFilter
{

public:


  enum EFilterType
  {
    filterTypeBlocking = 0,
    filterTypeWhiteList = 1,
    filterTypeElementHide = 2,
    filterTypeUnknown = 3
  } filterType;

  enum EFilterType m_filterType;
  bool m_isMatchCase;
  bool m_isFirstParty;
  bool m_isThirdParty;
  bool m_isFromStart;
  bool m_isFromEnd;
  int m_hitCount;
  std::wstring m_filterText;

  CFilter(const CFilter&);
  CFilter();
};

// ============================================================================
// CPluginFilter
// ============================================================================

class CPluginFilter
{

private:

  // (Tag,Name) -> Filter
  typedef std::multimap<std::pair<std::wstring, std::wstring>, CFilterElementHide> TFilterElementHideTagsNamed;

  // Tag -> Filter
  typedef std::multimap<std::wstring, CFilterElementHide> TFilterElementHideTags;


  TFilterElementHideTagsNamed m_elementHideTagsId;
  TFilterElementHideTagsNamed m_elementHideTagsClass;
  TFilterElementHideTags m_elementHideTags;
  std::vector<std::wstring> m_hideFilters;


public:
  explicit CPluginFilter(const std::vector<std::wstring>& filters);
  bool AddFilterElementHide(std::wstring filter);
  bool IsElementHidden(const std::wstring& tag, IHTMLElement* pEl, const std::wstring& domain, const std::wstring& indent) const;
  const std::vector<std::wstring>& GetHideFilters() const {
    return m_hideFilters;
  }
};

typedef std::shared_ptr<CPluginFilter> PluginFilterPtr;

#endif // _PLUGIN_FILTER_H_
