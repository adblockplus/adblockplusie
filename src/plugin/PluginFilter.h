#ifndef _PLUGIN_FILTER_H_
#define _PLUGIN_FILTER_H_


#include "PluginTypedef.h"
#include <memory>

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

  enum EContentType
  {
    contentTypeUnknown = 0,
    contentTypeOther = 1,
    contentTypeScript = 2,
    contentTypeImage = 4,
    contentTypeStyleSheet = 8,
    contentTypeObject = 16,
    contentTypeSubdocument = 32,
    contentTypeDocument = 64,
    contentTypeBackground = 256,
    contentTypeXbl = 512,
    contentTypePing = 1024,
    contentTypeXmlHttpRequest = 2048,
    contentTypeObjectSubrequest = 4096,
    contentTypeDtd = 8192,
    contentTypeAny = 65535
  } contentType;

  enum EFilterType
  {
    filterTypeBlocking = 0,
    filterTypeWhiteList = 1,
    filterTypeElementHide = 2,
    filterTypeUnknown = 3
  } filterType;

  int m_contentType;
  enum EFilterType m_filterType;
  std::vector<CString> m_stringElements;
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

  std::map<CString, int> m_contentMap;
  std::map<int, CString> m_contentMapText;

  static CComAutoCriticalSection s_criticalSectionFilterMap;

  typedef std::map<DWORD, CFilter> TFilterMap;
  typedef std::vector<CFilter> TFilterMapDefault;

  // (Tag,Name) -> Filter
  typedef std::multimap<std::pair<CString,CString>, CFilterElementHide> TFilterElementHideTagsNamed;

  // Tag -> Filter
  typedef std::multimap<CString, CFilterElementHide> TFilterElementHideTags;


  TFilterElementHideTagsNamed m_elementHideTagsId;
  TFilterElementHideTagsNamed m_elementHideTagsClass;
  TFilterElementHideTags m_elementHideTags;

  TFilterMap m_filterMap[2][2];
  TFilterMapDefault m_filterMapDefault[2];

  void ClearFilters();

  int FindMatch(const CString& src, CString filterPart, int startPos=0) const;
  bool IsSpecialChar(TCHAR testChar) const;
  bool IsSubdomain(const CString& subdomain, const CString& domain) const;

public:

  CPluginFilter(const CString& dataPath = "");

  bool LoadHideFilters(std::vector<std::string> filters);

  bool AddFilterElementHide(CString filter);


  bool IsElementHidden(const CString& tag, IHTMLElement* pEl, const CString& domain, const CString& indent) const;


  bool ShouldBlock(CString src, int contentType, const CString& domain, bool addDebug=false) const;
  bool ShouldWhiteList(CString url) const;
};


#endif // _PLUGIN_FILTER_H_
