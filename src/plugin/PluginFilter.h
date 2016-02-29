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

class CFilterElementHideAttrSelector
{

public:

  CFilterElementHideAttrPos m_pos;

  CFilterElementHideAttrType m_type;

  CComBSTR m_bstrAttr;
  CString m_value;

  CFilterElementHideAttrSelector();
  CFilterElementHideAttrSelector(const CFilterElementHideAttrSelector& filter);
  ~CFilterElementHideAttrSelector();
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


  CString m_filterText;

  // For domain specific filters only
  CString m_tagId;
  CString m_tagClassName;
  CString m_tag;

  std::vector<CFilterElementHideAttrSelector> m_attributeSelectors;
  std::shared_ptr<CFilterElementHide> m_predecessor;

  CFilterElementHide(const CString& filterText="");
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
  CString m_filterText;

  CFilter(const CFilter&);
  CFilter();
};

// ============================================================================
// CPluginFilter
// ============================================================================

class CPluginFilter
{

private:

  CString m_dataPath;

  // (Tag,Name) -> Filter
  typedef std::multimap<std::pair<CString,CString>, CFilterElementHide> TFilterElementHideTagsNamed;

  // Tag -> Filter
  typedef std::multimap<CString, CFilterElementHide> TFilterElementHideTags;


  TFilterElementHideTagsNamed m_elementHideTagsId;
  TFilterElementHideTagsNamed m_elementHideTagsClass;
  TFilterElementHideTags m_elementHideTags;

  void ClearFilters();

public:
  CPluginFilter(const CString& dataPath = "");
  bool LoadHideFilters(std::vector<std::wstring> filters);
  bool AddFilterElementHide(CString filter);
  bool IsElementHidden(const std::wstring& tag, IHTMLElement* pEl, const std::wstring& domain, const std::wstring& indent) const;
  HANDLE hideFiltersLoadedEvent;
};

typedef std::shared_ptr<CPluginFilter> PluginFilterPtr;

#endif // _PLUGIN_FILTER_H_
