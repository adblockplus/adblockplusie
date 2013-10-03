#include "PluginStdAfx.h"

#include "PluginFilter.h"

#if (defined PRODUCT_ADBLOCKPLUS)
#include "PluginSettings.h"
#include "PluginClient.h"
#include "PluginClientFactory.h"
#endif

#include "PluginMutex.h"
#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginClass.h"
#include "mlang.h"

#include "..\shared\CriticalSection.h"


// The filters are described at http://adblockplus.org/en/filters

static CriticalSection s_criticalSectionFilterMap;

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

  TCHAR firstTag = filterString.GetAt(0);
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
    TCHAR firstId = filterString.GetAt(0);

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
  m_contentType = filter.m_contentType;
  m_filterType  = filter.m_filterType;

  m_isFirstParty = filter.m_isFirstParty;
  m_isThirdParty = filter.m_isThirdParty;

  m_isMatchCase  = filter.m_isMatchCase;
  m_isFromStart = filter.m_isFromStart;
  m_isFromEnd = filter.m_isFromEnd;

  m_stringElements = filter.m_stringElements;

  m_filterText = filter.m_filterText;

  m_hitCount = filter.m_hitCount;
}


CFilter::CFilter() : m_isMatchCase(false), m_isFirstParty(false), m_isThirdParty(false), m_contentType(CFilter::contentTypeAny),
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
    tagName.ToLower();
    hr = pEl->get_tagName(&tagName);
    if ((hr != S_OK) || (tagName != CComBSTR(m_tag)))
    {
      return false;
    }
  }

  // Check attributes
  for (std::vector<CFilterElementHideAttrSelector>::const_iterator attrIt = m_attributeSelectors.begin(); 
        attrIt != m_attributeSelectors.end(); ++ attrIt)
  {
    CString value;
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
      CComVariant vAttr;
      if (SUCCEEDED(pEl->getAttribute(attrIt->m_bstrAttr, 0, &vAttr)))
      {
        attrFound = true;
        if (vAttr.vt == VT_BSTR)
        {
          value = vAttr.bstrVal;
        }
        else if (vAttr.vt == VT_I4)
        {
          value.Format(L"%u", vAttr.iVal);
        }
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
      if (pPrevSiblingNode)
      {
        long type;
        do
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
        } while (pPrevSiblingNode && type != 1);

        if (pPrevSiblingNode)
          hr = pPrevSiblingNode.QueryInterface(&pDomPredecessor);
        else
          return false;
      }
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
  m_contentMapText[CFilter::contentTypeDocument] = "DOCUMENT";
  m_contentMapText[CFilter::contentTypeObject] = "OBJECT";
  m_contentMapText[CFilter::contentTypeImage] = "IMAGE";
  m_contentMapText[CFilter::contentTypeScript] = "SCRIPT";
  m_contentMapText[CFilter::contentTypeOther] = "OTHER";
  m_contentMapText[CFilter::contentTypeUnknown] = "OTHER";
  m_contentMapText[CFilter::contentTypeSubdocument] = "SUBDOCUMENT";
  m_contentMapText[CFilter::contentTypeStyleSheet] = "STYLESHEET";
  m_contentMapText[CFilter::contentTypeXmlHttpRequest] = "XMLHTTPREQUEST";

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
    TCHAR separatorChar;
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

bool CPluginFilter::IsElementHidden(const CString& tag, IHTMLElement* pEl, const CString& domain, const CString& indent) const
{
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
    CString domainTest = domain;

    // Search tag/id filters
    if (!id.IsEmpty())
    {
      std::pair<TFilterElementHideTagsNamed::const_iterator, TFilterElementHideTagsNamed::const_iterator> idItEnum =
        m_elementHideTagsId.equal_range(std::make_pair(tag, id));
      for (TFilterElementHideTagsNamed::const_iterator idIt = idItEnum.first; idIt != idItEnum.second; idIt ++)
      {
        if (idIt->second.IsMatchFilterElementHide(pEl))
        {
#ifdef ENABLE_DEBUG_RESULT
          DEBUG_HIDE_EL(indent + "HideEl::Found (tag/id) filter:" + idIt->second.m_filterText)
            CPluginDebug::DebugResultHiding(tag, "id:" + id, idIt->second.m_filterText);
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
            CPluginDebug::DebugResultHiding(tag, "id:" + id, idIt->second.m_filterText);
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
          m_elementHideTagsClass.equal_range(std::make_pair(tag, className));

        for (TFilterElementHideTagsNamed::const_iterator classIt = classItEnum.first; classIt != classItEnum.second; ++classIt)
        {
          if (classIt->second.IsMatchFilterElementHide(pEl))
          {
#ifdef ENABLE_DEBUG_RESULT
            DEBUG_HIDE_EL(indent + "HideEl::Found (tag/class) filter:" + classIt->second.m_filterText)
              CPluginDebug::DebugResultHiding(tag, "class:" + className, classIt->second.m_filterText);
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
            DEBUG_HIDE_EL(indent + "HideEl::Found (?/class) filter:" + classIt->second.m_filterText)
              CPluginDebug::DebugResultHiding(tag, "class:" + className, classIt->second.m_filterText);
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
      = m_elementHideTags.equal_range(tag);
    for (TFilterElementHideTags::const_iterator tagIt = tagItEnum.first; tagIt != tagItEnum.second; ++ tagIt)
    {
      if (tagIt->second.IsMatchFilterElementHide(pEl))
      {
#ifdef ENABLE_DEBUG_RESULT
        DEBUG_HIDE_EL(indent + "HideEl::Found (tag) filter:" + tagIt->second.m_filterText)
          CPluginDebug::DebugResultHiding(tag, "-", tagIt->second.m_filterText);
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

#ifdef PRODUCT_ADBLOCKPLUS
  CPluginClient* client = CPluginClient::GetInstance();
#endif

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
          CPluginDebug::DebugResult(L"Error loading hide filter: " + filter);
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
  }
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
    type = "OTHER";

    std::map<int,CString>::const_iterator it = m_contentMapText.find(contentType);
    if (it != m_contentMapText.end())
    {
      type = it->second;
    }
  }

  CPluginClient* client = CPluginClient::GetInstance();
  if (client->Matches(std::wstring(src), std::wstring(type), std::wstring(domain)))
  {
    if (addDebug)
    {
      DEBUG_FILTER("Filter::ShouldBlock " + type + " YES")

#ifdef ENABLE_DEBUG_RESULT
        CPluginDebug::DebugResultBlocking(type, src, domain);
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
