#include "PluginStdAfx.h"

#include "PluginFilter.h"

#if (defined PRODUCT_ADBLOCKPLUS)
#include "PluginSettings.h"
#include "PluginClient.h"
#include "PluginClientFactory.h"
#endif

#include "PluginMutex.h"
#include "PluginHttpRequest.h"
#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginClass.h"
#include "mlang.h"

#if (defined PRODUCT_ADBLOCKPLUS)

class CPluginFilterLock : public CPluginMutex
{

private:

  static CComAutoCriticalSection s_criticalSectionFilterLock;

public:

  CPluginFilterLock(const CString& filterFile) : CPluginMutex("FilterFile" + filterFile, PLUGIN_ERROR_MUTEX_FILTER_FILE)
  {
    s_criticalSectionFilterLock.Lock();
  }

  ~CPluginFilterLock()
  {
    s_criticalSectionFilterLock.Unlock();
  }
};

CComAutoCriticalSection CPluginFilterLock::s_criticalSectionFilterLock;

#endif


// The filters are described at http://adblockplus.org/en/filters

CComAutoCriticalSection CPluginFilter::s_criticalSectionFilterMap;


// ============================================================================
// CFilterElementHideAttrSelector
// ============================================================================

CFilterElementHideAttrSelector::CFilterElementHideAttrSelector() : m_isStarting(false), m_isEnding(false), m_isAnywhere(false), m_isExact(false), m_isStyle(false), m_isId(false), m_isClass(false), m_bstrAttr(NULL)
{
}

CFilterElementHideAttrSelector::CFilterElementHideAttrSelector(const CFilterElementHideAttrSelector& filter)
{
  m_isStarting = filter.m_isStarting;
  m_isEnding = filter.m_isEnding;
  m_isAnywhere = filter.m_isAnywhere;
  m_isExact = filter.m_isExact;

  m_isStyle = filter.m_isStyle;
  m_isId = filter.m_isId;
  m_isClass = filter.m_isClass;

  m_bstrAttr = filter.m_bstrAttr;

  m_value = filter.m_value;
}

CFilterElementHideAttrSelector::~CFilterElementHideAttrSelector()
{
}


// ============================================================================
// CFilterElementHide
// ============================================================================

CFilterElementHide::CFilterElementHide(const CString& filterText) : m_filterText(filterText)
{
}

CFilterElementHide::CFilterElementHide(const CFilterElementHide& filter)
{
  m_filterText = filter.m_filterText;

  m_tagId = filter.m_tagId;
  m_tagClassName = filter.m_tagClassName;

  m_domainsNot = filter.m_domainsNot;
  m_attributeSelectors = filter.m_attributeSelectors;
}


// ============================================================================
// CFilter
// ============================================================================

CFilter::CFilter(const CFilter& filter)
{
  m_contentType = filter.m_contentType;
  m_filterType  = filter.m_filterType;

  m_isFirstParty = filter.m_isFirstParty;
  m_isThirdParty = filter.m_isThirdParty;

  m_isMatchCase  = filter.m_isMatchCase;
  m_isFromStart = filter.m_isFromStart;
  m_isFromStartDomain = filter.m_isFromStartDomain;
  m_isFromEnd = filter.m_isFromEnd;

  m_stringElements = filter.m_stringElements;

  m_filterText = filter.m_filterText;

  m_domains = filter.m_domains;
  m_domainsNot = filter.m_domainsNot;

  m_hitCount = filter.m_hitCount;
}


CFilter::CFilter() : m_isMatchCase(false), m_isFirstParty(false), m_isThirdParty(false), m_contentType(CFilter::contentTypeAny),
  m_isFromStart(false), m_isFromStartDomain(false), m_isFromEnd(false), m_hitCount(0)
{
}


// ============================================================================
// CPluginFilter
// ============================================================================

CPluginFilter::CPluginFilter(const CString& dataPath) : m_dataPath(dataPath)
{
  m_contentMap["document"] = CFilter::contentTypeDocument;
  m_contentMap["subdocument"] = CFilter::contentTypeSubdocument;
  m_contentMap["sub-document"] = CFilter::contentTypeSubdocument;
  m_contentMap["sub_document"] = CFilter::contentTypeSubdocument;
  m_contentMap["other"] = CFilter::contentTypeOther;
  m_contentMap["image"] = CFilter::contentTypeImage;
  m_contentMap["script"] = CFilter::contentTypeScript;
  m_contentMap["object"] = CFilter::contentTypeObject;
  m_contentMap["object-subrequest"] = CFilter::contentTypeObjectSubrequest;
  m_contentMap["object_subrequest"] = CFilter::contentTypeObjectSubrequest;
  m_contentMap["xml-request"] = CFilter::contentTypeXmlHttpRequest;
  m_contentMap["xml_request"] = CFilter::contentTypeXmlHttpRequest;
  m_contentMap["xmlhttprequest"] = CFilter::contentTypeXmlHttpRequest;
  m_contentMap["stylesheet"] = CFilter::contentTypeStyleSheet;
  m_contentMap["background"] = CFilter::contentTypeBackground;

  m_contentMapText[CFilter::contentTypeDocument] = "DOCUMENT";
  m_contentMapText[CFilter::contentTypeObject] = "OBJECT";
  m_contentMapText[CFilter::contentTypeImage] = "IMAGE";
  m_contentMapText[CFilter::contentTypeScript] = "SCRIPT";
  m_contentMapText[CFilter::contentTypeOther] = "OTHER";
  m_contentMapText[CFilter::contentTypeUnknown] = "OTHER";
  m_contentMapText[CFilter::contentTypeSubdocument] = "SUBDOCUMENT";
  m_contentMapText[CFilter::contentTypeStyleSheet] = "STYLESHEET";

  ClearFilters(); 
}


bool CPluginFilter::AddFilterElementHide(CString filterText)
{

  DEBUG_FILTER("Input: " + filterText + " filterFile" + filterFile);

  s_criticalSectionFilterMap.Lock();    
  {
    // Create filter descriptor
    CFilterElementHide filter(filterText);

    CString filterString  = filterText;

    // Find tag name, class or any (*)
    CString tag;

    TCHAR firstTag = filterString.GetAt(0);
    // Any tag
    if (firstTag == '*')
    {
      filterString = filterString.Right(filterString.GetLength() - 1);
    }
    // Any tag (implicitely)
    else if (firstTag == '[' || firstTag == '.' || firstTag == '#')
    {
    }
    // Real tag
    else if (isalnum(firstTag))
    {
      int pos = filterString.FindOneOf(L".#[(");
      if (pos > 0) 
      {
        tag = filterString.Left(pos).MakeLower();
      }
      else
      {
        tag = filterString.MakeLower();
      }
      filterString = filterString.Right(filterString.GetLength() - tag.GetLength());
    }
    // Error
    else
    {
      DEBUG_FILTER("Filter::Error parsing filter:" + filterFile + "/" + filterText + " (invalid tag)")
        s_criticalSectionFilterMap.Unlock();    
      return false;
    }

    // Find Id and class name
    CString id;
    CString className;

    // In old format, id/class is part of attributes
    if (!filterString.IsEmpty())
    {
      TCHAR firstId = filterString.GetAt(0);

      // Id
      if (firstId == '#')
      {
        int pos = filterString.Find('[');
        if (pos > 0)
        {
          id = filterString.Mid(1, pos - 1);
        }
        else
        {
          id = filterString.Right(filterString.GetLength() - 1);
        }
        filterString = filterString.Right(filterString.GetLength() - id.GetLength() - 1);
      }
      // Class name
      else if (firstId == '.')
      {
        int pos = filterString.Find('[');
        if (pos > 0)
        {
          className = filterString.Mid(1, pos - 1);
        }
        else
        {
          className = filterString.Right(filterString.GetLength() - 1);
        }
        filterString = filterString.Right(filterString.GetLength() - className.GetLength() - 1);
      }
      // None
      else if (firstId == '[')
      {
      }
      else
      {
        DEBUG_FILTER("Filter::Error parsing filter:" + filterFile + "/" + filterText + " (invalid id/class)")
          s_criticalSectionFilterMap.Unlock();    
        return false;
      }
    }

    char chAttrStart = '[';
    char chAttrEnd   = ']';

    // Find attribute selectors
    if (!filterString.IsEmpty() && filterString.GetAt(0) != chAttrStart)
    {
      DEBUG_FILTER("Filter::Error parsing filter:" + filterFile + "/" + filterText + " (invalid attr selector)")
        s_criticalSectionFilterMap.Unlock();    
      return false;
    }

    if (!filterString.IsEmpty())
    {
      int startPos = 0;
      int endPos = 0;

      while ((startPos = filterString.Find(chAttrStart, startPos)) >= 0 && (endPos = filterString.Find(chAttrEnd, startPos)) > startPos)
      {
        CFilterElementHideAttrSelector attrSelector;

        CString arg = filterString.Mid(startPos + 1, endPos - startPos - 1);
        int delimiterPos = 0;

        if ((delimiterPos = arg.Find('=')) > 0)
        {
          attrSelector.m_value = arg.Mid(delimiterPos + 1, arg.GetLength() - delimiterPos - 1);
          if (attrSelector.m_value.GetAt(0) == '\"' && attrSelector.m_value.GetAt(attrSelector.m_value.GetLength() - 1) == '\"' && attrSelector.m_value.GetLength() >= 2)
          {
            attrSelector.m_value = attrSelector.m_value.Mid(1, attrSelector.m_value.GetLength() - 2);
          }

          if (arg.GetAt(delimiterPos - 1) == '^')
          {
            attrSelector.m_bstrAttr = arg.Left(delimiterPos - 1);
            attrSelector.m_isStarting = true;
          }
          else if (arg.GetAt(delimiterPos - 1) == '*')
          {
            attrSelector.m_bstrAttr = arg.Left(delimiterPos - 1);
            attrSelector.m_isAnywhere = true;
          }
          else if (arg.GetAt(delimiterPos - 1) == '$')
          {
            attrSelector.m_bstrAttr = arg.Left(delimiterPos - 1);
            attrSelector.m_isEnding = true;
          }
          else
          {
            attrSelector.m_bstrAttr = arg.Left(delimiterPos);
            attrSelector.m_isExact = true;
          }

          CString tag = attrSelector.m_bstrAttr;
          if (tag == "style")
          {
            attrSelector.m_isStyle = true;
            attrSelector.m_value.MakeLower();
          }
          else if (tag == "id")
          {
            attrSelector.m_isId = true;
          }
          else if (tag == "class")
          {
            attrSelector.m_isClass = true;
          }

          filter.m_attributeSelectors.push_back(attrSelector);
        }

        startPos = endPos + 1;
      }

      // End check
      if (filterString.GetLength() != endPos + 1)
      {
        DEBUG_FILTER("Filter::Error parsing filter:" + filterFile + "/" + filterText + " (more data)")
          s_criticalSectionFilterMap.Unlock();    
        return false;
      }
    }

    if (!id.IsEmpty())
    {
      if (id.Find(L".") > 0)
      {
        id = id.Left(id.Find(L"."));
        filter.m_tagClassName = id.Right(id.Find(L"."));
        filter.m_tagId = id;
      }
      m_elementHideTagsId[std::make_pair(tag, id)] = filter;
    }
    else if (!className.IsEmpty())
    {
      m_elementHideTagsClass[std::make_pair(tag, className)] = filter;
    }
    else
    {
      m_elementHideTags[tag] = filter;
    }
  }
  s_criticalSectionFilterMap.Unlock();    

  return true;
}


bool CPluginFilter::IsMatchFilterElementHide(const CFilterElementHide& filter, IHTMLElement* pEl, const CString& domain) const
{
  bool isHidden = true;

  // Check is not domains
  if (!filter.m_domainsNot.empty())
  {
    for (std::set<CString>::const_iterator it = filter.m_domainsNot.begin(); isHidden && it != filter.m_domainsNot.end(); ++it)
    {
      if (domain == *(it) || IsSubdomain(domain, *it))
      {
        isHidden = false;
      }
    }
  }

  // Check attributes
  if (isHidden && !filter.m_attributeSelectors.empty())
  {
    CString style;

    CComPtr<IHTMLStyle> pStyle;
    if (SUCCEEDED(pEl->get_style(&pStyle)) && pStyle)
    {
      CComBSTR bstrStyle;

      if (SUCCEEDED(pStyle->get_cssText(&bstrStyle)) && bstrStyle)
      {
        style = bstrStyle;
        style.MakeLower();
      }
    }

    std::vector<CFilterElementHideAttrSelector>::const_iterator attrIt = filter.m_attributeSelectors.begin();

    while (isHidden && attrIt != filter.m_attributeSelectors.end())
    {
      CComVariant vAttr;
      UINT attrLength = 0;

      CString value;

      if (attrIt->m_isStyle)
      {
        if (!style.IsEmpty())
        {
          value = style;
        }
      }
      else if (attrIt->m_isClass)
      {
        CComBSTR bstrClassNames;
        if (SUCCEEDED(pEl->get_className(&bstrClassNames)) && bstrClassNames)
        {
          value = bstrClassNames;
        }
      }
      else if (attrIt->m_isId)
      {
        CComBSTR bstrId;
        if (SUCCEEDED(pEl->get_id(&bstrId)) && bstrId)
        {
          value = bstrId;
        }
      }
      else if (SUCCEEDED(pEl->getAttribute(attrIt->m_bstrAttr, 0, &vAttr)) && vAttr.vt != VT_NULL)
      {
        if (vAttr.vt == VT_BSTR)
        {
          value = vAttr.bstrVal;
        }
        else if (vAttr.vt == VT_I4)
        {
          value.Format(L"%u", vAttr.iVal);
        }
      }

      if (!value.IsEmpty())
      {
        if (attrIt->m_isExact)
        {
          if (attrIt->m_isStyle)
          {
            // IE adds ; to the end of the style
            isHidden = (value + ';' == attrIt->m_value);
          }
          else
          {
            isHidden = (value == attrIt->m_value);
          }
        }
        else if (attrIt->m_isStarting)
        {
          isHidden = value.Find(attrIt->m_value) == 0;
        }
        else if (attrIt->m_isEnding)
        {
          isHidden = value.Find(attrIt->m_value) == attrLength - attrIt->m_value.GetLength();
        }
        else if (attrIt->m_isAnywhere)
        {
          isHidden = value.Find(attrIt->m_value) >= 0;
        }
      }
      else
      {
        isHidden = false;
        break;
      }

      ++attrIt;
    }
  }

  return isHidden;
}


bool CPluginFilter::IsElementHidden(const CString& tag, IHTMLElement* pEl, const CString& domain, const CString& indent) const
{
  bool isHidden = false;

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

  s_criticalSectionFilterMap.Lock();
  {
    CString domainTest = domain;

    // Iterate through sub domains
    int lastDotPos = domainTest.ReverseFind('.');

    int dotPos = domainTest.Find('.');

    while (dotPos >= 0)
    {
      // Lookup domain list
      TFilterElementHideDomains::const_iterator domainIt = m_elementHideDomains.find(domainTest);
      if (domainIt != m_elementHideDomains.end())
      {
        // Iterate through all filter definitions for the domain
        TFilterElementHideDomain::const_iterator filterIt = domainIt->second.begin();
        while (isHidden == false && filterIt != domainIt->second.end())
        {
          if ((tag == filterIt->first || filterIt->first.IsEmpty()) && (filterIt->second.m_tagId.IsEmpty() || id == filterIt->second.m_tagId))
          {
            // Check all classes
            if (classNames == filterIt->second.m_tagClassName || filterIt->second.m_tagClassName.IsEmpty())
            {
              isHidden = IsMatchFilterElementHide(filterIt->second, pEl, domain);
#ifdef ENABLE_DEBUG_RESULT
              if (isHidden)
              {
                if (!id.IsEmpty() && !classNames.IsEmpty())
                {
                  DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                    CPluginDebug::DebugResultHiding(tag, "id:" + id + " class:" + classNames, filterIt->second.m_filterText);
                }
                else if (!id.IsEmpty())
                {
                  DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                    CPluginDebug::DebugResultHiding(tag, "id:" + id, filterIt->second.m_filterText);
                }
                else if (!classNames.IsEmpty())
                {
                  DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                    CPluginDebug::DebugResultHiding(tag, "class:" + classNames, filterIt->second.m_filterText);
                }
                else
                {
                  DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                    CPluginDebug::DebugResultHiding(tag, "-" + classNames, filterIt->second.m_filterText);
                }
              }
#endif
            }

            // Iterate through class names
            if (isHidden == false && !classNames.IsEmpty() && !filterIt->second.m_tagClassName.IsEmpty() && classNames.Find(' ') > 0)
            {
              int pos = 0;
              CString className = classNames.Tokenize(L" \t\n\r", pos);

              while (isHidden == false && pos >= 0)
              {
                if (className == filterIt->second.m_tagClassName)
                {
                  isHidden = IsMatchFilterElementHide(filterIt->second, pEl, domain);
#ifdef ENABLE_DEBUG_RESULT
                  if (isHidden)
                  {
                    if (!id.IsEmpty())
                    {
                      DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                        CPluginDebug::DebugResultHiding(tag, "id:" + id + " class:" + classNames, filterIt->second.m_filterText);
                    }
                    else
                    {
                      DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                        CPluginDebug::DebugResultHiding(tag, "-" + classNames, filterIt->second.m_filterText);
                    }
                  }
#endif
                }

                // Next class name
                className = classNames.Tokenize(L" \t\n\r", pos);
              }
            }
          }

          ++filterIt;
        }
      }

      if (dotPos != lastDotPos)
      {
        domainTest = domain.Right(domain.GetLength() - dotPos - 1);
      }

      dotPos = domain.Find('.', dotPos + 1);
    }

    // Search tag/id filters
    if (isHidden == false && !id.IsEmpty())
    {
      TFilterElementHideTagsNamed::const_iterator idIt = m_elementHideTagsId.find(std::make_pair(tag, id));
      if (idIt != m_elementHideTagsId.end())
      {
        isHidden = IsMatchFilterElementHide(idIt->second, pEl, domain);
#ifdef ENABLE_DEBUG_RESULT
        if (isHidden)
        {
          DEBUG_HIDE_EL(indent + "HideEl::Found (tag/id) filter:" + idIt->second.m_filterText)
            CPluginDebug::DebugResultHiding(tag, "id:" + id, idIt->second.m_filterText);
        }
#endif
      }

      // Search general id
      if (isHidden == false)
      {
        idIt = m_elementHideTagsId.find(std::make_pair("", id));
        if (idIt != m_elementHideTagsId.end())
        {
          isHidden = IsMatchFilterElementHide(idIt->second, pEl, domain);
#ifdef ENABLE_DEBUG_RESULT
          if (isHidden)
          {
            DEBUG_HIDE_EL(indent + "HideEl::Found (?/id) filter:" + idIt->second.m_filterText)
              CPluginDebug::DebugResultHiding(tag, "id:" + id, idIt->second.m_filterText);
          }
#endif
        }
      }
    }

    // Search tag/className filters
    if (isHidden == false && !classNames.IsEmpty())
    {
      int pos = 0;
      CString className = classNames.Tokenize(L" \t\n\r", pos);

      while (isHidden == false && pos >= 0)
      {
        TFilterElementHideTagsNamed::const_iterator classIt = m_elementHideTagsClass.find(std::make_pair(tag, className));
        if (classIt != m_elementHideTagsClass.end())
        {
          isHidden = IsMatchFilterElementHide(classIt->second, pEl, domain);
#ifdef ENABLE_DEBUG_RESULT
          if (isHidden)
          {
            DEBUG_HIDE_EL(indent + "HideEl::Found (tag/class) filter:" + classIt->second.m_filterText)
              CPluginDebug::DebugResultHiding(tag, "class:" + className, classIt->second.m_filterText);
          }
#endif
        }

        // Search general class name
        if (isHidden == false)
        {
          classIt = m_elementHideTagsClass.find(std::make_pair("", className));
          if (classIt != m_elementHideTagsClass.end())
          {
            isHidden = IsMatchFilterElementHide(classIt->second, pEl, domain);
#ifdef ENABLE_DEBUG_RESULT
            if (isHidden)
            {
              DEBUG_HIDE_EL(indent + "HideEl::Found (?/class) filter:" + classIt->second.m_filterText)
                CPluginDebug::DebugResultHiding(tag, "class:" + className, classIt->second.m_filterText);
            }
#endif
          }
        }

        // Next class name
        className = classNames.Tokenize(L" \t\n\r", pos);
      }
    }

    // Search tag filters
    if (isHidden == false)
    {
      TFilterElementHideTags::const_iterator tagIt = m_elementHideTags.find(tag);
      if (tagIt != m_elementHideTags.end())
      {
        isHidden = IsMatchFilterElementHide(tagIt->second, pEl, domain);
#ifdef ENABLE_DEBUG_RESULT
        if (isHidden)
        {
          DEBUG_HIDE_EL(indent + "HideEl::Found (tag) filter:" + tagIt->second.m_filterText)
            CPluginDebug::DebugResultHiding(tag, "-", tagIt->second.m_filterText);
        }
#endif
      }
    }
  }
  s_criticalSectionFilterMap.Unlock();

  return isHidden;
}

bool CPluginFilter::LoadHideFilters(std::vector<std::string> filters)
{
  bool isRead = false;

#ifdef PRODUCT_ADBLOCKPLUS
  CPluginClient* client = CPluginClient::GetInstance();
#endif

  // Parse hide string
  int pos = 0;

  s_criticalSectionFilterMap.Lock();
  {
    for (std::vector<std::string>::iterator it = filters.begin(); it < filters.end(); ++it)
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
          CPluginDebug::DebugResult(L"Error loading hide filter: " + filter);
#endif
        }
      }
    }
  }
  s_criticalSectionFilterMap.Unlock();

  return isRead;
}

void CPluginFilter::ClearFilters()
{
  // Clear filter maps
  s_criticalSectionFilterMap.Lock();    
  {
    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < 2; j++)
      {
        m_filterMap[i][j].clear();
      }
      m_filterMapDefault[i].clear();
    }

    m_elementHideTags.clear();
    m_elementHideTagsId.clear();
    m_elementHideTagsClass.clear();
    m_elementHideDomains.clear();
  }
  s_criticalSectionFilterMap.Unlock();
}


bool CPluginFilter::ShouldWhiteList(CString src) const
{
  // We should not block the empty string, so all filtering does not make sense
  // Therefore we just return
  if (src.Trim().IsEmpty())
  {
    return false;
  }

  //TODO: Implement whitelisting check from libadblockplus here. Probably not needed anymore
  return false;
}


bool CPluginFilter::ShouldBlock(CString src, int contentType, const CString& domain, bool addDebug) const
{
  // We should not block the empty string, so all filtering does not make sense
  // Therefore we just return
  if (src.Trim().IsEmpty())
  {
    return false;
  }

  CPluginSettings* settings = CPluginSettings::GetInstance();

  CString type;
  if (addDebug)
  {
    type = "???";

    std::map<int,CString>::const_iterator it = m_contentMapText.find(contentType);
    if (it != m_contentMapText.end())
    {
      type = it->second;
    }
  }

  CPluginClient* client = CPluginClient::GetInstance();
  AdblockPlus::FilterEngine* filterEngine = client->GetFilterEngine();

  //TODO: Make sure if the content type names are in sync with libadblockplus
  std::string contentTypeString = CT2A(type, CP_UTF8);

  CT2CA srcMb(src, CP_UTF8);
  std::string url(srcMb);

  std::string domainMb = CT2CA(domain);

  if (filterEngine->Matches(url, contentTypeString, domainMb))
  {
    if (addDebug)
    {
      DEBUG_FILTER("Filter::ShouldBlock " + type + " YES")

#ifdef ENABLE_DEBUG_RESULT
        CPluginDebug::DebugResultBlocking(type, src);
#endif
    }
    return true;
  }
  return false;
}

int CPluginFilter::FindMatch(const CString& src, CString filterPart, int srcStartPos) const
{
  int filterCurrentPos = filterPart.Find('^');
  if (filterCurrentPos >= 0)
  {
    int srcLength = src.GetLength();
    int srcFilterPos = -1;
    int srcCurrentPos = srcStartPos;
    int srcLastTestPos = -1;

    // Special char(s) as first char?
    int nFirst = 0;
    while (filterCurrentPos == 0)
    {
      filterPart = filterPart.Right(filterPart.GetLength() - 1);
      ++nFirst;
      filterCurrentPos = filterPart.Find('^');
    }

    // Find first part without special chars
    CString test = filterCurrentPos >= 0 ? filterPart.Left(filterCurrentPos) : filterPart;
    int testLength = test.GetLength();

    while (filterCurrentPos >= 0 || testLength > 0)
    {
      int srcFindPos = testLength > 0 ? src.Find(test, srcCurrentPos) : srcCurrentPos;
      if (srcFindPos < 0)
      {
        // Always fail - no need to iterate
        return -1;
      }

      if (testLength > 0)
      {
        srcLastTestPos = srcFindPos;
      }

      // Already found earlier part; this part must follow
      if (srcFilterPos >= 0)
      {
        // Found position must be position we are at in source
        if (srcFindPos != srcCurrentPos)
        {
          // Try to next iteration  maybe we will find it later
          return srcLastTestPos >= 0 ? FindMatch(src, filterPart, srcLastTestPos + 1) : -1;
        }
      }
      else
      {
        srcCurrentPos = srcFindPos;
        srcFilterPos = srcFindPos;

        // If starting with special char, check for that
        for (int n = 1; n <= nFirst; n++)
        {
          if (--srcFilterPos < 0 || srcFilterPos < srcStartPos || !IsSpecialChar(src.GetAt(srcFilterPos)))
          {
            // Try to next iteration  maybe we will find it later
            return srcLastTestPos >= 0 ? FindMatch(src, filterPart, srcLastTestPos + 1) : -1;
          }
        }
        nFirst = 0;
      }

      srcCurrentPos += testLength;

      // Next char must be special char
      if (filterCurrentPos >= 0 && !IsSpecialChar(src.GetAt(srcCurrentPos++)))
      {
        // Try to next iteration  maybe we will find it later
        return srcLastTestPos >= 0 ? FindMatch(src, filterPart, srcLastTestPos + 1) : -1;
      }

      // Upate test strin and position for next loop
      if (filterCurrentPos >= 0)
      {
        int filterNewPos = filterPart.Find('^', filterCurrentPos + 1);
        test = filterNewPos >= 0 ? filterPart.Mid(filterCurrentPos + 1, filterNewPos - filterCurrentPos - 1) : filterPart.Right(filterPart.GetLength() - filterCurrentPos - 1);

        filterCurrentPos = filterNewPos;
      }
      else
      {
        test.Empty();
      }
      testLength = test.GetLength();
    }

    // If only special chars, check for that
    if (nFirst > 0)
    {
      int nFound = 0;

      srcFilterPos = srcCurrentPos;

      while (nFound != nFirst && srcLength >= srcFilterPos + nFound)
      {
        if (IsSpecialChar(src.GetAt(srcFilterPos + nFound)))
        {
          nFound++;
        }
        else
        {
          if (nFound > 0)
          {
            nFound--;
          }

          srcFilterPos++;
        }
      }

      if (nFound != nFirst)
      {
        // Always fail - no need to iterate
        return -1;
      }
    }

    return srcFilterPos;
  }
  else
  {
    return src.Find(filterPart, srcStartPos);
  }
}

bool CPluginFilter::IsSpecialChar(TCHAR testChar) const
{
  if (isalnum(testChar) || testChar == '.' || testChar == '-' || testChar == '%')
  {
    return false;
  }

  return true;
}

bool CPluginFilter::IsSubdomain(const CString& subdomain, const CString& domain) const
{
  int pos = subdomain.Find(domain);

  if (pos > 0 && domain.GetLength() + pos == subdomain.GetLength())
  {
    if (subdomain.GetAt(pos - 1) == '.')
    {
      return true;
    }
  }

  return false;
}
