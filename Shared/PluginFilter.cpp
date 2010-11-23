#include "PluginStdAfx.h"

#include "PluginFilter.h"

#if (defined PRODUCT_SIMPLEADBLOCK)
 #include "PluginSettings.h"
 #include "PluginClient.h"
 #include "PluginClientFactory.h"
#endif

#include "PluginMutex.h"


#if (defined PRODUCT_SIMPLEADBLOCK)

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

CFilterElementHide::CFilterElementHide(const CString& filterText, const CString& filterFile) : m_filterText(filterText), m_filterFile(filterFile)
{
}

CFilterElementHide::CFilterElementHide(const CFilterElementHide& filter)
{
    m_filterText = filter.m_filterText;
    m_filterFile = filter.m_filterFile;

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
    m_filterFile = filter.m_filterFile;

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

CPluginFilter::CPluginFilter(const TFilterFileList& list, const CString& dataPath) : m_dataPath(dataPath)
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

    m_contentMapText[CFilter::contentTypeDocument] = "doc";
    m_contentMapText[CFilter::contentTypeObject] = "object";
    m_contentMapText[CFilter::contentTypeImage] = "img";
    m_contentMapText[CFilter::contentTypeScript] = "script";
    m_contentMapText[CFilter::contentTypeOther] = "other";
    m_contentMapText[CFilter::contentTypeUnknown] = "?";
    m_contentMapText[CFilter::contentTypeSubdocument] = "iframe";
    m_contentMapText[CFilter::contentTypeStyleSheet] = "css";

	ParseFilters(list); 
}


CPluginFilter::CPluginFilter(const CString& dataPath) : m_dataPath(dataPath)
{
}


bool CPluginFilter::AddFilterElementHide(CString filterText, CString filterFile)
{
    int delimiterPos = filterText.Find(L"#");
    if (delimiterPos < 0 || filterText.GetLength() <= delimiterPos + 1)
    {
        DEBUG_FILTER("Filter::Error parsing filter:" + filterFile + "/" + filterText + " (no tag)")
        return false;
    }
    
    bool isOldFormat = true;
    if (filterText.GetAt(delimiterPos + 1) == '#')
    {
        isOldFormat = false;
    }

    s_criticalSectionFilterMap.Lock();    
    {
        // Create filter descriptor
        CFilterElementHide filter(filterText, filterFile);

        CString filterDomains = filterText.Left(delimiterPos).MakeLower();
        CString filterString  = filterText.Right(filterText.GetLength() - delimiterPos - (isOldFormat ? 1 : 2));

        bool isDomainSpecific = delimiterPos > 0 && filterDomains.Find('~') < 0;
        
        // Add not-domains to filter
        if (!isDomainSpecific && delimiterPos > 0)
        {
            int endPos = 0;

            while ((endPos = filterDomains.Find(',')) >= 0 || !filterDomains.IsEmpty())
	        {
                CString domain;

	            if (endPos == -1)
	            {
                    domain = filterDomains;

                    filterDomains.Empty();
	            }
	            else
	            {
                    domain = filterDomains.Left(endPos);

                    filterDomains = filterDomains.Right(filterDomains.GetLength() - endPos - 1);
                }

                if (domain.GetAt(0) == '~')
                {
                    domain = domain.Right(domain.GetLength() - 1);
                }
                // Error
                else
                {
                    DEBUG_FILTER("Filter::Error parsing filter:" + filterFile + "/" + filterText + " (conflicting domains)")
                    s_criticalSectionFilterMap.Unlock();    
                    return false;
                }

                filter.m_domainsNot.insert(domain);
            }
        }
        
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
        if (isOldFormat == false && !filterString.IsEmpty())
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

        char chAttrStart = isOldFormat ? '(' : '[';
        char chAttrEnd   = isOldFormat ? ')' : ']';

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

                    // id/class name in old format should not be stored as attributes
                    if (isOldFormat && attrSelector.m_isExact && attrSelector.m_isId)
                    {
                        id = attrSelector.m_value;
                    }
                    else if (isOldFormat && attrSelector.m_isExact && attrSelector.m_isClass)
                    {
                        className = attrSelector.m_value;
                    }
                    else
                    {
                        filter.m_attributeSelectors.push_back(attrSelector);
                    }
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

        // Add the filter        
        if (isDomainSpecific)
        {
            filter.m_tagId = id;
            filter.m_tagClassName = className;
            
            int endPos = 0;

            while ((endPos = filterDomains.Find(',')) >= 0 || !filterDomains.IsEmpty())
            {
                CString domain;

                if (endPos == -1)
                {
                    domain = filterDomains;

                    filterDomains.Empty();
                }
                else
                {
                    domain = filterDomains.Left(endPos);

                    filterDomains = filterDomains.Right(filterDomains.GetLength() - endPos - 1);
                }
                
                TFilterElementHideDomains::iterator it = m_elementHideDomains.find(domain);
                if (it == m_elementHideDomains.end())
                {
                    TFilterElementHideDomain domainList;

                    domainList.insert(std::make_pair(tag, filter));

                    m_elementHideDomains[domain] = domainList;                    
                }
                else
                {
                    it->second.insert(std::make_pair(tag, filter));
                }
            }
        }
        else if (!id.IsEmpty())
        {
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
                                    CPluginDebug::DebugResultHiding(tag, "id:" + id + " class:" + classNames, filterIt->second.m_filterText, filterIt->second.m_filterFile);
                                }
                                else if (!id.IsEmpty())
                                {
                                    DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                                    CPluginDebug::DebugResultHiding(tag, "id:" + id, filterIt->second.m_filterText, filterIt->second.m_filterFile);
                                }
                                else if (!classNames.IsEmpty())
                                {
                                    DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                                    CPluginDebug::DebugResultHiding(tag, "class:" + classNames, filterIt->second.m_filterText, filterIt->second.m_filterFile);
                                }
                                else
                                {
                                    DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                                    CPluginDebug::DebugResultHiding(tag, "-" + classNames, filterIt->second.m_filterText, filterIt->second.m_filterFile);
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
                                            CPluginDebug::DebugResultHiding(tag, "id:" + id + " class:" + classNames, filterIt->second.m_filterText, filterIt->second.m_filterFile);
                                        }
                                        else
                                        {
                                            DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                                            CPluginDebug::DebugResultHiding(tag, "-" + classNames, filterIt->second.m_filterText, filterIt->second.m_filterFile);
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
                    CPluginDebug::DebugResultHiding(tag, "id:" + id, idIt->second.m_filterText, idIt->second.m_filterFile);
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
                        CPluginDebug::DebugResultHiding(tag, "id:" + id, idIt->second.m_filterText, idIt->second.m_filterFile);
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
                        CPluginDebug::DebugResultHiding(tag, "class:" + className, classIt->second.m_filterText, classIt->second.m_filterFile);
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
                            CPluginDebug::DebugResultHiding(tag, "class:" + className, classIt->second.m_filterText, classIt->second.m_filterFile);
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
                    CPluginDebug::DebugResultHiding(tag, "-", tagIt->second.m_filterText, tagIt->second.m_filterFile);
                }
#endif
            }
        }
    }
    s_criticalSectionFilterMap.Unlock();

    return isHidden;
}


void CPluginFilter::AddFilter(CString filterString, CString filterFile, int filterType)
{
    CString raw = filterString;
    
	// Here we should find a key for the filter
	// We find a string of max 8 characters that does not contain any wildcards and which are unique for the filter

    // Find settings part, identified by $
    CString filterSettings;

    int pos = filterString.Find('$');
    if (pos > 0)
    {
        filterSettings = filterString.Right(filterString.GetLength() - pos - 1);
        filterString = filterString.Left(pos);
    }

    // Split filterString to parts

    bool bCheckFromStartDomain = false;
    if (filterString.Find(L"||") == 0)
    {
        bCheckFromStartDomain = true;
        filterString = filterString.Right(filterString.GetLength() - 2);
    }

    bool bCheckFromStart = false;
    if (filterString.GetAt(0) == '|')
    {
        bCheckFromStart = true;
        filterString = filterString.Right(filterString.GetLength() - 1);
    }

    bool bCheckFromEnd = false;
    if (filterString.Right(1) == "|")
    {
        bCheckFromEnd = true;
        filterString = filterString.Left(filterString.GetLength() - 1);
    }

    std::vector<CString> filterParts;
    pos = 0;

    while ((pos = filterString.Find('*')) >= 0)
	{
        if (pos > 0)
        {
            filterParts.push_back(filterString.Left(pos));
        }
        filterString = filterString.Right(filterString.GetLength() - pos - 1);
    }

    if (!filterString.IsEmpty())
    {
        filterParts.push_back(filterString);
    }

    TFilterMap* filterMap = m_filterMap[filterType];

    // Define hash key
    DWORD dwKey = 0;
    DWORD dwKeyMap = 0;

	int keyLength = 4;
	int startCharacter = 0;

    CString filterPart = filterParts[0];
    filterPart.MakeLower();

    int nFilterParts = filterParts.size();
    int nFilterPart = 0;

    int filterPartLength = filterPart.GetLength();

    if (filterPartLength >= 7)
    {
        if (filterPart.Find(L"http://") == 0)
        {
            startCharacter = 7;
        }
        else if (filterPart.Find(L"https://") == 0)
        {
            startCharacter = 8;
        }
    }
    
    while (true)
    {
        // Part is too short as unique key? - try next filter part
        while (filterPartLength < startCharacter + keyLength)
        {
            nFilterPart++;
            if (nFilterPart >= nFilterParts)
            {
                break;
            }
            
            filterPart = filterParts[nFilterPart];
            filterPart.MakeLower();
            filterPartLength = filterPart.GetLength();
            
            startCharacter = 0;
        }

        if (nFilterPart >= nFilterParts)
        {
            break;
        }

        int posSpecial = filterPart.Find('^', startCharacter);
        if (posSpecial >= 0 && posSpecial < startCharacter + keyLength)
        {
            startCharacter = posSpecial + 1;
        }
        else
        {
            // Try key
            DWORD dwTestKey = (filterPart.GetAt(startCharacter) << 24) | (filterPart.GetAt(startCharacter+1) << 16) | (filterPart.GetAt(startCharacter+2) << 8) | filterPart.GetAt(startCharacter+3);
        	
	        // Now we have a substring which we can check
	        if (filterMap[0].find(dwTestKey) == filterMap[0].end()) 
	        {
                dwKey = dwTestKey;
                dwKeyMap = 0;
                break;
	        }

	        // We already had a match - increment the start character
	        startCharacter++;
        }
	}

    // Try second list
    if (dwKey == 0)
    {
        dwKeyMap = 1;

	    startCharacter = 0;

        filterPart = filterParts[0];
        filterPart.MakeLower();
        filterPartLength = filterPart.GetLength();

        nFilterPart = 0;

        if (filterPartLength >= 7)
        {
            if (filterPart.Find(L"http://") == 0)
            {
                startCharacter = 7;
            }
            else if (filterPart.Find(L"https://") == 0)
            {
                startCharacter = 8;
            }
        }
        
        while (true)
        {
            // Part is too short as unique key? - try next filter part
            while (filterPartLength < startCharacter + keyLength)
            {
                nFilterPart++;
                if (nFilterPart >= nFilterParts)
                {
                    break;
                }
                
                filterPart = filterParts[nFilterPart];
                filterPart.MakeLower();
                filterPartLength = filterPart.GetLength();
                
                startCharacter = 0;
            }

            if (nFilterPart >= nFilterParts)
            {
                break;
            }

            int posSpecial = filterPart.Find('^', startCharacter);
            if (posSpecial >= 0 && posSpecial < startCharacter + keyLength)
            {
                startCharacter = posSpecial + 1;
            }
            else
            {
                // Try key
                DWORD dwTestKey = (filterPart.GetAt(startCharacter) << 24) | (filterPart.GetAt(startCharacter+1) << 16) | (filterPart.GetAt(startCharacter+2) << 8) | filterPart.GetAt(startCharacter+3);
            	
	            // Now we have a substring which we can check
	            if (filterMap[1].find(dwTestKey) == filterMap[1].end()) 
	            {
                    dwKey = dwTestKey;
                    break;
	            }

	            // We already had a match - increment the start character
	            startCharacter++;
            }
	    }
    }

    // Create the filter
    CFilter filter;
    filter.m_filterType = CFilter::filterTypeBlocking;
    filter.m_stringElements = filterParts;
    filter.m_isFromStart = bCheckFromStart;
    filter.m_isFromStartDomain = bCheckFromStartDomain;
    filter.m_isFromEnd = bCheckFromEnd;
    filter.m_filterText = raw;
    filter.m_filterFile = filterFile;

    // Set content type of filter and other settings
    if (!filterSettings.IsEmpty())
    {
        // Split filterSettings to parts
        pos = 0;
        bool hasContent = false;

        while ((pos = filterSettings.Find(',')) >= 0 || !filterSettings.IsEmpty())
	    {
            CString setting = pos >= 0 ? filterSettings.Left(pos) : filterSettings;
            filterSettings = pos >= 0 ? filterSettings.Right(filterSettings.GetLength() - pos - 1) : "";

            // Is content type negated
            bool bNegate = false;
            if (setting.GetAt(0) == '~')
            {
                bNegate = true;
                setting = setting.Right(setting.GetLength() - 1);
            }

            // Apply content type
            std::map<CString, int>::iterator it = m_contentMap.find(setting);
            if (it != m_contentMap.end())
            {
                if (!hasContent)
                {
                    if (bNegate)
                    {
                        filter.m_contentType = ~it->second;
                    }
                    else
                    {
                        filter.m_contentType = it->second;
                    }
                    hasContent = true;
                }
                else if (bNegate)
                {
                    filter.m_contentType &= ~it->second;
                }
                else
                {
                    filter.m_contentType |= it->second;
                }
            }
            else if (setting == "match-case")
            {
                filter.m_isMatchCase = true;
            }
            else if (setting == "third-party")
            {
                if (bNegate)
                {
                    filter.m_isFirstParty = true;
                }
                else
                {
                    filter.m_isThirdParty = true;
                }
            }
            else if (setting.Left(7) == "domain=")
            {
                int posDomain = 0;
                setting = setting.Right(setting.GetLength() - 7);
                
                while ((posDomain = setting.Find('|')) >= 0 || !setting.IsEmpty())
	            {
                    CString domain = posDomain >= 0 ? setting.Left(posDomain) : setting;
                    setting = posDomain >= 0 ? setting.Right(setting.GetLength() - posDomain - 1) : "";

                    if (domain.GetAt(0) == '~')
                    {
                        domain = domain.Right(domain.GetLength() - 1);

                        filter.m_domainsNot.insert(domain);
                    }
                    else
                    {
                        filter.m_domains.insert(domain);
                    }
                }
            }
            else
            {
                DEBUG_FILTER("Filter::Error parsing filter:" + filterFile + "/" + raw + " (unhandled tag: " + setting + ")")
                return;
            }
        }
    }

    // Add the filter
    if (dwKey != 0)
    {
	    filterMap[dwKeyMap][dwKey] = filter;
	}
    else
    {
        m_filterMapDefault[filterType].push_back(filter);
	}
}

#ifdef PRODUCT_SIMPLEADBLOCK

bool CPluginFilter::DownloadFilterFile(const CString& url, const CString& filename)
{
    CString tempFile = CPluginSettings::GetTempFile(TEMP_FILE_PREFIX);

    DEBUG_GENERAL("*** Downloading filter file:" + filename + " (to " + tempFile + ")");

    bool bResult = !tempFile.IsEmpty();
    if (bResult)
    {
	    // if new filter urls are found download them and update the persistent data
	    HRESULT hr = ::URLDownloadToFile(NULL, url, tempFile, 0, NULL);
        if (SUCCEEDED(hr))
        {
            CPluginFilterLock lock(filename);
            if (lock.IsLocked())
            {
                // Move the temporary file to the new text file.
                if (!::MoveFileEx(tempFile, CPluginSettings::GetDataPath(filename), MOVEFILE_REPLACE_EXISTING))
                {
                    DWORD dwError = ::GetLastError();

                    // Not same device? copy/delete instead
                    if (dwError == ERROR_NOT_SAME_DEVICE)
                    {
                        if (!::CopyFile(tempFile, CPluginSettings::GetDataPath(filename), FALSE))
                        {
                            DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_COPY_FILE, "Filter::Unable to copy file:" + filename)

                            bResult = false;
                        }

                        ::DeleteFile(tempFile);
                    }
                    else
                    {
                        DEBUG_ERROR_LOG(dwError, PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_MOVE_FILE, "Filter::Unable to replace file:" + filename)

                        bResult = false;
                    }
                }
            }
            else
            {
                bResult = false;
            }
        }
        else
        {
	        DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_DOWNLOAD_FILE, "Filter::Unable to download file: " + filename)

            bResult = false;
        }
    }

    return bResult;
}

#endif

bool CPluginFilter::ReadFilter(const CString& filename, const CString& downloadPath)
{
    bool isRead = false;

#ifdef PRODUCT_SIMPLEADBLOCK
    CPluginClient* client = CPluginClient::GetInstance();
#endif
    CString fileContent;

#ifdef PRODUCT_SIMPLEADBLOCK
    CPluginFilterLock lock(filename);
    if (lock.IsLocked())
    {
#endif
        DEBUG_GENERAL("*** Loading filter:" + m_dataPath + filename);

        // Read file
        HANDLE hFile = ::CreateFile(m_dataPath + filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);  
        if (hFile == INVALID_HANDLE_VALUE)
        {
            DWORD dwError = ::GetLastError();
#ifdef PRODUCT_SIMPLEADBLOCK
            // File not found - request another download!
            if (dwError == ERROR_FILE_NOT_FOUND)
            {
                if (!downloadPath.IsEmpty())
                {
                    client->RequestFilterDownload(filename, downloadPath);
                }
                else if (filename == PERSONAL_FILTER_FILE)
                {
                    // Open new file
                    HANDLE hPersonalFile = ::CreateFile(CPluginSettings::GetDataPath(PERSONAL_FILTER_FILE), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);  
                    if (hPersonalFile)
                    {
                        // Build filter string
                        CStringA line;
                        
	                    line += "[Simple Adblock - personal filters]\r\n";
	                    line += "!\r\n";
                        line += "! In this file you can enter your own filters.\r\n";
                        line += "! Any updates to this file\r\n";
                        line += "! will take effect the next time a new tab or window is opened in\r\n";
                        line += "! Internet Explorer.\r\n";
	                    line += "!\r\n";
                        line += "! To define filters you should use the adblockplus format, described on:\r\n";
                        line += "! http://adblockplus.org/en/filters\r\n";
	                    line += "!\r\n";
	                    line += "! File encoding: ANSI\r\n";
	                    line += "!\r\n";
	                    line += "!-------------------------Ad blocking rules--------------------------!\r\n";

                        // Write file
                        DWORD dwBytesWritten = 0;
                        ::WriteFile(hPersonalFile, line.GetBuffer(), line.GetLength(), &dwBytesWritten, NULL);

                        // Close file
                        ::CloseHandle(hPersonalFile);
                    }
                }
            }
            else
#endif
            {
	            DEBUG_ERROR_LOG(dwError, PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_READ_FILE, "Filter::ParseFilters - Open file:" + filename)
            }
        }
        else
        {
            // Read file
            char buffer[8193];
            LPVOID pBuffer = buffer;
            LPBYTE pByteBuffer = (LPBYTE)pBuffer;
            DWORD dwBytesRead = 0;
            BOOL bRead = TRUE;
            while ((bRead = ::ReadFile(hFile, pBuffer, 8192, &dwBytesRead, NULL)) == TRUE && dwBytesRead > 0)
            {
                pByteBuffer[dwBytesRead] = 0;

                fileContent += buffer;
            }

            // Read error        
            if (!bRead)
            {
		        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_READ_FILE, "Filter::ParseFilters - Read")
            }
            else
            {
                isRead = true;
            }

            // Close file
            ::CloseHandle(hFile);
        }

#ifdef PRODUCT_SIMPLEADBLOCK
    }
#endif
    if (isRead)
    {
        // Parse file string
        int pos = 0;
        CString filter = fileContent.Tokenize(L"\n\r", pos);
    
        s_criticalSectionFilterMap.Lock();
	    {
            while (pos >= 0)
            {
			    // If the line is not commented out
			    if (!filter.Trim().IsEmpty() && filter.GetAt(0) != '!' && filter.GetAt(0) != '[')
			    {
				    int filterType = 0;

				    // We need to categorize the filters
				    // We have three options, whitelist, block or element hiding
				    // See http://adblockplus.org/en/filters for further documentation
					
				    // @@ indicates white listing rule
				    if (filter.Find(L"@@") == 0)
				    {
					    filterType = CFilter::filterTypeWhiteList;

					    filter.Delete(0, 2);
				    }
				    // If a filter contains ## then it is a element hiding rule
				    else if (filter.Find(L"#") >= 0)
				    {
					    filterType = CFilter::filterTypeElementHide;
				    }
				    // Else, it is a general rule
				    else
				    {
					    filterType = CFilter::filterTypeBlocking;
				    }

				    // Element hiding not supported yet
				    if (filterType == CFilter::filterTypeElementHide)
				    {
					    AddFilterElementHide(filter, filename);
				    }
				    else
				    {
					    AddFilter(filter, filename, filterType);
				    }
			    }

                filter = fileContent.Tokenize(L"\n\r", pos);
		    }
	    }
	    s_criticalSectionFilterMap.Unlock();
    } 

    return isRead;
}

void CPluginFilter::ParseFilters(const TFilterFileList& list)
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

    // Load the files
#ifdef PRODUCT_SIMPLEADBLOCK
    CPluginClient* client = CPluginClient::GetInstance();
#endif
	for (TFilterFileList::const_iterator it = list.begin(); it != list.end(); ++it) 
	{
	    ReadFilter(it->first, it->second);
    }

#ifdef PERSONAL_FILTER_FILE
    ReadFilter(PERSONAL_FILTER_FILE);
#endif

#ifdef ENABLE_DEBUG_SELFTEST
    CStringA sCount;
    s_criticalSectionFilterMap.Lock();
    {
        sCount.Format("Block:%d/%d - BlockDef:%d - White:%d - WhiteDef:%d - Hide:%d/%d", m_filterMap[0][0].size(), m_filterMap[0][1].size(), m_filterMapDefault[0].size(), m_filterMap[1][0].size(), m_filterMapDefault[1].size(), m_elementHideTags.size() + m_elementHideTagsClass.size() + m_elementHideTagsId.size(), m_elementHideDomains.size());
    }
    s_criticalSectionFilterMap.Unlock();
    DEBUG_GENERAL("*** Filter count:" + sCount);
#endif
}


bool CPluginFilter::IsMatchFilter(const CFilter& filter, CString src, const CString& srcDomain, const CString& domain) const
{
    // Initial checks

    // $match_case
    if (!filter.m_isMatchCase)
    {
        src.MakeLower();
    }

    // $domain
    if (!filter.m_domains.empty())
    {
        bool bFound = false;

        for (std::set<CString>::const_iterator it = filter.m_domains.begin(); !bFound && it != filter.m_domains.end(); ++it)
        {
            bFound = domain == *(it) || IsSubdomain(domain, *it);
        }
        
        if (!bFound)
        {
            return false;
        }
    }

    // $domain ~ 
    if (!filter.m_domainsNot.empty())
    {
        for (std::set<CString>::const_iterator it = filter.m_domainsNot.begin(); it != filter.m_domainsNot.end(); ++it)
        {
            if (domain == *(it) || IsSubdomain(domain, *it))
            {
                return false;
            }
        }
    }

    // $third_party
    if (filter.m_isThirdParty)
    {
        if (srcDomain == domain || IsSubdomain(srcDomain, domain))
        {
            return false;
        }
    }

    // $third_party ~ 
    if (filter.m_isFirstParty)
    {
        if (srcDomain != domain && !IsSubdomain(srcDomain, domain))
        {
            return false;
        }
    }

    // "regex" checks

    int startPos = 0;
    int srcLength = src.GetLength();
    UINT indexEnd = filter.m_stringElements.size() - 1;

    for (UINT index = 0; index <= indexEnd; index++)
    {
        if (index == 0 && filter.m_isFromStartDomain)
        {
            int domainPos = src.Find(srcDomain);
            int lastPos = src.Find('/', domainPos);
            
            bool bFoundDomain = false;
            bool bContinueDomainSearch = true;

            while (bContinueDomainSearch)
            {
                if (domainPos == FindMatch(src, filter.m_stringElements[index]))
                {
                    bContinueDomainSearch = false;
                    bFoundDomain = true;
                }
                else
                {
                    domainPos = src.Find('.', domainPos + 1) + 1;
                    if (domainPos == 0 || (domainPos >= lastPos && lastPos >= 0))
                    {
                        bContinueDomainSearch = false;
                    }
                }
            }

            if (!bFoundDomain)
            {
                return false;
            }
        }
        
        startPos = FindMatch(src, filter.m_stringElements[index], startPos);
        if (startPos < 0)
        {
            return false;
        }

        int length = filter.m_stringElements[index].GetLength();

        // Check from start
        if (index == 0 && filter.m_isFromStart && startPos > 0)
        {
            return false;
        }

        // Check from end
        if (index == indexEnd && filter.m_isFromEnd && startPos + length != srcLength)
        {
            return false;
        }

        startPos += length;
    }

//    filter.m_hitCount++;

    return true;
}


const CFilter* CPluginFilter::MatchFilter(int filterType, const CString& src, int contentType, const CString& domain) const
{
	const CFilter* filter = NULL;

	int startCharacter = 0;
	int keyLength = 4;

    CString srcLower = src;
    srcLower.MakeLower();
    int srcLowerLength = srcLower.GetLength();

    // Extract src domain
    DWORD length = 2048;
    CString srcDomain;
    
    if (SUCCEEDED(::UrlGetPart(src, srcDomain.GetBufferSetLength(2048), &length, URL_PART_HOSTNAME, 0)))
    {
        srcDomain.ReleaseBuffer();

        if (srcDomain.Left(4) == L"www.")
        {
            srcDomain = srcDomain.Right(srcDomain.GetLength() - 4);
        }
        else if (srcDomain.Left(5) == L"www2." || srcDomain.Left(5) == L"www3.")
        {
            srcDomain = srcDomain.Right(srcDomain.GetLength() - 5);
        }
    }
    else
    {
        srcDomain.ReleaseBuffer();
        srcDomain.Empty();
    }
    
    // Search in filter map
    s_criticalSectionFilterMap.Lock();
	{
		const TFilterMap* filterMap = m_filterMap[filterType];

        if (srcLowerLength >= 7)
        {
            if (srcLower.Find(L"http://") == 0)
            {
                startCharacter = 7;
            }
            else if (srcLower.Find(L"https://") == 0)
            {
                startCharacter = 8;
            }
        }
        
        DWORD dwKey = 0;

		while (filter == NULL && srcLowerLength >= startCharacter + keyLength)
		{
		    if (dwKey == 0)
		    {
                dwKey = (srcLower.GetAt(startCharacter) << 24) | (srcLower.GetAt(startCharacter+1) << 16) | (srcLower.GetAt(startCharacter+2) << 8) | srcLower.GetAt(startCharacter+3);
            }
            else
            {
                dwKey <<= 8;
                dwKey |= srcLower.GetAt(startCharacter+3);
            }
            
			TFilterMap::const_iterator foundEntry = filterMap[0].find(dwKey);
			if (foundEntry != filterMap[0].end()) 
			{
				if (((foundEntry->second.m_contentType & contentType) || foundEntry->second.m_contentType == CFilter::contentTypeAny) && IsMatchFilter(foundEntry->second, src, srcDomain, domain))
				{
					filter = &(foundEntry->second);
					break;
				}
			}

			// No match - increment the start character
			startCharacter++;
		}

        // Second list
        if (filter == NULL)
        {
            dwKey = 0;
            startCharacter = 0;

            if (srcLowerLength >= 7)
            {
                if (srcLower.Find(L"http://") == 0)
                {
                    startCharacter = 7;
                }
                else if (srcLower.Find(L"https://") == 0)
                {
                    startCharacter = 8;
                }
            }

		    while (filter == NULL && srcLowerLength >= startCharacter + keyLength)
		    {
		        if (dwKey == 0)
		        {
                    dwKey = (srcLower.GetAt(startCharacter) << 24) | (srcLower.GetAt(startCharacter+1) << 16) | (srcLower.GetAt(startCharacter+2) << 8) | srcLower.GetAt(startCharacter+3);
                }
                else
                {
                    dwKey <<= 8;
                    dwKey |= srcLower.GetAt(startCharacter+3);
                }
                
			    TFilterMap::const_iterator foundEntry = filterMap[1].find(dwKey);
			    if (foundEntry != filterMap[1].end()) 
			    {
				    if (((foundEntry->second.m_contentType & contentType) || foundEntry->second.m_contentType == CFilter::contentTypeAny) && IsMatchFilter(foundEntry->second, src, srcDomain, domain))
				    {
					    filter = &(foundEntry->second);
					    break;
				    }
			    }

			    // No match - increment the start character
			    startCharacter++;
		    }
		}

		// Search in default filter map (try all filters)
		if (filter == NULL)
		{
			for (TFilterMapDefault::const_iterator it = m_filterMapDefault[filterType].begin(); it != m_filterMapDefault[filterType].end(); ++it)
			{
				if (((it->m_contentType & contentType) || it->m_contentType == CFilter::contentTypeAny) && IsMatchFilter(*it, src, srcDomain, domain))
				{
					filter = &(*it);
					break;
				}
			}
		}
	}
    s_criticalSectionFilterMap.Unlock();

    return filter;
}


bool CPluginFilter::ShouldWhiteList(CString src) const
{
	// We should not block the empty string, so all filtering does not make sense
	// Therefore we just return
	if (src.Trim().IsEmpty())
	{
		return false;
	}

    const CFilter* filter = MatchFilter(CFilter::filterTypeWhiteList, src, CFilter::contentTypeDocument, "");

	return filter ? true : false;
}


bool CPluginFilter::ShouldBlock(CString src, int contentType, const CString& domain, bool addDebug) const
{
    // We should not block the empty string, so all filtering does not make sense
	// Therefore we just return
	if (src.Trim().IsEmpty())
	{
		return false;
	}

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

	const CFilter* blockFilter = MatchFilter(CFilter::filterTypeBlocking, src, contentType, domain);
	if (blockFilter)
	{
		const CFilter* whiteFilter = MatchFilter(CFilter::filterTypeWhiteList, src, contentType, domain);
		if (whiteFilter)
		{
			if (addDebug)
			{
				DEBUG_FILTER("Filter::ShouldBlock " + type + " NO  src:" + src + " - whitelist:\"" + whiteFilter->m_filterText + "\"");
			}
			blockFilter = NULL;
		}
		else if (addDebug)
		{
			DEBUG_FILTER("Filter::ShouldBlock " + type + " YES src:" + src + " - \"" + blockFilter->m_filterText + "\"")

#ifdef ENABLE_DEBUG_RESULT
            CPluginDebug::DebugResultBlocking(type, src, blockFilter->m_filterText, blockFilter->m_filterFile);
#endif
		}
	}
	else if (addDebug)
	{
		DEBUG_FILTER("Filter::ShouldBlock " + type + " NO  src:" + src)
	}

	return blockFilter ? true : false;
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

#ifdef PRODUCT_SIMPLEADBLOCK

void CPluginFilter::CreateFilters()
{
    CPluginFilterLock lock("easylist.txt");
    if (lock.IsLocked())
    {
        // Check file existence
        std::ifstream is;
	    is.open(CPluginSettings::GetDataPath("easylist.txt"), std::ios_base::in);
	    if (is.is_open())
	    {
            is.close();
            return;
        }

        // Open file
        HANDLE hFile = ::CreateFile(CPluginSettings::GetDataPath("easylist.txt"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);  
        if (hFile == INVALID_HANDLE_VALUE)
        {
		    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_CREATE_FILE_OPEN, "Filter::Create - CreateFile");
        }
        else
        {
            // Build filter string
            CStringA line;
	line += "[Adblock Plus 1.1]";
	line += "! Checksum: 0xN4e5SegJGeZuYZQGhShQ";
	line += "! EasyList - https://easylist.adblockplus.org/";
	line += "! Last modified:  4 Nov 2010 16:30 UTC";
	line += "! Expires: 5 days (update frequency)";
	line += "! Licence: https://easylist-downloads.adblockplus.org/COPYING";
	line += "!";
	line += "! Please report any unblocked adverts or problems";
	line += "! in the forums (http://forums.lanik.us/)";
	line += "! or via e-mail (easylist.subscription@gmail.com).";
	line += "!";
	line += "!-----------------General advert blocking filters-----------------!";
	line += "! *** easylist_general_block.txt ***";
	line += "&ad_keyword=";
	line += "&ad_type_";
	line += "&adname=";
	line += "&adspace=";
	line += "&adtype=";
	line += "&advertiserid=";
	line += "&clientype=*&adid=";
	line += "&googleadword=";
	line += "&program=revshare&";
	line += "+adverts/";
	line += "-ad-large.";
	line += "-ad-loading.";
	line += "-ad-manager/";
	line += "-ad-util-";
	line += "-ad1.jpg";
	line += "-adhelper.";
	line += "-banner-ads/";
	line += "-bin/ad_";
	line += "-content/adsys/";
	line += "-contest-ad.";
	line += "-leaderboard-ad-";
	line += "-rebels-ads/";
	line += "-text-ads.";
	line += "-third-ad.";
	line += ".ad.footer.";
	line += ".ad.page.";
	line += ".ad.premiere.";
	line += ".adplacement=";
	line += ".adriver.";
	line += ".ads.darla.";
	line += ".adserv/*";
	line += ".adserver.";
	line += ".aspx?ad=";
	line += ".aspx?zoneid=*&task=";
	line += ".au/_ads/";
	line += ".au/ads/";
	line += ".ca/ads/";
	line += ".cc/ads/";
	line += ".com/ad-";
	line += ".com/ad.";
	line += ".com/ad/";
	line += ".com/ad1.";
	line += ".com/ad2.";
	line += ".com/ad2/";
	line += ".com/ad?";
	line += ".com/ad_";
	line += ".com/adlib/";
	line += ".com/adops/";
	line += ".com/ads-";
	line += ".com/ads.";
	line += ".com/ads/$image,object,subdocument";
	line += ".com/ads?";
	line += ".com/ads_";
	line += ".com/advt/";
	line += ".com/adx/";
	line += ".com/gad/";
	line += ".com/gads/";
	line += ".com/openx/";
	line += ".com/ss/ad/";
	line += ".html?ad=";
	line += ".in/ads/";
	line += ".info/ads/";
	line += ".jp/ads/";
	line += ".net/_ads/";
	line += ".net/ad/$~object_subrequest";
	line += ".net/ads/";
	line += ".net/ads2/";
	line += ".net/ads3/";
	line += ".net/ads_";
	line += ".nz/ads/";
	line += ".org/ad/";
	line += ".org/ad2_";
	line += ".org/ad_";
	line += ".org/ads/";
	line += ".org/gads/";
	line += ".php?bannerid=";
	line += ".php?zoneid=*&loc=";
	line += ".swf?clicktag=";
	line += ".to/ad.php|";
	line += ".to/ads/";
	line += ".tv/ads/";
	line += ".uk/ads/";
	line += ".us/ads/";
	line += ".za/ads/";
	line += "/*;cue=pre;$object_subrequest";
	line += "/120ad.gif";
	line += "/468ad.gif";
	line += "/468xads.";
	line += "/?addyn|*";
	line += "/?advideo/*";
	line += "/a2/ads/*";
	line += "/aamsz=*/acc_random=";
	line += "/aamsz=*/pageid=";
	line += "/aamsz=*/position=";
	line += "/abm.asp?z=";
	line += "/abmw.asp?z=";
	line += "/abmw.aspx";
	line += "/acc_random=*/aamsz=";
	line += "/ad-468-";
	line += "/ad-amz.";
	line += "/ad-banner-";
	line += "/ad-box-";
	line += "/ad-cdn.";
	line += "/ad-frame.";
	line += "/ad-header.";
	line += "/ad-hug.";
	line += "/ad-iframe-";
	line += "/ad-inject/*";
	line += "/ad-leaderboard.";
	line += "/ad-letter.";
	line += "/ad-loader-";
	line += "/ad-local.";
	line += "/ad-managment/*";
	line += "/ad-server/*";
	line += "/ad-template/*";
	line += "/ad-top-";
	line += "/ad-topbanner-";
	line += "/ad-vert.";
	line += "/ad-vertical-";
	line += "/ad.asp?";
	line += "/ad.cgi?";
	line += "/ad.css?";
	line += "/ad.epl?";
	line += "/ad.jsp?";
	line += "/ad.mason?";
	line += "/ad.php?";
	line += "/ad/files/*";
	line += "/ad/iframe/*";
	line += "/ad/img/*";
	line += "/ad/script/*";
	line += "/ad/side_";
	line += "/ad/takeover/*";
	line += "/ad1.html";
	line += "/ad160.php";
	line += "/ad160x600.";
	line += "/ad1place.";
	line += "/ad1x1home.";
	line += "/ad2.aspx";
	line += "/ad2.html";
	line += "/ad2border.";
	line += "/ad300.htm";
	line += "/ad300x145.";
	line += "/ad350.html";
	line += "/ad728.php";
	line += "/ad728x15.";
	line += "/ad?count=";
	line += "/ad_agency/*";
	line += "/ad_area.";
	line += "/ad_banner.";
	line += "/ad_banner/*";
	line += "/ad_banner_";
	line += "/ad_bottom.";
	line += "/ad_bsb.";
	line += "/ad_campaigns/*";
	line += "/ad_code.";
	line += "/ad_configuration.";
	line += "/ad_content.";
	line += "/ad_count.";
	line += "/ad_creatives.";
	line += "/ad_editorials_";
	line += "/ad_engine?";
	line += "/ad_feed.js?";
	line += "/ad_footer.";
	line += "/ad_forum_";
	line += "/ad_frame.";
	line += "/ad_function.";
	line += "/ad_gif/*";
	line += "/ad_google.";
	line += "/ad_header_";
	line += "/ad_holder/*";
	line += "/ad_horizontal.";
	line += "/ad_html/*";
	line += "/ad_iframe.";
	line += "/ad_iframe_";
	line += "/ad_insert.";
	line += "/ad_jnaught/*";
	line += "/ad_label_";
	line += "/ad_leader.";
	line += "/ad_left.";
	line += "/ad_legend_";
	line += "/ad_link.";
	line += "/ad_load.";
	line += "/ad_manager.";
	line += "/ad_mpu.";
	line += "/ad_notice.";
	line += "/ad_os.php?";
	line += "/ad_page_";
	line += "/ad_print.";
	line += "/ad_rectangle_";
	line += "/ad_refresher.";
	line += "/ad_reloader_";
	line += "/ad_right.";
	line += "/ad_rotation.";
	line += "/ad_rotator.";
	line += "/ad_rotator_";
	line += "/ad_script.";
	line += "/ad_serv.";
	line += "/ad_serve.";
	line += "/ad_server.";
	line += "/ad_server/*";
	line += "/ad_sizes=";
	line += "/ad_skin_";
	line += "/ad_sky.";
	line += "/ad_skyscraper.";
	line += "/ad_slideout.";
	line += "/ad_space.";
	line += "/ad_square.";
	line += "/ad_supertile/*";
	line += "/ad_tag.";
	line += "/ad_tag_";
	line += "/ad_tags_";
	line += "/ad_tile/*";
	line += "/ad_title_";
	line += "/ad_top.";
	line += "/ad_topgray2.";
	line += "/ad_tpl.";
	line += "/ad_upload/*";
	line += "/ad_vert.";
	line += "/ad_vertical.";
	line += "/ad_view_";
	line += "/adaffiliate_";
	line += "/adanim/*";
	line += "/adaptvadplayer.";
	line += "/adbanner.";
	line += "/adbanner/*";
	line += "/adbanner_";
	line += "/adbanners/*";
	line += "/adbar.aspx";
	line += "/adbg.jpg";
	line += "/adbot_promos/*";
	line += "/adbottom.";
	line += "/adbox.gif";
	line += "/adbox.js";
	line += "/adbox.php";
	line += "/adbrite.";
	line += "/adbureau.";
	line += "/adcde.js";
	line += "/adcell/*";
	line += "/adcentral.";
	line += "/adchannel_";
	line += "/adclick.";
	line += "/adclient.";
	line += "/adclient/*";
	line += "/adclutter.";
	line += "/adcode.";
	line += "/adcode/*";
	line += "/adcodes/*";
	line += "/adcollector.";
	line += "/adcomponent/*";
	line += "/adconfig.js";
	line += "/adconfig.xml?";
	line += "/adconfig/*";
	line += "/adcontent.$~object_subrequest";
	line += "/adcontroller.";
	line += "/adcreative.";
	line += "/adcycle.";
	line += "/adcycle/*";
	line += "/addeals/*";
	line += "/addelivery/*";
	line += "/addyn/3.0/*";
	line += "/addyn|*|adtech;";
	line += "/adengage1.";
	line += "/adengage_";
	line += "/adengine/*";
	line += "/adexclude/*";
	line += "/adf.cgi?";
	line += "/adfactory.";
	line += "/adfarm.";
	line += "/adfetch?";
	line += "/adfetcher?";
	line += "/adfever_";
	line += "/adfile/*";
	line += "/adfooter.";
	line += "/adframe.";
	line += "/adframe/*";
	line += "/adframe?";
	line += "/adframe_";
	line += "/adframebottom.";
	line += "/adframemiddle.";
	line += "/adframetop.";
	line += "/adfshow?";
	line += "/adfunction.";
	line += "/adfunctions.";
	line += "/adgitize-";
	line += "/adgraphics/*";
	line += "/adguru.";
	line += "/adhalfbanner.";
	line += "/adhandler.";
	line += "/adhandler/*";
	line += "/adheader.";
	line += "/adheadertxt.";
	line += "/adhomepage.";
	line += "/adhtml/*";
	line += "/adhug_jc.";
	line += "/adiframe.";
	line += "/adiframe/*";
	line += "/adify_box.";
	line += "/adify_leader.";
	line += "/adify_sky.";
	line += "/adimage.";
	line += "/adimages.";
	line += "/adimages/*$~subdocument";
	line += "/adindex/*";
	line += "/adinjector.";
	line += "/adinsert.";
	line += "/adinterax.";
	line += "/adjs.php?";
	line += "/adjsmp.";
	line += "/adlabel.";
	line += "/adlabel_";
	line += "/adlayer.";
	line += "/adlayer/*";
	line += "/adleader.";
	line += "/adlink-";
	line += "/adlink.";
	line += "/adlink_";
	line += "/adlinks.";
	line += "/adlist_";
	line += "/adloader.";
	line += "/adm/ad/*";
	line += "/adman.js";
	line += "/adman/image/*";
	line += "/adman/www/*";
	line += "/admanagement/*";
	line += "/admanagementadvanced.";
	line += "/admanager.$~object_subrequest";
	line += "/admanager/*$~object_subrequest";
	line += "/admanager3.";
	line += "/admanagers/*";
	line += "/admanagerstatus/*";
	line += "/admarket/*";
	line += "/admaster.";
	line += "/admaster?";
	line += "/admedia.";
	line += "/admedia/*";
	line += "/admega.";
	line += "/admentor/*";
	line += "/admicro2.";
	line += "/admicro_";
	line += "/admin/ad_";
	line += "/adnetmedia.";
	line += "/adnetwork.";
	line += "/adnews.";
	line += "/adnext.";
	line += "/adng.html";
	line += "/adnotice.";
	line += "/adonline.";
	line += "/adotubeplugin.";
	line += "/adp.htm";
	line += "/adpage.";
	line += "/adpage/*";
	line += "/adpartner.";
	line += "/adpeeps.";
	line += "/adpeeps/*";
	line += "/adplayer.";
	line += "/adplayer/*";
	line += "/adplugin.";
	line += "/adpoint.";
	line += "/adpool/*";
	line += "/adpopup.";
	line += "/adproducts/*";
	line += "/adproxy.";
	line += "/adproxy/*";
	line += "/adrelated.";
	line += "/adreload?";
	line += "/adremote.";
	line += "/adrevenue/*";
	line += "/adrevolver/*";
	line += "/adriver.";
	line += "/adriver_";
	line += "/adrolays.";
	line += "/adroller.";
	line += "/adroot/*";
	line += "/adrot.js";
	line += "/adrotator/*";
	line += "/adrotv2.";
	line += "/adruptive.";
	line += "/ads-banner.";
	line += "/ads-common.";
	line += "/ads-footer.";
	line += "/ads-leader|";
	line += "/ads-rectangle.";
	line += "/ads-rec|";
	line += "/ads-right.";
	line += "/ads-service.";
	line += "/ads-skyscraper.";
	line += "/ads-sky|";
	line += "/ads.asp?";
	line += "/ads.dll/*";
	line += "/ads.htm";
	line += "/ads.jsp";
	line += "/ads.php?";
	line += "/ads.pl?";
	line += "/ads/2010/*";
	line += "/ads/cnvideo/*";
	line += "/ads/common/*";
	line += "/ads/footer_";
	line += "/ads/freewheel/*";
	line += "/ads/home/*";
	line += "/ads/house/*";
	line += "/ads/images/*";
	line += "/ads/interstitial/*";
	line += "/ads/labels/*";
	line += "/ads/layer.";
	line += "/ads/leaderboard_";
	line += "/ads/preloader/*";
	line += "/ads/preroll_";
	line += "/ads/promo_";
	line += "/ads/rectangle_";
	line += "/ads/sponsor_";
	line += "/ads/square-";
	line += "/ads/third-party/*";
	line += "/ads09a/*";
	line += "/ads2.html";
	line += "/ads2.php";
	line += "/ads2_2.";
	line += "/ads2_header.";
	line += "/ads9.dll";
	line += "/ads?id=";
	line += "/ads_code.";
	line += "/ads_global.";
	line += "/ads_google.";
	line += "/ads_iframe.";
	line += "/ads_openx/*";
	line += "/ads_php/*";
	line += "/ads_reporting/*";
	line += "/ads_yahoo.";
	line += "/adsa468.";
	line += "/adsa728.";
	line += "/adsadview.";
	line += "/adsales/*";
	line += "/adsatt.";
	line += "/adsbanner.";
	line += "/adsbannerjs.";
	line += "/adscale_";
	line += "/adscluster.";
	line += "/adscontent.";
	line += "/adscontent2.";
	line += "/adscript.";
	line += "/adscript_";
	line += "/adscripts/*";
	line += "/adscroll.";
	line += "/adsdaqbanner_";
	line += "/adsdaqbox_";
	line += "/adsdaqsky_";
	line += "/adsearch.";
	line += "/adsense-";
	line += "/adsense.";
	line += "/adsense/*";
	line += "/adsense23.";
	line += "/adsense24.";
	line += "/adsense?";
	line += "/adsense_";
	line += "/adsensegb.";
	line += "/adsensegoogle.";
	line += "/adsensets.";
	line += "/adserv.";
	line += "/adserv2.";
	line += "/adserve.";
	line += "/adserve/*";
	line += "/adserver.";
	line += "/adserver/*";
	line += "/adserver2.";
	line += "/adserver2/*";
	line += "/adserver?";
	line += "/adserver_";
	line += "/adserversolutions/*";
	line += "/adservice|";
	line += "/adserving.";
	line += "/adsfac.";
	line += "/adsfetch.";
	line += "/adsfile.";
	line += "/adsfolder/*";
	line += "/adsframe.";
	line += "/adshandler.";
	line += "/adsheader.";
	line += "/adshow.";
	line += "/adshow?";
	line += "/adshow_";
	line += "/adsiframe/*";
	line += "/adsign.";
	line += "/adsimage/*";
	line += "/adsinclude.";
	line += "/adsinsert.";
	line += "/adsky.php";
	line += "/adskyright.";
	line += "/adskyscraper.";
	line += "/adslots.";
	line += "/adslug-";
	line += "/adslug_";
	line += "/adsmanagement/*";
	line += "/adsmanager/*";
	line += "/adsmedia_";
	line += "/adsnew.";
	line += "/adsonar.";
	line += "/adsopenx/*";
	line += "/adspace.";
	line += "/adspace/*";
	line += "/adspaces.";
	line += "/adsponsor.";
	line += "/adspro/*";
	line += "/adsquare.";
	line += "/adsremote.";
	line += "/adsreporting/*";
	line += "/adsrich.";
	line += "/adsright.";
	line += "/adsrule.";
	line += "/adsserv.";
	line += "/adssrv.";
	line += "/adstemplate/*";
	line += "/adstorage.";
	line += "/adstracking.";
	line += "/adstream.";
	line += "/adstream_";
	line += "/adstub.";
	line += "/adstubs/*";
	line += "/adswap.";
	line += "/adswap/*";
	line += "/adswide.";
	line += "/adswidejs.";
	line += "/adswrapper.";
	line += "/adsx728.";
	line += "/adsx_728.";
	line += "/adsync/*";
	line += "/adsyndication.";
	line += "/adsys/ads.";
	line += "/adsystem/*";
	line += "/adtag/type/*";
	line += "/adtago.";
	line += "/adtags.";
	line += "/adtags/*";
	line += "/adtagtc.";
	line += "/adtagtranslator.";
	line += "/adtech.";
	line += "/adtech/*";
	line += "/adtech;";
	line += "/adtech_";
	line += "/adtext.";
	line += "/adtext4.";
	line += "/adtext_";
	line += "/adtitle.";
	line += "/adtology.";
	line += "/adtonomy.";
	line += "/adtop.do";
	line += "/adtop.js";
	line += "/adtrack/*";
	line += "/adtraff.";
	line += "/adtvideo.";
	line += "/adtype.";
	line += "/adunit.";
	line += "/adunits/*";
	line += "/adv/ads/*";
	line += "/adverserve.";
	line += "/advert-$~stylesheet";
	line += "/advert.$domain=~kp.ru";
	line += "/advert/*";
	line += "/advert?";
	line += "/advert_";
	line += "/advertise-";
	line += "/advertise.";
	line += "/advertise/*";
	line += "/advertisehere.";
	line += "/advertisement-";
	line += "/advertisement.";
	line += "/advertisement/*";
	line += "/advertisement2.";
	line += "/advertisement_";
	line += "/advertisementheader.";
	line += "/advertisementrotation.";
	line += "/advertisements.";
	line += "/advertisements/*";
	line += "/advertisementview/*";
	line += "/advertiser/*";
	line += "/advertisers/*";
	line += "/advertising.";
	line += "/advertising/*$~object,~object_subrequest";
	line += "/advertising2.";
	line += "/advertising_";
	line += "/advertisingcontent/*";
	line += "/advertisingmanual.";
	line += "/advertisingmodule.";
	line += "/advertisment.";
	line += "/advertize_";
	line += "/advertmedia/*";
	line += "/advertorials/*";
	line += "/advertphp/*";
	line += "/advertpro/*";
	line += "/adverts.";
	line += "/adverts/*";
	line += "/adverts_";
	line += "/adview.";
	line += "/adview?";
	line += "/adviewer.";
	line += "/advision.";
	line += "/advolatility.";
	line += "/advpartnerinit.";
	line += "/adwords/*";
	line += "/adworks.";
	line += "/adworks/*";
	line += "/adwrapper/*";
	line += "/adwrapperiframe.";
	line += "/adxx.php?";
	line += "/adzone.";
	line += "/adzones/*";
	line += "/aff_frame.";
	line += "/affad?q=";
	line += "/affads/*";
	line += "/affclick/*";
	line += "/affilatebanner.";
	line += "/affiliate/banners/*";
	line += "/affiliate/script.php?";
	line += "/affiliate_banners/*";
	line += "/affiliate_resources/*";
	line += "/affiliatebanner/*";
	line += "/affiliatebanners/*";
	line += "/affiliateimages/*";
	line += "/affiliatelinks.";
	line += "/affiliates.*.aspx?";
	line += "/affiliates/banner";
	line += "/affiliatewiz/*";
	line += "/affiliation/*";
	line += "/affiliationcash.";
	line += "/affilinet/ads/*";
	line += "/afimages.";
	line += "/afr.php?";
	line += "/ajax/ads/*";
	line += "/ajrotator/*";
	line += "/ajs.php?";
	line += "/annonser/*";
	line += "/api/ads/*";
	line += "/app/ads.js";
	line += "/article_ad.";
	line += "/as3adplayer.";
	line += "/aseadnshow.";
	line += "/aspbanner_inc.asp?";
	line += "/assets/ads/*";
	line += "/audioads/*";
	line += "/auditudeadunit.";
	line += "/austria_ad.";
	line += "/auto_ad_";
	line += "/back-ad.";
	line += "/ban_m.php?";
	line += "/banimpress.";
	line += "/banman.asp?";
	line += "/banman/*";
	line += "/banner-ad-";
	line += "/banner-ad/*";
	line += "/banner/ad_";
	line += "/banner/affiliate/*";
	line += "/banner_468.";
	line += "/banner_ad.";
	line += "/banner_ad_";
	line += "/banner_ads.";
	line += "/banner_ads_";
	line += "/banner_adv/*";
	line += "/banner_advert/*";
	line += "/banner_control.php?";
	line += "/banner_db.php?";
	line += "/banner_file.php?";
	line += "/banner_js.*?";
	line += "/banner_management/*";
	line += "/banner_skyscraper.";
	line += "/bannerad.";
	line += "/bannerad_";
	line += "/bannerads-";
	line += "/bannerads/*";
	line += "/banneradviva.";
	line += "/bannercode.php";
	line += "/bannerconduit.swf?";
	line += "/bannerexchange/*";
	line += "/bannerfarm/*";
	line += "/bannerframe.*?";
	line += "/bannerframeopenads.";
	line += "/bannerframeopenads_";
	line += "/bannermanager/*";
	line += "/bannermedia/*";
	line += "/bannerrotation.";
	line += "/bannerrotation/*";
	line += "/banners.*&iframe=";
	line += "/banners/affiliate/*";
	line += "/banners/promo/*";
	line += "/banners_rotation.";
	line += "/bannerscript/*";
	line += "/bannerserver/*";
	line += "/bannersyndication.";
	line += "/bannerview.*?";
	line += "/bannery/*?banner=";
	line += "/bar-ad.";
	line += "/baselinead.";
	line += "/basic/ad/*";
	line += "/behaviorads/*";
	line += "/beta-ad.";
	line += "/bg/ads/*";
	line += "/bg_ads_";
	line += "/bi_affiliate.js";
	line += "/bigad.p";
	line += "/bigboxad.";
	line += "/bkgrndads/*";
	line += "/blogad_";
	line += "/blogads.";
	line += "/blogads/*";
	line += "/blogads3/*";
	line += "/bmndoubleclickad.";
	line += "/bnrsrv.*?";
	line += "/bodyads/*";
	line += "/boomad.";
	line += "/bottom_ad.";
	line += "/bottomad.";
	line += "/bottomad/*";
	line += "/btn_ad_";
	line += "/bucketads.";
	line += "/butler.php?type=";
	line += "/buttonads.";
	line += "/buyad.html";
	line += "/buyclicks/*";
	line += "/buysellads.";
	line += "/bw_adsys.";
	line += "/bytemark_ad.";
	line += "/campus/ads/*";
	line += "/cashad.";
	line += "/cashad2.";
	line += "/central/ads/*";
	line += "/cgi-bin/ads/*";
	line += "/channelblockads.";
	line += "/chinaadclient.";
	line += "/chitika-ad?";
	line += "/circads.";
	line += "/cms/js/ad_";
	line += "/cnnslads.";
	line += "/cnwk.1d/ads/*";
	line += "/coldseal_ad.";
	line += "/commercial_horizontal.";
	line += "/commercial_top.";
	line += "/commercials/*";
	line += "/common/ad/*";
	line += "/common/ads/*";
	line += "/companion_ads.";
	line += "/content/ad_";
	line += "/content_ad.";
	line += "/content_ad_";
	line += "/contentadxxl.";
	line += "/contentad|";
	line += "/contextad.";
	line += "/controller/ads/*";
	line += "/corner_ads/*";
	line += "/country_ad.";
	line += "/cpxads.";
	line += "/ctamlive160x160.";
	line += "/customad.";
	line += "/customadsense.";
	line += "/cvs/ads/*";
	line += "/cwggoogleadshow.";
	line += "/dart_ads/*";
	line += "/dartads.";
	line += "/dateads.";
	line += "/dclk_ads.";
	line += "/dclk_ads_";
	line += "/dcloadads/*";
	line += "/de/ads/*";
	line += "/defer_ads.";
	line += "/deferads.";
	line += "/deliver.nmi?";
	line += "/deliverad/*";
	line += "/deliverjs.nmi?";
	line += "/delivery/ag.php";
	line += "/delivery/al.php";
	line += "/delivery/apu.php";
	line += "/delivery/avw.php";
	line += "/descpopup.js";
	line += "/direct_ads.";
	line += "/directads.";
	line += "/display_ads.";
	line += "/displayad.";
	line += "/displayad?";
	line += "/displayads/*";
	line += "/dne_ad.";
	line += "/dnsads.html?";
	line += "/doors/ads/*";
	line += "/doubleclick.phpi?";
	line += "/doubleclick/iframe.";
	line += "/doubleclick_ads.";
	line += "/doubleclick_ads/*";
	line += "/doubleclickcontainer.";
	line += "/doubleclicktag.";
	line += "/download/ad.";
	line += "/download/ad/*";
	line += "/drawad.php?";
	line += "/drivingrevenue/*";
	line += "/dsg/bnn/*";
	line += "/dxd/ads/*";
	line += "/dyn_banner.";
	line += "/dyn_banners_";
	line += "/dynamic/ads/*";
	line += "/dynamicad?";
	line += "/dynamiccsad?";
	line += "/dynamicvideoad?";
	line += "/dynanews/ad-";
	line += "/dynbanner/flash/*";
	line += "/ebay_ads/*";
	line += "/emailads/*";
	line += "/emediatead.";
	line += "/ext_ads.";
	line += "/external/ad.";
	line += "/external/ads/*";
	line += "/external_ads.";
	line += "/eyewondermanagement.";
	line += "/eyewondermanagement28.";
	line += "/fastclick160.";
	line += "/fastclick728.";
	line += "/fatads.";
	line += "/featuredadshome.";
	line += "/file/ad.";
	line += "/files/ad/*";
	line += "/files/ads/*";
	line += "/fimserve.";
	line += "/flash/ad_";
	line += "/flash/ads/*";
	line += "/flashad.";
	line += "/flashads.";
	line += "/flashads/*";
	line += "/footad-";
	line += "/footer-ad-";
	line += "/footer_ad_";
	line += "/framead-";
	line += "/framead.";
	line += "/framead/*";
	line += "/framead_";
	line += "/frequencyads.";
	line += "/frnads.";
	line += "/fuseads/*";
	line += "/gads.html";
	line += "/gads.js";
	line += "/gafsads?";
	line += "/galleryad.";
	line += "/gamead/*";
	line += "/gamersad.";
	line += "/genericrichmediabannerad/*";
	line += "/geo-ads_";
	line += "/geo/ads.";
	line += "/get-ad.";
	line += "/getad.aspx";
	line += "/getad.js";
	line += "/getad.php?";
	line += "/getad.php|";
	line += "/getad?n=";
	line += "/getadframe.";
	line += "/getads/*";
	line += "/getadvertimageservlet?";
	line += "/getarticleadvertimageservlet?";
	line += "/getbanner.cfm?";
	line += "/getsponslinks.";
	line += "/getsponslinksauto.";
	line += "/getvdopiaads.";
	line += "/getvideoad.";
	line += "/gexternalad.";
	line += "/gfx/ads/*";
	line += "/glam_ads.";
	line += "/google-ad?";
	line += "/google-ads.";
	line += "/google-adsense-";
	line += "/google-adsense.";
	line += "/google_ad_";
	line += "/google_ads.";
	line += "/google_ads/*";
	line += "/google_ads_";
	line += "/google_adsense.";
	line += "/google_adsense_";
	line += "/google_afc.";
	line += "/googlead-";
	line += "/googlead.";
	line += "/googlead_";
	line += "/googleadhtml/*";
	line += "/googleadright.";
	line += "/googleads-";
	line += "/googleads.";
	line += "/googleads2.";
	line += "/googleads3widetext.";
	line += "/googleads_";
	line += "/googleadsafs_";
	line += "/googleadsense.";
	line += "/googleafs.";
	line += "/graphics/ad_";
	line += "/gt6skyadtop.";
	line += "/header_ads_";
	line += "/headerads.";
	line += "/headvert.";
	line += "/hitbar_ad_";
	line += "/homepage_ads/*";
	line += "/house-ads/*";
	line += "/house_ad-";
	line += "/house_ad_";
	line += "/housead/*";
	line += "/houseads.";
	line += "/houseads/*";
	line += "/houseads?";
	line += "/hoverad.";
	line += "/html.ng/*";
	line += "/htmlads/*";
	line += "/httpads/*";
	line += "/icon_ad.";
	line += "/idevaffiliate/*";
	line += "/iframe-ads/*";
	line += "/iframe/ad/*";
	line += "/iframe_ad.";
	line += "/iframe_ads/*";
	line += "/iframe_ads?";
	line += "/iframe_chitika_";
	line += "/iframead.";
	line += "/iframead/*";
	line += "/iframead_";
	line += "/iframeadsense.";
	line += "/iframeadsensewrapper.";
	line += "/iframedartad.";
	line += "/imads.js";
	line += "/image/ads/*";
	line += "/image_ads/*";
	line += "/images/ad-";
	line += "/images/ad.";
	line += "/images/ad/*";
	line += "/images/ad1.";
	line += "/images/ad125.";
	line += "/images/ad2.";
	line += "/images/ad4.";
	line += "/images/ad5.";
	line += "/images/ad_";
	line += "/images/ads-";
	line += "/images/ads/*";
	line += "/images/ads_";
	line += "/images/aff-";
	line += "/images/gads_";
	line += "/images/sponsored/*";
	line += "/images_jtads/*";
	line += "/img/ad/*";
	line += "/img/ad_";
	line += "/img/ads/*";
	line += "/img/sponsor/*";
	line += "/img/topad_";
	line += "/imgs/ads/*";
	line += "/inad.php";
	line += "/inc/ads/*";
	line += "/include/ads/*";
	line += "/include/boxad_";
	line += "/include/skyad_";
	line += "/included_ads/*";
	line += "/includes/ad_";
	line += "/includes/ads/*";
	line += "/incmpuad.";
	line += "/index-ad.";
	line += "/index_ads.";
	line += "/inline_ad.";
	line += "/inline_ad_";
	line += "/innerads.";
	line += "/insertads.";
	line += "/instreamad/*";
	line += "/intellitext.js";
	line += "/interad.";
	line += "/intextads.";
	line += "/introduction_ad.";
	line += "/invideoad.";
	line += "/inx-ad.";
	line += "/ipadad.";
	line += "/irc_ad_";
	line += "/ireel/ad*.jpg";
	line += "/ispy/ads/*";
	line += "/iwadsense.";
	line += "/j/ads.js";
	line += "/jivoxadplayer.";
	line += "/jlist-affiliates/*";
	line += "/js/ads-";
	line += "/js/ads/*";
	line += "/js/ads_";
	line += "/jsadscripts/*";
	line += "/jsfiles/ads/*";
	line += "/keyade.js";
	line += "/kredit-ad.";
	line += "/label-advertisement.";
	line += "/layer-ad.";
	line += "/layer-ads.";
	line += "/layer/ad.";
	line += "/layer/ads.";
	line += "/layerads_";
	line += "/layout/ads/*";
	line += "/lbl_ad.";
	line += "/leader_ad.";
	line += "/leftad_";
	line += "/linkads.";
	line += "/links_sponsored_";
	line += "/linkshare/*";
	line += "/liveads.";
	line += "/loadad.aspx?";
	line += "/loadadwiz.";
	line += "/local_ads_";
	line += "/lotto_ad_";
	line += "/lrec_ad.";
	line += "/mac-ad?";
	line += "/magic-ads/*";
	line += "/main/ad_";
	line += "/main_ad.";
	line += "/main_ad/*";
	line += "/mainad.";
	line += "/mbn_ad.";
	line += "/mcad.php";
	line += "/media/ads/*";
	line += "/megaad.";
	line += "/metaadserver/*";
	line += "/mini-ads/*";
	line += "/mini_ads.";
	line += "/mint/ads/*";
	line += "/misc/ad-";
	line += "/miva_ads.";
	line += "/mnetorfad.js";
	line += "/mobile_ad.";
	line += "/mobilephonesad/*";
	line += "/mod/adman/*";
	line += "/mod_ad/*";
	line += "/modalad.";
	line += "/modules/ad_";
	line += "/modules/ads/*";
	line += "/mpumessage.";
	line += "/msnadimg.";
	line += "/mstextad?";
	line += "/mtvi_ads_";
	line += "/mylayer-ad/*";
	line += "/mysimpleads/*";
	line += "/neoads.";
	line += "/new/ad/*";
	line += "/newads.";
	line += "/newrightcolad.";
	line += "/news_ad.";
	line += "/newtopmsgad.";
	line += "/nextad/*";
	line += "/o2contentad.";
	line += "/oas_ad_";
	line += "/oasadframe.";
	line += "/oascentral.$~object_subrequest";
	line += "/oasdefault/*";
	line += "/oasisi-*.php?";
	line += "/oasisi.php?";
	line += "/oiopub-direct/*$~stylesheet";
	line += "/omb-ad-";
	line += "/online/ads/*";
	line += "/openads-";
	line += "/openads.";
	line += "/openads/*";
	line += "/openads2/*";
	line += "/openx/www/*";
	line += "/openx_fl.";
	line += "/other/ads/*";
	line += "/overlay_ad_";
	line += "/ovt_show.asp?";
	line += "/page-ads.";
	line += "/pagead/ads?";
	line += "/pageadimg/*";
	line += "/pageads/*";
	line += "/pageear.";
	line += "/pageear/*";
	line += "/pageear_";
	line += "/pagepeel-";
	line += "/pagepeel.";
	line += "/pagepeel/*";
	line += "/pagepeel_banner/*";
	line += "/pagepeelads.";
	line += "/paidlisting/*";
	line += "/partnerads/*";
	line += "/partnerads_";
	line += "/partnerbanner.";
	line += "/partnerbanner/*";
	line += "/partners/ads/*";
	line += "/partnersadbutler/*";
	line += "/peel.js";
	line += "/peel/?webscr=";
	line += "/peel1.js";
	line += "/peelad.";
	line += "/peelad/*";
	line += "/peeljs.php";
	line += "/perfads.";
	line += "/performancingads/*";
	line += "/phpads.";
	line += "/phpads/*";
	line += "/phpads2/*";
	line += "/phpadserver/*";
	line += "/phpadsnew/*";
	line += "/pictures/ads/*";
	line += "/pilot_ad.";
	line += "/pitattoad.";
	line += "/pix/ad/*";
	line += "/pix/ads/*";
	line += "/pixelads/*";
	line += "/play/ad/*";
	line += "/player/ads/*";
	line += "/pool.ads.";
	line += "/pop_ad.";
	line += "/popads.";
	line += "/popads/*";
	line += "/popunder.";
	line += "/position=*/aamsz=";
	line += "/post_ads_";
	line += "/ppd_ads.";
	line += "/predictad.";
	line += "/premierebtnad/*";
	line += "/premium_ad.";
	line += "/premiumads/*";
	line += "/previews/ad/*";
	line += "/printad.";
	line += "/printad/*";
	line += "/printads/*";
	line += "/processads.";
	line += "/promo/ad_";
	line += "/promobuttonad.";
	line += "/promotions/ads.";
	line += "/promotions/ads?";
	line += "/protection/ad/*";
	line += "/pub/ad/*";
	line += "/pub/ads/*";
	line += "/public/ad/*";
	line += "/public/ad?";
	line += "/publicidad.$~object_subrequest";
	line += "/publicidad/*";
	line += "/qandaads/*";
	line += "/quadadvert.";
	line += "/questions/ads/*";
	line += "/radopenx?";
	line += "/radwindowclient.js";
	line += "/railad.";
	line += "/railads.";
	line += "/random=*/aamsz=";
	line += "/randomad.";
	line += "/randomad2.";
	line += "/rcolads1.";
	line += "/rcolads2.";
	line += "/rcom-ads.";
	line += "/realmedia/ads/*";
	line += "/rec_ad1.";
	line += "/reclame/*";
	line += "/rectangle_ad.";
	line += "/refreshads-";
	line += "/reklam.";
	line += "/reklama.";
	line += "/relatedads.";
	line += "/requestadvertisement.";
	line += "/requestmyspacead.";
	line += "/retrad.";
	line += "/richmedia.adv?";
	line += "/right-ad-";
	line += "/right_ads?";
	line += "/rightad.";
	line += "/rightnavads.";
	line += "/rightnavadsanswer.";
	line += "/rotads/*";
	line += "/rotateads.";
	line += "/rotatingpeels.js";
	line += "/rsads.js";
	line += "/rsads/c";
	line += "/rsads/r";
	line += "/rsads/u";
	line += "/rss/ads/*";
	line += "/satnetads.";
	line += "/satnetgoogleads.";
	line += "/scanscoutoverlayadrenderer.";
	line += "/scaradcontrol.";
	line += "/scripts/ad-";
	line += "/scripts/ad.";
	line += "/scripts/ad/*";
	line += "/scripts/ads/*";
	line += "/scripts/clickjs.php";
	line += "/search/ad/*";
	line += "/search/ads?";
	line += "/searchad.";
	line += "/searchads/*";
	line += "/secondads.";
	line += "/secondads_";
	line += "/serveads.";
	line += "/services/ads/*";
	line += "/sevenl_ad.";
	line += "/share/ads/*";
	line += "/shared/ads/*";
	line += "/show-ad.";
	line += "/show_ad.";
	line += "/show_ad_";
	line += "/show_ads.js";
	line += "/show_ads_";
	line += "/showad.";
	line += "/showad/*";
	line += "/showad_";
	line += "/showads.";
	line += "/showads/*";
	line += "/showadvertising.";
	line += "/showflashad.";
	line += "/showlayer.";
	line += "/showmarketingmaterial.";
	line += "/side-ad-";
	line += "/side-ads-";
	line += "/sidead.";
	line += "/sideads/*";
	line += "/sideads|";
	line += "/sidebar_ad.";
	line += "/sidebarad/*";
	line += "/sidecol_ad.";
	line += "/silver/ads/*";
	line += "/site_ads.";
	line += "/siteads.";
	line += "/siteads/*";
	line += "/siteafs.txt?";
	line += "/sites/ad_";
	line += "/skyad.php";
	line += "/skyadjs/*";
	line += "/skybar_ad.";
	line += "/skyframeopenads.";
	line += "/skyframeopenads_";
	line += "/skyscraperad.";
	line += "/slideadverts/*";
	line += "/slideinad.";
	line += "/small_ad.";
	line += "/smartad.";
	line += "/smartads.";
	line += "/smartlinks.";
	line += "/smb/ads/*";
	line += "/socialads.";
	line += "/socialads/*";
	line += "/someads.";
	line += "/spc.php?";
	line += "/spcjs.php?";
	line += "/special-ads/*";
	line += "/special_ads/*";
	line += "/specials/htads/*";
	line += "/spo_show.asp?";
	line += "/sponser.";
	line += "/sponsimages/*";
	line += "/sponslink_";
	line += "/sponsor-ad|";
	line += "/sponsor-right.";
	line += "/sponsor-top.";
	line += "/sponsor_images/*";
	line += "/sponsorad.";
	line += "/sponsoradds/*";
	line += "/sponsorads/*";
	line += "/sponsored_ad.";
	line += "/sponsored_links_";
	line += "/sponsored_text.";
	line += "/sponsored_top.";
	line += "/sponsoredcontent.";
	line += "/sponsoredlinks.";
	line += "/sponsoredlinks/*";
	line += "/sponsoredlinksiframe.";
	line += "/sponsoring/*";
	line += "/sponsors_box.";
	line += "/sponsorship_";
	line += "/sponsorstrips/*";
	line += "/square-ads/*";
	line += "/squaread.";
	line += "/srv/ad/*";
	line += "/static/ad_";
	line += "/static/ads/*";
	line += "/stickyad.";
	line += "/storage/ads/*";
	line += "/story_ad.";
	line += "/subad2_";
	line += "/swf/ad-";
	line += "/synad2.";
	line += "/system/ad/*";
	line += "/systemad.";
	line += "/systemad_";
	line += "/td_ads/*";
	line += "/tdlads/*";
	line += "/templates/_ads/*";
	line += "/testingad.";
	line += "/textad.";
	line += "/textad/*";
	line += "/textad?";
	line += "/textadrotate.";
	line += "/textads/*";
	line += "/textads_";
	line += "/thirdparty/ad/*";
	line += "/thirdpartyads/*";
	line += "/tii_ads.";
	line += "/tikilink?";
	line += "/tmo/ads/*";
	line += "/tmobilead.";
	line += "/toigoogleads.";
	line += "/toolkitads.";
	line += "/tools/ad.";
	line += "/top-ad-";
	line += "/top_ad.";
	line += "/top_ad_";
	line += "/top_ads/*";
	line += "/top_ads_";
	line += "/topad.php";
	line += "/topads.";
	line += "/topperad.";
	line += "/tracked_ad.";
	line += "/tradead_";
	line += "/tribalad.";
	line += "/ttz_ad.";
	line += "/tx_macinabanners/*";
	line += "/txt_ad.";
	line += "/ukc-ad.";
	line += "/unity/ad/*";
	line += "/update_ads/*";
	line += "/upload/ads/*";
	line += "/uploads/ads/*";
	line += "/us-adcentre.";
	line += "/valueclick.";
	line += "/vclkads.";
	line += "/video/ads/*";
	line += "/video_ad.";
	line += "/video_ad_";
	line += "/videoad.";
	line += "/videoads.";
	line += "/videoads/*";
	line += "/videowall-ad.";
	line += "/view/banner/*/zone?zid=";
	line += "/vtextads.";
	line += "/wallpaperads/*";
	line += "/webad?a";
	line += "/webadimg/*";
	line += "/webads.";
	line += "/webads_";
	line += "/webadverts/*";
	line += "/welcome_ad.";
	line += "/widget/ads.";
	line += "/wipeads/*";
	line += "/wp-content/ads/*";
	line += "/wp-content/plugins/fasterim-optin/*";
	line += "/wp-srv/ad/*";
	line += "/wpads/iframe.";
	line += "/writelayerad.";
	line += "/www/ads/*";
	line += "/www/delivery/*";
	line += "/xmladparser.";
	line += "/yahoo-ads/*";
	line += "/your-ad.";
	line += "/ysmads.";
	line += "/zanox.js";
	line += "/zanox/banner/*";
	line += "/zanox_ad/*";
	line += "2010/ads/";
	line += "8080/ads/";
	line += ";adsense_";
	line += ";iframeid=ad_";
	line += "=adtech_";
	line += "=advert/";
	line += "=advertorial&";
	line += "?ad_ids=";
	line += "?ad_type=";
	line += "?ad_width=";
	line += "?adarea=";
	line += "?adclass=";
	line += "?adpage=";
	line += "?adpartner=";
	line += "?adsize=";
	line += "?adslot=";
	line += "?adtype=";
	line += "?advertising=";
	line += "?advtile=";
	line += "?advurl=";
	line += "?file=ads&";
	line += "?getad=&$~object_subrequest";
	line += "?view=ad&";
	line += "_140x600_";
	line += "_160_ad_";
	line += "_acorn_ad_";
	line += "_ad.php?";
	line += "_ad120x120_";
	line += "_ad234x90-";
	line += "_ad_big.";
	line += "_ad_bsb.";
	line += "_ad_code.";
	line += "_ad_content.";
	line += "_ad_controller.";
	line += "_ad_count.";
	line += "_ad_courier.";
	line += "_ad_footer.";
	line += "_ad_homepage.";
	line += "_ad_iframe.";
	line += "_ad_images/";
	line += "_ad_label.";
	line += "_ad_leaderboard.";
	line += "_ad_placeholder-";
	line += "_ad_right.";
	line += "_ad_skyscraper.";
	line += "_ad_square.";
	line += "_ad_widesky.";
	line += "_adagency/";
	line += "_adbanner.";
	line += "_adbreak.";
	line += "_adcall_";
	line += "_adfunction.";
	line += "_adpage=";
	line += "_adpartner.";
	line += "_adplugin.";
	line += "_ads.html";
	line += "_ads.php?";
	line += "_ads/script.";
	line += "_ads1.asp";
	line += "_ads?pid=";
	line += "_ads_index_";
	line += "_ads_reporting.";
	line += "_ads_single_";
	line += "_adserve/";
	line += "_adshare.";
	line += "_adshow.";
	line += "_adsjs.php?";
	line += "_adsys.js";
	line += "_adtext_";
	line += "_adtitle.";
	line += "_advert.";
	line += "_advert1.";
	line += "_advertise.";
	line += "_advertise180.";
	line += "_advertisehere.";
	line += "_advertisement.";
	line += "_advertisement_";
	line += "_advertising/";
	line += "_advertisment.";
	line += "_advertorials/";
	line += "_adwrap.";
	line += "_afs_ads.";
	line += "_argus_ad_";
	line += "_assets/ads/";
	line += "_background_ad/";
	line += "_banner_adv_";
	line += "_bannerad.";
	line += "_blogads.";
	line += "_bottom_ads_";
	line += "_box_ads/";
	line += "_buttonad.";
	line += "_companionad.";
	line += "_contest_ad_";
	line += "_custom_ad.";
	line += "_custom_ad_";
	line += "_displaytopads.";
	line += "_dynamicads/";
	line += "_externalad.";
	line += "_fach_ad.";
	line += "_fd_adbg1a.";
	line += "_fd_adbg2.";
	line += "_fd_adbg2a.";
	line += "_fd_adtop.";
	line += "_feast_ad.";
	line += "_gads_bottom.";
	line += "_gads_footer.";
	line += "_gads_top.";
	line += "_headerad.";
	line += "_home_adrow-";
	line += "_images/ads/";
	line += "_inc/adsrc.";
	line += "_index_ad.";
	line += "_mainad.";
	line += "_media/ads/*";
	line += "_mmsadbanner/";
	line += "_org_ad.";
	line += "_overlay_ad.";
	line += "_paidadvert_";
	line += "_player_ads_";
	line += "_request_ad.";
	line += "_right_ad.";
	line += "_special_ads/";
	line += "_stack_ads/";
	line += "_temp/ad_";
	line += "_top_ad.";
	line += "_topad.php";
	line += "_tribalfusion.";
	line += "_videoad.";
	line += "a/adx.js";
	line += "couk/ads/";
	line += "d/adx.js";
	line += "e/adx.js";
	line += "m/adbox/";
	line += "m/adx.js";
	line += "m/topads|";
	line += "n/adx.js";
	line += "s/adx.js";
	line += "t/adx.js";
	line += "u/adx.js";
	line += "z/adx.js";
	line += "|http://a.ads.";
	line += "|http://ad-uk.";
	line += "|http://ad.$~object_subrequest,domain=~europa.eu|~gogopin.com|~sjsu.edu|~uitm.edu.my|~uni-freiburg.de";
	line += "|http://ad0.";
	line += "|http://ad1.";
	line += "|http://ad2.$domain=~ad2.zophar.net";
	line += "|http://ad3.";
	line += "|http://ad4.";
	line += "|http://ad5.";
	line += "|http://ad6.";
	line += "|http://ad7.";
	line += "|http://adbox.";
	line += "|http://adimg.";
	line += "|http://adnet.";
	line += "|http://ads.$domain=~ahds.ac.uk";
	line += "|http://ads0.";
	line += "|http://ads1.";
	line += "|http://ads18.";
	line += "|http://ads2.";
	line += "|http://ads3.";
	line += "|http://ads4.";
	line += "|http://ads5.";
	line += "|http://adserv";
	line += "|http://adseu.";
	line += "|http://adsrv.";
	line += "|http://adsvr.";
	line += "|http://adsys.";
	line += "|http://adtag.";
	line += "|http://adver.";
	line += "|http://advertiser.";
	line += "|http://bwp.*/search";
	line += "|http://feeds.*/~a/";
	line += "|http://getad.";
	line += "|http://jazad.";
	line += "|http://openx.";
	line += "|http://pubad.";
	line += "|http://rss.*/~a/";
	line += "|http://synad.";
	line += "|http://u-ads.";
	line += "|http://wrapper.*/a?";
	line += "|https://ads.";
	line += "||adserver1.";
	line += "!Dimensions";
	line += "-120x60-";
	line += "-120x60.";
	line += "-360x110.";
	line += "-468_60.";
	line += "-468x60-";
	line += "-468x60.";
	line += "-468x60/";
	line += "-468x60_";
	line += "-468x80-";
	line += "-468x80.";
	line += "-468x80/";
	line += "-468x80_";
	line += "-480x60-";
	line += "-480x60.";
	line += "-480x60/";
	line += "-480x60_";
	line += "-728x90-";
	line += "-728x90.";
	line += "-728x90/";
	line += "-728x90_";
	line += ".468x60-";
	line += ".468x60.";
	line += ".468x60/";
	line += ".468x60_";
	line += ".468x80-";
	line += ".468x80.";
	line += ".468x80/";
	line += ".468x80_";
	line += ".480x60-";
	line += ".480x60.";
	line += ".480x60/";
	line += ".480x60_";
	line += ".728x90-";
	line += ".728x90.";
	line += ".728x90/";
	line += ".728x90_";
	line += ".com/160_";
	line += ".com/728_";
	line += "/125x125_banner.";
	line += "/180x150-";
	line += "/180x150_";
	line += "/250x250.";
	line += "/300x250-";
	line += "/300x250.";
	line += "/300x250_";
	line += "/468-60.";
	line += "/468_60.";
	line += "/468x60-";
	line += "/468x60.";
	line += "/468x60/*";
	line += "/468x60_";
	line += "/468x80-";
	line += "/468x80.";
	line += "/468x80/*";
	line += "/468x80_";
	line += "/480x60-";
	line += "/480x60.";
	line += "/480x60/*";
	line += "/480x60_";
	line += "/600x160_";
	line += "/728_90/*";
	line += "/728x79_";
	line += "/728x90-";
	line += "/728x90.";
	line += "/728x90/*";
	line += "/728x90_";
	line += "/768x90-";
	line += "=300x250;";
	line += "_120_600";
	line += "_120x60.";
	line += "_120x600.";
	line += "_120x60_";
	line += "_160_600_";
	line += "_160x600_";
	line += "_300_250_";
	line += "_300x250-";
	line += "_300x250_";
	line += "_460x60.";
	line += "_468-60.";
	line += "_468.gif";
	line += "_468.htm";
	line += "_468_60.";
	line += "_468_60_";
	line += "_468x120.";
	line += "_468x60-";
	line += "_468x60.";
	line += "_468x60/";
	line += "_468x60_";
	line += "_468x80-";
	line += "_468x80.";
	line += "_468x80/";
	line += "_468x80_";
	line += "_468x90.";
	line += "_480x60-";
	line += "_480x60.";
	line += "_480x60/";
	line += "_480x60_";
	line += "_720x90.";
	line += "_728.gif";
	line += "_728.htm";
	line += "_728_90.";
	line += "_728_90_";
	line += "_728x90-";
	line += "_728x90.";
	line += "_728x90/";
	line += "_728x90_";
	line += "_768x90_";
	line += "!-----------------General element hiding rules-----------------!";
	line += "! *** easylist_general_hide.txt ***";
	line += "###A9AdsMiddleBoxTop";
	line += "###A9AdsOutOfStockWidgetTop";
	line += "###A9AdsServicesWidgetTop";
	line += "###ADSLOT_1";
	line += "###ADSLOT_2";
	line += "###ADSLOT_3";
	line += "###ADSLOT_4";
	line += "###AD_CONTROL_22";
	line += "###ADsmallWrapper";
	line += "~digitalhome.ca###Ad1";
	line += "###Ad160x600";
	line += "###Ad2";
	line += "###Ad300x250";
	line += "###Ad3Left";
	line += "###Ad3Right";
	line += "###Ad3TextAd";
	line += "###AdBanner_F1";
	line += "###AdBar";
	line += "###AdBar1";
	line += "###AdContainerTop";
	line += "###AdContentModule_F";
	line += "###AdDetails_GoogleLinksBottom";
	line += "###AdDetails_InsureWith";
	line += "###AdFrame4";
	line += "~ksl.com###AdHeader";
	line += "###AdMiddle";
	line += "###AdMobileLink";
	line += "###AdRectangle";
	line += "###AdSenseDiv";
	line += "###AdServer";
	line += "###AdShowcase_F1";
	line += "###AdSky23";
	line += "###AdSkyscraper";
	line += "###AdSponsor_SF";
	line += "###AdSubsectionShowcase_F1";
	line += "###AdTargetControl1_iframe";
	line += "###AdText";
	line += "###AdTop";
	line += "###Ad_Block";
	line += "###Ad_Center1";
	line += "###Ad_Right1";
	line += "###Ad_Top";
	line += "~cynamite.de###Adbanner";
	line += "###Adrectangle";
	line += "###Ads";
	line += "###AdsContent";
	line += "###AdsRight";
	line += "###AdsWrap";
	line += "###Ads_BA_CAD";
	line += "###Ads_BA_CAD2";
	line += "###Ads_BA_CAD_box";
	line += "###Ads_BA_SKY";
	line += "###AdvertMPU23b";
	line += "###AdvertPanel";
	line += "~designspotter.com###AdvertiseFrame";
	line += "~winload.de###Advertisement";
	line += "###Advertisements";
	line += "###Advertorial";
	line += "###Advertorials";
	line += "###BannerAdvert";
	line += "###BigBoxAd";
	line += "###BodyAd";
	line += "###ButtonAd";
	line += "###CompanyDetailsNarrowGoogleAdsPresentationControl";
	line += "###CompanyDetailsWideGoogleAdsPresentationControl";
	line += "###ContentAd";
	line += "###ContentAd1";
	line += "###ContentAd2";
	line += "###ContentAdPlaceHolder1";
	line += "###ContentAdPlaceHolder2";
	line += "###ContentAdXXL";
	line += "###ContentPolepositionAds_Result";
	line += "###DivAdEggHeadCafeTopBanner";
	line += "###FooterAd";
	line += "###FooterAdContainer";
	line += "###GoogleAd1";
	line += "###GoogleAd2";
	line += "###GoogleAd3";
	line += "###GoogleAdsPresentationControl";
	line += "###GoogleAdsense";
	line += "###Google_Adsense_Main";
	line += "###HEADERAD";
	line += "###HOME_TOP_RIGHT_BOXAD";
	line += "###HeaderAdsBlock";
	line += "###HeaderAdsBlockFront";
	line += "###HeaderBannerAdSpacer";
	line += "###HeaderTextAd";
	line += "###HeroAd";
	line += "###HomeAd1";
	line += "###HouseAd";
	line += "###ID_Ad_Sky";
	line += "###Journal_Ad_125";
	line += "###Journal_Ad_300";
	line += "###KH-contentAd";
	line += "###LeftAd";
	line += "###LeftAdF1";
	line += "###LeftAdF2";
	line += "###LftAd";
	line += "###LoungeAdsDiv";
	line += "###LowerContentAd";
	line += "###MainSponsoredLinks";
	line += "###Nightly_adContainer";
	line += "###PREFOOTER_LEFT_BOXAD";
	line += "###PREFOOTER_RIGHT_BOXAD";
	line += "###PageLeaderAd";
	line += "###RelevantAds";
	line += "###RgtAd1";
	line += "###RightAd";
	line += "###RightNavTopAdSpot";
	line += "###RightSponsoredAd";
	line += "###SectionAd300-250";
	line += "###SectionSponsorAd";
	line += "###SidebarAdContainer";
	line += "###SkyAd";
	line += "###SpecialAds";
	line += "###SponsoredAd";
	line += "###SponsoredLinks";
	line += "###TOP_ADROW";
	line += "###TOP_RIGHT_BOXAD";
	line += "~kids.t-online.de###Tadspacehead";
	line += "###TopAd";
	line += "###TopAdContainer";
	line += "###TopAdDiv";
	line += "###TopAdPos";
	line += "###VM-MPU-adspace";
	line += "###VM-footer-adspace";
	line += "###VM-header-adspace";
	line += "###VM-header-adwrap";
	line += "###XEadLeaderboard";
	line += "###XEadSkyscraper";
	line += "###_ads";
	line += "###about_adsbottom";
	line += "###ad-120x600-sidebar";
	line += "###ad-120x60Div";
	line += "###ad-160x600";
	line += "###ad-160x600-sidebar";
	line += "###ad-250";
	line += "###ad-250x300";
	line += "###ad-300";
	line += "###ad-300x250";
	line += "###ad-300x250-sidebar";
	line += "###ad-300x250Div";
	line += "###ad-728";
	line += "###ad-728x90-leaderboard-top";
	line += "###ad-article";
	line += "###ad-banner";
	line += "###ad-bottom";
	line += "###ad-bottom-wrapper";
	line += "###ad-boxes";
	line += "###ad-bs";
	line += "###ad-buttons";
	line += "###ad-colB-1";
	line += "###ad-column";
	line += "~madame.lefigaro.fr###ad-container";
	line += "###ad-content";
	line += "###ad-contentad";
	line += "###ad-footer";
	line += "###ad-footprint-160x600";
	line += "###ad-front-footer";
	line += "###ad-front-sponsoredlinks";
	line += "###ad-halfpage";
	line += "~ifokus.se###ad-header";
	line += "###ad-inner";
	line += "###ad-label";
	line += "###ad-leaderboard";
	line += "###ad-leaderboard-bottom";
	line += "###ad-leaderboard-container";
	line += "###ad-leaderboard-spot";
	line += "###ad-leaderboard-top";
	line += "###ad-left";
	line += "###ad-list-row";
	line += "###ad-lrec";
	line += "###ad-medium-rectangle";
	line += "###ad-medrec";
	line += "###ad-middlethree";
	line += "###ad-middletwo";
	line += "###ad-module";
	line += "###ad-mpu";
	line += "###ad-mpu1-spot";
	line += "###ad-mpu2";
	line += "###ad-mpu2-spot";
	line += "###ad-north";
	line += "###ad-one";
	line += "###ad-placard";
	line += "###ad-placeholder";
	line += "###ad-rectangle";
	line += "###ad-right";
	line += "###ad-righttop";
	line += "###ad-row";
	line += "###ad-side-text";
	line += "###ad-sky";
	line += "###ad-skyscraper";
	line += "###ad-slug-wrapper";
	line += "###ad-small-banner";
	line += "###ad-space";
	line += "###ad-splash";
	line += "###ad-spot";
	line += "###ad-target";
	line += "###ad-target-Leaderbord";
	line += "###ad-teaser";
	line += "###ad-text";
	line += "~gismeteo.com,~gismeteo.ru,~gismeteo.ua###ad-top";
	line += "###ad-top-banner";
	line += "###ad-top-text-low";
	line += "###ad-top-wrap";
	line += "###ad-tower";
	line += "###ad-trailerboard-spot";
	line += "###ad-typ1";
	line += "###ad-west";
	line += "###ad-wrap";
	line += "###ad-wrap-right";
	line += "~collegeslackers.com###ad-wrapper";
	line += "###ad-wrapper1";
	line += "###ad-yahoo-simple";
	line += "~tphousing.com,~tvgorge.com###ad1";
	line += "###ad1006";
	line += "###ad125BL";
	line += "###ad125BR";
	line += "###ad125TL";
	line += "###ad125TR";
	line += "###ad125x125";
	line += "###ad160x600";
	line += "###ad160x600right";
	line += "###ad1Sp";
	line += "###ad2";
	line += "###ad2Sp";
	line += "~absoluteradio.co.uk###ad3";
	line += "###ad300";
	line += "###ad300-250";
	line += "###ad300X250";
	line += "###ad300_x_250";
	line += "###ad300x150";
	line += "###ad300x250";
	line += "###ad300x250Module";
	line += "###ad300x60";
	line += "###ad300x600";
	line += "###ad300x600_callout";
	line += "###ad336";
	line += "###ad336x280";
	line += "###ad375x85";
	line += "###ad4";
	line += "###ad468";
	line += "###ad468x60";
	line += "###ad468x60_top";
	line += "###ad526x250";
	line += "###ad600";
	line += "###ad7";
	line += "###ad728";
	line += "###ad728Mid";
	line += "###ad728Top";
	line += "###ad728Wrapper";
	line += "~natgeo.tv###ad728x90";
	line += "###adBadges";
	line += "~campusdish.com###adBanner";
	line += "###adBanner120x600";
	line += "###adBanner160x600";
	line += "###adBanner336x280";
	line += "###adBannerTable";
	line += "###adBannerTop";
	line += "###adBar";
	line += "###adBlock125";
	line += "###adBlocks";
	line += "###adBox";
	line += "###adBox350";
	line += "###adBox390";
	line += "###adComponentWrapper";
	line += "~jobs.wa.gov.au,~pricewatch.com###adContainer";
	line += "###adContainer_1";
	line += "###adContainer_2";
	line += "###adContainer_3";
	line += "~remixshare.com###adDiv";
	line += "###adFps";
	line += "###adFtofrs";
	line += "###adGallery";
	line += "###adGroup1";
	line += "###adHeader";
	line += "###adIsland";
	line += "###adL";
	line += "###adLB";
	line += "###adLabel";
	line += "###adLayer";
	line += "###adLeaderTop";
	line += "###adLeaderboard";
	line += "###adMPU";
	line += "###adMiddle0Frontpage";
	line += "###adMiniPremiere";
	line += "###adP";
	line += "###adPlaceHolderRight";
	line += "###adPlacer";
	line += "###adRight";
	line += "###adSenseModule";
	line += "###adSenseWrapper";
	line += "###adServer_marginal";
	line += "###adSidebar";
	line += "###adSidebarSq";
	line += "###adSky";
	line += "###adSkyscraper";
	line += "###adSlider";
	line += "###adSpace";
	line += "###adSpace3";
	line += "###adSpace300_ifrMain";
	line += "###adSpace4";
	line += "###adSpace5";
	line += "###adSpace6";
	line += "###adSpace7";
	line += "###adSpace_footer";
	line += "###adSpace_right";
	line += "###adSpace_top";
	line += "###adSpacer";
	line += "###adSpecial";
	line += "###adSpot-Leader";
	line += "###adSpot-banner";
	line += "###adSpot-island";
	line += "###adSpot-mrec1";
	line += "###adSpot-sponsoredlinks";
	line += "###adSpot-textbox1";
	line += "###adSpot-widestrip";
	line += "###adSpotAdvertorial";
	line += "###adSpotIsland";
	line += "###adSpotSponsoredLinks";
	line += "###adSquare";
	line += "###adStaticA";
	line += "###adStrip";
	line += "###adSuperAd";
	line += "###adSuperPremiere";
	line += "###adSuperbanner";
	line += "###adTableCell";
	line += "###adTag1";
	line += "###adTag2";
	line += "###adText";
	line += "###adText_container";
	line += "###adTile";
	line += "~ran.de###adTop";
	line += "###adTopboxright";
	line += "###adTower";
	line += "###adUnit";
	line += "~bankenverband.de,~thisisleicestershire.co.uk###adWrapper";
	line += "###adZoneTop";
	line += "###ad_160x160";
	line += "###ad_160x600";
	line += "###ad_190x90";
	line += "###ad_300";
	line += "###ad_300_250";
	line += "###ad_300_250_1";
	line += "###ad_300x250";
	line += "###ad_300x250_content_column";
	line += "###ad_300x90";
	line += "###ad_468_60";
	line += "###ad_5";
	line += "###ad_728_foot";
	line += "###ad_728x90";
	line += "###ad_940";
	line += "###ad_984";
	line += "###ad_A";
	line += "###ad_B";
	line += "###ad_Banner";
	line += "###ad_C";
	line += "###ad_C2";
	line += "###ad_D";
	line += "###ad_E";
	line += "###ad_F";
	line += "###ad_G";
	line += "###ad_H";
	line += "###ad_I";
	line += "###ad_J";
	line += "###ad_K";
	line += "###ad_L";
	line += "###ad_M";
	line += "###ad_N";
	line += "###ad_O";
	line += "###ad_P";
	line += "###ad_YieldManager-300x250";
	line += "###ad_anchor";
	line += "###ad_area";
	line += "###ad_banner";
	line += "###ad_banner_top";
	line += "###ad_bar";
	line += "###ad_bellow_post";
	line += "###ad_block_1";
	line += "###ad_block_2";
	line += "###ad_bottom";
	line += "###ad_box_colspan";
	line += "###ad_branding";
	line += "###ad_bs_area";
	line += "###ad_center_monster";
	line += "###ad_cont";
	line += "~academics.de###ad_container";
	line += "###ad_container_marginal";
	line += "###ad_container_side";
	line += "###ad_container_top";
	line += "###ad_content_top";
	line += "###ad_content_wrap";
	line += "###ad_feature";
	line += "###ad_firstpost";
	line += "###ad_footer";
	line += "###ad_front_three";
	line += "###ad_fullbanner";
	line += "~arablionz.com,~djluv.in,~egymedicine.net###ad_global_below_navbar";
	line += "###ad_global_header";
	line += "###ad_haha_1";
	line += "###ad_haha_4";
	line += "###ad_halfpage";
	line += "###ad_head";
	line += "###ad_header";
	line += "###ad_horizontal";
	line += "###ad_horseshoe_left";
	line += "###ad_horseshoe_right";
	line += "###ad_horseshoe_spacer";
	line += "###ad_horseshoe_top";
	line += "###ad_hotpots";
	line += "###ad_in_arti";
	line += "###ad_island";
	line += "###ad_label";
	line += "###ad_lastpost";
	line += "###ad_layer2";
	line += "###ad_leader";
	line += "###ad_leaderBoard";
	line += "###ad_leaderboard";
	line += "###ad_leaderboard_top";
	line += "###ad_left";
	line += "###ad_lrec";
	line += "###ad_lwr_square";
	line += "###ad_main";
	line += "###ad_medium_rectangle";
	line += "###ad_medium_rectangular";
	line += "###ad_mediumrectangle";
	line += "###ad_menu_header";
	line += "###ad_middle";
	line += "###ad_most_pop_234x60_req_wrapper";
	line += "###ad_mpu";
	line += "###ad_mpuav";
	line += "###ad_mrcontent";
	line += "###ad_overlay";
	line += "###ad_play_300";
	line += "###ad_rect";
	line += "###ad_rect_body";
	line += "###ad_rect_bottom";
	line += "###ad_rectangle";
	line += "###ad_rectangle_medium";
	line += "###ad_related_links_div";
	line += "###ad_related_links_div_program";
	line += "###ad_replace_div_0";
	line += "###ad_replace_div_1";
	line += "###ad_report_leaderboard";
	line += "###ad_report_rectangle";
	line += "###ad_right";
	line += "###ad_right_main";
	line += "###ad_ros_tower";
	line += "###ad_rr_1";
	line += "###ad_sec";
	line += "###ad_sec_div";
	line += "###ad_sidebar";
	line += "###ad_sidebar1";
	line += "###ad_sidebar2";
	line += "###ad_sidebar3";
	line += "###ad_skyscraper";
	line += "###ad_skyscraper_text";
	line += "###ad_slot_leaderboard";
	line += "###ad_slot_livesky";
	line += "###ad_slot_sky_top";
	line += "~streetinsider.com###ad_space";
	line += "~wretch.cc###ad_square";
	line += "###ad_ss";
	line += "###ad_table";
	line += "###ad_term_bottom_place";
	line += "###ad_thread_first_post_content";
	line += "###ad_top";
	line += "###ad_top_holder";
	line += "###ad_tp_banner_1";
	line += "###ad_tp_banner_2";
	line += "###ad_unit";
	line += "###ad_vertical";
	line += "###ad_widget";
	line += "###ad_window";
	line += "~amnestyusa.org,~drownedinsound.com###ad_wrapper";
	line += "###adbanner";
	line += "###adbig";
	line += "###adbnr";
	line += "###adboard";
	line += "###adbody";
	line += "###adbottom";
	line += "~kalaydo.de###adbox";
	line += "###adbox1";
	line += "###adbox2";
	line += "###adclear";
	line += "###adcode";
	line += "###adcode1";
	line += "###adcode2";
	line += "###adcode3";
	line += "###adcode4";
	line += "###adcolumnwrapper";
	line += "###adcontainer";
	line += "###adcontainerRight";
	line += "###adcontainsm";
	line += "###adcontent";
	line += "###adcontrolPushSite";
	line += "###add_ciao2";
	line += "###addbottomleft";
	line += "###addiv-bottom";
	line += "###addiv-top";
	line += "###adfooter_728x90";
	line += "###adframe:not(frameset)";
	line += "###adhead";
	line += "###adhead_g";
	line += "###adheader";
	line += "###adhome";
	line += "###adiframe1_iframe";
	line += "###adiframe2_iframe";
	line += "###adiframe3_iframe";
	line += "###adimg";
	line += "###adition_content_ad";
	line += "###adlabel";
	line += "###adlabelFooter";
	line += "###adlayerad";
	line += "###adleaderboard";
	line += "###adleft";
	line += "###adlinks";
	line += "###adlinkws";
	line += "###adlrec";
	line += "###admid";
	line += "###admiddle3center";
	line += "###admiddle3left";
	line += "###adposition";
	line += "###adposition-C";
	line += "###adposition-FPMM";
	line += "###adposition2";
	line += "~contracostatimes.com,~mercurynews.com,~siliconvalley.com###adposition3";
	line += "###adposition4";
	line += "###adrectangle";
	line += "###adrectanglea";
	line += "###adrectangleb";
	line += "###adrig";
	line += "###adright";
	line += "###adright2";
	line += "###adrighthome";
	line += "~2gb-hosting.com,~addoway.com,~bash.org.ru,~block-adblock-plus.com,~dailykos.com,~divxhosting.net,~facebook.com,~harpers.org,~miloyski.com,~radio.de,~tomwans.com,~tonprenom.com,~www.google.com,~zuploads.com,~zuploads.net###ads";
	line += "###ads-468";
	line += "###ads-area";
	line += "###ads-block";
	line += "###ads-bot";
	line += "###ads-bottom";
	line += "###ads-col";
	line += "###ads-dell";
	line += "###ads-horizontal";
	line += "###ads-indextext";
	line += "###ads-leaderboard1";
	line += "###ads-lrec";
	line += "###ads-menu";
	line += "###ads-middle";
	line += "###ads-prices";
	line += "###ads-rhs";
	line += "###ads-right";
	line += "###ads-top";
	line += "###ads-vers7";
	line += "###ads160left";
	line += "###ads2";
	line += "###ads300";
	line += "###ads300Bottom";
	line += "###ads300Top";
	line += "###ads336x280";
	line += "###ads7";
	line += "###ads728bottom";
	line += "###ads728top";
	line += "###ads790";
	line += "###adsDisplay";
	line += "###adsID";
	line += "###ads_160";
	line += "###ads_300";
	line += "###ads_728";
	line += "###ads_banner";
	line += "###ads_belowforumlist";
	line += "###ads_belownav";
	line += "###ads_bottom_inner";
	line += "###ads_bottom_outer";
	line += "###ads_box";
	line += "###ads_button";
	line += "###ads_catDiv";
	line += "###ads_footer";
	line += "###ads_html1";
	line += "###ads_html2";
	line += "###ads_right";
	line += "###ads_right_sidebar";
	line += "###ads_sidebar_roadblock";
	line += "###ads_space";
	line += "###ads_top";
	line += "###ads_watch_top_square";
	line += "###ads_zone27";
	line += "###adsbottom";
	line += "###adsbox";
	line += "###adscolumn";
	line += "###adsd_contentad_r1";
	line += "###adsd_contentad_r2";
	line += "###adsd_contentad_r3";
	line += "###adsense";
	line += "###adsense-tag";
	line += "###adsense-text";
	line += "###adsenseOne";
	line += "###adsenseWrap";
	line += "~jeeppatriot.com###adsense_inline";
	line += "###adsense_leaderboard";
	line += "###adsense_overlay";
	line += "###adsense_placeholder_2";
	line += "###adsenseheader";
	line += "###adsensetopplay";
	line += "###adsensewidget-3";
	line += "###adserv";
	line += "###adsky";
	line += "###adskyscraper";
	line += "###adslot";
	line += "###adsonar";
	line += "~metblogs.com,~oreilly.com###adspace";
	line += "###adspace-300x250";
	line += "###adspace300x250";
	line += "###adspaceBox";
	line += "###adspaceBox300";
	line += "###adspace_header";
	line += "###adspot-1";
	line += "###adspot-149x170";
	line += "###adspot-1x4";
	line += "###adspot-2";
	line += "###adspot-295x60";
	line += "###adspot-2a";
	line += "###adspot-2b";
	line += "###adspot-300x250-pos-1";
	line += "###adspot-300x250-pos-2";
	line += "###adspot-468x60-pos-2";
	line += "###adspot-a";
	line += "###adspot300x250";
	line += "###adsright";
	line += "###adstop";
	line += "###adt";
	line += "###adtab";
	line += "###adtag_right_side";
	line += "###adtech_googleslot_03c";
	line += "###adtech_takeover";
	line += "###adtop";
	line += "###adtxt";
	line += "###adv-masthead";
	line += "###adv_google_300";
	line += "###adv_google_728";
	line += "###adv_top_banner_wrapper";
	line += "###adver1";
	line += "###adver2";
	line += "###adver3";
	line += "###adver4";
	line += "###adver5";
	line += "###adver6";
	line += "###adver7";
	line += "~finn.no,~gcrg.org,~kalaydo.de,~m424.com,~secondamano.it,~sepa.org.uk###advert";
	line += "###advert-1";
	line += "###advert-120";
	line += "###advert-boomer";
	line += "###advert-display";
	line += "###advert-header";
	line += "###advert-leaderboard";
	line += "###advert-links-bottom";
	line += "###advert-skyscraper";
	line += "###advert-top";
	line += "###advert1";
	line += "###advertBanner";
	line += "###advertRight";
	line += "###advert_250x250";
	line += "###advert_box";
	line += "###advert_leaderboard";
	line += "###advert_lrec_format";
	line += "###advert_mid";
	line += "###advert_mpu";
	line += "###advert_right_skyscraper";
	line += "###advertbox";
	line += "###advertbox2";
	line += "###advertbox3";
	line += "###advertbox4";
	line += "###adverthome";
	line += "~zlatestranky.cz###advertise";
	line += "###advertise-now";
	line += "###advertise1";
	line += "###advertiseHere";
	line += "~ping-timeout.de###advertisement";
	line += "###advertisement160x600";
	line += "###advertisement728x90";
	line += "###advertisementLigatus";
	line += "###advertisementPrio2";
	line += "###advertiser-container";
	line += "###advertiserLinks";
	line += "~uscbookstore.com###advertising";
	line += "###advertising-banner";
	line += "###advertising-caption";
	line += "###advertising-container";
	line += "###advertising-control";
	line += "###advertising-skyscraper";
	line += "###advertisingModule160x600";
	line += "###advertisingModule728x90";
	line += "###advertisment";
	line += "###advertismentElementInUniversalbox";
	line += "###advertorial";
	line += "~markt.de###adverts";
	line += "###adverts-top-container";
	line += "###adverts-top-left";
	line += "###adverts-top-middle";
	line += "###adverts-top-right";
	line += "###advertsingle";
	line += "###advt";
	line += "###adwhitepaperwidget";
	line += "###adwin_rec";
	line += "###adwith";
	line += "###adwords-4-container";
	line += "~musicstar.de###adwrapper";
	line += "###adxBigAd";
	line += "###adxMiddle5";
	line += "###adxSponLink";
	line += "###adxSponLinkA";
	line += "###adxtop";
	line += "###adzbanner";
	line += "###adzerk";
	line += "###adzoneBANNER";
	line += "###affinityBannerAd";
	line += "###agi-ad300x250";
	line += "###agi-ad300x250overlay";
	line += "###agi-sponsored";
	line += "###alert_ads";
	line += "###anchorAd";
	line += "###annoying_ad";
	line += "###ap_adframe";
	line += "###apiBackgroundAd";
	line += "###apiTopAdWrap";
	line += "###apmNADiv";
	line += "###araHealthSponsorAd";
	line += "###article-ad-container";
	line += "###article-box-ad";
	line += "###articleAdReplacement";
	line += "###articleLeftAdColumn";
	line += "###articleSideAd";
	line += "###article_ad";
	line += "###article_box_ad";
	line += "###asinglead";
	line += "###atlasAdDivGame";
	line += "###awds-nt1-ad";
	line += "###banner-300x250";
	line += "###banner-ad";
	line += "###banner-ad-container";
	line += "###banner-ads";
	line += "###banner250x250";
	line += "###banner468x60";
	line += "###banner728x90";
	line += "###bannerAd";
	line += "###bannerAdTop";
	line += "###bannerAd_ctr";
	line += "###banner_ad";
	line += "###banner_ad_footer";
	line += "###banner_admicro";
	line += "###banner_ads";
	line += "###banner_content_ad";
	line += "###banner_topad";
	line += "###bannerad";
	line += "###bannerad2";
	line += "###bbccom_mpu";
	line += "###bbccom_storyprintsponsorship";
	line += "###bbo_ad1";
	line += "###bg-footer-ads";
	line += "###bg-footer-ads2";
	line += "###bg_YieldManager-300x250";
	line += "###bigAd";
	line += "###bigBoxAd";
	line += "###bigad300outer";
	line += "###bigadbox";
	line += "###bigadspot";
	line += "###billboard_ad";
	line += "###block-ad_cube-1";
	line += "###block-openads-1";
	line += "###block-openads-3";
	line += "###block-openads-4";
	line += "###block-openads-5";
	line += "###block-thewrap_ads_250x300-0";
	line += "###block_advert";
	line += "###blog-ad";
	line += "###blog_ad_content";
	line += "###blog_ad_opa";
	line += "###blox-big-ad";
	line += "###blox-big-ad-bottom";
	line += "###blox-big-ad-top";
	line += "###blox-halfpage-ad";
	line += "###blox-tile-ad";
	line += "###blox-tower-ad";
	line += "###book-ad";
	line += "###botad";
	line += "###bott_ad2";
	line += "###bott_ad2_300";
	line += "###bottom-ad";
	line += "###bottom-ad-container";
	line += "###bottom-ads";
	line += "###bottomAd";
	line += "###bottomAdCCBucket";
	line += "###bottomAdContainer";
	line += "###bottomAdSense";
	line += "###bottomAdSenseDiv";
	line += "###bottomAds";
	line += "###bottomRightAd";
	line += "###bottomRightAdSpace";
	line += "###bottom_ad";
	line += "###bottom_ad_area";
	line += "###bottom_ads";
	line += "###bottom_banner_ad";
	line += "###bottom_overture";
	line += "###bottom_sponsor_ads";
	line += "###bottom_sponsored_links";
	line += "###bottom_text_ad";
	line += "###bottomad";
	line += "###bottomads";
	line += "###bottomadsense";
	line += "###bottomadwrapper";
	line += "###bottomleaderboardad";
	line += "###box-content-ad";
	line += "###box-googleadsense-1";
	line += "###box-googleadsense-r";
	line += "###box1ad";
	line += "###boxAd300";
	line += "###boxAdContainer";
	line += "###box_ad";
	line += "###box_mod_googleadsense";
	line += "###boxad1";
	line += "###boxad2";
	line += "###boxad3";
	line += "###boxad4";
	line += "###boxad5";
	line += "###bpAd";
	line += "###bps-header-ad-container";
	line += "###btr_horiz_ad";
	line += "###burn_header_ad";
	line += "###button-ads-horizontal";
	line += "###button-ads-vertical";
	line += "###buttonAdWrapper1";
	line += "###buttonAdWrapper2";
	line += "###buttonAds";
	line += "###buttonAdsContainer";
	line += "###button_ad_container";
	line += "###button_ad_wrap";
	line += "###buttonad";
	line += "###buy-sell-ads";
	line += "###c4ad-Middle1";
	line += "###caAdLarger";
	line += "###catad";
	line += "###cellAd";
	line += "###channel_ad";
	line += "###channel_ads";
	line += "###ciHomeRHSAdslot";
	line += "###circ_ad";
	line += "###cnnRR336ad";
	line += "###cnnTopAd";
	line += "###col3_advertising";
	line += "###colRightAd";
	line += "###collapseobj_adsection";
	line += "###column4-google-ads";
	line += "###commercial_ads";
	line += "###common_right_ad_wrapper";
	line += "###common_right_lower_ad_wrapper";
	line += "###common_right_lower_adspace";
	line += "###common_right_lower_player_ad_wrapper";
	line += "###common_right_lower_player_adspace";
	line += "###common_right_player_ad_wrapper";
	line += "###common_right_player_adspace";
	line += "###common_right_right_adspace";
	line += "###common_top_adspace";
	line += "###companion-ad";
	line += "###companionAdDiv";
	line += "###containerLocalAds";
	line += "###containerLocalAdsInner";
	line += "###containerMrecAd";
	line += "###content-ad-header";
	line += "###content-header-ad";
	line += "###contentAd";
	line += "###contentTopAds2";
	line += "~filestage.to###content_ad";
	line += "###content_ad_square";
	line += "###content_ad_top";
	line += "###content_ads_content";
	line += "###content_box_300body_sponsoredoffers";
	line += "###content_box_adright300_google";
	line += "###content_mpu";
	line += "###contentad";
	line += "###contentad_imtext";
	line += "###contentad_right";
	line += "###contentads";
	line += "###contentinlineAd";
	line += "###contextad";
	line += "###contextual-ads";
	line += "###contextual-ads-block";
	line += "###contextualad";
	line += "###coverads";
	line += "###ctl00_Adspace_Top_Height";
	line += "###ctl00_BottomAd";
	line += "###ctl00_ContentRightColumn_RightColumn_Ad1_BanManAd";
	line += "###ctl00_ContentRightColumn_RightColumn_PremiumAd1_ucBanMan_BanManAd";
	line += "###ctl00_LHTowerAd";
	line += "###ctl00_LeftHandAd";
	line += "###ctl00_MasterHolder_IBanner_adHolder";
	line += "###ctl00_TopAd";
	line += "###ctl00_TowerAd";
	line += "###ctl00_VBanner_adHolder";
	line += "###ctl00_abot_bb";
	line += "###ctl00_adFooter";
	line += "###ctl00_atop_bt";
	line += "###ctl00_cphMain_hlAd1";
	line += "###ctl00_cphMain_hlAd2";
	line += "###ctl00_cphMain_hlAd3";
	line += "###ctl00_ctl00_MainPlaceHolder_itvAdSkyscraper";
	line += "###ctl00_ctl00_ctl00_Main_Main_PlaceHolderGoogleTopBanner_MPTopBannerAd";
	line += "###ctl00_ctl00_ctl00_Main_Main_SideBar_MPSideAd";
	line += "###ctl00_ctl00_ctl00_tableAdsTop";
	line += "###ctl00_dlTilesAds";
	line += "###ctl00_m_skinTracker_m_adLBL";
	line += "###ctl00_phCrackerMain_ucAffiliateAdvertDisplayMiddle_pnlAffiliateAdvert";
	line += "###ctl00_phCrackerMain_ucAffiliateAdvertDisplayRight_pnlAffiliateAdvert";
	line += "###ctrlsponsored";
	line += "###cubeAd";
	line += "###cube_ads";
	line += "###cube_ads_inner";
	line += "###cubead";
	line += "###cubead-2";
	line += "###dItemBox_ads";
	line += "###dart_160x600";
	line += "###dc-display-right-ad-1";
	line += "###dcol-sponsored";
	line += "###defer-adright";
	line += "###detail_page_vid_topads";
	line += "~mtanyct.info###divAd";
	line += "###divAdBox";
	line += "###divMenuAds";
	line += "###divWNAdHeader";
	line += "###divWrapper_Ad";
	line += "###div_video_ads";
	line += "###dlads";
	line += "###dni-header-ad";
	line += "###dnn_ad_banner";
	line += "###download_ads";
	line += "###ds-mpu";
	line += "###editorsmpu";
	line += "###evotopTen_advert";
	line += "###ex-ligatus";
	line += "###exads";
	line += "~discuss.com.hk,~uwants.com###featuread";
	line += "###featured-advertisements";
	line += "###featuredAdContainer2";
	line += "###featuredAds";
	line += "###feed_links_ad_container";
	line += "###first-300-ad";
	line += "###first-adlayer";
	line += "###first_ad_unit";
	line += "###firstad";
	line += "###fl_hdrAd";
	line += "###flexiad";
	line += "###footer-ad";
	line += "###footer-advert";
	line += "###footer-adverts";
	line += "###footer-sponsored";
	line += "###footerAd";
	line += "###footerAdDiv";
	line += "###footerAds";
	line += "###footerAdvertisement";
	line += "###footerAdverts";
	line += "###footer_ad";
	line += "###footer_ad_01";
	line += "###footer_ad_block";
	line += "###footer_ad_container";
	line += "~investopedia.com###footer_ads";
	line += "###footer_adspace";
	line += "###footer_text_ad";
	line += "###footerad";
	line += "###fr_ad_center";
	line += "###frame_admain";
	line += "###frnAdSky";
	line += "###frnBannerAd";
	line += "###frnContentAd";
	line += "###from_our_sponsors";
	line += "###front_advert";
	line += "###front_mpu";
	line += "###ft-ad";
	line += "###ft-ad-1";
	line += "###ft-ad-container";
	line += "###ft_mpu";
	line += "###fusionad";
	line += "###fw-advertisement";
	line += "###g_ad";
	line += "###g_adsense";
	line += "###ga_300x250";
	line += "###gad";
	line += "###galleries-tower-ad";
	line += "###gallery-ad-m0";
	line += "###gallery_ads";
	line += "###game-info-ad";
	line += "###gasense";
	line += "###global_header_ad_area";
	line += "###gmi-ResourcePageAd";
	line += "###gmi-ResourcePageLowerAd";
	line += "###goads";
	line += "###google-ad";
	line += "###google-ad-art";
	line += "###google-ad-table-right";
	line += "###google-ad-tower";
	line += "###google-ads";
	line += "###google-ads-bottom";
	line += "###google-ads-header";
	line += "###google-ads-left-side";
	line += "###google-adsense-mpusize";
	line += "###googleAd";
	line += "###googleAds";
	line += "###googleAdsSml";
	line += "###googleAdsense";
	line += "###googleAdsenseBanner";
	line += "###googleAdsenseBannerBlog";
	line += "###googleAdwordsModule";
	line += "###googleAfcContainer";
	line += "###googleSearchAds";
	line += "###googleShoppingAdsRight";
	line += "###googleShoppingAdsTop";
	line += "###googleSubAds";
	line += "###google_ad";
	line += "###google_ad_container";
	line += "###google_ad_inline";
	line += "###google_ad_test";
	line += "###google_ads";
	line += "###google_ads_frame1";
	line += "###google_ads_frame1_anchor";
	line += "###google_ads_test";
	line += "###google_ads_top";
	line += "###google_adsense_home_468x60_1";
	line += "###googlead";
	line += "###googleadbox";
	line += "###googleads";
	line += "###googleadsense";
	line += "###googlesponsor";
	line += "###grid_ad";
	line += "###gsyadrectangleload";
	line += "###gsyadrightload";
	line += "###gsyadtop";
	line += "###gsyadtopload";
	line += "###gtopadvts";
	line += "###half-page-ad";
	line += "###halfPageAd";
	line += "###halfe-page-ad-box";
	line += "###hdtv_ad_ss";
	line += "~uwcu.org###head-ad";
	line += "###headAd";
	line += "###head_advert";
	line += "###headad";
	line += "###header-ad";
	line += "###header-ad-rectangle-container";
	line += "###header-ads";
	line += "###header-adspace";
	line += "###header-advert";
	line += "###header-advertisement";
	line += "###header-advertising";
	line += "###headerAd";
	line += "###headerAdBackground";
	line += "###headerAdContainer";
	line += "###headerAdWrap";
	line += "###headerAds";
	line += "###headerAdsWrapper";
	line += "###headerTopAd";
	line += "~cmt.com###header_ad";
	line += "###header_ad_728_90";
	line += "###header_ad_container";
	line += "###header_adcode";
	line += "###header_ads";
	line += "###header_advertisement_top";
	line += "###header_leaderboard_ad_container";
	line += "###header_publicidad";
	line += "###headerad";
	line += "###headeradbox";
	line += "###headerads";
	line += "###headeradwrap";
	line += "###headline_ad";
	line += "###headlinesAdBlock";
	line += "###hiddenadAC";
	line += "###hideads";
	line += "###hl-sponsored-results";
	line += "###homeTopRightAd";
	line += "###home_ad";
	line += "###home_bottom_ad";
	line += "###home_contentad";
	line += "###home_mpu";
	line += "###home_spensoredlinks";
	line += "###homepage-ad";
	line += "###homepageAdsTop";
	line += "###homepage_right_ad";
	line += "###homepage_right_ad_container";
	line += "###homepage_top_ads";
	line += "###hometop_234x60ad";
	line += "###hor_ad";
	line += "###horizontal-banner-ad";
	line += "###horizontal_ad";
	line += "###horizontal_ad_top";
	line += "###horizontalads";
	line += "###houseAd";
	line += "###hp-header-ad";
	line += "###hp-right-ad";
	line += "###hp-store-ad";
	line += "###hpV2_300x250Ad";
	line += "###hpV2_googAds";
	line += "###icePage_SearchLinks_AdRightDiv";
	line += "###icePage_SearchLinks_DownloadToolbarAdRightDiv";
	line += "###in_serp_ad";
	line += "###inadspace";
	line += "###indexad";
	line += "###inlinead";
	line += "###inlinegoogleads";
	line += "###inlist-ad-block";
	line += "###inner-advert-row";
	line += "###insider_ad_wrapper";
	line += "###instoryad";
	line += "###int-ad";
	line += "###interstitial_ad_wrapper";
	line += "###islandAd";
	line += "###j_ad";
	line += "###ji_medShowAdBox";
	line += "###jmp-ad-buttons";
	line += "###joead";
	line += "###joead2";
	line += "###ka_adRightSkyscraperWide";
	line += "###landing-adserver";
	line += "###largead";
	line += "###lateAd";
	line += "###layerTLDADSERV";
	line += "###lb-sponsor-left";
	line += "###lb-sponsor-right";
	line += "###leader-board-ad";
	line += "###leader-sponsor";
	line += "###leaderAdContainer";
	line += "###leader_board_ad";
	line += "###leaderad";
	line += "###leaderad_section";
	line += "###leaderboard-ad";
	line += "###leaderboard-bottom-ad";
	line += "###leaderboard_ad";
	line += "###left-ad-skin";
	line += "###left-lower-adverts";
	line += "###left-lower-adverts-container";
	line += "###leftAdContainer";
	line += "###leftAd_rdr";
	line += "###leftAdvert";
	line += "###leftSectionAd300-100";
	line += "###left_ad";
	line += "###left_adspace";
	line += "###leftad";
	line += "###leftads";
	line += "###lg-banner-ad";
	line += "###ligatus";
	line += "###linkAds";
	line += "###linkads";
	line += "###live-ad";
	line += "###longAdSpace";
	line += "###lowerAdvertisementImg";
	line += "###lowerads";
	line += "###lowerthirdad";
	line += "###lowertop-adverts";
	line += "###lowertop-adverts-container";
	line += "###lrecad";
	line += "###lsadvert-left_menu_1";
	line += "###lsadvert-left_menu_2";
	line += "###lsadvert-top";
	line += "###mBannerAd";
	line += "###main-ad";
	line += "###main-ad160x600";
	line += "###main-ad160x600-img";
	line += "###main-ad728x90";
	line += "###main-bottom-ad";
	line += "###mainAd";
	line += "###mainAdUnit";
	line += "###mainAdvert";
	line += "###main_ad";
	line += "###main_rec_ad";
	line += "###main_top_ad_container";
	line += "###marketing-promo";
	line += "###mastAdvert";
	line += "###mastad";
	line += "###mastercardAd";
	line += "###masthead_ad";
	line += "###masthead_topad";
	line += "###medRecAd";
	line += "###media_ad";
	line += "###mediumAdvertisement";
	line += "###medrectad";
	line += "###menuAds";
	line += "###mi_story_assets_ad";
	line += "###mid-ad300x250";
	line += "###mid-table-ad";
	line += "###midRightTextAds";
	line += "###mid_ad_div";
	line += "###mid_ad_title";
	line += "###mid_mpu";
	line += "###midadd";
	line += "###midadspace";
	line += "###middle-ad";
	line += "###middlead";
	line += "###middleads";
	line += "###midrect_ad";
	line += "###midstrip_ad";
	line += "###mini-ad";
	line += "###module-google_ads";
	line += "###module_ad";
	line += "###module_box_ad";
	line += "###module_sky_scraper";
	line += "###monsterAd";
	line += "###moogleAd";
	line += "###most_popular_ad";
	line += "###motionAd";
	line += "###mpu";
	line += "###mpu-advert";
	line += "###mpuAd";
	line += "###mpuDiv";
	line += "###mpuSlot";
	line += "###mpuWrapper";
	line += "###mpuWrapperAd";
	line += "###mpu_banner";
	line += "###mpu_holder";
	line += "###mpu_text_ad";
	line += "###mpuad";
	line += "###mrecAdContainer";
	line += "###ms_ad";
	line += "###msad";
	line += "###multiLinkAdContainer";
	line += "###myads_HeaderButton";
	line += "###n_sponsor_ads";
	line += "###namecom_ad_hosting_main";
	line += "###narrow_ad_unit";
	line += "###natadad300x250";
	line += "###national_microlink_ads";
	line += "###nationalad";
	line += "###navi_banner_ad_780";
	line += "###nba300Ad";
	line += "###nbaMidAds";
	line += "###nbaVid300Ad";
	line += "###new_topad";
	line += "###newads";
	line += "###ng_rtcol_ad";
	line += "###noresultsads";
	line += "###northad";
	line += "###oanda_ads";
	line += "###onespot-ads";
	line += "###online_ad";
	line += "###p-googleadsense";
	line += "###page-header-ad";
	line += "###pageAds";
	line += "###pageAdsDiv";
	line += "###page_content_top_ad";
	line += "###pagelet_adbox";
	line += "###panelAd";
	line += "###pb_report_ad";
	line += "###pcworldAdBottom";
	line += "###pcworldAdTop";
	line += "###pinball_ad";
	line += "###player-below-advert";
	line += "###player_ad";
	line += "###player_ads";
	line += "###pod-ad-video-page";
	line += "###populate_ad_bottom";
	line += "###populate_ad_left";
	line += "###portlet-advertisement-left";
	line += "###portlet-advertisement-right";
	line += "###post-promo-ad";
	line += "###post5_adbox";
	line += "###post_ad";
	line += "###premium_ad";
	line += "###priceGrabberAd";
	line += "###print_ads";
	line += "~bipbip.co.il###printads";
	line += "###product-adsense";
	line += "~flickr.com###promo-ad";
	line += "###promoAds";
	line += "###ps-vertical-ads";
	line += "###pub468x60";
	line += "###publicidad";
	line += "###pushdown_ad";
	line += "###qm-ad-big-box";
	line += "###qm-ad-sky";
	line += "###qm-dvdad";
	line += "###r1SoftAd";
	line += "###rail_ad1";
	line += "###rail_ad2";
	line += "###realEstateAds";
	line += "###rectAd";
	line += "###rect_ad";
	line += "###rectangle-ad";
	line += "###rectangle_ad";
	line += "###refine-300-ad";
	line += "###region-top-ad";
	line += "###rh-ad-container";
	line += "###rh_tower_ad";
	line += "###rhs_ads";
	line += "###rhsadvert";
	line += "###right-ad";
	line += "###right-ad-skin";
	line += "###right-ad-title";
	line += "###right-ads-3";
	line += "###right-box-ad";
	line += "###right-featured-ad";
	line += "###right-mpu-1-ad-container";
	line += "###right-uppder-adverts";
	line += "###right-uppder-adverts-container";
	line += "###rightAd";
	line += "###rightAd300x250";
	line += "###rightAdColumn";
	line += "###rightAd_rdr";
	line += "###rightColAd";
	line += "###rightColumnMpuAd";
	line += "###rightColumnSkyAd";
	line += "###right_ad";
	line += "###right_ad_wrapper";
	line += "###right_ads";
	line += "###right_advertisement";
	line += "###right_advertising";
	line += "###right_column_ads";
	line += "###rightad";
	line += "###rightadContainer";
	line += "###rightadvertbar-doubleclickads";
	line += "###rightbar-ad";
	line += "###rightside-ads";
	line += "###rightside_ad";
	line += "###righttop-adverts";
	line += "###righttop-adverts-container";
	line += "###rm_ad_text";
	line += "###ros_ad";
	line += "###rotatingads";
	line += "###row2AdContainer";
	line += "###rt-ad";
	line += "###rt-ad-top";
	line += "###rt-ad468";
	line += "###rtMod_ad";
	line += "###rtmod_ad";
	line += "###sAdsBox";
	line += "###sb-ad-sq";
	line += "###sb_advert";
	line += "###sb_sponsors";
	line += "###search-google-ads";
	line += "###searchAdSenseBox";
	line += "###searchAdSenseBoxAd";
	line += "###searchAdSkyscraperBox";
	line += "###search_ads";
	line += "###search_result_ad";
	line += "###second-adlayer";
	line += "###secondBoxAdContainer";
	line += "###section-container-ddc_ads";
	line += "###section-sponsors";
	line += "###section_advertorial_feature";
	line += "###servfail-ads";
	line += "###sew-ad1";
	line += "###shoppingads";
	line += "###show-ad";
	line += "###showAd";
	line += "###showad";
	line += "###side-ad";
	line += "###side-ad-container";
	line += "###sideAd";
	line += "###sideAdSub";
	line += "###sideBarAd";
	line += "###side_ad";
	line += "###side_ad_wrapper";
	line += "###side_ads_by_google";
	line += "###side_sky_ad";
	line += "###sidead";
	line += "###sideads";
	line += "###sidebar-125x125-ads";
	line += "###sidebar-125x125-ads-below-index";
	line += "###sidebar-ad";
	line += "###sidebar-ad-boxes";
	line += "###sidebar-ad-space";
	line += "###sidebar-ad-wrap";
	line += "###sidebar-ad3";
	line += "~gaelick.com###sidebar-ads";
	line += "###sidebar2ads";
	line += "###sidebar_ad_widget";
	line += "~facebook.com,~japantoday.com###sidebar_ads";
	line += "###sidebar_ads_180";
	line += "###sidebar_sponsoredresult_body";
	line += "###sidebarad";
	line += "###sideline-ad";
	line += "###single-mpu";
	line += "###singlead";
	line += "###site-leaderboard-ads";
	line += "###site_top_ad";
	line += "###sitead";
	line += "###sky-ad";
	line += "###skyAd";
	line += "###skyAdContainer";
	line += "###skyScrapperAd";
	line += "###skyWrapperAds";
	line += "###sky_ad";
	line += "###sky_advert";
	line += "###skyads";
	line += "###skyscraper-ad";
	line += "###skyscraperAd";
	line += "###skyscraperAdContainer";
	line += "###skyscraper_ad";
	line += "###skyscraper_advert";
	line += "###skyscraperad";
	line += "###sliderAdHolder";
	line += "###slideshow_ad_300x250";
	line += "###sm-banner-ad";
	line += "###small_ad";
	line += "###smallerAd";
	line += "###specials_ads";
	line += "###speeds_ads";
	line += "###speeds_ads_fstitem";
	line += "###speedtest_mrec_ad";
	line += "###sphereAd";
	line += "###splinks";
	line += "###sponLinkDiv_1";
	line += "###sponlink";
	line += "###sponsAds";
	line += "###sponsLinks";
	line += "###spons_left";
	line += "###sponseredlinks";
	line += "###sponsor-search";
	line += "###sponsorAd1";
	line += "###sponsorAd2";
	line += "###sponsorAdDiv";
	line += "###sponsorLinks";
	line += "###sponsorTextLink";
	line += "###sponsor_banderole";
	line += "###sponsor_box";
	line += "###sponsor_deals";
	line += "###sponsor_panSponsor";
	line += "###sponsor_recommendations";
	line += "###sponsorbar";
	line += "###sponsorbox";
	line += "~hollywood.com,~worldsbestbars.com###sponsored";
	line += "###sponsored-ads";
	line += "###sponsored-features";
	line += "###sponsored-links";
	line += "###sponsored-resources";
	line += "###sponsored1";
	line += "###sponsoredBox1";
	line += "###sponsoredBox2";
	line += "###sponsoredLinks";
	line += "###sponsoredList";
	line += "###sponsoredResults";
	line += "###sponsoredSiteMainline";
	line += "###sponsoredSiteSidebar";
	line += "###sponsored_ads_v4";
	line += "###sponsored_content";
	line += "###sponsored_game_row_listing";
	line += "###sponsored_links";
	line += "###sponsored_v12";
	line += "###sponsoredlinks";
	line += "###sponsoredlinks_cntr";
	line += "###sponsoredresults_top";
	line += "###sponsoredwellcontainerbottom";
	line += "###sponsoredwellcontainertop";
	line += "###sponsorlink";
	line += "###sponsors";
	line += "###sponsors_top_container";
	line += "###sponsorshipBadge";
	line += "###spotlightAds";
	line += "###spotlightad";
	line += "###sqAd";
	line += "###square-sponsors";
	line += "###squareAd";
	line += "###squareAdSpace";
	line += "###squareAds";
	line += "###square_ad";
	line += "###start_middle_container_advertisment";
	line += "###sticky-ad";
	line += "###stickyBottomAd";
	line += "###story-ad-a";
	line += "###story-ad-b";
	line += "###story-leaderboard-ad";
	line += "###story-sponsoredlinks";
	line += "###storyAd";
	line += "###storyAdWrap";
	line += "###storyad2";
	line += "###subpage-ad-right";
	line += "###subpage-ad-top";
	line += "###swads";
	line += "###synch-ad";
	line += "###systemad_background";
	line += "###tabAdvertising";
	line += "###takeoverad";
	line += "###tblAd";
	line += "###tbl_googlead";
	line += "###tcwAd";
	line += "###template_ad_leaderboard";
	line += "###tertiary_advertising";
	line += "###text-ad";
	line += "###text-ads";
	line += "###textAd";
	line += "###textAds";
	line += "###text_ad";
	line += "###text_ads";
	line += "###text_advert";
	line += "###textad";
	line += "###textad3";
	line += "###the-last-ad-standing";
	line += "###thefooterad";
	line += "###themis-ads";
	line += "###tile-ad";
	line += "###tmglBannerAd";
	line += "###top-ad";
	line += "###top-ad-container";
	line += "###top-ad-menu";
	line += "###top-ads";
	line += "###top-ads-tabs";
	line += "###top-advertisement";
	line += "###top-banner-ad";
	line += "###top-search-ad-wrapper";
	line += "###topAd";
	line += "###topAd728x90";
	line += "###topAdBanner";
	line += "###topAdContainer";
	line += "###topAdSenseDiv";
	line += "###topAdcontainer";
	line += "###topAds";
	line += "###topAdsContainer";
	line += "###topAdvert";
	line += "~neowin.net###topBannerAd";
	line += "###topNavLeaderboardAdHolder";
	line += "###topRightBlockAdSense";
	line += "~morningstar.se###top_ad";
	line += "###top_ad_area";
	line += "###top_ad_game";
	line += "###top_ad_wrapper";
	line += "###top_ads";
	line += "###top_advertise";
	line += "###top_advertising";
	line += "###top_right_ad";
	line += "###top_wide_ad";
	line += "~bumpshack.com###topad";
	line += "###topad_left";
	line += "###topad_right";
	line += "###topadblock";
	line += "###topaddwide";
	line += "###topads";
	line += "###topadsense";
	line += "###topadspace";
	line += "###topadzone";
	line += "###topcustomad";
	line += "###topleaderboardad";
	line += "###toprightAdvert";
	line += "###toprightad";
	line += "###topsponsored";
	line += "###toptextad";
	line += "###towerad";
	line += "###ttp_ad_slot1";
	line += "###ttp_ad_slot2";
	line += "###twogamesAd";
	line += "###txt_link_ads";
	line += "###undergameAd";
	line += "###upperAdvertisementImg";
	line += "###upperMpu";
	line += "###upperad";
	line += "###urban_contentad_1";
	line += "###urban_contentad_2";
	line += "###urban_contentad_article";
	line += "###v_ad";
	line += "###vert_ad";
	line += "###vert_ad_placeholder";
	line += "###vertical_ad";
	line += "###vertical_ads";
	line += "###videoAd";
	line += "###video_cnv_ad";
	line += "###video_overlay_ad";
	line += "###videoadlogo";
	line += "###viewportAds";
	line += "###walltopad";
	line += "###weblink_ads_container";
	line += "###welcomeAdsContainer";
	line += "###welcome_ad_mrec";
	line += "###welcome_advertisement";
	line += "###wf_ContentAd";
	line += "###wf_FrontSingleAd";
	line += "###wf_SingleAd";
	line += "###wf_bottomContentAd";
	line += "###wgtAd";
	line += "###whatsnews_top_ad";
	line += "###whitepaper-ad";
	line += "###whoisRightAdContainer";
	line += "###wide_ad_unit_top";
	line += "###widget_advertisement";
	line += "###wrapAdRight";
	line += "###wrapAdTop";
	line += "###y-ad-units";
	line += "###y708-ad-expedia";
	line += "###y708-ad-lrec";
	line += "###y708-ad-partners";
	line += "###y708-ad-ysm";
	line += "###y708-advertorial-marketplace";
	line += "###yahoo-ads";
	line += "###yahoo-sponsors";
	line += "###yahooSponsored";
	line += "###yahoo_ads";
	line += "###yahoo_ads_2010";
	line += "###yahooad-tbl";
	line += "###yan-sponsored";
	line += "###ybf-ads";
	line += "###yfi_fp_ad_mort";
	line += "###yfi_fp_ad_nns";
	line += "###yfi_pf_ad_mort";
	line += "###ygrp-sponsored-links";
	line += "###ymap_adbanner";
	line += "###yn-gmy-ad-lrec";
	line += "###yreSponsoredLinks";
	line += "###ysm_ad_iframe";
	line += "###zoneAdserverMrec";
	line += "###zoneAdserverSuper";
	line += "##.ADBAR";
	line += "##.ADPod";
	line += "##.AD_ALBUM_ITEMLIST";
	line += "##.AD_MOVIE_ITEM";
	line += "##.AD_MOVIE_ITEMLIST";
	line += "##.AD_MOVIE_ITEMROW";
	line += "##.Ad-MPU";
	line += "##.Ad120x600";
	line += "##.Ad160x600";
	line += "##.Ad160x600left";
	line += "##.Ad160x600right";
	line += "##.Ad247x90";
	line += "##.Ad300x250";
	line += "##.Ad300x250L";
	line += "##.Ad728x90";
	line += "##.AdBorder";
	line += "~co-operative.coop##.AdBox";
	line += "##.AdBox7";
	line += "##.AdContainerBox308";
	line += "##.AdHeader";
	line += "##.AdHere";
	line += "~backpage.com##.AdInfo";
	line += "##.AdMedium";
	line += "##.AdPlaceHolder";
	line += "##.AdRingtone";
	line += "##.AdSense";
	line += "##.AdSpace";
	line += "##.AdTextSmallFont";
	line += "~buy.com,~superbikeplanet.com##.AdTitle";
	line += "##.AdUnit";
	line += "##.AdUnit300";
	line += "##.Ad_C";
	line += "##.Ad_D_Wrapper";
	line += "##.Ad_E_Wrapper";
	line += "##.Ad_Right";
	line += "~thecoolhunter.net##.Ads";
	line += "##.AdsBoxBottom";
	line += "##.AdsBoxSection";
	line += "##.AdsBoxTop";
	line += "##.AdsLinks1";
	line += "##.AdsLinks2";
	line += "~swanseacity.net,~wrexhamafc.co.uk##.Advert";
	line += "##.AdvertMidPage";
	line += "##.AdvertiseWithUs";
	line += "##.AdvertisementTextTag";
	line += "##.ArticleAd";
	line += "##.ArticleInlineAd";
	line += "##.BannerAd";
	line += "##.BigBoxAd";
	line += "##.BlockAd";
	line += "##.BottomAdContainer";
	line += "##.BottomAffiliate";
	line += "##.BoxAd";
	line += "##.CG_adkit_leaderboard";
	line += "##.CG_details_ad_dropzone";
	line += "##.ComAread";
	line += "##.CommentAd";
	line += "##.ContentAd";
	line += "##.ContentAds";
	line += "##.DAWRadvertisement";
	line += "##.DeptAd";
	line += "##.DisplayAd";
	line += "##.FT_Ad";
	line += "##.FlatAds";
	line += "##.GOOGLE_AD";
	line += "##.GoogleAd";
	line += "##.HPNewAdsBannerDiv";
	line += "##.HPRoundedAd";
	line += "##.HomeContentAd";
	line += "##.IABAdSpace";
	line += "##.IndexRightAd";
	line += "##.LazyLoadAd";
	line += "##.LeftAd";
	line += "##.LeftTowerAd";
	line += "##.M2Advertisement";
	line += "##.MD_adZone";
	line += "##.MOS-ad-hack";
	line += "##.MPU";
	line += "##.MPUHolder";
	line += "##.MPUTitleWrapperClass";
	line += "##.MiddleAd";
	line += "##.MiddleAdContainer";
	line += "##.OpenXad";
	line += "##.PU_DoubleClickAdsContent";
	line += "##.Post5ad";
	line += "##.RBboxAd";
	line += "##.RectangleAd";
	line += "##.RelatedAds";
	line += "##.RightAd1";
	line += "##.RightGoogleAFC";
	line += "##.RightRailTop300x250Ad";
	line += "##.RightSponsoredAdTitle";
	line += "##.RightTowerAd";
	line += "##.SideAdCol";
	line += "##.SidebarAd";
	line += "##.SitesGoogleAdsModule";
	line += "##.SkyAdContainer";
	line += "##.SponsorCFrame";
	line += "##.SponsoredAdTitle";
	line += "##.SponsoredContent";
	line += "##.SponsoredLinks";
	line += "##.SponsoredLinksGrayBox";
	line += "##.SponsorshipText";
	line += "##.SquareAd";
	line += "##.StandardAdLeft";
	line += "##.StandardAdRight";
	line += "##.TextAd";
	line += "##.TheEagleGoogleAdSense300x250";
	line += "##.TopAd";
	line += "##.TopAdContainer";
	line += "##.TopAdL";
	line += "##.TopAdR";
	line += "##.TopBannerAd";
	line += "##.UIStandardFrame_SidebarAds";
	line += "##.UIWashFrame_SidebarAds";
	line += "##.UnderAd";
	line += "##.VerticalAd";
	line += "##.VideoAd";
	line += "##.WidgetAdvertiser";
	line += "##.a160x600";
	line += "##.a728x90";
	line += "##.ad-120x600";
	line += "##.ad-160";
	line += "##.ad-160x600";
	line += "##.ad-250";
	line += "##.ad-300";
	line += "##.ad-300-block";
	line += "##.ad-300-blog";
	line += "##.ad-300x100";
	line += "##.ad-300x250";
	line += "##.ad-300x250-right0";
	line += "##.ad-350";
	line += "##.ad-355x75";
	line += "##.ad-600";
	line += "##.ad-635x40";
	line += "##.ad-728";
	line += "##.ad-728x90";
	line += "##.ad-728x90-1";
	line += "##.ad-728x90_forum";
	line += "##.ad-above-header";
	line += "##.ad-adlink-bottom";
	line += "##.ad-adlink-side";
	line += "##.ad-background";
	line += "##.ad-banner";
	line += "##.ad-bigsize";
	line += "##.ad-block";
	line += "##.ad-blog2biz";
	line += "##.ad-bottom";
	line += "##.ad-box";
	line += "##.ad-break";
	line += "##.ad-btn";
	line += "##.ad-btn-heading";
	line += "~assetbar.com##.ad-button";
	line += "##.ad-cell";
	line += "~arbetsformedlingen.se##.ad-container";
	line += "##.ad-disclaimer";
	line += "##.ad-display";
	line += "##.ad-div";
	line += "##.ad-enabled";
	line += "##.ad-feedback";
	line += "##.ad-filler";
	line += "##.ad-footer";
	line += "##.ad-footer-leaderboard";
	line += "##.ad-google";
	line += "##.ad-graphic-large";
	line += "##.ad-gray";
	line += "##.ad-hdr";
	line += "##.ad-head";
	line += "##.ad-holder";
	line += "##.ad-homeleaderboard";
	line += "##.ad-img";
	line += "##.ad-island";
	line += "##.ad-label";
	line += "##.ad-leaderboard";
	line += "##.ad-links";
	line += "##.ad-lrec";
	line += "##.ad-medium";
	line += "##.ad-medium-two";
	line += "##.ad-mpu";
	line += "##.ad-note";
	line += "##.ad-notice";
	line += "##.ad-other";
	line += "##.ad-permalink";
	line += "##.ad-placeholder";
	line += "##.ad-postText";
	line += "##.ad-poster";
	line += "##.ad-priority";
	line += "##.ad-rect";
	line += "##.ad-rectangle";
	line += "##.ad-rectangle-text";
	line += "##.ad-related";
	line += "##.ad-rh";
	line += "##.ad-ri";
	line += "##.ad-right";
	line += "##.ad-right-header";
	line += "##.ad-right-txt";
	line += "##.ad-row";
	line += "##.ad-section";
	line += "~ifokus.se##.ad-sidebar";
	line += "##.ad-sidebar-outer";
	line += "##.ad-sidebar300";
	line += "##.ad-sky";
	line += "##.ad-slot";
	line += "##.ad-slot-234-60";
	line += "##.ad-slot-300-250";
	line += "##.ad-slot-728-90";
	line += "##.ad-space";
	line += "##.ad-space-mpu-box";
	line += "##.ad-spot";
	line += "##.ad-squares";
	line += "##.ad-statement";
	line += "##.ad-tabs";
	line += "##.ad-text";
	line += "##.ad-text-links";
	line += "##.ad-tile";
	line += "##.ad-title";
	line += "##.ad-top";
	line += "##.ad-top-left";
	line += "##.ad-unit";
	line += "##.ad-unit-300";
	line += "##.ad-unit-300-wrapper";
	line += "##.ad-unit-anchor";
	line += "##.ad-vert";
	line += "##.ad-vtu";
	line += "##.ad-wrap";
	line += "##.ad-wrapper";
	line += "##.ad-zone-s-q-l";
	line += "##.ad.super";
	line += "##.ad0";
	line += "##.ad1";
	line += "##.ad10";
	line += "##.ad120";
	line += "##.ad120x600";
	line += "##.ad125";
	line += "##.ad160";
	line += "##.ad160x600";
	line += "##.ad18";
	line += "##.ad19";
	line += "##.ad2";
	line += "##.ad21";
	line += "##.ad250";
	line += "##.ad250c";
	line += "##.ad3";
	line += "##.ad300";
	line += "##.ad300250";
	line += "##.ad300_250";
	line += "##.ad300x100";
	line += "##.ad300x250";
	line += "##.ad300x250-hp-features";
	line += "##.ad300x250Top";
	line += "##.ad300x250_container";
	line += "##.ad300x250box";
	line += "##.ad300x50-right";
	line += "##.ad300x600";
	line += "##.ad310";
	line += "##.ad336x280";
	line += "##.ad343x290";
	line += "##.ad4";
	line += "##.ad400right";
	line += "##.ad450";
	line += "~itavisen.no##.ad468";
	line += "##.ad468_60";
	line += "##.ad468x60";
	line += "##.ad6";
	line += "##.ad620x70";
	line += "##.ad626X35";
	line += "##.ad7";
	line += "##.ad728";
	line += "##.ad728_90";
	line += "##.ad728x90";
	line += "##.ad728x90_container";
	line += "##.ad8";
	line += "##.ad90x780";
	line += "##.adAgate";
	line += "##.adArea674x60";
	line += "##.adBanner";
	line += "##.adBanner300x250";
	line += "##.adBanner728x90";
	line += "##.adBannerTyp1";
	line += "##.adBannerTypSortableList";
	line += "##.adBannerTypW300";
	line += "##.adBar";
	line += "##.adBgBottom";
	line += "##.adBgMId";
	line += "##.adBgTop";
	line += "##.adBlock";
	line += "##.adBottomboxright";
	line += "~ksl.com##.adBox";
	line += "##.adBoxBody";
	line += "##.adBoxBorder";
	line += "##.adBoxContent";
	line += "##.adBoxInBignews";
	line += "##.adBoxSidebar";
	line += "##.adBoxSingle";
	line += "##.adCMRight";
	line += "##.adColumn";
	line += "##.adCont";
	line += "##.adContTop";
	line += "~mycareer.com.au,~nytimes.com##.adContainer";
	line += "##.adContour";
	line += "##.adCreative";
	line += "~superbikeplanet.com##.adDiv";
	line += "~contracostatimes.com,~mercurynews.com,~siliconvalley.com##.adElement";
	line += "##.adFender3";
	line += "##.adFrame";
	line += "##.adFtr";
	line += "##.adFullWidthMiddle";
	line += "##.adGoogle";
	line += "##.adHeader";
	line += "##.adHeadline";
	line += "~superhry.cz##.adHolder";
	line += "##.adHome300x250";
	line += "##.adHorisontal";
	line += "##.adInNews";
	line += "##.adLabel";
	line += "##.adLeader";
	line += "##.adLeaderForum";
	line += "##.adLeaderboard";
	line += "##.adLeft";
	line += "##.adLoaded";
	line += "##.adLocal";
	line += "##.adMastheadLeft";
	line += "##.adMastheadRight";
	line += "##.adMegaBoard";
	line += "##.adMkt2Colw";
	line += "~outspark.com##.adModule";
	line += "##.adMpu";
	line += "##.adNewsChannel";
	line += "##.adNoOutline";
	line += "##.adNotice";
	line += "##.adNoticeOut";
	line += "##.adObj";
	line += "##.adPageBorderL";
	line += "##.adPageBorderR";
	line += "##.adPanel";
	line += "##.adRect";
	line += "##.adRight";
	line += "##.adSelfServiceAdvertiseLink";
	line += "##.adServer";
	line += "##.adSkyscraperHolder";
	line += "##.adSlot";
	line += "##.adSpBelow";
	line += "~o2online.de##.adSpace";
	line += "##.adSpacer";
	line += "##.adSponsor";
	line += "##.adSpot";
	line += "##.adSpot-searchAd";
	line += "##.adSpot-textBox";
	line += "##.adSpot-twin";
	line += "##.adSpotIsland";
	line += "##.adSquare";
	line += "~marktplaats.nl##.adSummary";
	line += "##.adSuperboard";
	line += "##.adSupertower";
	line += "##.adTD";
	line += "##.adTab";
	line += "##.adTag";
	line += "~bipbip.co.il##.adText";
	line += "##.adTileWrap";
	line += "##.adTiler";
	line += "~ksl.com,~stadtlist.de,~superbikeplanet.com##.adTitle";
	line += "##.adTopboxright";
	line += "##.adTout";
	line += "##.adTxt";
	line += "##.adUnitHorz";
	line += "##.adUnitVert";
	line += "##.adUnitVert_noImage";
	line += "##.adWebBoard";
	line += "##.adWidget";
	line += "##.adWithTab";
	line += "##.adWrap";
	line += "##.adWrapper";
	line += "##.ad_0";
	line += "##.ad_1";
	line += "##.ad_120x90";
	line += "##.ad_125";
	line += "##.ad_130x90";
	line += "##.ad_160";
	line += "##.ad_160x600";
	line += "##.ad_2";
	line += "##.ad_200";
	line += "##.ad_200x200";
	line += "##.ad_250x250";
	line += "##.ad_250x250_w";
	line += "##.ad_3";
	line += "##.ad_300";
	line += "##.ad_300_250";
	line += "##.ad_300x250";
	line += "##.ad_300x250_box_right";
	line += "##.ad_336";
	line += "##.ad_336x280";
	line += "##.ad_350x100";
	line += "##.ad_350x250";
	line += "##.ad_400x200";
	line += "##.ad_468";
	line += "##.ad_468x60";
	line += "##.ad_600";
	line += "##.ad_728";
	line += "##.ad_728x90";
	line += "##.ad_Left";
	line += "~nirmaltv.com##.ad_Right";
	line += "##.ad_amazon";
	line += "##.ad_banner";
	line += "##.ad_banner_border";
	line += "##.ad_biz";
	line += "##.ad_block_338";
	line += "##.ad_body";
	line += "##.ad_border";
	line += "##.ad_botbanner";
	line += "##.ad_bottom_leaderboard";
	line += "##.ad_box";
	line += "##.ad_box2";
	line += "##.ad_box_ad";
	line += "##.ad_box_div";
	line += "##.ad_callout";
	line += "##.ad_caption";
	line += "##.ad_contain";
	line += "##.ad_container";
	line += "~salon.com##.ad_content";
	line += "##.ad_content_wide";
	line += "##.ad_contents";
	line += "##.ad_descriptor";
	line += "##.ad_eyebrow";
	line += "##.ad_footer";
	line += "##.ad_framed";
	line += "##.ad_front_promo";
	line += "##.ad_head";
	line += "~news.yahoo.com,~speurders.nl##.ad_header";
	line += "##.ad_hpm";
	line += "##.ad_info_block";
	line += "##.ad_inline";
	line += "##.ad_island";
	line += "##.ad_label";
	line += "##.ad_launchpad";
	line += "##.ad_leader";
	line += "##.ad_leaderboard";
	line += "##.ad_left";
	line += "~leboncoin.fr##.ad_links";
	line += "##.ad_linkunit";
	line += "##.ad_loc";
	line += "##.ad_lrec";
	line += "##.ad_main";
	line += "##.ad_medrec";
	line += "##.ad_medrect";
	line += "##.ad_middle";
	line += "##.ad_mpu";
	line += "##.ad_mr";
	line += "##.ad_mrec";
	line += "##.ad_mrec_title_article";
	line += "##.ad_mrect";
	line += "##.ad_news";
	line += "##.ad_notice";
	line += "##.ad_one";
	line += "##.ad_p360";
	line += "##.ad_partner";
	line += "##.ad_partners";
	line += "##.ad_plus";
	line += "##.ad_post";
	line += "##.ad_power";
	line += "##.ad_rectangle";
	line += "~didaktik-der-mathematik.de##.ad_right";
	line += "##.ad_right_col";
	line += "##.ad_row";
	line += "##.ad_sidebar";
	line += "##.ad_skyscraper";
	line += "##.ad_slug";
	line += "##.ad_slug_table";
	line += "~chinapost.com.tw##.ad_space";
	line += "##.ad_space_300_250";
	line += "##.ad_sponsor";
	line += "##.ad_sponsoredsection";
	line += "##.ad_spot_b";
	line += "##.ad_spot_c";
	line += "##.ad_square_r";
	line += "##.ad_square_top";
	line += "~bbs.newhua.com,~leboncoin.fr##.ad_text";
	line += "##.ad_text_w";
	line += "##.ad_title";
	line += "##.ad_top";
	line += "##.ad_top_leaderboard";
	line += "##.ad_topright";
	line += "##.ad_tower";
	line += "##.ad_unit";
	line += "##.ad_unit_rail";
	line += "##.ad_url";
	line += "##.ad_warning";
	line += "##.ad_wid300";
	line += "##.ad_wide";
	line += "##.ad_wrap";
	line += "##.ad_wrapper";
	line += "##.ad_wrapper_fixed";
	line += "##.ad_wrapper_top";
	line += "##.ad_zone";
	line += "##.adarea";
	line += "##.adarea-long";
	line += "##.adbanner";
	line += "##.adbannerbox";
	line += "##.adbannerright";
	line += "##.adbar";
	line += "##.adbg";
	line += "##.adborder";
	line += "##.adbot";
	line += "##.adbottom";
	line += "##.adbottomright";
	line += "~bodybuilding.com,~gametop.com,~lidovky.cz,~nordea.fi##.adbox";
	line += "##.adbox-outer";
	line += "##.adbox_300x600";
	line += "##.adbox_366x280";
	line += "##.adbox_468X60";
	line += "##.adbox_bottom";
	line += "##.adboxclass";
	line += "##.adbuttons";
	line += "##.adcode";
	line += "##.adcol1";
	line += "##.adcol2";
	line += "##.adcolumn";
	line += "##.adcolumn_wrapper";
	line += "~subito.it##.adcont";
	line += "##.adcopy";
	line += "~superbikeplanet.com##.addiv";
	line += "##.adfoot";
	line += "##.adfootbox";
	line += "~linux.com##.adframe";
	line += "##.adhead";
	line += "##.adheader";
	line += "##.adheader100";
	line += "##.adhere";
	line += "##.adhered";
	line += "##.adhi";
	line += "##.adhint";
	line += "~northjersey.com##.adholder";
	line += "##.adhoriz";
	line += "##.adi";
	line += "##.adiframe";
	line += "~backpage.com##.adinfo";
	line += "##.adinside";
	line += "##.adintro";
	line += "##.adjlink";
	line += "##.adkit";
	line += "##.adkit-advert";
	line += "##.adkit-lb-footer";
	line += "##.adlabel-horz";
	line += "##.adlabel-vert";
	line += "~gmhightechperformance.com,~hotrod.com,~miloyski.com,~superchevy.com##.adleft";
	line += "##.adleft1";
	line += "##.adline";
	line += "~superbikeplanet.com##.adlink";
	line += "##.adlinks";
	line += "~bipbip.co.il##.adlist";
	line += "##.adlnklst";
	line += "##.admarker";
	line += "##.admedrec";
	line += "##.admessage";
	line += "##.admodule";
	line += "##.admpu";
	line += "##.adnation-banner";
	line += "##.adnotice";
	line += "##.adops";
	line += "##.adp-AdPrefix";
	line += "##.adpadding";
	line += "##.adpane";
	line += "~bipbip.co.il,~quoka.de##.adpic";
	line += "##.adprice";
	line += "~tomwans.com##.adright";
	line += "##.adroot";
	line += "##.adrotate_widget";
	line += "##.adrow";
	line += "##.adrow-post";
	line += "##.adrule";
	line += "##.ads-125";
	line += "##.ads-728x90-wrap";
	line += "##.ads-banner";
	line += "##.ads-below-content";
	line += "##.ads-categories-bsa";
	line += "##.ads-favicon";
	line += "##.ads-links-general";
	line += "##.ads-mpu";
	line += "##.ads-profile";
	line += "##.ads-right";
	line += "##.ads-section";
	line += "##.ads-sidebar";
	line += "##.ads-sky";
	line += "##.ads-stripe";
	line += "##.ads-text";
	line += "##.ads-widget-partner-gallery";
	line += "##.ads2";
	line += "##.ads3";
	line += "##.ads300";
	line += "##.ads468";
	line += "##.ads728";
	line += "~amusingplanet.com,~bakeca.it,~chw.net,~cub.com,~joinmyband.co.uk,~lets-sell.info,~najauto.pl,~repubblica.it,~tonprenom.com##.ads:not(body)";
	line += "##.adsArea";
	line += "##.adsBelowHeadingNormal";
	line += "##.adsBlock";
	line += "##.adsBox";
	line += "##.adsCont";
	line += "##.adsDiv";
	line += "##.adsFull";
	line += "##.adsImages";
	line += "##.adsMPU";
	line += "##.adsRight";
	line += "##.adsTextHouse";
	line += "##.adsTop";
	line += "##.adsTower2";
	line += "##.adsTowerWrap";
	line += "##.adsWithUs";
	line += "##.ads_125_square";
	line += "##.ads_180";
	line += "##.ads_300";
	line += "##.ads_300x250";
	line += "##.ads_337x280";
	line += "##.ads_728x90";
	line += "##.ads_big";
	line += "##.ads_big-half";
	line += "##.ads_brace";
	line += "##.ads_catDiv";
	line += "##.ads_container";
	line += "##.ads_disc_anchor";
	line += "##.ads_disc_leader";
	line += "##.ads_disc_lwr_square";
	line += "##.ads_disc_skyscraper";
	line += "##.ads_disc_square";
	line += "##.ads_div";
	line += "##.ads_header";
	line += "##.ads_leaderboard";
	line += "##.ads_mpu";
	line += "##.ads_outer";
	line += "##.ads_rectangle";
	line += "##.ads_right";
	line += "##.ads_sc_bl_i";
	line += "##.ads_sc_tl_i";
	line += "##.ads_show_if";
	line += "##.ads_side";
	line += "##.ads_sidebar";
	line += "##.ads_singlepost";
	line += "##.ads_spacer";
	line += "##.ads_takeover";
	line += "##.ads_title";
	line += "##.ads_tr";
	line += "##.ads_widesky";
	line += "##.ads_wrapperads_top";
	line += "##.adsblockvert";
	line += "##.adsborder";
	line += "##.adsbottom";
	line += "##.adsbyyahoo";
	line += "##.adsc";
	line += "##.adscaleAdvert";
	line += "##.adsclick";
	line += "##.adscontainer";
	line += "##.adscreen";
	line += "##.adsection_a2";
	line += "##.adsection_c2";
	line += "~lalsace.fr,~lepays.fr,~tonprenom.com##.adsense";
	line += "##.adsense-ad";
	line += "##.adsense-category";
	line += "##.adsense-category-bottom";
	line += "##.adsense-heading";
	line += "##.adsense-post";
	line += "##.adsense-right";
	line += "##.adsense-title";
	line += "##.adsense3";
	line += "##.adsenseAds";
	line += "##.adsenseBlock";
	line += "##.adsenseContainer";
	line += "##.adsenseGreenBox";
	line += "##.adsense_bdc_v2";
	line += "##.adsensebig";
	line += "##.adsenseblock";
	line += "##.adsenseblock_bottom";
	line += "##.adsenseblock_top";
	line += "##.adsenselr";
	line += "##.adsensem_widget";
	line += "##.adsensesq";
	line += "##.adsenvelope";
	line += "##.adset";
	line += "##.adsforums";
	line += "##.adsghori";
	line += "##.adsgvert";
	line += "##.adside";
	line += "##.adsidebox";
	line += "##.adsider";
	line += "##.adsingle";
	line += "##.adsleft";
	line += "##.adslogan";
	line += "##.adsmalltext";
	line += "##.adsmessage";
	line += "##.adspace";
	line += "##.adspace-MR";
	line += "##.adspace180";
	line += "##.adspace_bottom";
	line += "##.adspace_buysell";
	line += "##.adspace_rotate";
	line += "##.adspace_skyscraper";
	line += "##.adspacer";
	line += "##.adspot";
	line += "##.adspot728x90";
	line += "##.adstextpad";
	line += "##.adstop";
	line += "##.adstrip";
	line += "~rinkworks.com##.adtable";
	line += "##.adtag";
	line += "##.adtech";
	line += "~anzwers.com.au,~bipbip.co.il,~ksl.com,~quoka.de,~u-file.net##.adtext";
	line += "##.adtext_gray";
	line += "##.adtext_horizontal";
	line += "##.adtext_onwhite";
	line += "##.adtext_vertical";
	line += "##.adtile";
	line += "##.adtips";
	line += "##.adtips1";
	line += "##.adtop";
	line += "##.adtravel";
	line += "##.adtxt";
	line += "##.adv-mpu";
	line += "##.adver";
	line += "##.adverTag";
	line += "##.adver_cont_below";
	line += "~beginyouridea.com,~irr.ru,~jobs.wa.gov.au,~manxtelecom.com,~storegate.co.uk,~storegate.com,~storegate.se,~swanseacity.net,~toonzaki.com,~travelblog.dailymail.co.uk,~tu-chemnitz.de,~wrexhamafc.co.uk,~yourvids.nl##.advert";
	line += "##.advert-article-bottom";
	line += "##.advert-bannerad";
	line += "##.advert-box";
	line += "##.advert-head";
	line += "~mobifrance.com##.advert-horizontal";
	line += "##.advert-iab-300-250";
	line += "##.advert-iab-468-60";
	line += "##.advert-mpu";
	line += "##.advert-skyscraper";
	line += "##.advert-text";
	line += "##.advert300";
	line += "##.advert4";
	line += "##.advert5";
	line += "##.advert8";
	line += "##.advertColumn";
	line += "##.advertCont";
	line += "##.advertContainer";
	line += "##.advertHeadline";
	line += "##.advertRight";
	line += "##.advertText";
	line += "##.advertTitleSky";
	line += "##.advert_468x60";
	line += "##.advert_box";
	line += "##.advert_cont";
	line += "##.advert_label";
	line += "##.advert_leaderboard";
	line += "~browsershots.org##.advert_list";
	line += "##.advert_note";
	line += "##.advert_top";
	line += "##.advertheader-red";
	line += "~tonprenom.com##.advertise";
	line += "##.advertise-here";
	line += "##.advertise-homestrip";
	line += "##.advertise-horz";
	line += "##.advertise-leaderboard";
	line += "##.advertise-vert";
	line += "##.advertiseContainer";
	line += "##.advertiseText";
	line += "##.advertise_ads";
	line += "##.advertise_here";
	line += "##.advertise_link";
	line += "##.advertise_link_sidebar";
	line += "~andkon.com,~wired.com##.advertisement";
	line += "##.advertisement-728x90";
	line += "##.advertisement-block";
	line += "##.advertisement-text";
	line += "##.advertisement-top";
	line += "##.advertisement468";
	line += "##.advertisementBox";
	line += "##.advertisementColumnGroup";
	line += "##.advertisementContainer";
	line += "##.advertisementHeader";
	line += "##.advertisementLabel";
	line += "##.advertisementPanel";
	line += "##.advertisement_btm";
	line += "##.advertisement_caption";
	line += "##.advertisement_g";
	line += "##.advertisement_header";
	line += "##.advertisement_horizontal";
	line += "##.advertisement_top";
	line += "~zlinked.com##.advertiser";
	line += "##.advertiser-links";
	line += "##.advertisespace_div";
	line += "~tonprenom.com,~trove.nla.gov.au##.advertising";
	line += "##.advertising-banner";
	line += "##.advertising-header";
	line += "##.advertising-local-links";
	line += "##.advertising2";
	line += "##.advertisingTable";
	line += "##.advertising_block";
	line += "##.advertising_images";
	line += "~macwelt.de##.advertisment";
	line += "##.advertisment_two";
	line += "##.advertize";
	line += "##.advertorial";
	line += "##.advertorial-2";
	line += "##.advertorial-promo-box";
	line += "##.adverts";
	line += "##.advt";
	line += "##.advt-banner-3";
	line += "##.advt-block";
	line += "##.advt300";
	line += "##.advt720";
	line += "##.adwordListings";
	line += "##.adwordsHeader";
	line += "##.adwrap";
	line += "~calgaryherald.com,~montrealgazette.com,~vancouversun.com,~windsorstar.com##.adwrapper";
	line += "##.adwrapper-lrec";
	line += "##.adwrapper948";
	line += "~virginmobile.fr##.affiliate";
	line += "##.affiliate-link";
	line += "##.affiliate-sidebar";
	line += "##.affiliateAdvertText";
	line += "##.affinityAdHeader";
	line += "##.after_ad";
	line += "##.agi-adsaleslinks";
	line += "##.alb-content-ad";
	line += "##.alt_ad";
	line += "##.anchorAd";
	line += "##.another_text_ad";
	line += "##.answer_ad_content";
	line += "##.aolSponsoredLinks";
	line += "##.aopsadvert";
	line += "##.apiAdMarkerAbove";
	line += "##.apiAds";
	line += "##.archive-ads";
	line += "##.art_ads";
	line += "##.article-ads";
	line += "##.articleAd";
	line += "##.articleAds";
	line += "##.articleAdsL";
	line += "##.articleEmbeddedAdBox";
	line += "##.article_ad";
	line += "##.article_adbox";
	line += "##.article_mpu_box";
	line += "##.articleads";
	line += "##.aseadn";
	line += "##.aux-ad-widget-2";
	line += "##.b-astro-sponsored-links_horizontal";
	line += "##.b-astro-sponsored-links_vertical";
	line += "##.banner-ad";
	line += "##.banner-ads";
	line += "##.banner-adverts";
	line += "##.banner300x100";
	line += "##.banner300x250";
	line += "##.banner468";
	line += "##.bannerAd";
	line += "##.bannerAdWrapper300x250";
	line += "##.bannerAdWrapper730x86";
	line += "##.bannerRightAd";
	line += "##.banner_300x250";
	line += "##.banner_728x90";
	line += "##.banner_ad";
	line += "##.banner_ad_footer";
	line += "##.banner_ad_leaderboard";
	line += "##.bannerad";
	line += "##.barkerAd";
	line += "##.base-ad-mpu";
	line += "##.base_ad";
	line += "##.bgnavad";
	line += "##.big-ads";
	line += "##.bigAd";
	line += "##.big_ad";
	line += "##.big_ads";
	line += "##.bigad";
	line += "##.bigad2";
	line += "##.bigbox_ad";
	line += "##.bigboxad";
	line += "##.billboard_ad";
	line += "##.blk_advert";
	line += "##.block-ad";
	line += "##.block-ad300";
	line += "##.block-admanager";
	line += "##.block-ads-bottom";
	line += "##.block-ads-top";
	line += "##.block-adsense";
	line += "##.block-openadstream";
	line += "##.block-openx";
	line += "##.block-thirdage-ads";
	line += "~kin0.org##.block_ad";
	line += "##.block_ad_sb_text";
	line += "##.block_ad_sponsored_links";
	line += "##.block_ad_sponsored_links-wrapper";
	line += "##.blocked-ads";
	line += "##.blog-ad-leader-inner";
	line += "##.blog-ads-container";
	line += "##.blogAd";
	line += "##.blogAdvertisement";
	line += "##.blogBigAd";
	line += "##.blog_ad";
	line += "##.blogads";
	line += "##.blox3featuredAd";
	line += "##.body_ad";
	line += "##.body_sponsoredresults_bottom";
	line += "##.body_sponsoredresults_middle";
	line += "##.body_sponsoredresults_top";
	line += "##.bookseller-header-advt";
	line += "##.bottomAd";
	line += "##.bottomAds";
	line += "##.bottom_ad";
	line += "~ixbtlabs.com##.bottom_ad_block";
	line += "##.bottom_sponsor";
	line += "##.bottomad";
	line += "##.bottomadvert";
	line += "##.bottomrightrailAd";
	line += "##.bottomvidad";
	line += "##.box-ad";
	line += "##.box-ads";
	line += "##.box-adsense";
	line += "##.boxAd";
	line += "##.box_ad";
	line += "##.box_ads";
	line += "##.box_advertising";
	line += "##.box_advertisment_62_border";
	line += "##.box_content_ad";
	line += "##.box_content_ads";
	line += "##.boxad";
	line += "##.boxyads";
	line += "##.bps-ad-wrapper";
	line += "##.bps-advertisement";
	line += "##.bps-advertisement-inline-ads";
	line += "##.br-ad";
	line += "##.bsa_ads";
	line += "##.btm_ad";
	line += "##.bullet-sponsored-links";
	line += "##.bullet-sponsored-links-gray";
	line += "##.burstContentAdIndex";
	line += "##.buttonAd";
	line += "##.buttonAds";
	line += "##.buttonadbox";
	line += "##.bx_ad";
	line += "##.bx_ad_right";
	line += "##.cA-adStrap";
	line += "##.cColumn-TextAdsBox";
	line += "##.care2_adspace";
	line += "##.catalog_ads";
	line += "##.cb-ad-container";
	line += "##.cb_footer_sponsor";
	line += "##.cb_navigation_ad";
	line += "##.cbstv_ad_label";
	line += "##.cbzadvert";
	line += "##.cbzadvert_block";
	line += "##.cdAdTitle";
	line += "##.cdmainlineSearchAdParent";
	line += "##.cdsidebarSearchAdParent";
	line += "##.centerAd";
	line += "##.center_ad";
	line += "##.centerad";
	line += "##.centered-ad";
	line += "##.cinemabotad";
	line += "##.clearerad";
	line += "##.cm_ads";
	line += "##.cms-Advert";
	line += "##.cnbc_badge_banner_ad_area";
	line += "##.cnn160AdFooter";
	line += "##.cnnAd";
	line += "##.cnnMosaic160Container";
	line += "##.cnnSearchSponsorBox";
	line += "##.cnnStoreAd";
	line += "##.cnnStoryElementBoxAd";
	line += "##.cnnWCAdBox";
	line += "##.cnnWireAdLtgBox";
	line += "##.cnn_728adbin";
	line += "##.cnn_adcntr300x100";
	line += "##.cnn_adcntr728x90";
	line += "##.cnn_adspc336cntr";
	line += "##.cnn_adtitle";
	line += "##.column2-ad";
	line += "##.com-ad-server";
	line += "##.comment-advertisement";
	line += "##.common_advertisement_title";
	line += "##.communityAd";
	line += "##.conTSponsored";
	line += "##.conductor_ad";
	line += "##.confirm_ad_left";
	line += "##.confirm_ad_right";
	line += "##.confirm_leader_ad";
	line += "##.consoleAd";
	line += "##.container-adwords";
	line += "##.containerSqAd";
	line += "##.container_serendipity_plugin_google_adsense";
	line += "##.content-ad";
	line += "~theology.edu##.contentAd";
	line += "##.contentAdFoot";
	line += "##.contentAdsWrapper";
	line += "##.content_ad";
	line += "##.content_ad_728";
	line += "##.content_adsq";
	line += "##.contentad";
	line += "##.contentad300x250";
	line += "##.contentad_right_col";
	line += "##.contentadcontainer";
	line += "##.contentadleft";
	line += "##.contenttextad";
	line += "##.contest_ad";
	line += "##.cp_ad";
	line += "##.cpmstarHeadline";
	line += "##.cpmstarText";
	line += "##.create_ad";
	line += "##.cs-mpu";
	line += "##.cscTextAd";
	line += "##.cse_ads";
	line += "##.cspAd";
	line += "##.ct_ad";
	line += "##.cube-ad";
	line += "##.cubeAd";
	line += "##.cube_ads";
	line += "##.currency_ad";
	line += "##.custom_ads";
	line += "##.darla_ad";
	line += "##.dartAdImage";
	line += "##.dart_ad";
	line += "##.dart_tag";
	line += "##.dartadvert";
	line += "##.dartiframe";
	line += "##.dc-ad";
	line += "##.dcAdvertHeader";
	line += "##.deckAd";
	line += "##.deckads";
	line += "##.detail-ads";
	line += "##.detailMpu";
	line += "##.detail_top_advert";
	line += "##.divAd";
	line += "##.divad1";
	line += "##.divad2";
	line += "##.divad3";
	line += "##.divads";
	line += "##.divider_ad";
	line += "##.dmco_advert_iabrighttitle";
	line += "##.download_ad";
	line += "##.downloadad";
	line += "##.dynamic-ads";
	line += "##.dynamic_ad";
	line += "##.e-ad";
	line += "##.ec-ads";
	line += "##.em-ad";
	line += "##.embed-ad";
	line += "##.entry_sidebar_ads";
	line += "##.entryad";
	line += "##.ez-clientAd";
	line += "##.f_Ads";
	line += "##.featuredAds";
	line += "##.featuredadvertising";
	line += "##.firstpost_advert_container";
	line += "##.flagads";
	line += "##.flash-advertisement";
	line += "##.flash_ad";
	line += "##.flash_advert";
	line += "##.flashad";
	line += "##.flexiad";
	line += "##.flipbook_v2_sponsor_ad";
	line += "##.floatad";
	line += "##.floated_right_ad";
	line += "##.footad";
	line += "##.footer-ad";
	line += "##.footerAd";
	line += "##.footerAdModule";
	line += "##.footerAdslot";
	line += "##.footerTextAd";
	line += "##.footer_ad";
	line += "##.footer_ads";
	line += "##.footer_block_ad";
	line += "##.footer_bottomad";
	line += "##.footer_line_ad";
	line += "##.footer_text_ad";
	line += "##.footerad";
	line += "##.forumtopad";
	line += "##.frn_adbox";
	line += "##.frn_cont_adbox";
	line += "##.ft-ad";
	line += "##.ftdAdBar";
	line += "##.ftdContentAd";
	line += "##.full_ad_box";
	line += "##.fullbannerad";
	line += "##.g3rtn-ad-site";
	line += "##.gAdvertising";
	line += "##.g_ggl_ad";
	line += "##.ga-textads-bottom";
	line += "##.ga-textads-top";
	line += "##.gaTeaserAdsBox";
	line += "##.gads";
	line += "##.gads_cb";
	line += "##.gads_container";
	line += "##.gamesPage_ad_content";
	line += "##.gglAds";
	line += "##.global_banner_ad";
	line += "##.googad";
	line += "##.googads";
	line += "##.google-ad";
	line += "##.google-ad-container";
	line += "##.google-ads";
	line += "##.google-ads-boxout";
	line += "##.google-ads-slim";
	line += "##.google-right-ad";
	line += "##.google-sponsored-ads";
	line += "##.google-sponsored-link";
	line += "##.google468_60";
	line += "##.googleAd";
	line += "##.googleAd-content";
	line += "##.googleAd-list";
	line += "##.googleAdBox";
	line += "##.googleAdSense";
	line += "##.googleAdSenseModule";
	line += "##.googleAd_body";
	line += "##.googleAds";
	line += "##.googleAds_article_page_above_comments";
	line += "##.googleAdsense";
	line += "##.googleContentAds";
	line += "##.googleProfileAd";
	line += "##.googleSearchAd_content";
	line += "##.googleSearchAd_sidebar";
	line += "##.google_ad";
	line += "##.google_add_container";
	line += "##.google_ads";
	line += "##.google_ads_bom_title";
	line += "##.google_ads_content";
	line += "##.googlead";
	line += "##.googleaddiv";
	line += "##.googleaddiv2";
	line += "##.googleads";
	line += "##.googleads_300x250";
	line += "##.googleads_title";
	line += "##.googley_ads";
	line += "##.gpAdBox";
	line += "##.gpAds";
	line += "##.gradientAd";
	line += "##.grey-ad-line";
	line += "##.group_ad";
	line += "##.gsfAd";
	line += "##.gt_ad";
	line += "##.gt_ad_300x250";
	line += "##.gt_ad_728x90";
	line += "##.gt_adlabel";
	line += "##.gutter-ad-left";
	line += "##.gutter-ad-right";
	line += "##.h-ad-728x90-bottom";
	line += "##.h_Ads";
	line += "##.h_ad";
	line += "##.half-ad";
	line += "##.half_ad_box";
	line += "##.hd_advert";
	line += "##.hdr-ads";
	line += "~assetbar.com,~burningangel.com##.header-ad";
	line += "##.header-advert";
	line += "~photobucket.com##.headerAd";
	line += "##.headerAds";
	line += "##.headerAdvert";
	line += "##.header_ad";
	line += "~associatedcontent.com##.header_ad_center";
	line += "##.header_ad_div";
	line += "##.header_advertisment";
	line += "##.headerad";
	line += "##.hi5-ad";
	line += "##.highlightsAd";
	line += "##.hm_advertisment";
	line += "##.home-ad-links";
	line += "##.homeAd";
	line += "##.homeAdBoxA";
	line += "##.homeAdBoxBetweenBlocks";
	line += "##.homeAdBoxInBignews";
	line += "##.homeAdSection";
	line += "##.homeMediumAdGroup";
	line += "##.home_ad_bottom";
	line += "##.home_advertisement";
	line += "##.home_mrec_ad";
	line += "##.homead";
	line += "##.homepage-ad";
	line += "##.homepage300ad";
	line += "##.homepageFlexAdOuter";
	line += "##.homepageMPU";
	line += "##.homepage_middle_right_ad";
	line += "##.hor_ad";
	line += "##.horiz_adspace";
	line += "##.horizontalAd";
	line += "~radaronline.com##.horizontal_ad";
	line += "##.horizontal_ads";
	line += "##.horizontaltextadbox";
	line += "##.horizsponsoredlinks";
	line += "##.hortad";
	line += "##.houseAdsStyle";
	line += "##.housead";
	line += "##.hp2-adtag";
	line += "##.hp_ad_cont";
	line += "##.hp_ad_text";
	line += "##.hp_t_ad";
	line += "##.hp_w_ad";
	line += "##.ic-ads";
	line += "##.ico-adv";
	line += "##.idMultiAd";
	line += "##.image-advertisement";
	line += "##.imageads";
	line += "##.imgad";
	line += "##.in-page-ad";
	line += "##.in-story-text-ad";
	line += "##.indie-sidead";
	line += "##.indy_googleads";
	line += "##.inline-ad";
	line += "##.inline-mpu-left";
	line += "##.inlineSideAd";
	line += "##.inline_ad";
	line += "##.inline_ad_title";
	line += "##.inlinead";
	line += "##.inlineadsense";
	line += "##.inlineadtitle";
	line += "##.inlist-ad";
	line += "##.inlistAd";
	line += "##.inner-advt-banner-3";
	line += "##.innerAds";
	line += "##.innerad";
	line += "##.inpostad";
	line += "##.insert_advertisement";
	line += "##.insertad";
	line += "##.insideStoryAd";
	line += "##.is24-adplace";
	line += "##.islandAd";
	line += "##.islandAdvert";
	line += "##.islandad";
	line += "##.jimdoAdDisclaimer";
	line += "##.jp-advertisment-promotional";
	line += "##.js-advert";
	line += "##.kw_advert";
	line += "##.kw_advert_pair";
	line += "##.l_ad_sub";
	line += "##.l_banner.ads_show_if";
	line += "##.labelads";
	line += "##.largeRectangleAd";
	line += "##.lastRowAd";
	line += "##.lcontentbox_ad";
	line += "##.leaderAdTop";
	line += "##.leaderAdvert";
	line += "##.leader_ad";
	line += "##.leaderboardAd";
	line += "##.leaderboardad";
	line += "##.leaderboardadtop";
	line += "##.left-ad";
	line += "##.leftAd";
	line += "##.leftAdColumn";
	line += "##.leftAds";
	line += "##.left_adlink";
	line += "##.left_ads";
	line += "##.leftad";
	line += "##.leftadtag";
	line += "##.leftbar_ad_160_600";
	line += "##.leftbarads";
	line += "##.leftnavad";
	line += "##.lgRecAd";
	line += "##.lg_ad";
	line += "##.ligatus";
	line += "##.linead";
	line += "##.link_adslider";
	line += "##.link_advertise";
	line += "##.live-search-list-ad-container";
	line += "##.ljad";
	line += "##.log_ads";
	line += "##.logoAds";
	line += "##.longAd";
	line += "##.lowerAds";
	line += "##.m-ad-tvguide-box";
	line += "##.m4-adsbygoogle";
	line += "##.m_banner_ads";
	line += "##.macAd";
	line += "##.macad";
	line += "##.main-ad";
	line += "##.main-tabs-ad-block";
	line += "##.main_ad";
	line += "##.main_adbox";
	line += "##.main_intro_ad";
	line += "##.map_media_banner_ad";
	line += "##.marginadsthin";
	line += "##.marketing-ad";
	line += "##.masthead_topad";
	line += "##.mdl-ad";
	line += "##.media-advert";
	line += "##.mediaAd";
	line += "##.mediaAdContainer";
	line += "##.medium-rectangle-ad";
	line += "##.mediumRectangleAdvert";
	line += "##.menuItemBannerAd";
	line += "##.messageBoardAd";
	line += "##.micro_ad";
	line += "##.mid_ad";
	line += "##.midad";
	line += "##.middleAds";
	line += "##.middleads";
	line += "##.min_navi_ad";
	line += "##.miniad";
	line += "##.mobile-sponsoring";
	line += "##.mod-ad-lrec";
	line += "##.mod-ad-n";
	line += "##.mod-adopenx";
	line += "##.mod_admodule";
	line += "~corrieredicomo.it##.module-ad";
	line += "##.module-ad-small";
	line += "##.module-ads";
	line += "##.moduleAdvertContent";
	line += "##.module_ad";
	line += "##.module_box_ad";
	line += "##.modulegad";
	line += "##.moduletable-advert";
	line += "##.moduletable-googleads";
	line += "##.moduletablesquaread";
	line += "~gamespot.com##.mpu";
	line += "##.mpu-ad";
	line += "##.mpu-advert";
	line += "##.mpu-footer";
	line += "##.mpu-fp";
	line += "##.mpu-title";
	line += "##.mpu-top-left";
	line += "##.mpu-top-left-banner";
	line += "##.mpu-top-right";
	line += "##.mpuAd";
	line += "##.mpuAdSlot";
	line += "##.mpuAdvert";
	line += "##.mpuArea";
	line += "##.mpuBox";
	line += "##.mpuContainer";
	line += "##.mpuHolder";
	line += "##.mpuTextAd";
	line += "##.mpu_ad";
	line += "##.mpu_advert";
	line += "##.mpu_gold";
	line += "##.mpu_holder";
	line += "##.mpu_platinum";
	line += "##.mpu_text_ad";
	line += "##.mpuad";
	line += "##.mpuholderportalpage";
	line += "##.mrec_advert";
	line += "##.ms-ads-link";
	line += "##.msfg-shopping-mpu";
	line += "##.mwaads";
	line += "##.nSponsoredLcContent";
	line += "##.nSponsoredLcTopic";
	line += "##.nadvt300";
	line += "##.narrow_ad_unit";
	line += "##.narrow_ads";
	line += "##.navAdsBanner";
	line += "##.navi_ad300";
	line += "##.naviad";
	line += "##.nba300Ad";
	line += "##.nbaT3Ad160";
	line += "##.nbaTVPodAd";
	line += "##.nbaTwo130Ads";
	line += "##.nbc_ad_carousel_wrp";
	line += "##.newTopAdContainer";
	line += "##.newad";
	line += "##.newsviewAdBoxInNews";
	line += "##.nf-adbox";
	line += "##.nn-mpu";
	line += "##.noAdForLead";
	line += "##.normalAds";
	line += "##.nrAds";
	line += "##.nsAdRow";
	line += "##.oas-ad";
	line += "##.oas-bottom-ads";
	line += "##.offer_sponsoredlinks";
	line += "##.oio-banner-zone";
	line += "##.oio-link-sidebar";
	line += "##.oio-zone-position";
	line += "##.on_single_ad_box";
	line += "##.onethirdadholder";
	line += "##.openads";
	line += "##.openadstext_after";
	line += "##.openx";
	line += "##.openx-ad";
	line += "##.osan-ads";
	line += "##.other_adv2";
	line += "##.ovAdPromo";
	line += "##.ovAdSky";
	line += "##.ovAdartikel";
	line += "##.ov_spns";
	line += "##.pageGoogleAd";
	line += "##.pageGoogleAdFlat";
	line += "##.pageLeaderAd";
	line += "##.page_content_right_ad";
	line += "##.pagead";
	line += "##.pagenavindexcontentad";
	line += "##.partnersTextLinks";
	line += "##.pencil_ad";
	line += "##.player_ad_box";
	line += "##.player_page_ad_box";
	line += "##.plista_inimg_box";
	line += "##.pnp_ad";
	line += "##.pod-ad-300";
	line += "##.podRelatedAdLinksWidget";
	line += "##.podSponsoredLink";
	line += "##.portalCenterContentAdBottom";
	line += "##.portalCenterContentAdMiddle";
	line += "##.portalCenterContentAdTop";
	line += "##.portalcontentad";
	line += "##.post-ad";
	line += "##.post_ad";
	line += "##.post_ads";
	line += "##.post_sponsor_unit";
	line += "##.postbit_adbit_register";
	line += "##.postbit_adcode";
	line += "##.postgroup-ads";
	line += "##.postgroup-ads-middle";
	line += "##.prebodyads";
	line += "##.premium_ad_container";
	line += "##.promoAd";
	line += "##.promoAds";
	line += "##.promo_ad";
	line += "##.publication-ad";
	line += "##.publicidad";
	line += "##.puff-advertorials";
	line += "##.qa_ad_left";
	line += "##.qm-ad-content";
	line += "##.qm-ad-content-news";
	line += "##.quigo-ad";
	line += "##.qzvAdDiv";
	line += "##.r_ad_box";
	line += "##.r_ads";
	line += "##.rad_container";
	line += "##.rect_ad_module";
	line += "##.rectad";
	line += "##.rectangleAd";
	line += "##.rectanglead";
	line += "##.redads_cont";
	line += "##.regular_728_ad";
	line += "##.regularad";
	line += "##.relatedAds";
	line += "##.related_post_google_ad";
	line += "##.remads";
	line += "##.resourceImagetAd";
	line += "##.result_ad";
	line += "##.results_sponsor";
	line += "##.results_sponsor_right";
	line += "##.reviewMidAdvertAlign";
	line += "##.rght300x250";
	line += "##.rhads";
	line += "##.rhs-ad";
	line += "##.rhs-ads-panel";
	line += "##.right-ad";
	line += "##.right-ad-holder";
	line += "##.right-ad2";
	line += "##.right-ads";
	line += "##.right-ads2";
	line += "##.rightAd";
	line += "##.rightColAd";
	line += "##.rightRailAd";
	line += "##.right_ad";
	line += "##.right_ad_text";
	line += "##.right_ad_top";
	line += "##.right_ads";
	line += "~dailymotion.com,~dailymotion.virgilio.it##.right_ads_column";
	line += "##.right_col_ad";
	line += "##.right_hand_advert_column";
	line += "##.rightad";
	line += "##.rightad_1";
	line += "##.rightad_2";
	line += "##.rightadbox1";
	line += "##.rightads";
	line += "##.rightadunit";
	line += "##.rightcol_boxad";
	line += "##.rightcoladvert";
	line += "##.rightcoltowerad";
	line += "##.rnav_ad";
	line += "##.rngtAd";
	line += "##.roundingrayboxads";
	line += "##.rt_ad1_300x90";
	line += "##.rt_ad_300x250";
	line += "##.rt_ad_call";
	line += "##.savvyad_unit";
	line += "##.sb-ad-sq-bg";
	line += "##.sbAd";
	line += "##.sbAdUnitContainer";
	line += "##.sb_adsN";
	line += "##.sb_adsNv2";
	line += "##.sb_adsW";
	line += "##.sb_adsWv2";
	line += "##.scanAd";
	line += "##.scc_advert";
	line += "##.sci-ad-main";
	line += "##.sci-ad-sub";
	line += "##.search-ad";
	line += "##.search-results-ad";
	line += "##.search-sponsor";
	line += "##.search-sponsored";
	line += "##.searchAd";
	line += "##.searchSponsoredResultsBox";
	line += "##.searchSponsoredResultsList";
	line += "##.search_column_results_sponsored";
	line += "##.search_results_sponsored_top";
	line += "##.section-ad2";
	line += "##.section-sponsor";
	line += "##.section_mpu_wrapper";
	line += "##.section_mpu_wrapper_wrapper";
	line += "##.selfServeAds";
	line += "##.serp_sponsored";
	line += "##.servsponserLinks";
	line += "##.shoppingGoogleAdSense";
	line += "##.sidbaread";
	line += "##.side-ad";
	line += "##.side-ads";
	line += "##.sideAd";
	line += "##.sideBoxAd";
	line += "##.side_ad";
	line += "##.side_ad2";
	line += "##.side_ad_1";
	line += "##.side_ad_2";
	line += "##.side_ad_3";
	line += "##.sidead";
	line += "##.sideads";
	line += "##.sideadsbox";
	line += "##.sideadvert";
	line += "##.sidebar-ad";
	line += "##.sidebar-ads";
	line += "##.sidebar-text-ad";
	line += "##.sidebarAd";
	line += "##.sidebarAdUnit";
	line += "##.sidebarAdvert";
	line += "##.sidebar_ad";
	line += "##.sidebar_ad_300_250";
	line += "##.sidebar_ads";
	line += "##.sidebar_ads_336";
	line += "##.sidebar_adsense";
	line += "##.sidebar_box_ad";
	line += "##.sidebarad";
	line += "##.sidebarad_bottom";
	line += "##.sidebaradbox";
	line += "##.sidebarboxad";
	line += "##.sideheadnarrowad";
	line += "##.sideheadsponsorsad";
	line += "##.singleAd";
	line += "##.singleAdsContainer";
	line += "##.singlead";
	line += "##.sitesponsor";
	line += "##.skinAd";
	line += "##.skin_ad_638";
	line += "##.sky-ad";
	line += "##.skyAd";
	line += "##.skyAdd";
	line += "##.sky_ad";
	line += "##.sky_scraper_ad";
	line += "##.skyad";
	line += "##.skyscraper-ad";
	line += "##.skyscraper_ad";
	line += "##.skyscraper_bannerAdHome";
	line += "##.slideshow-ad";
	line += "##.slpBigSlimAdUnit";
	line += "##.slpSquareAdUnit";
	line += "##.sm_ad";
	line += "##.smallSkyAd1";
	line += "##.smallSkyAd2";
	line += "##.small_ad";
	line += "##.small_ads";
	line += "##.smallad-left";
	line += "##.smallads";
	line += "##.smallsponsorad";
	line += "##.smart_ads_bom_title";
	line += "##.specialAd175x90";
	line += "##.speedyads";
	line += "##.sphereAdContainer";
	line += "##.spl-ads";
	line += "##.spl_ad";
	line += "##.spl_ad2";
	line += "##.spl_ad_plus";
	line += "##.splitAd";
	line += "##.sponlinkbox";
	line += "##.spons-link";
	line += "##.spons_links";
	line += "##.sponslink";
	line += "##.sponsor-ad";
	line += "##.sponsor-bottom";
	line += "##.sponsor-link";
	line += "##.sponsor-links";
	line += "##.sponsor-right";
	line += "##.sponsor-services";
	line += "##.sponsor-top";
	line += "##.sponsorArea";
	line += "##.sponsorBox";
	line += "##.sponsorPost";
	line += "##.sponsorPostWrap";
	line += "##.sponsorStrip";
	line += "##.sponsorTop";
	line += "##.sponsor_ad_area";
	line += "##.sponsor_footer";
	line += "##.sponsor_horizontal";
	line += "##.sponsor_line";
	line += "##.sponsor_links";
	line += "##.sponsor_logo";
	line += "##.sponsor_top";
	line += "##.sponsor_units";
	line += "##.sponsoradtitle";
	line += "##.sponsorbox";
	line += "~gamespot.com,~mint.com,~slidetoplay.com,~smh.com.au,~zattoo.com##.sponsored";
	line += "##.sponsored-ads";
	line += "##.sponsored-chunk";
	line += "##.sponsored-editorial";
	line += "##.sponsored-features";
	line += "##.sponsored-links";
	line += "##.sponsored-links-alt-b";
	line += "##.sponsored-links-holder";
	line += "##.sponsored-links-right";
	line += "##.sponsored-post";
	line += "##.sponsored-post_ad";
	line += "##.sponsored-results";
	line += "##.sponsored-right-border";
	line += "##.sponsored-text";
	line += "##.sponsoredInner";
	line += "##.sponsoredLinks";
	line += "##.sponsoredLinksHeader";
	line += "##.sponsoredProduct";
	line += "##.sponsoredSideInner";
	line += "##.sponsored_ads";
	line += "##.sponsored_box";
	line += "##.sponsored_box_search";
	line += "##.sponsored_by";
	line += "##.sponsored_links";
	line += "##.sponsored_links_title_container";
	line += "##.sponsored_links_title_container_top";
	line += "##.sponsored_links_top";
	line += "##.sponsored_results";
	line += "##.sponsored_well";
	line += "##.sponsoredibbox";
	line += "##.sponsoredlink";
	line += "##.sponsoredlinks";
	line += "##.sponsoredlinkscontainer";
	line += "##.sponsoredresults";
	line += "~excite.eu##.sponsoredtextlink_container";
	line += "##.sponsoredtextlink_container_ovt";
	line += "##.sponsorlink";
	line += "##.sponsorlink2";
	line += "##.sponsors";
	line += "##.sponsors-box";
	line += "##.sponsorshipbox";
	line += "##.spotlightAd";
	line += "##.squareAd";
	line += "##.square_ad";
	line += "##.squared_ad";
	line += "##.ss-ad-mpu";
	line += "##.staticAd";
	line += "##.stocks-ad-tag";
	line += "##.store-ads";
	line += "##.story_AD";
	line += "##.subad";
	line += "##.subcontent-ad";
	line += "##.super-ad";
	line += "##.supercommentad_left";
	line += "##.supercommentad_right";
	line += "##.supp-ads";
	line += "##.supportAdItem";
	line += "##.surveyad";
	line += "##.t10ad";
	line += "##.tab_ad";
	line += "##.tab_ad_area";
	line += "##.tablebordersponsor";
	line += "##.tadsanzeige";
	line += "##.tadsbanner";
	line += "##.tadselement";
	line += "##.tallad";
	line += "##.tblTopAds";
	line += "##.tbl_ad";
	line += "##.tbox_ad";
	line += "##.teaser-sponsor";
	line += "##.teaserAdContainer";
	line += "##.teaser_adtiles";
	line += "##.text-ad-links";
	line += "##.text-g-advertisement";
	line += "##.text-g-group-short-rec-ad";
	line += "##.text-g-net-grp-google-ads-article-page";
	line += "##.textAd";
	line += "##.textAdBox";
	line += "##.textAds";
	line += "##.text_ad";
	line += "##.text_ads";
	line += "##.textad";
	line += "##.textadContainer";
	line += "##.textad_headline";
	line += "##.textadbox";
	line += "~frogueros.com##.textads";
	line += "##.textadsfoot";
	line += "##.textlink-ads";
	line += "##.tf_page_ad_search";
	line += "##.thisIsAd";
	line += "##.thisIsAnAd";
	line += "##.ticket-ad";
	line += "##.tileAds";
	line += "##.tips_advertisement";
	line += "##.title-ad";
	line += "##.title_adbig";
	line += "##.tncms-region-ads";
	line += "##.toolad";
	line += "##.toolbar-ad";
	line += "##.top-ad";
	line += "##.top-ad-space";
	line += "##.top-ads";
	line += "##.top-menu-ads";
	line += "##.top-sponsors";
	line += "##.topAd";
	line += "##.topAdWrap";
	line += "~timescall.com##.topAds";
	line += "##.topAdvertisement";
	line += "##.topBannerAd";
	line += "##.topLeaderboardAd";
	line += "##.top_Ad";
	line += "##.top_ad";
	line += "##.top_ad_728";
	line += "##.top_ad_728_90";
	line += "##.top_ad_disclaimer";
	line += "##.top_ad_div";
	line += "##.top_ad_post";
	line += "##.top_ad_wrapper";
	line += "~trailvoy.com##.top_ads";
	line += "##.top_advert";
	line += "##.top_advertising_lb";
	line += "##.top_container_ad";
	line += "##.top_sponsor";
	line += "~pchome.com.tw##.topad";
	line += "##.topad-bar";
	line += "##.topadbox";
	line += "##.topads";
	line += "##.topadspot";
	line += "##.topadvertisementsegment";
	line += "##.topcontentadvertisement";
	line += "##.topic_inad";
	line += "##.topstoriesad";
	line += "##.toptenAdBoxA";
	line += "##.towerAd";
	line += "##.towerAdLeft";
	line += "##.towerAds";
	line += "##.tower_ad";
	line += "##.tower_ad_disclaimer";
	line += "##.towerad";
	line += "##.ts-ad_unit_bigbox";
	line += "##.ts-banner_ad";
	line += "##.ttlAdsensel";
	line += "##.tto-sponsored-element";
	line += "##.tvs-mpu";
	line += "##.twoColumnAd";
	line += "##.twoadcoll";
	line += "##.twoadcolr";
	line += "##.tx_smartadserver_pi1";
	line += "##.txt-ads";
	line += "##.txtAds";
	line += "##.txt_ads";
	line += "##.txtadvertise";
	line += "##.type_adscontainer";
	line += "##.type_miniad";
	line += "##.type_promoads";
	line += "##.ukAds";
	line += "##.undertimyads";
	line += "##.universalboxADVBOX01";
	line += "##.universalboxADVBOX03";
	line += "##.universalboxADVBOX04a";
	line += "##.usenext";
	line += "##.vertad";
	line += "##.videoAd";
	line += "##.videoBoxAd";
	line += "##.video_ad";
	line += "##.view-promo-mpu-right";
	line += "##.view_rig_ad";
	line += "##.virgin-mpu";
	line += "##.wa_adsbottom";
	line += "##.wide-ad";
	line += "##.wide-skyscraper-ad";
	line += "##.wideAdTable";
	line += "##.wide_ad";
	line += "##.wide_ad_unit_top";
	line += "##.wide_ads";
	line += "##.wide_google_ads";
	line += "##.widget-ad";
	line += "##.widget-ad300x250";
	line += "##.widget-entry-ads-160";
	line += "##.widgetYahooAds";
	line += "##.widget_ad";
	line += "##.widget_ad_rotator";
	line += "##.widget_island_ad";
	line += "##.widget_sdac_footer_ads_widget";
	line += "##.wikia-ad";
	line += "##.wikia_ad_placeholder";
	line += "##.withAds";
	line += "##.wnMultiAd";
	line += "##.wp125ad";
	line += "##.wp125ad_2";
	line += "##.wpn_ad_content";
	line += "##.wrap-ads";
	line += "##.wsSponsoredLinksRight";
	line += "##.wsTopSposoredLinks";
	line += "##.x03-adunit";
	line += "##.x04-adunit";
	line += "##.xads-blk2";
	line += "##.xads-ojedn";
	line += "##.y-ads";
	line += "##.y-ads-wide";
	line += "##.y7-advertisement";
	line += "##.yahoo-sponsored";
	line += "##.yahoo-sponsored-links";
	line += "##.yahooAds";
	line += "##.yahoo_ads";
	line += "##.yan-sponsored";
	line += "##.ygrp-ad";
	line += "##.yrail_ad_wrap";
	line += "##.yrail_ads";
	line += "##.ysmsponsor";
	line += "##.ysponsor";
	line += "##.yw-ad";
	line += "~marketgid.com,~mgid.com,~thechive.com##[id^=\"MarketGid\"]";
	line += "##a[href^=\"http://ad.doubleclick.net/\"]";
	line += "##a[href^=\"http://adserving.liveuniversenetwork.com/\"]";
	line += "##a[href^=\"http://galleries.pinballpublishernetwork.com/\"]";
	line += "##a[href^=\"http://galleries.securewebsiteaccess.com/\"]";
	line += "##a[href^=\"http://install.securewebsiteaccess.com/\"]";
	line += "##a[href^=\"http://latestdownloads.net/download.php?\"]";
	line += "##a[href^=\"http://secure.signup-page.com/\"]";
	line += "##a[href^=\"http://secure.signup-way.com/\"]";
	line += "##a[href^=\"http://www.FriendlyDuck.com/AF_\"]";
	line += "##a[href^=\"http://www.adbrite.com/mb/commerce/purchase_form.php?\"]";
	line += "##a[href^=\"http://www.friendlyduck.com/AF_\"]";
	line += "##a[href^=\"http://www.google.com/aclk?\"]";
	line += "##a[href^=\"http://www.liutilities.com/aff\"]";
	line += "##a[href^=\"http://www.liutilities.com/products/campaigns/adv/\"]";
	line += "##a[href^=\"http://www.my-dirty-hobby.com/?sub=\"]";
	line += "##a[href^=\"http://www.ringtonematcher.com/\"]";
	line += "!Google";
	line += "###mbEnd[cellspacing=\"0\"][cellpadding=\"0\"][style=\"padding: 0pt;\"]";
	line += "###mbEnd[cellspacing=\"0\"][style=\"padding: 0pt; white-space: nowrap;\"]";
	line += "##div#mclip_container:first-child:last-child";
	line += "##div#rhs_block[style=\"padding-top: 5px;\"]";
	line += "##div#rhs_block[style=\"padding-top:5px\"]";
	line += "##div#tads.c";
	line += "##table.ra[align=\"left\"][width=\"30%\"]";
	line += "##table.ra[align=\"right\"][width=\"30%\"]";
	line += "!-----------------Third-party advertisers-----------------!";
	line += "! *** easylist_adservers.txt ***";
	line += "||10pipsaffiliates.com^$third-party";
	line += "||1100i.com^$third-party";
	line += "||188server.com^$third-party";
	line += "||247realmedia.com^$third-party";
	line += "||2mdn.net^$third-party";
	line += "||360ads.com^$third-party";
	line += "||3rdads.com^$third-party";
	line += "||43plc.com^$third-party";
	line += "||600z.com^$third-party";
	line += "||777seo.com^$third-party";
	line += "||7search.com^$third-party";
	line += "||aa.voice2page.com^$third-party";
	line += "||accuserveadsystem.com^$third-party";
	line += "||acf-webmaster.net^$third-party";
	line += "||acronym.com^$third-party";
	line += "||ad-flow.com^$third-party";
	line += "||ad20.net^$third-party";
	line += "||ad2games.com^$third-party";
	line += "||ad4game.com^$third-party";
	line += "||adaction.se^$third-party";
	line += "||adaos-ads.net^$third-party";
	line += "||adbard.net^$third-party";
	line += "||adbasket.net^$third-party";
	line += "||adblade.com^$third-party";
	line += "||adbrite.com^$third-party";
	line += "||adbull.com^$third-party";
	line += "||adbureau.net^$third-party";
	line += "||adbutler.com^$third-party";
	line += "||adcde.com^$third-party";
	line += "||adcentriconline.com^$third-party";
	line += "||adchap.com^$third-party";
	line += "||adclickmedia.com^$third-party";
	line += "||adcolo.com^$third-party";
	line += "||adcru.com^$third-party";
	line += "||addynamo.com^$third-party";
	line += "||adecn.com^$third-party";
	line += "||adengage.com^$third-party";
	line += "||adf01.net^$third-party";
	line += "||adfactory88.com^$third-party";
	line += "||adfrontiers.com^$third-party";
	line += "||adfusion.com^$third-party";
	line += "||adgardener.com^$third-party";
	line += "||adgear.com^$third-party";
	line += "||adgent007.com^$third-party";
	line += "||adgine.net^$third-party";
	line += "||adgitize.com^$third-party";
	line += "||adgroups.com^$third-party";
	line += "||adhese.be^$third-party";
	line += "||adhese.net^$third-party";
	line += "||adhitzads.com^$third-party";
	line += "||adhostingsolutions.com^$third-party";
	line += "||adicate.com^$third-party";
	line += "||adimise.com^$third-party";
	line += "||adimpact.com^$third-party";
	line += "||adinterax.com^$third-party";
	line += "||adireland.com^$third-party";
	line += "||adisfy.com^$third-party";
	line += "||adisn.com^$third-party";
	line += "||adition.com^$third-party";
	line += "||adjal.com^$third-party";
	line += "||adjug.com^$third-party";
	line += "||adjuggler.com^$third-party";
	line += "||adjuggler.net^$third-party";
	line += "||adkonekt.com^$third-party";
	line += "||adlink.net^$third-party";
	line += "||adlisher.com^$third-party";
	line += "||admarketplace.net^$third-party";
	line += "||admaya.in^$third-party";
	line += "||admeld.com^$third-party";
	line += "||admeta.com^$third-party";
	line += "||admitad.com^$third-party";
	line += "||admpads.com^$third-party";
	line += "||adnet.biz^$third-party";
	line += "||adnet.com^$third-party";
	line += "||adnet.ru^$third-party";
	line += "||adocean.pl^$third-party";
	line += "||adoperator.com^$third-party";
	line += "||adoptim.com^$third-party";
	line += "||adotube.com^$third-party";
	line += "||adparlor.com^$third-party";
	line += "||adperium.com^$third-party";
	line += "||adpinion.com^$third-party";
	line += "||adpionier.de^$third-party";
	line += "||adpremo.com^$third-party";
	line += "||adprs.net^$third-party";
	line += "||adquest3d.com^$third-party";
	line += "||adreadytractions.com^$third-party";
	line += "||adrocket.com^$third-party";
	line += "||adroll.com^$third-party";
	line += "||ads-stats.com^$third-party";
	line += "||ads4cheap.com^$third-party";
	line += "||adscendmedia.com^$third-party";
	line += "||adsdk.com^$third-party";
	line += "||adsensecamp.com^$third-party";
	line += "||adservinginternational.com^$third-party";
	line += "||adsfactor.net^$third-party";
	line += "||adsfast.com^$third-party";
	line += "||adsforindians.com^$third-party";
	line += "||adsfuse.com^$third-party";
	line += "||adshopping.com^$third-party";
	line += "||adshuffle.com^$third-party";
	line += "||adsignals.com^$third-party";
	line += "||adsmarket.com^$third-party";
	line += "||adsmedia.cc^$third-party";
	line += "||adsonar.com^$third-party";
	line += "||adspeed.com^$third-party";
	line += "||adsrevenue.net^$third-party";
	line += "||adsupermarket.com^$third-party";
	line += "||adswizz.com^$third-party";
	line += "||adtaily.com^$third-party";
	line += "||adtaily.eu^$third-party";
	line += "||adtech.de^$third-party";
	line += "||adtechus.com^$third-party";
	line += "||adtoll.com^$third-party";
	line += "||adtology1.com^$third-party";
	line += "||adtology2.com^$third-party";
	line += "||adtology3.com^$third-party";
	line += "||adtoma.com^$third-party";
	line += "||adtotal.pl^$third-party";
	line += "||adtrgt.com^$third-party";
	line += "||adtrix.com^$third-party";
	line += "||adult-adv.com^$third-party";
	line += "||adultadworld.com^$third-party";
	line += "||adversalservers.com^$third-party";
	line += "||adverserve.net^$third-party";
	line += "||advertarium.com.ua^$third-party";
	line += "||adverticum.net^$third-party";
	line += "||advertise.com^$third-party";
	line += "||advertiseyourgame.com^$third-party";
	line += "||advertising-department.com^$third-party";
	line += "||advertising.com^$third-party";
	line += "||advertisingiq.com^$third-party";
	line += "||advertlets.com^$third-party";
	line += "||advertpay.net^$third-party";
	line += "||advertserve.com^$third-party";
	line += "||advertstatic.com^$third-party";
	line += "||advg.jp/$third-party";
	line += "||advgoogle.com^$third-party";
	line += "||adviva.net^$third-party";
	line += "||advmaker.ru^$third-party";
	line += "||advmd.com^$third-party";
	line += "||advpoints.com^$third-party";
	line += "||adworldmedia.com^$third-party";
	line += "||adxpower.com^$third-party";
	line += "||adyoz.com^$third-party";
	line += "||adzerk.net^$third-party";
	line += "||afcyhf.com^$third-party";
	line += "||affiliate.com^$third-party";
	line += "||affiliate.cx^$third-party";
	line += "||affiliatefuel.com^$third-party";
	line += "||affiliatefuture.com^$third-party";
	line += "||affiliatelounge.com^$third-party";
	line += "||affiliatemembership.com^$third-party";
	line += "||affiliatesensor.com^$third-party";
	line += "||affiliproducts.com^$third-party";
	line += "||affinity.com^$third-party";
	line += "||afterdownload.com^$third-party";
	line += "||afy11.net^$third-party";
	line += "||agentcenters.com^$third-party";
	line += "||aggregateknowledge.com^$third-party";
	line += "||aim4media.com^$third-party";
	line += "||aimatch.com^$third-party";
	line += "||ajansreklam.net^$third-party";
	line += "||alimama.cn^$third-party";
	line += "||alphagodaddy.com^$third-party";
	line += "||amgdgt.com^$third-party";
	line += "||ampxchange.com^$third-party";
	line += "||anrdoezrs.net^$third-party";
	line += "||apmebf.com^$third-party";
	line += "||arcade-advertisement.com^$third-party";
	line += "||arcadebannerexchange.net^$third-party";
	line += "||arcadebanners.com^$third-party";
	line += "||arcadebe.com^$third-party";
	line += "||arti-mediagroup.com^$third-party";
	line += "||as5000.com^$third-party";
	line += "||asklots.com^$third-party";
	line += "||assetize.com^$third-party";
	line += "||assoc-amazon.co.uk^$third-party";
	line += "||assoc-amazon.com^$third-party";
	line += "||atdmt.com^$third-party";
	line += "||atmalinks.com^$third-party";
	line += "||atwola.com^$third-party";
	line += "||audienceprofiler.com^$third-party";
	line += "||auditude.com^$third-party";
	line += "||auspipe.com^$third-party";
	line += "||automateyourlist.com^$third-party";
	line += "||avads.co.uk^$third-party";
	line += "||avantlink.com^$third-party";
	line += "||awaps.net^$third-party";
	line += "||awin1.com^$third-party";
	line += "||awltovhc.com^$third-party";
	line += "||axill.com^$third-party";
	line += "||azads.com^$third-party";
	line += "||azjmp.com^$third-party";
	line += "||azoogleads.com^$third-party";
	line += "||backbeatmedia.com^$third-party";
	line += "||banner-clix.com^$third-party";
	line += "||bannerbank.ru^$third-party";
	line += "||bannerblasters.com^$third-party";
	line += "||bannercde.com^$third-party";
	line += "||bannerconnect.com^$third-party";
	line += "||bannerconnect.net^$third-party";
	line += "||bannerflux.com^$third-party";
	line += "||bannerjammers.com^$third-party";
	line += "||bannerlot.com^$third-party";
	line += "||bannerrage.com^$third-party";
	line += "||bannersmania.com^$third-party";
	line += "||bannersnack.net^$third-party";
	line += "||bannertgt.com^$third-party";
	line += "||bbelements.com^$third-party";
	line += "||beaconads.com^$third-party";
	line += "||begun.ru^$third-party";
	line += "||belointeractive.com^$third-party";
	line += "||bestcasinopartner.com^$third-party";
	line += "||bestdeals.ws^$third-party";
	line += "||bestfindsite.com^$third-party";
	line += "||bestofferdirect.com^$third-party";
	line += "||bet365affiliates.com^$third-party";
	line += "||bfast.com^$third-party";
	line += "||bidvertiser.com^$third-party";
	line += "||biemedia.com^$third-party";
	line += "||bin-layer.de^$third-party";
	line += "||bin-layer.ru^$third-party";
	line += "||bingo4affiliates.com^$third-party";
	line += "||binlayer.de^$third-party";
	line += "||bittads.com^$third-party";
	line += "||blogads.com^$third-party";
	line += "||bluestreak.com^$third-party";
	line += "||bmanpn.com^$third-party";
	line += "||bnetworx.com^$third-party";
	line += "||bnr.sys.lv^$third-party";
	line += "||boo-box.com^$third-party";
	line += "||boylesportsreklame.com^$third-party";
	line += "||branchr.com^$third-party";
	line += "||bravenetmedianetwork.com^$third-party";
	line += "||bridgetrack.com^$third-party";
	line += "||btrll.com^$third-party";
	line += "||bu520.com^$third-party";
	line += "||buildtrafficx.com^$third-party";
	line += "||burstnet.com^$third-party";
	line += "||buysellads.com^$third-party";
	line += "||buzzparadise.com^$third-party";
	line += "||c-on-text.com^$third-party";
	line += "||c-planet.net^$third-party";
	line += "||c8.net.ua^$third-party";
	line += "||captainad.com^$third-party";
	line += "||casalemedia.com^$third-party";
	line += "||cash4members.com^$third-party";
	line += "||cbclickbank.com^$third-party";
	line += "||cc-dt.com^$third-party";
	line += "||cdna.tremormedia.com^$third-party";
	line += "||cgecwm.org^$third-party";
	line += "||checkm8.com^$third-party";
	line += "||checkmystats.com.au^$third-party";
	line += "||checkoutfree.com^$third-party";
	line += "||chipleader.com^$third-party";
	line += "||chitika.net^$third-party";
	line += "||cjt1.net^$third-party";
	line += "||clash-media.com^$third-party";
	line += "||claxonmedia.com^$third-party";
	line += "||click4free.info^$third-party";
	line += "||clickad.pl^$third-party";
	line += "||clickbooth.com^$third-party";
	line += "||clickexa.com^$third-party";
	line += "||clickexperts.net^$third-party";
	line += "||clickfuse.com^$third-party";
	line += "||clickintext.net^$third-party";
	line += "||clicksor.com^$third-party";
	line += "||clicksor.net^$third-party";
	line += "||clickthrucash.com^$third-party";
	line += "||clixgalore.com^$third-party";
	line += "||coadvertise.com^$third-party";
	line += "||cogsdigital.com^$third-party";
	line += "||collection-day.com^$third-party";
	line += "||collective-media.net^$third-party";
	line += "||come2play.net^$third-party";
	line += "||commission-junction.com^$third-party";
	line += "||commissionmonster.com^$third-party";
	line += "||comscore.com^$third-party";
	line += "||conduit-banners.com^$third-party";
	line += "||connectedads.net^$third-party";
	line += "||connextra.com^$third-party";
	line += "||contenture.com^$third-party";
	line += "||contexlink.se^$third-party";
	line += "||contextuads.com^$third-party";
	line += "||contextweb.com^$third-party";
	line += "||cpaclicks.com^$third-party";
	line += "||cpalead.com^$third-party";
	line += "||cpays.com^$third-party";
	line += "||cpmstar.com^$third-party";
	line += "||cpuim.com^$third-party";
	line += "||cpxinteractive.com^$third-party";
	line += "||crispads.com^$third-party";
	line += "||crowdgravity.com^$third-party";
	line += "||ctasnet.com^$third-party";
	line += "||ctm-media.com^$third-party";
	line += "||ctrhub.com^$third-party";
	line += "||cubics.com^$third-party";
	line += "||d.m3.net^$third-party";
	line += "||dashboardad.net^$third-party";
	line += "||dbbsrv.com^$third-party";
	line += "||decisionmark.com^$third-party";
	line += "||decisionnews.com^$third-party";
	line += "||decknetwork.net^$third-party";
	line += "||deepmetrix.com^$third-party";
	line += "||defaultimg.com^$third-party";
	line += "||deplayer.net^$third-party";
	line += "||destinationurl.com^$third-party";
	line += "||dexplatform.com^$third-party";
	line += "||dgmaustralia.com^$third-party";
	line += "||digitrevenue.com^$third-party";
	line += "||dinclinx.com^$third-party";
	line += "||directorym.com^$third-party";
	line += "||directtrack.com^$third-party";
	line += "||dl-rms.com^$third-party";
	line += "||domainsponsor.com^$third-party";
	line += "||dotomi.com^$third-party";
	line += "||doubleclick.net/ad/sevenload.*.smartclip/video;$object_subrequest";
	line += "||doubleclick.net/adx/*.collegehumor/$object_subrequest,third-party";
	line += "||doubleclick.net/pfadx/*.mtvi$object_subrequest,third-party";
	line += "||doubleclick.net/pfadx/*.sevenload.com_$object_subrequest";
	line += "||doubleclick.net/pfadx/*adcat=$object_subrequest,third-party";
	line += "||doubleclick.net^$object_subrequest,third-party,domain=addictinggames.com|atom.com|break.com|businessweek.com|cbs4denver.com|cnbc.com|darkhorizons.com|doubleviking.com|eonline.com|fandango.com|foxbusiness.com|foxnews.com|g4tv.com|joblo.com|mtv.co.uk|mtv.com|mtv.com.au|mtv.com.nz|mtvbase.com|mtvmusic.com|myfoxorlando.com|myfoxphoenix.com|newsweek.com|nick.com|nintendoeverything.com|pandora.com|play.it|ps3news.com|rte.ie|sbsun.com|sevenload.com|shockwave.com|southpark.nl|space.com|spike.com|thedailygreen.com|thedailyshow.com|thewire.com|ustream.tv|washingtonpost.com|wcbstv.com|wired.com|wkbw.com|wsj.com|wwe.com|youtube.com|zoomin.tv";
	line += "||doubleclick.net^$~object_subrequest,third-party";
	line += "||doubleclick.net^*;sz=$object_subrequest,third-party,domain=1up.com|breitbart.tv|digitaltrends.com|gamesradar.com|gametrailers.com|heavy.com|myfoxny.com|myspace.com|nbc.com|nfl.com|nhl.com|wptv.com";
	line += "||dpbolvw.net^$third-party";
	line += "||dt00.net^$domain=~marketgid.com|~mgid.com|~thechive.com";
	line += "||dt07.net^$domain=~marketgid.com|~mgid.com|~thechive.com";
	line += "||e-planning.net^$third-party";
	line += "||easyhits4u.com^$third-party";
	line += "||ebannertraffic.com^$third-party";
	line += "||ebayobjects.com.au^$third-party";
	line += "||ebayobjects.com^$third-party";
	line += "||edge-dl.andomedia.com^$third-party";
	line += "||egamingonline.com^$third-party";
	line += "||ekmas.com^$third-party";
	line += "||emediate.eu^$third-party";
	line += "||emediate.se^$third-party";
	line += "||engineseeker.com^$third-party";
	line += "||ero-advertising.com^$third-party";
	line += "||etology.com^$third-party";
	line += "||euroclick.com^$third-party";
	line += "||euros4click.de^$third-party";
	line += "||exelator.com^$third-party";
	line += "||exitexplosion.com^$third-party";
	line += "||exitjunction.com^$third-party";
	line += "||exponential.com^$third-party";
	line += "||eyereturn.com^$third-party";
	line += "||eyewonder.com^$third-party";
	line += "||fairadsnetwork.com^$third-party";
	line += "||fairfax.com.au^$~stylesheet,third-party";
	line += "||falkag.net^$third-party";
	line += "||fastclick.net^$third-party";
	line += "||fimserve.com^$third-party";
	line += "||findsthat.com^$third-party";
	line += "||firstadsolution.com^$third-party";
	line += "||firstlightera.com^$third-party";
	line += "||fixionmedia.com^$third-party";
	line += "||flashtalking.com^$third-party";
	line += "||fluxads.com^$third-party";
	line += "||fmpub.net^$third-party";
	line += "||footerslideupad.com^$third-party";
	line += "||forexyard.com^$third-party";
	line += "||forrestersurveys.com^$third-party";
	line += "||freebannerswap.co.uk^$third-party";
	line += "||freelancer.com^$third-party";
	line += "||friendlyduck.com^$third-party";
	line += "||ftjcfx.com^$third-party";
	line += "||funklicks.com^$third-party";
	line += "||fusionads.net^$third-party";
	line += "||fwmrm.net^$third-party";
	line += "||g.doubleclick.net^$third-party";
	line += "||gambling-affiliation.com^$third-party";
	line += "||game-advertising-online.com^$third-party";
	line += "||gameads.com^$third-party";
	line += "||gamecetera.com^$third-party";
	line += "||gamersbanner.com^$third-party";
	line += "||gannett.gcion.com^$third-party";
	line += "||gate-ru.com^$third-party";
	line += "||geek2us.net^$third-party";
	line += "||geo-idm.fr^$third-party";
	line += "||geopromos.com^$third-party";
	line += "||gestionpub.com^$third-party";
	line += "||ggncpm.com^$third-party";
	line += "||gimiclub.com^$third-party";
	line += "||gklmedia.com^$third-party";
	line += "||globaladsales.com^$third-party";
	line += "||globaladv.net^$third-party";
	line += "||gmads.net^$third-party";
	line += "||go2media.org^$third-party";
	line += "||googleadservices.com^$third-party";
	line += "||grabmyads.com^$third-party";
	line += "||gratisnetwork.com^$third-party";
	line += "||guardiandigitalcomparison.co.uk^$third-party";
	line += "||gumgum.com^$third-party";
	line += "||halogennetwork.com^$third-party";
	line += "||havamedia.net^$third-party";
	line += "||hb-247.com^$third-party";
	line += "||hit-now.com^$third-party";
	line += "||hits.sys.lv^$third-party";
	line += "||hopfeed.com^$third-party";
	line += "||hosticanaffiliate.com^$third-party";
	line += "||hot-hits.us^$third-party";
	line += "||hotptp.com^$third-party";
	line += "||httpool.com^$third-party";
	line += "||hypemakers.net^$third-party";
	line += "||hypervre.com^$third-party";
	line += "||ibatom.com^$third-party";
	line += "||icdirect.com^$third-party";
	line += "||imagesatlantic.com^$third-party";
	line += "||imedia.co.il^$third-party";
	line += "||imglt.com^$third-party";
	line += "||imho.ru/$third-party";
	line += "||imiclk.com^$third-party";
	line += "||impact-ad.jp^$third-party";
	line += "||impresionesweb.com^$third-party";
	line += "||indiabanner.com^$third-party";
	line += "||indiads.com^$third-party";
	line += "||indianbannerexchange.com^$third-party";
	line += "||indianlinkexchange.com^$third-party";
	line += "||industrybrains.com^$third-party";
	line += "||inetinteractive.com^$third-party";
	line += "||infinite-ads.com^$third-party";
	line += "||influads.com^$third-party";
	line += "||infolinks.com^$third-party";
	line += "||information-sale.com^$third-party";
	line += "||innity.com^$third-party";
	line += "||insightexpressai.com^$third-party";
	line += "||inskinad.com^$third-party";
	line += "||inskinmedia.com^$third-party";
	line += "||instantbannercreator.com^$third-party";
	line += "||intellibanners.com^$third-party";
	line += "||intellitxt.com^$third-party";
	line += "||interclick.com^$third-party";
	line += "||interpolls.com^$third-party";
	line += "||inuvo.com^$third-party";
	line += "||investingchannel.com^$third-party";
	line += "||ipromote.com^$third-party";
	line += "||jangonetwork.com^$third-party";
	line += "||jdoqocy.com^$third-party";
	line += "||jsfeedadsget.com^$third-party";
	line += "||jumboaffiliates.com^$third-party";
	line += "||justrelevant.com^$third-party";
	line += "||kalooga.com^$third-party";
	line += "||kanoodle.com^$third-party";
	line += "||kavanga.ru^$third-party";
	line += "||kehalim.com^$third-party";
	line += "||kerg.net^$third-party";
	line += "||ketoo.com^$third-party";
	line += "||kitnmedia.com^$third-party";
	line += "||klikvip.com^$third-party";
	line += "||klipmart.com^$third-party";
	line += "||kontera.com^$third-party";
	line += "||kqzyfj.com^$third-party";
	line += "||lakequincy.com^$third-party";
	line += "||lduhtrp.net^$third-party";
	line += "||leadacceptor.com^$third-party";
	line += "||liftdna.com^$third-party";
	line += "||ligatus.com^$third-party";
	line += "||lightningcast.net^$~object_subrequest,third-party";
	line += "||lingospot.com^$third-party";
	line += "||linkbucks.com^$third-party";
	line += "||linkbuddies.com^$third-party";
	line += "||linkexchange.com^$third-party";
	line += "||linkreferral.com^$third-party";
	line += "||linkshowoff.com^$third-party";
	line += "||linkstorm.net^$third-party";
	line += "||linksynergy.com^$third-party";
	line += "||linkworth.com^$third-party";
	line += "||linkz.net^$third-party";
	line += "||liverail.com^$third-party";
	line += "||liveuniversenetwork.com^$third-party";
	line += "||looksmart.com^$third-party";
	line += "||ltassrv.com.s3.amazonaws.com^$third-party";
	line += "||ltassrv.com^$third-party";
	line += "||lzjl.com^$third-party";
	line += "||madisonlogic.com^$third-party";
	line += "||markethealth.com^$third-party";
	line += "||marketingsolutions.yahoo.com^$third-party";
	line += "||marketnetwork.com^$third-party";
	line += "||maxserving.com^$third-party";
	line += "||mb01.com^$third-party";
	line += "||mbn.com.ua^$third-party";
	line += "||media6degrees.com^$third-party";
	line += "||mediag4.com^$third-party";
	line += "||mediagridwork.com^$third-party";
	line += "||medialand.ru^$third-party";
	line += "||medialation.net^$third-party";
	line += "||mediaonenetwork.net^$third-party";
	line += "||mediaplex.com^$third-party";
	line += "||mediatarget.com^$third-party";
	line += "||medleyads.com^$third-party";
	line += "||medrx.sensis.com.au^$third-party";
	line += "||meetic-partners.com^$third-party";
	line += "||megaclick.com^$third-party";
	line += "||mercuras.com^$third-party";
	line += "||metaffiliation.com^$third-party";
	line += "||mezimedia.com^$third-party";
	line += "||microsoftaffiliates.net^$third-party";
	line += "||milabra.com^$third-party";
	line += "||mirago.com^$third-party";
	line += "||miva.com^$third-party";
	line += "||mixpo.com^$third-party";
	line += "||mktseek.com^$third-party";
	line += "||money4ads.com^$third-party";
	line += "||mookie1.com^$third-party";
	line += "||mootermedia.com^$third-party";
	line += "||moregamers.com^$third-party";
	line += "||moreplayerz.com^$third-party";
	line += "||mpression.net^$third-party";
	line += "||msads.net^$third-party";
	line += "||nabbr.com^$third-party";
	line += "||nbjmp.com^$third-party";
	line += "||nbstatic.com^$third-party";
	line += "||neodatagroup.com^$third-party";
	line += "||neoffic.com^$third-party";
	line += "||net3media.com^$third-party";
	line += "||netavenir.com^$third-party";
	line += "||netseer.com^$third-party";
	line += "||networldmedia.net^$third-party";
	line += "||newsadstream.com^$third-party";
	line += "||newtention.net^$third-party";
	line += "||nexac.com^$third-party";
	line += "||nicheads.com^$third-party";
	line += "||nobleppc.com^$third-party";
	line += "||northmay.com^$third-party";
	line += "||nowlooking.net^$third-party";
	line += "||nvero.net^$third-party";
	line += "||nyadmcncserve-05y06a.com^$third-party";
	line += "||obeus.com^$third-party";
	line += "||obibanners.com^$third-party";
	line += "||objects.tremormedia.com^$~object_subrequest,third-party";
	line += "||objectservers.com^$third-party";
	line += "||oclus.com^$third-party";
	line += "||omg2.com^$third-party";
	line += "||omguk.com^$third-party";
	line += "||onads.com^$third-party";
	line += "||onenetworkdirect.net^$third-party";
	line += "||onlineadtracker.co.uk^$third-party";
	line += "||opensourceadvertisementnetwork.info^$third-party";
	line += "||openx.com^$third-party";
	line += "||openx.net^$third-party";
	line += "||openx.org^$third-party";
	line += "||opinionbar.com^$third-party";
	line += "||othersonline.com^$third-party";
	line += "||overture.com^$third-party";
	line += "||oxado.com^$third-party";
	line += "||p-advg.com^$third-party";
	line += "||pagead2.googlesyndication.com^$~object_subrequest,third-party";
	line += "||pakbanners.com^$third-party";
	line += "||paperg.com^$third-party";
	line += "||partner.video.syndication.msn.com^$~object_subrequest,third-party";
	line += "||partypartners.com^$third-party";
	line += "||payperpost.com^$third-party";
	line += "||pc-ads.com^$third-party";
	line += "||peer39.net^$third-party";
	line += "||pepperjamnetwork.com^$third-party";
	line += "||perfb.com^$third-party";
	line += "||performancingads.com^$third-party";
	line += "||pgmediaserve.com^$third-party";
	line += "||pgpartner.com^$third-party";
	line += "||pheedo.com^$third-party";
	line += "||picadmedia.com^$third-party";
	line += "||pinballpublishernetwork.com^$third-party";
	line += "||pixazza.com^$third-party";
	line += "||platinumadvertisement.com^$third-party";
	line += "||playertraffic.com^$third-party";
	line += "||pmsrvr.com^$third-party";
	line += "||pntra.com^$third-party";
	line += "||pntrac.com^$third-party";
	line += "||pntrs.com^$third-party";
	line += "||pointroll.com^$third-party";
	line += "||popads.net^$third-party";
	line += "||popadscdn.net^$third-party";
	line += "||ppclinking.com^$third-party";
	line += "||precisionclick.com^$third-party";
	line += "||predictad.com^$third-party";
	line += "||primaryads.com^$third-party";
	line += "||pro-advertising.com^$third-party";
	line += "||pro-market.net^$third-party";
	line += "||proadsdirect.com^$third-party";
	line += "||probannerswap.com^$third-party";
	line += "||prod.untd.com^$third-party";
	line += "||profitpeelers.com^$third-party";
	line += "||projectwonderful.com^$third-party";
	line += "||proximic.com^$third-party";
	line += "||psclicks.com^$third-party";
	line += "||ptp.lolco.net^$third-party";
	line += "||pubmatic.com^$third-party";
	line += "||pulse360.com^$third-party";
	line += "||qksrv.net^$third-party";
	line += "||qksz.net^$third-party";
	line += "||questionmarket.com^$third-party";
	line += "||questus.com^$third-party";
	line += "||quisma.com^$third-party";
	line += "||radiusmarketing.com^$third-party";
	line += "||rapt.com^$third-party";
	line += "||rbcdn.com^$third-party";
	line += "||realclick.co.kr^$third-party";
	line += "||realmedia.com^$third-party";
	line += "||reelcentric.com^$third-party";
	line += "||reklamz.com^$third-party";
	line += "||resultlinks.com^$third-party";
	line += "||revenuegiants.com^$third-party";
	line += "||revfusion.net^$third-party";
	line += "||revresda.com^$third-party";
	line += "||ricead.com^$third-party";
	line += "||ringtonematcher.com^$third-party";
	line += "||rmxads.com^$third-party";
	line += "||roirocket.com^$third-party";
	line += "||rotatingad.com^$third-party";
	line += "||rovion.com^$third-party";
	line += "||ru4.com/$third-party";
	line += "||rubiconproject.com^$third-party";
	line += "||rwpads.com^$third-party";
	line += "||sa.entireweb.com^$third-party";
	line += "||safelistextreme.com^$third-party";
	line += "||salvador24.com^$third-party";
	line += "||saple.net^$third-party";
	line += "||sbaffiliates.com^$third-party";
	line += "||scanscout.com^$third-party";
	line += "||search123.uk.com^$third-party";
	line += "||securewebsiteaccess.com^$third-party";
	line += "||sendptp.com^$third-party";
	line += "||servali.net^$third-party";
	line += "||sev4ifmxa.com^$third-party";
	line += "||sexmoney.com^$third-party";
	line += "||shareasale.com^$third-party";
	line += "||shareresults.com^$third-party";
	line += "||shinobi.jp^$third-party";
	line += "||simply.com^$third-party";
	line += "||siteencore.com^$third-party";
	line += "||skimlinks.com^$third-party";
	line += "||skimresources.com^$third-party";
	line += "||skoovyads.com^$third-party";
	line += "||smart.allocine.fr$third-party";
	line += "||smart2.allocine.fr^$third-party";
	line += "||smartadserver.com^$third-party";
	line += "||smarttargetting.co.uk^$third-party";
	line += "||smarttargetting.com^$third-party";
	line += "||smarttargetting.net^$third-party";
	line += "||smpgfx.com^$third-party";
	line += "||snap.com^$third-party";
	line += "||so-excited.com^$third-party";
	line += "||sochr.com^$third-party";
	line += "||sociallypublish.com^$third-party";
	line += "||socialmedia.com^$third-party";
	line += "||socialspark.com^$third-party";
	line += "||softonicads.com^$third-party";
	line += "||sonnerie.net^$third-party";
	line += "||sparkstudios.com^$third-party";
	line += "||specificclick.net^$third-party";
	line += "||specificmedia.com^$third-party";
	line += "||speedsuccess.net^$third-party";
	line += "||spinbox.freedom.com^$third-party";
	line += "||sponsorads.de^$third-party";
	line += "||sponsoredtweets.com^$third-party";
	line += "||sponsormob.com^$third-party";
	line += "||sponsorpalace.com^$third-party";
	line += "||sportsyndicator.com^$third-party";
	line += "||spotrails.com^$third-party";
	line += "||spottt.com^$third-party";
	line += "||spotxchange.com^$third-party,domain=~supernovatube.com";
	line += "||sproose.com^$third-party";
	line += "||srtk.net^$third-party";
	line += "||sta-ads.com^$third-party";
	line += "||starlayer.com^$third-party";
	line += "||statcamp.net^$third-party";
	line += "||stocker.bonnint.net^$third-party";
	line += "||struq.com^$third-party";
	line += "||sublimemedia.net^$third-party";
	line += "||supremeadsonline.com^$third-party";
	line += "||survey-poll.com^$third-party";
	line += "||tacoda.net^$third-party";
	line += "||tailsweep.com^$third-party";
	line += "||targetnet.com^$third-party";
	line += "||targetpoint.com^$third-party";
	line += "||targetspot.com^$third-party";
	line += "||teracent.net^$third-party";
	line += "||testnet.nl^$third-party";
	line += "||text-link-ads.com^$third-party";
	line += "||theloungenet.com^$third-party";
	line += "||thewebgemnetwork.com^$third-party";
	line += "||tidaltv.com^$third-party";
	line += "||tiser.com^$third-party";
	line += "||tkqlhce.com^$third-party";
	line += "||topauto10.com^$third-party";
	line += "||total-media.net^$third-party";
	line += "||tqlkg.com^$third-party";
	line += "||tradedoubler.com^$third-party";
	line += "||tradepub.com^$third-party";
	line += "||tradetracker.net^$third-party";
	line += "||trafficbarads.com^$third-party";
	line += "||trafficjunky.net^$third-party";
	line += "||trafficmasterz.net^$third-party";
	line += "||trafficrevenue.net^$third-party";
	line += "||trafficwave.net^$third-party";
	line += "||traveladvertising.com^$third-party";
	line += "||travelscream.com^$third-party";
	line += "||travidia.com^$third-party";
	line += "||triadmedianetwork.com^$third-party";
	line += "||tribalfusion.com^$third-party";
	line += "||trigami.com^$third-party";
	line += "||trker.com^$third-party";
	line += "||tvprocessing.com^$third-party";
	line += "||twinplan.com^$third-party";
	line += "||twittad.com^$third-party";
	line += "||tyroo.com^$third-party";
	line += "||udmserve.net^$third-party";
	line += "||ukbanners.com^$third-party";
	line += "||unanimis.co.uk^$third-party";
	line += "||unicast.com^$third-party";
	line += "||unrulymedia.com^$third-party";
	line += "||usbanners.com^$third-party";
	line += "||usemax.de^$third-party";
	line += "||usenetpassport.com^$third-party";
	line += "||usercash.com^$third-party";
	line += "||utarget.co.uk^$third-party";
	line += "||v.movad.de^*/ad.xml$third-party";
	line += "||validclick.com^$third-party";
	line += "||valuead.com^$third-party";
	line += "||valueclick.com^$third-party";
	line += "||valueclickmedia.com^$third-party";
	line += "||vcmedia.com^$third-party";
	line += "||velmedia.net^$third-party";
	line += "||versetime.com^$third-party";
	line += "||vianadserver.com^$third-party";
	line += "||vibrantmedia.com^$third-party";
	line += "||videoegg.com^$third-party";
	line += "||videostrip.com^$~object_subrequest,third-party";
	line += "||videostrip.com^*/admatcherclient.$object_subrequest,third-party";
	line += "||vidpay.com^$third-party";
	line += "||viglink.com^$third-party";
	line += "||vipquesting.com^$third-party";
	line += "||viraladnetwork.net^$third-party";
	line += "||visitdetails.com^$third-party";
	line += "||vitalads.net^$third-party";
	line += "||vpico.com^$third-party";
	line += "||vs20060817.com^$third-party";
	line += "||vsservers.net^$third-party";
	line += "||webads.co.nz^$third-party";
	line += "||webgains.com^$third-party";
	line += "||webmasterplan.com^$third-party";
	line += "||weborama.fr^$third-party";
	line += "||webtraffic.ttinet.com^$third-party";
	line += "||wgreatdream.com^$third-party";
	line += "||widgetbucks.com^$third-party";
	line += "||widgets.fccinteractive.com^$third-party";
	line += "||wootmedia.net^$third-party";
	line += "||worlddatinghere.com^$third-party";
	line += "||worthathousandwords.com^$third-party";
	line += "||wwbn.com^$third-party";
	line += "||wwwadcntr.com^$third-party";
	line += "||x4300tiz.com^$third-party";
	line += "||xcelltech.com^$third-party";
	line += "||xcelsiusadserver.com^$third-party";
	line += "||xchangebanners.com^$third-party";
	line += "||xgraph.net^$third-party";
	line += "||yceml.net^$third-party";
	line += "||yesnexus.com^$third-party";
	line += "||yieldbuild.com^$third-party";
	line += "||yieldmanager.com^$third-party";
	line += "||yieldmanager.net^$third-party";
	line += "||yldmgrimg.net^$third-party";
	line += "||yottacash.com^$third-party";
	line += "||yumenetworks.com^$third-party";
	line += "||zangocash.com^$third-party";
	line += "||zanox.com^$third-party";
	line += "||zeads.com^$third-party";
	line += "||zedo.com^$third-party";
	line += "||zoomdirect.com.au^$third-party";
	line += "||zxxds.net^$third-party";
	line += "!Mobile";
	line += "||admob.com^$third-party";
	line += "||adwhirl.com^$third-party";
	line += "||adzmob.com^$third-party";
	line += "||amobee.com^$third-party";
	line += "||mkhoj.com^$third-party";
	line += "||mojiva.com^$third-party";
	line += "||smaato.net^$third-party";
	line += "||waptrick.com^$third-party";
	line += "!-----------------Third-party adverts-----------------!";
	line += "! *** easylist_thirdparty.txt ***";
	line += "||208.43.84.120/trueswordsa3.gif$third-party";
	line += "||21nova.com/promodisplay?";
	line += "||770.com/banniere.php?";
	line += "||a.ucoz.net^";
	line += "||ablacrack.com/popup-pvd.js$third-party";
	line += "||adn.ebay.com^";
	line += "||ads.mp.mydas.mobi^";
	line += "||adserver-live.yoc.mobi^";
	line += "||adstil.indiatimes.com^";
	line += "||adultfriendfinder.com/banners/$third-party";
	line += "||adultfriendfinder.com/go/page/js_im_box?$third-party";
	line += "||advanced-intelligence.com/banner";
	line += "||affil.mupromo.com^";
	line += "||affiliate.astraweb.com^";
	line += "||affiliates.a2hosting.com^";
	line += "||affiliates.bravenet.com^";
	line += "||affiliates.generatorsoftware.com^";
	line += "||affiliates.hotelclub.com^";
	line += "||affiliates.jlist.com^";
	line += "||affiliates.supergreenhosting.com^";
	line += "||affiliation.fotovista.com^";
	line += "||affutdmedia.com^$third-party";
	line += "||allsend.com/public/assets/images/";
	line += "||allsolutionsnetwork.com/banners/";
	line += "||aolcdn.com/os/music/img/*-skin.jpg";
	line += "||apple.com/itunesaffiliates/";
	line += "||appwork.org/hoster/banner_$image";
	line += "||autoprivileges.net/news/";
	line += "||award.sitekeuring.net^";
	line += "||aweber.com/banners/";
	line += "||b.livesport.eu^";
	line += "||b.sell.com^$third-party";
	line += "||b92.putniktravel.com^";
	line += "||b92s.net/images/banners/";
	line += "||babylon.com/trans_box/*&affiliate=";
	line += "||banner.1and1.com^";
	line += "||banner.3ddownloads.com^";
	line += "||banner.telefragged.com^";
	line += "||banners.adultfriendfinder.com^$third-party";
	line += "||banners.cams.com^";
	line += "||banners.friendfinder.com^";
	line += "||banners.getiton.com^";
	line += "||banners.ixitools.com^";
	line += "||banners.penthouse.com^";
	line += "||banners.smarttweak.com^";
	line += "||banners.virtuagirlhd.com^";
	line += "||bc.coupons.com^$third-party";
	line += "||bet-at-home.com/oddbanner.aspx?";
	line += "||beta.down2crazy.com^$third-party";
	line += "||bigcdn.com^*/adttext.swf";
	line += "||bijk.com^*/banners/";
	line += "||bittorrent.am/serws.php?$third-party";
	line += "||blissful-sin.com/affiliates/";
	line += "||box.anchorfree.net^";
	line += "||bplaced.net/pub/";
	line += "||bravenet.com/cserv.php?";
	line += "||break.com^*/partnerpublish/";
	line += "||btguard.com/images/$third-party";
	line += "||bullguard.com^*/banners/";
	line += "||buy.com^*/affiliate/";
	line += "||buzznet.com^*/showpping-banner-$third-party";
	line += "||cas.clickability.com^";
	line += "||cash.neweramediaworks.com^";
	line += "||cashmakingpowersites.com^*/banners/";
	line += "||cashmyvideo.com/images/cashmyvideo_banner.gif";
	line += "||cazoz.com/banner.php";
	line += "||cbanners.virtuagirlhd.com^$third-party";
	line += "||cdn.sweeva.com/images/$third-party";
	line += "||challies.com^*/wtsbooks5.png$third-party";
	line += "||cimg.in/images/banners/";
	line += "||connect.summit.co.uk^";
	line += "||counter-strike.com/banners/";
	line += "||creatives.summitconnect.co.uk^";
	line += "||dapatwang.com/images/banner/";
	line += "||datakl.com/banner/";
	line += "||desi4m.com/desi4m.gif$third-party";
	line += "||dynw.com/banner";
	line += "||enticelabs.com/el/";
	line += "||entrecard.com/static/banners/";
	line += "||eplreplays.com/wl/";
	line += "||esport-betting.com^*/betbanner/";
	line += "||everestpoker.com^*/?adv=";
	line += "||facebook.com/whitepages/wpminiprofile.php?partner_id=$third-party";
	line += "||fantaz.com^*/banners/$third-party";
	line += "||fapturbo.com/testoid/";
	line += "||farmholidays.is/iframeallfarmsearch.aspx?$third-party";
	line += "||feedburner.com/~a/";
	line += "||filedownloader.net/design/$third-party";
	line += "||filesonic.com^*/banners/";
	line += "||flipchat.com/index.php?$third-party";
	line += "||forms.aweber.com/form/styled_popovers_and_lightboxes.js$third-party";
	line += "||fragfestservers.com/bannerb.gif";
	line += "||freakshare.net/banner/";
	line += "||free-football.tv/images/usd/";
	line += "||frogatto.com/images/$third-party";
	line += "||frontsight.com^*/banners/";
	line += "||fugger.netfirms.com/moa.swf$third-party";
	line += "||futuresite.register.com/us?$third-party";
	line += "||gamersaloon.com/images/banners/";
	line += "||gamestop.com^*/aflbanners/";
	line += "||gawkerassets.com/assets/marquee/$object,third-party";
	line += "||gfxa.sheetmusicplus.com^$third-party";
	line += "||ggmania.com^*.jpg$third-party";
	line += "||giganews.com/banners/$third-party";
	line += "||gogousenet.com^*/promo.cgi";
	line += "||googlesyndication.com^*/domainpark.cgi?";
	line += "||graboid.com/affiliates/";
	line += "||graduateinjapan.com/affiliates/";
	line += "||grammar.coursekey.com/inter/$third-party";
	line += "||gsniper.com/images/$third-party";
	line += "||hostingcatalog.com/banner.php?";
	line += "||idg.com.au/ggg/images/*_home.jpg$third-party";
	line += "||idownloadunlimited.com/aff-exchange/";
	line += "||ifilm.com/website/*_skin_$third-party";
	line += "||ign.com/js.ng/";
	line += "||image.com.com^*/skin2.jpg$third-party";
	line += "||img.mybet.com^$third-party";
	line += "||iol.co.za^*/sponsors/";
	line += "||iselectmedia.com^*/banners/";
	line += "||jimdo.com/s/img/aff/";
	line += "||jlist.com/feed.php?affid=$third-party";
	line += "||joylandcasino.com/promoredirect?$third-party";
	line += "||justcutegirls.com/banners/$third-party";
	line += "||kaango.com/fecustomwidgetdisplay?";
	line += "||kallout.com^*.php?id=";
	line += "||keyword-winner.com/demo/images/";
	line += "||krillion.com^*/productoffers.js";
	line += "||l.yimg.com^*&partner=*&url=";
	line += "||ladbrokes.com^*&aff_id=";
	line += "||lastlocation.com/images/banner";
	line += "||lego.com^*/affiliate/";
	line += "||letters.coursekey.com/lettertemplates_$third-party";
	line += "||liutilities.com/partners/affiliate/";
	line += "||livejasmin.com/?t_id=*&psid=$third-party";
	line += "||longtailvideo.com/ltas.swf$third-party";
	line += "||lowbird.com/random/$third-party";
	line += "||marketing.888.com^";
	line += "||marketsamurai.com/affiliate/";
	line += "||mastiway.com/webimages/$third-party";
	line += "||match.com^*/prm/$third-party";
	line += "||mazda.com.au/banners/";
	line += "||media-toolbar.com^$third-party";
	line += "||media.onlineteachers.co.in^$third-party";
	line += "||meta4-group.com^*/promotioncorner.js?";
	line += "||metaboli.fr^*/adgude_$third-party";
	line += "||mfeed.newzfind.com^$third-party";
	line += "||missnowmrs.com/images/banners/";
	line += "||mto.mediatakeout.com^$third-party";
	line += "||my-dirty-hobby.com/getmdhlink.$third-party";
	line += "||mydirtyhobby.com/?sub=$third-party";
	line += "||mydirtyhobby.com/banner/$third-party";
	line += "||mydirtyhobby.com/custom/$third-party";
	line += "||mydirtyhobby.com/getmdhlink.$third-party";
	line += "||mydirtyhobby.com/gpromo/$third-party";
	line += "||mydirtyhobby.com^*.php?*&goto=join$third-party";
	line += "||mydirtyhobby.com^*/gpromo/$third-party";
	line += "||myfreepaysite.info^*.gif$third-party";
	line += "||myfreeresources.com/getimg.php?$third-party";
	line += "||myhpf.co.uk/banners/";
	line += "||mytrafficstrategy.com/images/$third-party";
	line += "||myusenet.net/promo.cgi?";
	line += "||netload.in^*?refer_id=";
	line += "||nzpages.co.nz^*/banners/";
	line += "||nzphoenix.com/nzgamer/$third-party";
	line += "||onegameplace.com/iframe.php$third-party";
	line += "||oriongadgets.com^*/banners/";
	line += "||partner.bargaindomains.com^";
	line += "||partner.catchy.com^";
	line += "||partner.premiumdomains.com^";
	line += "||partners.agoda.com^";
	line += "||partners.dogtime.com/network/";
	line += "||partycasino.com^*?wm=$third-party";
	line += "||partypoker.com/hp_landingpages/$third-party";
	line += "||partypoker.com^*?wm=$third-party";
	line += "||pcash.imlive.com^$third-party";
	line += "||play-asia.com/paos-$third-party";
	line += "||pokerstars.com/euro_bnrs/";
	line += "||pop6.com/banners/";
	line += "||pornturbo.com/tmarket.php";
	line += "||ppc-coach.com/jamaffiliates/";
	line += "||pricegrabber.com/cb_table.php$third-party";
	line += "||pricegrabber.com/mlink.php?$third-party";
	line += "||promo.bauermedia.co.uk^";
	line += "||promos.fling.com^";
	line += "||promote.pair.com^";
	line += "||proxies2u.com/images/btn/$third-party";
	line += "||proxyroll.com/proxybanner.php";
	line += "||pub.betclick.com^";
	line += "||pubs.hiddennetwork.com^";
	line += "||qiksilver.net^*/banners/";
	line += "||radiocentre.ca/randomimages/$third-party";
	line += "||radioshack.com^*/promo/";
	line += "||rapidjazz.com/banner_rotation/";
	line += "||rcm*.amazon.$third-party";
	line += "||redvase.bravenet.com^$third-party";
	line += "||regnow.com/vendor/";
	line += "||robofish.com/cgi-bin/banner.cgi?";
	line += "||sayswap.com/banners/";
	line += "||searchportal.information.com/?$third-party";
	line += "||secondspin.com/twcontent/";
	line += "||sfimg.com/images/banners/";
	line += "||shaadi.com^*/get-banner.php?";
	line += "||shareflare.net/images/$third-party";
	line += "||shop-top1000.com/images/";
	line += "||shop4tech.com^*/banner/";
	line += "||shragle.com^*?ref=";
	line += "||singlemuslim.com/affiliates/";
	line += "||sitegrip.com^*/swagbucks-";
	line += "||skykingscasino.com/promoloaddisplay?";
	line += "||slickdeals.meritline.com^";
	line += "||smartclip.net/delivery/tag?";
	line += "||smilepk.com/bnrsbtns/";
	line += "||snapdeal.com^*.php$third-party";
	line += "||splashpagemaker.com/images/$third-party";
	line += "||stats.sitesuite.org^";
	line += "||stockroom.com/banners/";
	line += "||storage.to/affiliate/";
	line += "||sweed.to/affiliates/";
	line += "||sweeva.com/widget.php?w=$third-party";
	line += "||swiftco.net/banner/";
	line += "||theatm.info/images/$third-party";
	line += "||thebigchair.com.au^$subdocument,third-party";
	line += "||themes420.com/bnrsbtns/";
	line += "||themis-media.com^*/sponsorships/";
	line += "||ticketmaster.com/promotionalcontent/";
	line += "||tigerdirect.com^*/affiliate_";
	line += "||top5result.com/promo/";
	line += "||toptenreviews.com/widgets/af_widget.js$third-party";
	line += "||torrentfreebie.com/index.asp?pid=$third-party";
	line += "||tosol.co.uk/international.php?$third-party";
	line += "||toysrus.com/graphics/promo/";
	line += "||travelmail.traveltek.net^$third-party";
	line += "||turbotrafficsystem.com^*/banners/";
	line += "||twivert.com/external/banner234x60.";
	line += "||u-loader.com/image/hotspot_";
	line += "||unsereuni.at/resources/img/$third-party";
	line += "||valuate.com/banners/";
	line += "||veospot.com^*.html";
	line += "||videodetective.net/flash/players/plugins/iva_adaptvad.swf";
	line += "||videoplaza.com/creatives/";
	line += "||visit.homepagle.com^$third-party";
	line += "||visitorboost.com/images/$third-party";
	line += "||website.ws^*/banners/";
	line += "||williamhill.com/promoloaddisplay?";
	line += "||williamhillcasino.com/promoredirect?";
	line += "||wonderlabs.com/affiliate_pro/banners/";
	line += "||ws.amazon.*/widgets/q?$third-party";
	line += "||xgaming.com/rotate*.php?$third-party";
	line += "||xml.exactseek.com/cgi-bin/js-feed.cgi?$third-party";
	line += "!Preliminary third-party adult section";
	line += "||awempire.com/ban/$third-party";
	line += "||hotcaracum.com/banner/$third-party";
	line += "!Mobile";
	line += "||iadc.qwapi.com^";
	line += "!-----------------Specific advert blocking filters-----------------!";
	line += "! *** easylist_specific_block.txt ***";
	line += "||1057theoasis.com/addrotate_content.php?";
	line += "||1079thealternative.com/addrotate_content.php?";
	line += "||174.143.241.129^$domain=astalavista.com";
	line += "||1up.com^*/promos/";
	line += "||216.151.186.5^*/serve.php?$domain=sendspace.com";
	line += "||4chan.org/support/";
	line += "||5min.com^*/banners/";
	line += "||77.247.178.36/layer/$domain=movie2k.com";
	line += "||84.234.22.104/ads/$domain=tvcatchup.com";
	line += "||85.17.254.150^*.php?$domain=wiretarget.com";
	line += "||87.230.102.24/ads/";
	line += "||87.230.102.24/gads/";
	line += "||911tabs.com/img/takeover_app_";
	line += "||911tabs.com^*/ringtones_overlay.js";
	line += "||963kklz.com/addrotate_content.php?";
	line += "||9news.com/promo/";
	line += "||a.giantrealm.com^";
	line += "||a.thefreedictionary.com^";
	line += "||a7.org/info_en/";
	line += "||about.com/0g/$subdocument";
	line += "||abovetopsecret.com/300_";
	line += "||ac2.msn.com^";
	line += "||access.njherald.com^";
	line += "||activewin.com^*/blaze_static2.gif";
	line += "||adelaidecityfc.com.au/oak.swf";
	line += "||adpaths.com/_aspx/cpcinclude.aspx?";
	line += "||ads.readwriteweb.com^";
	line += "||adshare.freedocast.com^";
	line += "||adultswim.com^*/admanager.swf?";
	line += "||adv.letitbit.net^";
	line += "||advt.manoramaonline.com^";
	line += "||akipress.com/_ban/";
	line += "||akipress.org/ban/";
	line += "||akipress.org/bimages/";
	line += "||allmovieportal.com/dynbanner.php?";
	line += "||allthelyrics.com^*/popup.js";
	line += "||analytics.mmosite.com^";
	line += "||androidpit.com/app-seller/app-seller.swf?xmlpath=$object";
	line += "||anime-source.com/banzai/banner.$subdocument";
	line += "||animekuro.com/layout/google$subdocument";
	line += "||animenewsnetwork.com^*.aframe?";
	line += "||aniscartujo.com^*/layer.js";
	line += "||anonib.com/zimages/";
	line += "||anti-leech.com/al.php?";
	line += "||armorgames.com^*/banners/";
	line += "||armorgames.com^*/site-skins/";
	line += "||armorgames.com^*/siteskin.css";
	line += "||artima.com/zcr/";
	line += "||asianewsnet.net/banner/";
	line += "||astalavista.com/avtng/";
	line += "||athena-ads.wikia.com^";
	line += "||autosport.com/skinning/";
	line += "||avaxhome.ws/banners/";
	line += "||azlyrics.com^*_az.js";
	line += "||b92.net/images/banners/";
	line += "||banner.atomicgamer.com^";
	line += "||banner.itweb.co.za^";
	line += "||banners.expressindia.com^";
	line += "||banners.friday-ad.co.uk/hpbanneruploads/$image";
	line += "||banners.i-comers.com^";
	line += "||banners.itweb.co.za^";
	line += "||bbc.co.uk^*/bbccom.js?";
	line += "||bcdb.com^*/banners.pl?";
	line += "||beingpc.com^*/banners/";
	line += "||belfasttelegraph.co.uk/editorial/web/survey/recruit-div-img.js";
	line += "||bigpoint.com/xml/recommender.swf?";
	line += "||bigpond.com/home/skin_";
	line += "||bit-tech.net/images/backgrounds/skin/";
	line += "||bittorrent.am/banners/";
	line += "||blackberryforums.net/banners/";
	line += "||blinkx.com/adhocnetwork/";
	line += "||blinkx.com/f2/overlays/adhoc";
	line += "||blogspider.net/images/promo/";
	line += "||bloomberg.com^*/banner.js";
	line += "||bnrs.ilm.ee^";
	line += "||bollywoodbuzz.in^*/728x70.gif";
	line += "||bookingbuddy.com/js/bookingbuddy.strings.php?$domain=smartertravel.com";
	line += "||boyplz.com^*/layer.js";
	line += "||brothersoft.com/softsale/";
	line += "||brothersoft.com^*/float.js";
	line += "||browsershots.org/static/images/creative/";
	line += "||budapesttimes.hu/images/banners/";
	line += "||burnsoftware.info*/!";
	line += "||businesstimes.com.sg^*/ad";
	line += "||bwp.theinsider.com.com^";
	line += "||c-sharpcorner.com^*/banners/";
	line += "||c21media.net/uploads/flash/*.swf";
	line += "||cafimg.com/images/other/";
	line += "||candystand.com/banners/";
	line += "||carsguide.com.au^*/marketing/";
	line += "||cdmediaworld.com*/!";
	line += "||cdnlayer.com/howtogeek/geekers/up/netshel125x125.gif";
	line += "||celebjihad.com/widget/widget.js$domain=popbytes.com";
	line += "||centos.org/donors/";
	line += "||chapala.com/wwwboard/webboardtop.htm";
	line += "||china.com^*/googlehead.js";
	line += "||chinapost.com.tw/ad/";
	line += "||ciao.co.uk/load_file.php?";
	line += "||classicfeel.co.za^*/banners/";
	line += "||click.livedoor.com^";
	line += "||clk.about.com^";
	line += "||cms.myspacecdn.com^*/splash_assets/";
	line += "||codeasily.com^*/codeasily.js";
	line += "||codeproject.com^*/adm/";
	line += "||coderanch.com/shingles/";
	line += "||comm.kino.to^";
	line += "||comparestoreprices.co.uk/images/promotions/";
	line += "||complexmedianetwork.com^*/takeovers/";
	line += "||computerandvideogames.com^*/promos/";
	line += "||computerworld.com^*/jobroll/";
	line += "||consumerreports.org^*/sx.js";
	line += "||countrychannel.tv/telvos_banners/";
	line += "||covertarget.com^*_*.php";
	line += "||cpuid.com^*/cpuidbanner72x90_2.";
	line += "||crazymotion.net/video_*.php?key=";
	line += "||crushorflush.com/html/promoframe.html";
	line += "||d-addicts.com^*/banner/";
	line += "||da.feedsportal.com^";
	line += "||dads.new.digg.com^";
	line += "||dailydeals.sfgate.com/widget/";
	line += "||dailymail.co.uk^*/promoboxes/";
	line += "||dailymotion.com/images/ie.png";
	line += "||dailymotion.com/skin/data/default/partner/$~stylesheet";
	line += "||dailymotion.com^*masscast/";
	line += "||dailystar.com.lb/bannerin1.htm";
	line += "||dailystar.com.lb/bottombanner.htm";
	line += "||dailystar.com.lb/centerbanner.htm";
	line += "||dailystar.com.lb/googlearticle468.htm";
	line += "||dailystar.com.lb/leaderboard.htm";
	line += "||dailystar.com.lb/spcovbannerin.htm";
	line += "||dailytimes.com.pk/banners/";
	line += "||dailywritingtips.com^*/money-making.gif";
	line += "||davesite.com^*/aff/";
	line += "||dcad.watersoul.com^";
	line += "||demonoid.com/cached/ab_";
	line += "||demonoid.com/cached/bnr_";
	line += "||develop-online.net/static/banners/";
	line += "||dig.abclocal.go.com/preroll/";
	line += "||digdug.divxnetworks.com^";
	line += "||digitaljournal.com/promo/";
	line += "||digitallook.com^*/rbs-logo-ticker.gif";
	line += "||digitalreality.co.nz^*/360_hacks_banner.gif";
	line += "||divxme.com/images/play.png";
	line += "||divxstage.net/images/download.png";
	line += "||dl4all.com/data4.files/dpopupwindow.js";
	line += "||domaining.com/banners/";
	line += "||dontblockme.modaco.com^$~image";
	line += "||dvdvideosoft.com^*/banners/";
	line += "||earthlink.net^*/promos/";
	line += "||ebayrtm.com/rtm?";
	line += "||ebuddy.com/banners/";
	line += "||ebuddy.com/textlink.php?";
	line += "||ebuddy.com/web_banners/";
	line += "||ebuddy.com/web_banners_";
	line += "||ecommerce-journal.com/specdata.php?";
	line += "||ehow.com/images/brands/";
	line += "||ekrit.de/serious-gamer/1.swf";
	line += "||ekrit.de/serious-gamer/film1.swf";
	line += "||ekrit.de/serious-gamer/images/stories/city-quest.jpg";
	line += "||el33tonline.com^*/el33t_bg_";
	line += "||electricenergyonline.com^*/bannieres/";
	line += "||episodic.com^*/logos/player-";
	line += "||espn.vad.go.com^$domain=youtube.com";
	line += "||esus.com/images/regiochat_logo.png";
	line += "||eurogamer.net/quad.php";
	line += "||eva.ucas.com^";
	line += "||eweek.com/images/stories/marketing/";
	line += "||excite.com/gca_iframe.html?";
	line += "||expertreviews.co.uk/images/skins/";
	line += "||expreview.com/exp2/";
	line += "||fallout3nexus.com^*/300x600.php";
	line += "||feedsportal.com/creative/";
	line += "||ffiles.com/counters.js";
	line += "||fgfx.co.uk/banner.js?";
	line += "||filebase.to/gfx/*.jpg";
	line += "||filebase.to/xtend/";
	line += "||filebase.to^*/note.js";
	line += "||filefront.com/linkto/";
	line += "||filespazz.com/imx/template_r2_c3.jpg";
	line += "||filespazz.com^*/copyartwork_side_banner.gif";
	line += "||filetarget.com*/!";
	line += "||filetarget.com^*_*.php";
	line += "||findfiles.com/images/icatchallfree.png";
	line += "||findfiles.com/images/knife-dancing-1.gif";
	line += "||flixstertomatoes.com^*/jquery.js?";
	line += "||flixstertomatoes.com^*/jquery.rt_scrollmultimedia.js";
	line += "||flixstertomatoes.com^*/jquery.tooltip.min.js?";
	line += "||flv.sales.cbs.com^$object_subrequest,domain=cbsnews.com";
	line += "||flyordie.com/games/free/b/";
	line += "||fmr.co.za^*/banners/";
	line += "||fordforums.com.au/banner.swf";
	line += "||forumimg.ipmart.com/swf/ipmart_forum/banner";
	line += "||forumw.org/images/uploading.gif";
	line += "||foxbusiness.com/html/google_homepage_promo";
	line += "||foxnews1280.com^*/clientgraphics/";
	line += "||foxradio.com/common/dfpframe.";
	line += "||foxradio.com/media/module/billboards/";
	line += "||free-tv-video-online.info/300.html";
	line += "||free-tv-video-online.info/300s.html";
	line += "||freemediatv.com/images/inmemoryofmichael.jpg";
	line += "||freeworldgroup.com/banner";
	line += "||friday-ad.co.uk/banner.js?";
	line += "||fudzilla.com^*/banners/";
	line += "||gamecopyworld.com*/!";
	line += "||gamemakerblog.com/gma/gatob.php";
	line += "||gameplanet.co.nz^*-takeover.jpg";
	line += "||gametrailers.com^*/gt6_siteskin_$stylesheet";
	line += "||gbrej.com/c/";
	line += "||geocities.com/js_source/";
	line += "||geocities.yahoo.*/js/sq.";
	line += "||getprice.com.au/searchwidget.aspx?$subdocument";
	line += "||ghacks.net/skin-";
	line += "||glam.com^*/affiliate/";
	line += "||goauto.com.au/mellor/mellor.nsf/toy$subdocument";
	line += "||goodgearguide.com.au/files/skins/";
	line += "||gowilkes.com/cj/";
	line += "||gowilkes.com/other/";
	line += "||grapevine.is/media/flash/*.swf";
	line += "||guitaretab.com^*/ringtones_overlay.js";
	line += "||gumtree.com^*/dart_wrapper_";
	line += "||gwinnettdailypost.com/1.iframe.asp?";
	line += "||hdtvtest.co.uk^*/pricerunner.php";
	line += "||helsinkitimes.fi^*/banners/";
	line += "||holyfragger.com/images/skins/";
	line += "||horriblesubs.net/playasia*.gif";
	line += "||horriblesubs.org/playasia*.gif";
	line += "||hostsearch.com/creative/";
	line += "||hotfrog.com/adblock.ashx?";
	line += "||howtogeek.com/go/";
	line += "||hummy.org.uk^*/brotator/";
	line += "||i.com.com^*/vendor_bg_";
	line += "||i.i.com.com/cnwk.1d/*/tt_post_dl.jpg";
	line += "||i.neoseeker.com/d/$subdocument";
	line += "||i4u.com/_banner/";
	line += "||ibanners.empoweredcomms.com.au^";
	line += "||ibtimes.com/banner/";
	line += "||ibtimes.com^*/sponsor_";
	line += "||idg.com.au/images/*_promo$image";
	line += "||idg.com.au^*_skin.jpg";
	line += "||ifilm.com/website/*-skin-";
	line += "||iloveim.com/cadv4.jsp?";
	line += "||images-amazon.com^*/marqueepushdown/";
	line += "||imageshack.us/images/contests/*/lp-bg.jpg";
	line += "||imageshack.us/ym.php?";
	line += "||img*.i-comers.com^";
	line += "||impulsedriven.com/app_images/wallpaper/";
	line += "||independent.co.uk/multimedia/archive/$subdocument";
	line += "||informationmadness.com^*/banners/";
	line += "||informer.com/images/sponsored.gif";
	line += "||infoseek.co.jp/isweb/clip.html";
	line += "||injpn.net/images/banners/";
	line += "||insidehw.com/images/banners/";
	line += "||interfacelift.com/inc_new/$subdocument";
	line += "||internet.ziffdavis.com^";
	line += "||iptools.com/sky.php";
	line += "||isitnormal.com/img/iphone_hp_promo_wide.png";
	line += "||itpro.co.uk/images/skins/";
	line += "||itweb.co.za/banners/";
	line += "||itweb.co.za/logos/";
	line += "||iwebtool.com^*/bannerview.php";
	line += "||jame-world.com^*/adv/";
	line += "||japanvisitor.com/banners/";
	line += "||jdownloader.org/_media/screenshots/banner.png";
	line += "||jdownloader.org^*/smbanner.png";
	line += "||jewlicious.com/banners/";
	line += "||jewtube.com/banners/";
	line += "||johnbridge.com/vbulletin/banner_rotate.js";
	line += "||johnbridge.com/vbulletin/images/tyw/cdlogo-john-bridge.jpg";
	line += "||johnbridge.com/vbulletin/images/tyw/wedi-shower-systems-solutions.png";
	line += "||jollo.com/images/travel.gif";
	line += "||jpost.com/images/*/promos/";
	line += "||jpost.com/images/2009/newsite/";
	line += "||kcye.com/addrotate_content.php?";
	line += "||kdwn.com/addrotate_content.php?";
	line += "||kermit.macnn.com^";
	line += "||kestrel.ospreymedialp.com^";
	line += "||kewlshare.com/reward.html";
	line += "||kino.to/gr/blob/";
	line += "||kitz.co.uk/files/jump2/";
	line += "||kjul1047.com^*/clientgraphics/";
	line += "||klav1230am.com^*/banners/";
	line += "||knowfree.net^*/ezm125x125.gif";
	line += "||krapps.com^*-banner-";
	line += "||krebsonsecurity.com^*banner.swf?";
	line += "||kstp.com^*/flexhousepromotions/";
	line += "||kxlh.com/images/banner/";
	line += "||kyivpost.com^*/adv_";
	line += "||kyivpost.com^*/banner/";
	line += "||labtimes.org/banner/";
	line += "||lastminute.com^*/universal.html?";
	line += "||latex-community.org/images/banners/";
	line += "||lightningcast.net^*/getplaylist?$third-party,domain=reuters.com";
	line += "||linksafe.info^*/mirror.png";
	line += "||linuxtopia.org/includes/$subdocument";
	line += "||livestream.com^*/overlay/";
	line += "||loaded.it/images/ban*.swf";
	line += "||loaded.it^*/geld-internet-verdienen.jpg";
	line += "||loaded.it^*/iframe_vid.";
	line += "||loaded.it^*/my_banner";
	line += "||londonstockexchange.com/prices-and-news/*/fx.gif";
	line += "||looky.hyves.org^";
	line += "||loveolgy.com/banners/";
	line += "||lowbird.com/lbpu.php";
	line += "||lowellsun.com/litebanner/";
	line += "||lowyat.net/catfish/";
	line += "||lowyat.net^*/images/header/";
	line += "||lyricsfreak.com^*/overlay.js";
	line += "||macmillandictionary.com/info/frame.html?zone=";
	line += "||macobserver.com/js/givetotmo.js";
	line += "||macobserver.com^*/deal_brothers/";
	line += "||macworld.co.uk^*/textdeals/";
	line += "||madskristensen.net/discount2.js";
	line += "||mail.google.com/mail/*&view=ad";
	line += "||majorgeeks.com/aff/";
	line += "||majorgeeks.com/images/mb-hb-2.jpg";
	line += "||majorgeeks.com/images/mg120.jpg";
	line += "||majorgeeks.com^*/banners/";
	line += "||mangafox.com/media/game321/";
	line += "||mangaupdates.com/affiliates/";
	line += "||mani-admin-plugin.com^*/banners/";
	line += "||mccont.com/sda/";
	line += "||mccont.com/takeover/";
	line += "||mcstatic.com^*/billboard_";
	line += "||medhelp.org/hserver/";
	line += "||media.abc.go.com^*/callouts/";
	line += "||media.mtvnservices.com/player/scripts/mtvn_player_control.js$domain=spike.com";
	line += "||mediafire.com^*/linkto/default-$subdocument";
	line += "||mediafire.com^*/remove_ads.gif";
	line += "||mediamgr.ugo.com^";
	line += "||meetic.com/js/*/site_under_";
	line += "||megaupload.com/mc.php?";
	line += "||megavideo.com/goviral.php";
	line += "||megavideo.com/unruley.php";
	line += "||merriam-webster.com^*/accipiter.js";
	line += "||mgnetwork.com/dealtaker/";
	line += "||mirror.co.uk^*/gutters/";
	line += "||mirror.co.uk^*/m4_gutters/";
	line += "||mirror.co.uk^*/m4_partners/";
	line += "||mirror.co.uk^*/people_promotions/";
	line += "||mmorpg.com/images/skins/";
	line += "||mochiads.com/srv/";
	line += "||movshare.net^*/remove_ads.jpg";
	line += "||movstreaming.com/images/edhim.jpg";
	line += "||mp3mediaworld.com*/!";
	line += "||mp3raid.com/imesh.gif";
	line += "||msn.com/?adunitid";
	line += "||musicremedy.com/banner/";
	line += "||musictarget.com*/!";
	line += "||myspace.com^*.adtooltip&";
	line += "||mystream.to^*/button_close.png";
	line += "||myway.com/gca_iframe.html";
	line += "||nationalturk.com^*/banner";
	line += "||naukimg.com/banner/";
	line += "||nba.com^*/amex_logo";
	line += "||nba.com^*/steinersports_";
	line += "||nearlygood.com^*/_aff.php?";
	line += "||neoseeker.com/a_pane.php";
	line += "||nerej.com/c/";
	line += "||newport-county.co.uk/images/general_images/blue_square_update_01.gif";
	line += "||newport-county.co.uk/images/home_page_images/234x60.gif";
	line += "||newport-county.co.uk/images/home_page_images/premier_sport_anmin.gif";
	line += "||news-leader.com^*/banner.js";
	line += "||news.com.au/news/vodafone/$object";
	line += "||news.com.au^*-promo$image";
	line += "||news.com.au^*/promos/";
	line += "||nirsoft.net/banners/";
	line += "||ntdtv.com^*/adv/";
	line += "||ny1.com^*/servecontent.aspx?iframe=";
	line += "||nyaatorrents.org/images/nw.";
	line += "||nyaatorrents.org/images/skyscraper.";
	line += "||nymag.com^*/metrony_";
	line += "||nyrej.com/c/";
	line += "||objects.tremormedia.com/embed/swf/bcacudeomodule.swf$domain=radaronline.com";
	line += "||ocforums.com/adj/";
	line += "||oldgames.sk/images/topbar/";
	line += "||osdir.com/ml/$subdocument";
	line += "||oyetimes.com/join/advertisers.html";
	line += "||payplay.fm^*/mastercs.js";
	line += "||pbsrc.com/sponsor/";
	line += "||pbsrc.com^*/sponsor/";
	line += "||pcauthority.com.au^*/skins/";
	line += "||pcpro.co.uk/images/*_siteskin";
	line += "||pcpro.co.uk/images/skins/";
	line += "||pcpro.co.uk^*/pcprositeskin";
	line += "||pcpro.co.uk^*skin_wide.";
	line += "||pcworld.idg.com.au/files/skins/";
	line += "||pettube.com/images/*-partner.";
	line += "||phobos.apple.com^$object_subrequest,domain=dailymotion.com";
	line += "||photoshopguides.com/banners/";
	line += "||photosupload.net/photosupload.js";
	line += "||pinknews.co.uk/newweb/";
	line += "||pitchero.com^*/toolstation.gif";
	line += "||popbytes.com/img/*-ad.jpg";
	line += "||popbytes.com/img/becomeasponsor.gif";
	line += "||popbytes.com/img/no-phone-zone.gif";
	line += "||popbytes.com/img/sunset-idle-1.gif";
	line += "||popbytes.com/img/thinkups-230x115.gif";
	line += "||popbytes.com/img/visitmysponsors.gif";
	line += "||prisonplanet.com^*advert.";
	line += "||prisonplanet.com^*banner";
	line += "||prisonplanet.com^*sky.";
	line += "||project-for-sell.com/_google.php";
	line += "||promo.fileforum.com^";
	line += "||proxy.org/af.html";
	line += "||proxy.org/ah.html";
	line += "||ps3news.com/banner/";
	line += "||ps3news.com^*.swf";
	line += "||ps3news.com^*/200x90.jpg";
	line += "||ps3news.com^*/200x90_";
	line += "||ps3news.com^*/200x90f.jpg";
	line += "||ps3news.com^*/global_background_ps3break.jpg";
	line += "||psx-scene.com^*/cyb_banners/";
	line += "||psx-scene.com^*/sponsors/";
	line += "||qrz.com/pix/*.gif";
	line += "||querverweis.net/pu.js";
	line += "||quickload.to^*/layer.divx.js";
	line += "||quickload.to^*/note.divx.js";
	line += "||quickload.to^*/note.js";
	line += "||rad.msn.com^";
	line += "||radiovaticana.org^*/alitalia";
	line += "||readwriteweb.com^*/clouddownloadvmwarepromo.png";
	line += "||readwriteweb.com^*/rwcloudlearnmorebutton.png";
	line += "||rejournal.com/images/banners/";
	line += "||rejournal.com/users/blinks/";
	line += "||rejournal.com^*/images/homepage/";
	line += "||retrevo.com^*/pcwframe.jsp?";
	line += "||rfu.com/js/jquery.jcarousel.js";
	line += "||richmedia.yimg.com^";
	line += "||riderfans.com/other/";
	line += "||sameip.org/images/froghost.gif";
	line += "||satelliteguys.us/burst_header_iframe.";
	line += "||satelliteguys.us/burstbox_iframe.";
	line += "||satelliteguys.us/burstsky_iframe.";
	line += "||scenereleases.info/wp-content/*.swf";
	line += "||sciencedaily.com^*/google-story2-rb.js";
	line += "||seatguru.com/deals?";
	line += "||seeingwithsound.com/noad.gif";
	line += "||sendspace.com/defaults/framer.html?z=";
	line += "||sendspace.com^*?zone=";
	line += "||sensongs.com/nfls/";
	line += "||serialzz.us/ad.js";
	line += "||sharebeast.com^*/remove_ads.gif";
	line += "||sharetera.com/images/icon_download.png";
	line += "||sharetera.com/promo.php?";
	line += "||shop.com/cc.class/dfp?";
	line += "||shopping.stylelist.com/widget?";
	line += "||shoppingpartners2.futurenet.com^";
	line += "||shops.tgdaily.com^*&widget=";
	line += "||shortcuts.search.yahoo.com^*&callback=yahoo.shortcuts.utils.setdittoadcontents&";
	line += "||showstreet.com/banner.";
	line += "||sify.com^*/gads_";
	line += "||sk-gaming.com/image/acersocialw.gif";
	line += "||sk-gaming.com/image/pts/";
	line += "||sk-gaming.com/www/skdelivery/";
	line += "||slyck.com/pics/*304x83_";
	line += "||smh.com.au/images/promo/";
	line += "||snopes.com^*/casalebox.asp";
	line += "||snopes.com^*/tribalbox.asp";
	line += "||soccerlens.com/files1/";
	line += "||soccerway.com/banners/";
	line += "||soccerway.com/buttons/120x90_";
	line += "||soccerway.com/media/img/mybet_banner.gif";
	line += "||softarchive.net/js/getbanner.php?";
	line += "||softcab.com/google.php?";
	line += "||softonic.com/specials_leaderboard/";
	line += "||soundtracklyrics.net^*_az.js";
	line += "||space.com/promo/";
	line += "||sternfannetwork.com/forum/images/banners/";
	line += "||steroid.com/banner/";
	line += "||steroid.com/dsoct09.swf";
	line += "||stlyrics.com^*_az.js";
	line += "||stuff.tv/client/skinning/";
	line += "||suntimes.com^*/banners/";
	line += "||supernovatube.com/spark.html";
	line += "||sydneyolympicfc.com/admin/media_manager/media/mm_magic_display/$image";
	line += "||techpowerup.com/images/bnnrs/";
	line += "||techradar.com^*/img/*_takeover_";
	line += "||techsupportforum.com^*/banners/";
	line += "||techtree.com^*/jquery.catfish.js";
	line += "||teesoft.info/images/uniblue.png";
	line += "||telegraphindia.com^*/banners/";
	line += "||telegraphindia.com^*/hoabanner.";
	line += "||tentonhammer.com^*/takeovers/";
	line += "||theaquarian.com^*/banners/";
	line += "||thecorrsmisc.com/10feet_banner.gif";
	line += "||thecorrsmisc.com/brokenthread.jpg";
	line += "||thecorrsmisc.com/msb_banner.jpg";
	line += "||thehighstreetweb.com^*/banners/";
	line += "||theispguide.com/premiumisp.html";
	line += "||theispguide.com/topbanner.asp?";
	line += "||themis-media.com/media/global/images/cskins/";
	line += "||themis.yahoo.com^";
	line += "||thepiratebay.org/img/bar.gif";
	line += "||thewb.com/thewb/swf/tmz-adblock/";
	line += "||tigerdroppings.com^*&adcode=";
	line += "||times-herald.com/pubfiles/";
	line += "||titanbet.com/promoloaddisplay?";
	line += "||tomsguide.com/*/cdntests_cedexis.php";
	line += "||torrentfreak.com^*/wyzo.gif";
	line += "||trackitdown.net/skins/*_campaign/";
	line += "||tripadvisor.co.uk/adp/";
	line += "||tripadvisor.com/adp/";
	line += "||tripadvisor.com^*/skyscraper.jpg";
	line += "||turbobit.net/js/popunder2.js";
	line += "||tweaktown.com/cms/includes/i*.php";
	line += "||typicallyspanish.com/banners/";
	line += "||ua.badongo.com^";
	line += "||ubuntugeek.com/images/dnsstock.png";
	line += "||uimserv.net^";
	line += "||ultimate-guitar.com^*/takeover/";
	line += "||uncoached.com/smallpics/ashley";
	line += "||unicast.ign.com^";
	line += "||unicast.msn.com^";
	line += "||universalhub.com/bban/";
	line += "||videodownloadtoolbar.com/fancybox/";
	line += "||videogamer.com^*/css/skins/$stylesheet";
	line += "||videoplaza.com/resources/preroll_interactive/";
	line += "||videos.mediaite.com/decor/live/white_alpha_60.";
	line += "||videosift.com/bnr.php?";
	line += "||videoweed.com^*/remove_ads.png";
	line += "||videoweed.com^*/stream_movies_hd_button.png";
	line += "||viewdocsonline.com/images/banners/";
	line += "||vortez.co.uk^*120x600.swf";
	line += "||vortez.co.uk^*skyscraper.jpg";
	line += "||w3schools.com/banners/";
	line += "||wareseeker.com/banners/";
	line += "||webhostingtalk.com/js/hail.js";
	line += "||webnewswire.com/images/banner";
	line += "||weddingtv.com/src/usefulstuff/*banner";
	line += "||werlv.com^*banner";
	line += "||weselectmodels.com^*/new_banner.jpg";
	line += "||whispersinthecorridors.com/banner2009/";
	line += "||wikia.com/__varnish_";
	line += "||windowsitpro.com^*/doubleclick/";
	line += "||windowsitpro.com^*/googleafc";
	line += "||windowsitpro.com^*/roadblock.js";
	line += "||wnst.net/img/coupon/";
	line += "||wolf-howl.com/wp-content/banners/";
	line += "||wollongongfc.com.au/images/banners/";
	line += "||worthdownloading.com/images/mirrors/preview_logo.gif";
	line += "||wowwiki.com/__varnish_";
	line += "||www2.sys-con.com^*.cfm";
	line += "||xbitlabs.com^*/xbanner.php?";
	line += "||xbox-scene.com/crave/logo_on_white_s160.jpg";
	line += "||xoops-theme.com/images/banners/";
	line += "||yahoo.*/serv?s=";
	line += "||yahoo.com/darla/";
	line += "||yahoo.com/ysmload.html?";
	line += "||yahoo.com^*/eyc-themis?";
	line += "||yfrog.com/images/contests/*/lp-bg.jpg";
	line += "||yfrog.com/ym.php?";
	line += "||yimg.com/a/1-$~stylesheet";
	line += "||yimg.com^*/fairfax/$image";
	line += "||yimg.com^*/flash/promotions/";
	line += "||yourmovies.com.au^*/side_panels_";
	line += "||yourtomtom.com^*/back_adblock.gif";
	line += "||ytimg.com^*/new_watch_background$domain=youtube.com";
	line += "||ytimg.com^*_banner$domain=youtube.com";
	line += "||zam.com/i/promos/*-skin.";
	line += "||zambiz.co.zm/banners/";
	line += "||zophar.net/files/tf_";
	line += "||zurrieqfc.com/images/banners/";
	line += "!Anti-Adblock";
	line += "||illimitux.net/js/abp.js";
	line += "||indieclicktv.com/player/swf/*/icmmva%5eplugin.swf$object_subrequest";
	line += "!-----------------Specific element hiding rules-----------------!";
	line += "! *** easylist_specific_hide.txt ***";
	line += "10minutemail.com###shoutouts";
	line += "123people.co.uk###l_banner";
	line += "1911encyclopedia.org##.google_block_style";
	line += "2gb-hosting.com##div.info[align=\"center\"]";
	line += "4chan.org###ad";
	line += "4megaupload.com##table[width=\"100%\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][bgcolor=\"#d8d8d0\"]";
	line += "4shared.com##.signupbanner";
	line += "4shared.com##center > img[width=\"13\"][height=\"84\"][style=\"cursor: pointer;\"]";
	line += "4shared.com##img[alt=\"Remove Ads\"]";
	line += "6lyrics.com##.ad";
	line += "7tutorials.com##.block-openx";
	line += "9news.com##.poster";
	line += "9to5mac.com###block-dealmac-0";
	line += "9to5mac.com###page-top";
	line += "a10.com###gameunderbanner";
	line += "abc2news.com##.ad";
	line += "abclocal.go.com###bannerTop";
	line += "abclocal.go.com##.linksWeLike";
	line += "abcnews.go.com##.ad";
	line += "abndigital.com###banner468";
	line += "abndigital.com###leaderboard_728x90";
	line += "about.com###adB";
	line += "about.com##.gB";
	line += "accuweather.com###googleContainer";
	line += "achieve360points.com###a1";
	line += "achieve360points.com###a3";
	line += "actiontrip.com###banner300";
	line += "adelaidecityfc.com.au##td[width=\"130\"][valign=\"top\"][align=\"right\"]:last-child > table[width=\"125\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][align=\"right\"]:first-child:last-child";
	line += "adelaideunited.com.au##.promotion_wrapper";
	line += "adultswim.com###ad";
	line += "advocate.com###BottomBanners";
	line += "advocate.com###TopBanners";
	line += "afl.com.au##div[style=\"width: 300px; height: 250px;\"]";
	line += "afro.com###leaderboard";
	line += "afterdawn.com###dlSoftwareDesc300x250";
	line += "afterdawn.com##.uniblue";
	line += "airspacemag.com###top-banners";
	line += "ajaxian.com###taeheader";
	line += "akeelwap.net##a[href^=\"http://c.admob.com/\"]";
	line += "akeelwap.net##a[href^=\"http://click.buzzcity.net/click.php?\"]";
	line += "akihabaranews.com###bbTop";
	line += "akihabaranews.com###recSidebar";
	line += "alarabiya.net###side_banner";
	line += "allakhazam.com###bannerMain";
	line += "allakhazam.com###towerRt";
	line += "allbusiness.com##.search_results";
	line += "allexperts.com###sl";
	line += "allmovieportal.com##table[width=\"100%\"][height=\"90\"]";
	line += "allmusicals.com##img[width=\"190\"]";
	line += "allshopsuk.co.uk##table[border=\"0\"][align=\"center\"][width=\"100%\"]";
	line += "allthelyrics.com##div[style=\"padding: 0px 0px 15px;\"]";
	line += "altavista.com###spons";
	line += "altavista.com##a[href*=\".overture.com/d/sr/\"]";
	line += "alternet.org###premium";
	line += "alternet.org###premium2_container";
	line += "alternet.org##.premium-container";
	line += "amazon.co.uk##.bm";
	line += "amazon.co.uk##.tigerbox";
	line += "amazon.co.uk##iframe[title=\"Ad\"]";
	line += "amazon.com##.pa_containerWrapper";
	line += "amazon.com##iframe[title=\"Ad\"]";
	line += "america.fm###banbo";
	line += "ampercent.com###centersidebar";
	line += "ampercent.com##.titlebelow";
	line += "anandtech.com##.ad";
	line += "ancestry.com##.uprPromo";
	line += "androidpit.com##.boxLightLeft[style=\"width: 620px; text-align: center; font-size: 95%;\"]";
	line += "anime-planet.com##.medrec";
	line += "animea.net##.ad";
	line += "animenewsnetwork.com###page-header-banner";
	line += "animepaper.net###ifiblockthisthenicheatap";
	line += "animetake.com##.top-banner";
	line += "anonymouse.org###mouselayer";
	line += "ansearch.com.au##.sponsor";
	line += "answerology.com##.leaderboard";
	line += "answers.com###radLinks";
	line += "aol.ca###rA";
	line += "aol.co.uk###banner";
	line += "aol.co.uk###rA";
	line += "aol.co.uk##.sad_cont";
	line += "aol.co.uk##.sidebarBanner";
	line += "aol.com###rA";
	line += "aol.com##.g_slm";
	line += "aol.com##.gsl";
	line += "aol.com##.sad_cont";
	line += "aolnews.com##.fauxArticleIMU";
	line += "ap.org##td[width=\"160\"]";
	line += "app.com,argusleader.com,battlecreekenquirer.com,baxterbulletin.com,bucyrustelegraphforum.com,burlingtonfreepress.com,centralohio.com,chillicothegazette.com,cincinnati.com,citizen-times.com,clarionledger.com,coloradoan.com,coshoctontribune.com,courier-journal.com,courierpostonline.com,dailyrecord.com,dailyworld.com,delawareonline.com,delmarvanow.com,democratandchronicle.com,desmoinesregister.com,dnj.com,fdlreporter.com,freep.com,greatfallstribune.com,greenbaypressgazette.com,greenvilleonline.com,guampdn.com,hattiesburgamerican.com,hometownlife.com,honoluluadvertiser.com,htrnews.com,indystar.com,jacksonsun.com,jconline.com,lancastereaglegazette.com,lansingstatejournal.com,livingstondaily.com,lohud.com,mansfieldnewsjournal.com,marionstar.com,marshfieldnewsherald.com,montgomeryadvertiser.com,mycentraljersey.com,mydesert.com,newarkadvocate.com,news-leader.com,news-press.com,newsleader.com,pal-item.com,pnj.com,portclintonnewsherald.com,postcrescent.com,poughkeepsiejournal.com,press-citizen.com,pressconnects.com,rgj.com,sctimes.com,sheboyganpress.com,shreveporttimes.com,stargazette.com,statesmanjournal.com,stevenspointjournal.com,tallahassee.com,tennessean.com,theadvertiser.com,thecalifornian.com,thedailyjournal.com,theithacajournal.com,theleafchronicle.com,thenews-messenger.com,thenewsstar.com,thenorthwestern.com,thespectrum.com,thestarpress.com,thetimesherald.com,thetowntalk.com,visaliatimesdelta.com,wausaudailyherald.com,wisconsinrapidstribune.com,zanesvilletimesrecorder.com##.articleflex-container";
	line += "app.com,argusleader.com,battlecreekenquirer.com,baxterbulletin.com,bucyrustelegraphforum.com,burlingtonfreepress.com,centralohio.com,chillicothegazette.com,cincinnati.com,citizen-times.com,clarionledger.com,coloradoan.com,coshoctontribune.com,courier-journal.com,courierpostonline.com,dailyrecord.com,dailyworld.com,delawareonline.com,delmarvanow.com,democratandchronicle.com,desmoinesregister.com,dnj.com,fdlreporter.com,freep.com,greatfallstribune.com,greenbaypressgazette.com,greenvilleonline.com,guampdn.com,hattiesburgamerican.com,hometownlife.com,honoluluadvertiser.com,htrnews.com,indystar.com,jacksonsun.com,jconline.com,lancastereaglegazette.com,lansingstatejournal.com,livingstondaily.com,lohud.com,mansfieldnewsjournal.com,marionstar.com,marshfieldnewsherald.com,montgomeryadvertiser.com,mycentraljersey.com,mydesert.com,newarkadvocate.com,news-leader.com,news-press.com,newsleader.com,pal-item.com,pnj.com,portclintonnewsherald.com,postcrescent.com,poughkeepsiejournal.com,press-citizen.com,pressconnects.com,rgj.com,sctimes.com,sheboyganpress.com,shreveporttimes.com,stargazette.com,statesmanjournal.com,stevenspointjournal.com,tallahassee.com,tennessean.com,theadvertiser.com,thecalifornian.com,thedailyjournal.com,theithacajournal.com,theleafchronicle.com,thenews-messenger.com,thenewsstar.com,thenorthwestern.com,thespectrum.com,thestarpress.com,thetimesherald.com,thetowntalk.com,visaliatimesdelta.com,wausaudailyherald.com,wisconsinrapidstribune.com,zanesvilletimesrecorder.com##.leaderboard-container";
	line += "app.com,argusleader.com,battlecreekenquirer.com,baxterbulletin.com,bucyrustelegraphforum.com,burlingtonfreepress.com,centralohio.com,chillicothegazette.com,cincinnati.com,citizen-times.com,clarionledger.com,coloradoan.com,coshoctontribune.com,courier-journal.com,courierpostonline.com,dailyrecord.com,dailyworld.com,delawareonline.com,delmarvanow.com,democratandchronicle.com,desmoinesregister.com,dnj.com,fdlreporter.com,freep.com,greatfallstribune.com,greenbaypressgazette.com,greenvilleonline.com,guampdn.com,hattiesburgamerican.com,hometownlife.com,honoluluadvertiser.com,htrnews.com,indystar.com,jacksonsun.com,jconline.com,lancastereaglegazette.com,lansingstatejournal.com,livingstondaily.com,lohud.com,mansfieldnewsjournal.com,marionstar.com,marshfieldnewsherald.com,montgomeryadvertiser.com,mycentraljersey.com,mydesert.com,newarkadvocate.com,news-leader.com,news-press.com,newsleader.com,pal-item.com,pnj.com,portclintonnewsherald.com,postcrescent.com,poughkeepsiejournal.com,press-citizen.com,pressconnects.com,rgj.com,sctimes.com,sheboyganpress.com,shreveporttimes.com,stargazette.com,statesmanjournal.com,stevenspointjournal.com,tallahassee.com,tennessean.com,theadvertiser.com,thecalifornian.com,thedailyjournal.com,theithacajournal.com,theleafchronicle.com,thenews-messenger.com,thenewsstar.com,thenorthwestern.com,thespectrum.com,thestarpress.com,thetimesherald.com,thetowntalk.com,visaliatimesdelta.com,wausaudailyherald.com,wisconsinrapidstribune.com,zanesvilletimesrecorder.com##.leaderboard-container-top";
	line += "appleinsider.com###aadbox";
	line += "appleinsider.com###ldbd";
	line += "appleinsider.com##.bottombox";
	line += "appleinsider.com##.leaderboard";
	line += "appleinsider.com##.main_box4";
	line += "appleinsider.com##div[style=\"border: 1px solid rgb(221, 221, 221); width: 498px; height: 250px; font-size: 14px;\"]";
	line += "appleinsider.com##div[style=\"padding: 10px 0pt; width: auto; height: 60px; margin: 0pt 0pt 0pt 348px;\"]";
	line += "appleinsider.com##div[style=\"padding: 10px 0pt; width: auto; height: 60px; margin: 0pt 0pt 0pt 348px;\"]";
	line += "appleinsider.com##img[width=\"300\"][height=\"250\"]";
	line += "appleinsider.com##td[width=\"150\"][valign=\"top\"]";
	line += "appleinsider.com##td[width=\"180\"][valign=\"top\"]";
	line += "aquariumfish.net##table[width=\"440\"][height=\"330\"]";
	line += "aquariumfish.net##td[align=\"center\"][width=\"100%\"][height=\"100\"]";
	line += "arabianbusiness.com###banner-container";
	line += "arabiclookup.com##td[style=\"width: 156px; border-style: solid; text-align: center;\"]";
	line += "arabiclookup.com##td[style=\"width: 157px; border-style: solid; text-align: left;\"]";
	line += "armorgames.com###leaderboard";
	line += "arnnet.com.au###marketplace";
	line += "arnnet.com.au##.careerone_search";
	line += "arsenal.com###banner";
	line += "arstechnica.com###daehtsam-da";
	line += "artima.com###floatingbox";
	line += "artima.com###topbanner";
	line += "arto.com###BannerInfobox";
	line += "asia.cnet.com###sp-box";
	line += "asia.cnet.com##.splink";
	line += "ask.com###rbox";
	line += "ask.com##.spl_unshd";
	line += "associatedcontent.com##div[style=\"width: 300px; height: 250px; position: relative;\"]";
	line += "associatedcontent.com##div[style=\"width: 300px; height: 250px;\"]";
	line += "asylum.co.uk##.sidebarBanner";
	line += "asylum.com##.sidebarBanner";
	line += "asylum.com##.topBanner";
	line += "atom.com###iframe_container300x250";
	line += "atomicgamer.com###bannerFeatures";
	line += "au.movies.yahoo.com##table.y7mv-wraptable[width=\"750\"][height=\"112\"]";
	line += "au.yahoo.com###y708-windowshade";
	line += "audioreview.com##.MiddleTableRightColumn";
	line += "audioreview.com##script + table[width=\"539\"]";
	line += "audioreview.com##table[width=\"300\"][style=\"border: 1px solid rgb(65, 103, 122); margin-left: 10px;\"]";
	line += "autoblog.com##.leader";
	line += "autoblog.com##.medrect";
	line += "autoblog.com##.topleader";
	line += "autobloggreen.com##.medrect";
	line += "autonews.com##div[style=\"width: 300px; height: 128px; margin-bottom: 5px; border-top: 2px solid rgb(236, 236, 236); border-bottom: 2px solid rgb(236, 236, 236); padding-top: 3px; font-family: arial,helvetica; font-size: 10px; text-align: center;\"]";
	line += "autonewseurope.com###header_bottom";
	line += "autonewseurope.com##div[style=\"width: 300px; height: 128px; margin-bottom: 5px; border-top: 2px solid rgb(236, 236, 236); border-bottom: 2px solid rgb(236, 236, 236); padding-top: 3px; font-family: arial,helvetica; font-size: 10px; text-align: center;\"]";
	line += "autosport.com##.content[width] td[height=\"17\"][bgcolor=\"#dcdcdc\"]";
	line += "autosport.com##td[align=\"center\"][valign=\"top\"][height=\"266\"][bgcolor=\"#dcdcdc\"]";
	line += "autotrader.co.uk###placeholderTopLeaderboard";
	line += "avaxsearch.com###bottom_block";
	line += "avaxsearch.com###top_block";
	line += "avsforum.com##td[width=\"125\"][valign=\"top\"][style=\"padding-left: 15px;\"]";
	line += "avsforum.com##td[width=\"193\"][valign=\"top\"]";
	line += "avsforum.com##td[width=\"300\"][valign=\"top\"][rowspan=\"3\"]";
	line += "awfulplasticsurgery.com##a[href=\"http://www.blogads.com\"]";
	line += "awfulplasticsurgery.com##a[href^=\"http://www.freeipadoffer.com/default.aspx?r=\"]";
	line += "azarask.in##.ad";
	line += "azstarnet.com##.bannerinstory";
	line += "babble.com.au###leaderboard-bottom";
	line += "babble.com.au###leaderboard-top";
	line += "babble.com.au###medium-rectangle";
	line += "babelfish.yahoo.com##.ovt";
	line += "babynamegenie.com##.promo";
	line += "bangkokpost.com##.boomboxSize1";
	line += "bangkokpost.com##.buzzBoombox";
	line += "basketball.com##td[width=\"530\"] + td[width=\"120\"]";
	line += "battellemedia.com##.sidebar";
	line += "bbc.co.uk##.bbccom_display_none";
	line += "bdnews24.com###bannerdiv2";
	line += "bdnews24.com##.add";
	line += "bebo.com##.spon-mod";
	line += "bebo.com##table[style=\"background-color: rgb(247, 246, 246);\"]";
	line += "belfasttelegraph.co.uk###yahooLinks";
	line += "belfasttelegraph.co.uk##.googleThird";
	line += "belfasttelegraph.co.uk##table[width=\"300\"][height=\"250\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"]";
	line += "bellinghamherald.com###mastBanner";
	line += "bestbuy.com###dart-container-728x90";
	line += "betterpropaganda.com##div[style=\"width: 848px; height: 91px; margin: 0pt; position: relative;\"]";
	line += "bigblueball.com###text-384255551";
	line += "bigdownload.com##.quigo";
	line += "bigpond.com###header_banner";
	line += "bikeradar.com###shopping_partner_box_fat";
	line += "bingo-hunter.com##img[width=\"250\"][height=\"250\"]";
	line += "birminghampost.net##.promotop";
	line += "bit-tech.net##div[style=\"width: 728px; height: 90px;\"]";
	line += "biz.yahoo.com##table[bgcolor=\"white\"][width=\"100%\"]";
	line += "bizrate.com###banner_top";
	line += "bizrate.com###rectangular";
	line += "blackberryforums.com##td[align=\"left\"][width=\"160\"][valign=\"top\"]";
	line += "blackberryforums.com.au##td[valign=\"top\"][style=\"width: 175px;\"]";
	line += "blackmesasource.com##.ad";
	line += "block.opendns.com###acbox";
	line += "bloggingbuyouts.com##.topleader";
	line += "bloggingstocks.com##.topleader";
	line += "blogoscoped.com##.adBlock";
	line += "blogoscoped.com##.adBlockBottom";
	line += "blogoscoped.com##.adBlockBottomBreak";
	line += "blogtv.com##div[style=\"width: 752px; height: 115px; padding-top: 5px; overflow: hidden;\"]";
	line += "blogtv.com##div[style=\"width: 752px; top: 5px; height: 100px; text-align: center; padding-top: 5px;\"]";
	line += "blogtv.com##div[style=\"width: 990px; height: 115px; padding-top: 5px; overflow: hidden;\"]";
	line += "blogtv.com##div[style=\"width: 990px; top: 5px; height: 100px; text-align: center; padding-top: 5px;\"]";
	line += "bloomberg.com##.leaderboard";
	line += "blurtit.com##.adblock";
	line += "boingboing.net###cheetos_collapsed";
	line += "boingboing.net##.ad";
	line += "boingboing.net##div[style=\"height: 630px; width: 300px;\"]";
	line += "bollywoodbuzz.in##div[style=\"height: 250px; width: 300px;\"]";
	line += "bollywoodbuzz.in##div[style=\"height: 90px; width: 728px;\"]";
	line += "books.google.ca###rhswrapper font[size=\"-1\"]";
	line += "books.google.co.nz###rhswrapper font[size=\"-1\"]";
	line += "books.google.co.uk###rhswrapper font[size=\"-1\"]";
	line += "books.google.co.za###rhswrapper font[size=\"-1\"]";
	line += "books.google.com###rhswrapper font[size=\"-1\"]";
	line += "books.google.com.au###rhswrapper font[size=\"-1\"]";
	line += "booookmark.com###sitematches";
	line += "boston.com###externalBanner";
	line += "bostonherald.com##div[style=\"position: relative; margin-bottom: 16px; background-color: rgb(233, 233, 233); border-left: 16px solid rgb(23, 23, 23); padding: 20px 12px 20px 20px; clear: both;\"]";
	line += "brandsoftheworld.com###leaderboardTop";
	line += "break.com##.ad";
	line += "break.com##.breaking_news";
	line += "breakingviews.com###floatit";
	line += "breitbart.com##.sidebar";
	line += "briefmobile.com##td[style=\"height: 90px; width: 960px; background-color: rgb(255, 255, 255); padding: 10px; vertical-align: middle;\"]";
	line += "brighouseecho.co.uk###banner01";
	line += "brisbaneroar.com.au##.promotion_wrapper";
	line += "brisbanetimes.com.au##.ad";
	line += "broadbandreports.com##td[width=\"125\"][style=\"border-right: 1px solid rgb(204, 204, 204);\"]";
	line += "broadcastnewsroom.com###shopperartbox";
	line += "broadcastnewsroom.com##.bfua";
	line += "broadcastnewsroom.com##.bottombanner";
	line += "brothersoft.com##.sponsor";
	line += "btjunkie.org###main > div[height=\"10\"]:first-child + table[width=\"100%\"]";
	line += "btjunkie.org###main > div[height=\"10\"]:first-child + table[width=\"100%\"] + .tab_results";
	line += "btjunkie.org##th[align=\"left\"][height=\"100%\"]";
	line += "buenosairesherald.com###publiTopHeader";
	line += "buffalonews.com###bot-main";
	line += "buffalonews.com##.leaderboard_top";
	line += "builderau.com.au###leaderboard";
	line += "bunalti.com##img[width=\"728\"][height=\"90\"]";
	line += "business.com###railFls";
	line += "business.com###sponsoredwellcontainerbottom";
	line += "business.com###sponsoredwellcontainertop";
	line += "business.com##.wellFls";
	line += "businessdailyafrica.com##.c15r";
	line += "businessdictionary.com###topBnr";
	line += "businessinsider.com###FM1";
	line += "businessinsurance.com##.StoryInsert";
	line += "businesstimes.com.sg##td[bgcolor=\"#333333\"]";
	line += "businessweek.com###bwMall";
	line += "businessweek.com##.ad";
	line += "buxtonadvertiser.co.uk###banner01";
	line += "buzzfocus.com###eyebrowtop";
	line += "buzznet.com###topSection";
	line += "c21media.net##table[border=\"0\"][width=\"130\"]";
	line += "caller.com###content_match_wrapper";
	line += "caller.com##.bigbox_wrapper";
	line += "campustechnology.com###leaderboard";
	line += "candystand.com##.cs_square_banner";
	line += "candystand.com##.cs_wide_banner";
	line += "canmag.com##td[align=\"center\"][height=\"278\"]";
	line += "canoe.ca###commerce";
	line += "canoe.ca###subbanner";
	line += "cantbeunseen.com###top-leaderboard";
	line += "cantbeunseen.com##.leaderboard";
	line += "caranddriver.com##.shopping-tools";
	line += "carrentals.co.uk##div[style=\"float: right; width: 220px; height: 220px;\"]";
	line += "carsguide.com.au##.CG_loancalculator";
	line += "cataloguecity.co.uk##.bordered";
	line += "catholicnewsagency.com##div[style=\"background-color: rgb(247, 247, 247); width: 256px; height: 250px;\"]";
	line += "caymannewsservice.com###content-top";
	line += "caymannewsservice.com##[style=\"width: 175px; height: 200px;\"]";
	line += "caymannewsservice.com##[style=\"width: 450px; height: 100px;\"]";
	line += "caymannewsservice.com##[style=\"width: 550px; height: 100px;\"]";
	line += "cboe.com###simplemodal-overlay";
	line += "cbs5.com##.cbstv_partners_wrap";
	line += "cbsnews.com##.searchSponsoredResultsList";
	line += "cbssports.com###leaderboardRow";
	line += "cbssports.com##table[cellpadding=\"0\"][width=\"310\"]";
	line += "ccfcforum.com##.tablepad";
	line += "ccfcforum.com##img[alt=\"ISS\"]";
	line += "ccmariners.com.au##.promotion_wrapper";
	line += "celebnipslipblog.com###HeaderBanner";
	line += "celebuzz.com###bmSuperheader";
	line += "cell.com###main_banner";
	line += "cgenie.com###ja-banner";
	line += "cgenie.com##.cgenie_banner4";
	line += "chacha.com##.show-me-the-money";
	line += "chairmanlol.com###top-leaderboard";
	line += "chairmanlol.com##.leaderboard";
	line += "chami.com##.c";
	line += "channel3000.com###leaderboard-sticky";
	line += "checkoutmyink.com###centerbanner";
	line += "checkoutmyink.com###mid";
	line += "checkthisvid.com##a[href^=\"http://links.verotel.com/\"]";
	line += "chicagobreakingbusiness.com##.ad";
	line += "chicagotribune.com###story-body-parent + .rail";
	line += "chinadaily.com.cn##table[width=\"130\"][height=\"130\"]";
	line += "chinapost.com.tw###winner";
	line += "chinasmack.com##.ad";
	line += "chinatechnews.com###banner1";
	line += "christianitytoday.com##.bgbanner";
	line += "christianitytoday.com##.bgshop";
	line += "chrome-hacks.net##div[style=\"width: 600px; height: 250px;\"]";
	line += "chronicle.northcoastnow.com##table[width=\"100%\"][height=\"90\"][bgcolor=\"#236aa7\"]";
	line += "cio.com.au##.careerone_search";
	line += "cio.com.au##.careerone_tj_box";
	line += "citynews.ca###SuperBannerContainer";
	line += "citynews.ca##.Box.BigBox";
	line += "citypaper.com###topLeaderboard";
	line += "citypaper.com##div[style=\"display: block; width: 980px; height: 120px;\"]";
	line += "classifieds.aol.co.uk###dmn_results";
	line += "classifieds.aol.co.uk###dmn_results1";
	line += "clubwebsite.co.uk##td[width=\"158\"][valign=\"top\"] > start_lspl_exclude > end_lspl_exclude > .boxpadbot[width=\"100%\"][cellspacing=\"0\"][cellpadding=\"6\"][border=\"0\"][style=\"background-color: rgb(0, 51, 0);\"]:last-child";
	line += "cnbc.com##.fL[style=\"width: 185px; height: 40px; margin: 10px 0pt 0pt 25px; float: none;\"]";
	line += "cnbc.com##.fL[style=\"width: 365px; margin-bottom: 20px; margin-top: 0px; padding-top: 0px; padding-left: 25px; padding-bottom: 100px; border-top: 1px solid rgb(204, 204, 204); border-left: 1px solid rgb(204, 204, 204);\"]";
	line += "cnbc.com##.fL[style=\"width: 960px; height: 90px; margin: 0pt 0pt 5px;\"]";
	line += "cnet.com##.ad";
	line += "cnet.com##.bidwar";
	line += "cnet.com.au##.ad";
	line += "cnet.com.au##.explain-promo";
	line += "cnmnewsnetwork.com###rightcol";
	line += "cnn.com##.cnnSearchSponsorBox";
	line += "cnsnews.com###ctl00_leaderboard";
	line += "cocoia.com###ad";
	line += "codeasily.com##.money";
	line += "codinghorror.com##.welovecodinghorror";
	line += "coffeegeek.com##img[width=\"200\"][height=\"250\"]";
	line += "coffeegeek.com##img[width=\"200\"][height=\"90\"]";
	line += "coffeegeek.com##td[align=\"center\"][width=\"100%\"][valign=\"middle\"]";
	line += "coldfusion.sys-con.com###header-title";
	line += "collegehumor.com##.partner_links";
	line += "columbiatribune.com##.ad";
	line += "columbiatribune.com##.skyscraper";
	line += "com.au##table.fdMember";
	line += "comcast.net##.ad";
	line += "comedy.com###hat";
	line += "comicartfans.com###contentcolumn:last-child > center > table[cellspacing=\"0\"][cellpadding=\"1\"][border=\"0\"]:first-child";
	line += "comicgenesis.com###ks_da";
	line += "comicsalliance.com##.sidebarBanner";
	line += "comicsalliance.com##.topBanner";
	line += "comingsoon.net###col2TopPub";
	line += "comingsoon.net###upperPub";
	line += "complex.com##.ad";
	line += "complex.com##div[style=\"float: left; position: relative; margin: 5px auto; width: 960px; height: 90px; border: 0px solid rgb(0, 0, 0); text-align: center;\"]";
	line += "computeractive.co.uk##.leaderboard";
	line += "computerandvideogames.com###skyslot";
	line += "computerweekly.com##.sponsors";
	line += "computerworld.com##table[align=\"center\"][width=\"336\"][valign=\"top\"]";
	line += "computerworld.com##table[width=\"342\"][height=\"290\"][bgcolor=\"#bbbbbb\"]";
	line += "computerworlduk.com###bottomBanner";
	line += "computerworlduk.com###topBanner";
	line += "computing.co.uk##.leaderboard";
	line += "computing.net###top_banner";
	line += "computingondemand.com###sub-nav";
	line += "cookingforengineers.com##div[style=\"border: 0px solid rgb(255, 255, 160); width: 160px; height: 600px;\"]";
	line += "cookingforengineers.com##div[style=\"border: 0px solid rgb(255, 255, 160); width: 728px; height: 90px; margin: 0pt auto;\"]";
	line += "cookingforengineers.com##div[style=\"height: 60px; width: 120px; margin: 0pt 20px 5px;\"]";
	line += "coolest-gadgets.com##.banner1";
	line += "coolest-gadgets.com##.contentbox";
	line += "coolest-gadgets.com##.contentboxred";
	line += "core77.com###rsDesignDir";
	line += "countryliving.com###sub_promo";
	line += "cpu-world.com##table[width=\"760\"][style=\"border: 1px solid rgb(64, 64, 64);\"]";
	line += "cracked.com##.Ad";
	line += "crackserver.com##input[onclick^=\"window.open('http://www.friendlyduck.com/AF_\"]";
	line += "crazymotion.net###fadeinbox";
	line += "crazymotion.net##[style=\"margin: 10px auto 0pt; width: 875px;\"]";
	line += "cricbuzz.com###chrome_home_banner";
	line += "cricbuzz.com###tata_phton_home_banner";
	line += "cricinfo.com##.ciHomeSponcerLink";
	line += "cricinfo.com##.hpSpncrHead";
	line += "cricinfo.com##.seatwaveM";
	line += "cricinfo.com##.seriesSpncr";
	line += "cricvid.info###bannerfloat2";
	line += "crikey.com.au###top";
	line += "crikey.com.au##.crikey_widget_small_island";
	line += "crmbuyer.com##.content-block-slinks";
	line += "crmbuyer.com##.content-tab-slinks";
	line += "crn.com###channelwebmarketplacewrapper";
	line += "crooksandliars.com###block-clam-1";
	line += "crooksandliars.com###block-clam-3";
	line += "crooksandliars.com###block-clam-7";
	line += "crunchgear.com##.ad";
	line += "crunchyroll.com##.anime-mrec";
	line += "crunchyroll.com##a[href^=\"http://clk.atdmt.com/\"]";
	line += "cubeecraft.com###leaderboard";
	line += "cultofmac.com###skyBlock";
	line += "cyberciti.biz###leaderboard";
	line += "cynagames.com##li[style=\"width: 25%; margin: 0pt; clear: none; padding: 0pt; float: left; display: block;\"]";
	line += "dailyblogtips.com##img[border=\"0\"]";
	line += "dailyblogtips.com##img[style=\"margin-right: 16px;\"]";
	line += "dailyblogtips.com##img[width=\"125\"][height=\"125\"]";
	line += "dailyfinance.com##div[style=\"background: url(\\\"http://o.aolcdn.com/art/ch_pf/advertisement-text\\\") no-repeat scroll 295px 90px rgb(240, 240, 240); padding-top: 20px; margin: 0pt 0pt 10px; height: 84px;\"]";
	line += "dailyfreegames.com###banner_886x40";
	line += "dailyfreegames.com###topratedgames";
	line += "dailyhaha.com###sponheader";
	line += "dailymail.co.uk##.classified-list";
	line += "dailymotion.com##.dmpi_masscast";
	line += "dailymotion.com##.dmpi_subheader";
	line += "dailymotion.com##.ie_download";
	line += "dailymotion.com##.masscast_box_Middle";
	line += "dailystar.co.uk###hugebanner";
	line += "dailystar.co.uk##.greyPanelOuter";
	line += "dailystar.co.uk##.greyPanelOuterSmall";
	line += "dailystar.co.uk##div[style=\"width: 165px; text-align: center; border: 1px solid rgb(184, 184, 184);\"]";
	line += "dailystar.co.uk##div[style=\"width: 300px; height: 250px; background: url(\\\"http://cdn.images.dailystar-uk.co.uk/img/adverts/mpufail.gif\\\") repeat scroll 0% 0% transparent;\"]";
	line += "dancingwhilewhite.com##.add";
	line += "daniweb.com###textsponsor";
	line += "daparto.de###leaderboard";
	line += "daringfireball.net###SidebarTheDeck";
	line += "darkhorizons.com###content-island";
	line += "dataopedia.com##.container_banner";
	line += "deadspin.com###skyscraper";
	line += "dealbrothers.com##.specials";
	line += "dealmac.com###banner-bottom";
	line += "dealnews.com##.banner";
	line += "deargirlsaboveme.com##.ad";
	line += "deditv.com##.overlayVid";
	line += "deletedspam.blogspot.com##.LinkList";
	line += "deletedspam.blogspot.com##img[width=\"125\"][height=\"125\"]";
	line += "delicious.com###spns";
	line += "deliciousdays.com###adlove";
	line += "deliciousdays.com###book";
	line += "deliciousdays.com###recipeshelf";
	line += "demogeek.com##div[style=\"height: 250px; width: 250px; margin: 10px;\"]";
	line += "demogeek.com##div[style=\"height: 280px; width: 336px; margin: 10px;\"]";
	line += "demonoid.com##.pad9px_right";
	line += "denofgeek.com##.skyright";
	line += "depositfiles.com###adv_banner_sidebar";
	line += "derbyshiretimes.co.uk###banner01";
	line += "derbyshiretimes.co.uk##.roundedboxesgoogle";
	line += "deseretnews.com##.continue";
	line += "deskbeauty.com##tr > td[width=\"100%\"][height=\"95\"][align=\"center\"]";
	line += "destructoid.com##div[style=\"overflow: hidden; width: 300px; height: 250px;\"]";
	line += "develop-online.net##.newsinsert";
	line += "deviantart.com###overhead-you-know-what";
	line += "deviantart.com##.ad-blocking-makes-fella-confused";
	line += "deviantart.com##.hidoframe";
	line += "deviantart.com##.sleekadbubble";
	line += "deviantart.com##.subbyCloseX";
	line += "deviantart.com##a[href^=\"http://advertising.deviantart.com/\"]";
	line += "deviantart.com##div[gmi-name=\"ad_zone\"]";
	line += "deviantart.com##div[style=\"float: right; position: relative; width: 410px; text-align: left;\"]";
	line += "devx.com##.expwhitebox > table[cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][align=\"center\"][style=\"margin-left: 0pt; margin-bottom: 0pt;\"]:last-child";
	line += "devx.com##.expwhitebox[style=\"border: 0px none;\"] > table[cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][align=\"right\"]:first-child";
	line += "devx.com##div[align=\"center\"][style=\"margin-top: 0px; margin-bottom: 0px; width: 100%;\"]";
	line += "devx.com##div[style=\"margin: 20px auto auto;\"] > div[align=\"center\"][style=\"margin-top: 0px; margin-bottom: 0px; width: 100%; padding: 10px;\"]";
	line += "devx.com##div[style=\"margin: 20px auto auto;\"] > table[align=\"center\"][style=\"border: 2px solid rgb(255, 102, 0); padding-right: 2px; width: 444px; background-color: rgb(255, 255, 255); text-align: left;\"]";
	line += "devx.com##table[width=\"164\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][style=\"margin-bottom: 5px;\"]:first-child:last-child";
	line += "dgvid.com##.overlayVid";
	line += "dickens-literature.com##td[width=\"7%\"][valign=\"top\"]";
	line += "dictionary.co.uk##table[border=\"0\"][width=\"570\"]";
	line += "didyouwatchporn.com###bottomBanner";
	line += "didyouwatchporn.com###recos_box";
	line += "digitalspy.co.uk##.marketing_puff";
	line += "dir.yahoo.com##td[width=\"215\"]";
	line += "discountvouchers.co.uk##a[rel=\"nofollow\"]";
	line += "discovery.com##.rectangle";
	line += "dishusa.net##div[style=\"border: 1px dotted rgb(190, 190, 190); background-color: rgb(255, 255, 224); padding: 5px;\"]";
	line += "dishusa.net##table[style=\"border: 3px outset rgb(87, 173, 198); font-size: 16px; background-color: rgb(253, 252, 240); margin-bottom: 10px;\"]";
	line += "disney.go.com###banner";
	line += "disney.go.com###superBanner";
	line += "disney.go.com##div[style=\"position: relative; float: right; clear: right; width: 300px; height: 260px; top: 5px; margin: 10px 0px 5px 5px;\"]";
	line += "divxden.com###divxshowboxt > a[target=\"_blank\"] > img[width=\"158\"]";
	line += "divxden.com##.ad";
	line += "divxden.com##.header_greenbar";
	line += "divxme.com##a[href^=\"http://www.jdoqocy.com/\"]";
	line += "divxstage.net##.ad";
	line += "diyfail.com###top-leaderboard";
	line += "diyfail.com##.leaderboard";
	line += "dkszone.net##.liutilities-top";
	line += "dl4all.com###deluxePopupWindow-container";
	line += "dogpile.com##.paidSearchResult";
	line += "dosgamesarchive.com###banner";
	line += "dosgamesarchive.com##.rectangle";
	line += "dosgamesarchive.com##.skyscraper";
	line += "dotsauce.com###leadsponsor";
	line += "dotsauce.com##.onetwentyfive";
	line += "dotsauce.com##img[width=\"260\"][height=\"260\"]";
	line += "downforeveryoneorjustme.com###container > .domain + p + br + center:last-child";
	line += "downloadhelper.net##.banner-468x60";
	line += "downloadsquad.com###newjobs-module";
	line += "downloadsquad.com###topleader-wrap";
	line += "downloadsquad.com##.medrect";
	line += "dragcave.net###prefooter";
	line += "drive.com.au##.cA-twinPromo";
	line += "drugs.com###topbannerWrap";
	line += "dt-updates.com##.adv_items";
	line += "dt-updates.com##div[style=\"margin: 20px auto 0pt; text-align: left;\"]";
	line += "dubbed-scene.com##.adblock";
	line += "dubcnn.com##img[border=\"0\"][width=\"200\"]";
	line += "dumbassdaily.com##a[href$=\".clickbank.net\"]";
	line += "dumbassdaily.com##a[href^=\"http://www.badjocks.com\"]";
	line += "dumblittleman.com##.ad";
	line += "dumblittleman.com##.cats_box2";
	line += "dv.com##table[width=\"665\"]";
	line += "eartheasy.com##td[width=\"200\"][height=\"4451\"][bgcolor=\"#e1e3de\"][rowspan=\"19\"]";
	line += "earthweb.com##.footerbanner";
	line += "easybib.com##.banner";
	line += "ebaumsworld.com###eacs-sidebar";
	line += "ebuddy.com###Rectangle";
	line += "ebuddy.com###banner_rectangle";
	line += "eclipse.org##.ad";
	line += "ecommerce-journal.com###runnews";
	line += "ecommercetimes.com##.slink-text";
	line += "ecommercetimes.com##.slink-title";
	line += "economist.com###classified_wrapper";
	line += "economist.com###footer-classifieds";
	line += "economist.com###leaderboard";
	line += "economist.com###top_banner";
	line += "ecr.co.za###thebug";
	line += "ecr.co.za##.block_120x600";
	line += "ectnews.com###welcome-box";
	line += "ectnews.com##.content-block-slinks";
	line += "ectnews.com##.content-tab-slinks";
	line += "edge-online.com###above-header-region";
	line += "edn.com###headerwildcard";
	line += "edn.com##.sponsorcontent";
	line += "edn.com##div[style=\"font-family: verdana,sans-serif; font-style: normal; font-variant: normal; font-weight: normal; font-size: 10px; line-height: normal; font-size-adjust: none; font-stretch: normal; -x-system-font: none; color: rgb(51, 51, 51); background-color: rgb(160, 186, 200); padding-bottom: 7px;\"]";
	line += "eeeuser.com###header";
	line += "efinancialnews.com##.promo-leaderboard";
	line += "egreetings.com##td[style=\"background-color: rgb(255, 255, 255); vertical-align: top;\"]";
	line += "ehow.com###jsWhoCanHelp";
	line += "ehow.com##.takeoverBanner";
	line += "el33tonline.com###AdA";
	line += "el33tonline.com###AdB";
	line += "el33tonline.com###AdC";
	line += "el33tonline.com###AdD";
	line += "el33tonline.com###AdE";
	line += "el33tonline.com###AdF";
	line += "el33tonline.com###AdG";
	line += "el33tonline.com###AdH";
	line += "el33tonline.com###AdI";
	line += "electricenergyonline.com###top_pub";
	line += "electricenergyonline.com###tower_pub";
	line += "electricenergyonline.com##.sponsor";
	line += "electronista.com###footerleft";
	line += "electronista.com###footerright";
	line += "electronista.com###leaderboard";
	line += "electronista.com###supportbod";
	line += "elizium.nu##center > ul[style=\"padding: 0pt; width: 100%; margin: 0pt; list-style: none outside none;\"]";
	line += "elle.com###ad-block-bottom";
	line += "emaillargefile.com##a[href^=\"http://www.mb01.com/lnk.asp?\"]";
	line += "emedtv.com###leaderboard";
	line += "empireonline.com##table[align=\"center\"][width=\"950\"][height=\"130\"]";
	line += "empireonline.com##td.smallgrey[width=\"300\"][height=\"250\"]";
	line += "engadget.com##.medrect";
	line += "engadget.com##.siteswelike";
	line += "england.fm###banbo";
	line += "englishforum.ch##td[style=\"width: 160px; padding-left: 15px;\"]";
	line += "englishforum.ch##td[width=\"176\"][style=\"padding-left: 15px;\"]";
	line += "environmentalgraffiti.com##div[style=\"width: 300px; height: 250px; overflow: hidden;\"]";
	line += "eonline.com###franchise";
	line += "eonline.com###module_sky_scraper";
	line += "epicurious.com###sweepstakes";
	line += "epinions.com##td[width=\"180\"][valign=\"top\"]";
	line += "eq2flames.com##td[width=\"120\"][style=\"padding-left: 5px; white-space: normal;\"]";
	line += "erictric.com###head-banner468";
	line += "esecurityplanet.com###gemhover";
	line += "esecurityplanet.com###partners";
	line += "esecurityplanet.com##.vspace";
	line += "esl.eu##.bannerContainer";
	line += "esoft.web.id###content-top";
	line += "espn.go.com##.mast-container";
	line += "espn.go.com##.spons_optIn";
	line += "esus.com##a[href=\"http://www.regiochat.be\"]";
	line += "etonline.com##.superbanner";
	line += "eurogamer.net###skyscraper";
	line += "eurogamer.net###tabbaz";
	line += "euronews.net###OAS1";
	line += "euronews.net###OAS2";
	line += "euronews.net##.col-pub-skyscraper";
	line += "euronews.net##.google-banner";
	line += "everyjoe.com##.ad";
	line += "eweek.com###Table_01";
	line += "eweek.com###hp_special_reports";
	line += "eweek.com###syndication";
	line += "eweek.com##.hp_link_online_classifieds";
	line += "eweek.com##.omniture_module_tracker";
	line += "eweek.com##table[width=\"500\"][style=\"border: 1px solid rgb(204, 204, 204); margin: 5px 10px 5px 20px;\"]";
	line += "exactseek.com###featured";
	line += "exactseek.com##.recommended";
	line += "examiner.co.uk##.promotop";
	line += "examiner.com##.headerbg";
	line += "excelforum.com##.contentLeft";
	line += "excelforum.com##div[style=\"width: 300px; height: 250px; float: left; display: inline; margin-left: 5%;\"]";
	line += "excelforum.com##div[style=\"width: 300px; height: 250px; float: right; display: inline; margin-left: 5%;\"]";
	line += "excite.com##.mexContentBdr";
	line += "expertreviews.co.uk###skin";
	line += "expertreviews.co.uk###skyScrapper";
	line += "expertreviews.co.uk##.leaderBoard";
	line += "expertreviews.co.uk##.leaderLeft";
	line += "expertreviews.co.uk##.leaderRight";
	line += "experts-exchange.com###compCorpAcc";
	line += "experts-exchange.com###compSky";
	line += "experts-exchange.com##.ontopBanner";
	line += "explainthisimage.com###top-leaderboard";
	line += "explainthisimage.com##.leaderboard";
	line += "explosm.net##td[height=\"90\"][bgcolor=\"#000000\"][style=\"border: 3px solid rgb(55, 62, 70);\"]";
	line += "extremeoverclocking.com##td[height=\"104\"][colspan=\"2\"]";
	line += "facebook.com###home_sponsor_nile";
	line += "facebook.com##.ego_spo";
	line += "facebook.com##.fbEmu";
	line += "factoidz.com##div[style=\"float: left; margin: 0pt 30px 20px 0pt; width: 336px; height: 280px;\"]";
	line += "fairyshare.com##.google_top";
	line += "famousbloggers.net###hot_offer";
	line += "famousbloggers.net##.stop_sign";
	line += "fanhouse.com##.ad";
	line += "fanpix.net###leaderboard";
	line += "fanpop.com###rgad";
	line += "fanpop.com##div[style=\"width: 300px; height: 250px; background-color: rgb(0, 0, 0); color: rgb(153, 153, 153);\"]";
	line += "fark.com###rightSideRightMenubar";
	line += "fasterlouder.com.au##.ad";
	line += "faststats.cricbuzz.com##td[style=\"width: 300px; height: 250px;\"]";
	line += "fatwallet.com###promoBand";
	line += "favicon.co.uk##img[width=\"190\"][height=\"380\"]";
	line += "faxo.com###fa_l";
	line += "fayobserver.com###bottom-leaderboard";
	line += "feedburner.com##a[href^=\"http://ads.pheedo.com/\"]";
	line += "feedicons.com###footerboard";
	line += "feministing.com###bannerBottom";
	line += "feministing.com###bannerLeft";
	line += "feministing.com###bannerTop";
	line += "ffiles.com###right_col";
	line += "file-extensions.org###uniBan";
	line += "filedropper.com###sidebar";
	line += "filefactory.com###aContainer";
	line += "filefront.com##div[style=\"width: 300px; min-height: 250px;\"]";
	line += "filehippo.com##.ad";
	line += "filestube.com##.nova";
	line += "filetrip.net###products";
	line += "finance.yahoo.com###yfi_pf_ysm";
	line += "finance.yahoo.com###yfi_ysm";
	line += "finance.yahoo.com##.ad";
	line += "finance.yahoo.com##.ysm";
	line += "financialpost.com###npLeaderboardRow";
	line += "findfiles.com##.tvisible[width=\"468\"][cellspacing=\"0\"][cellpadding=\"0\"][bgcolor=\"#ffffff\"][align=\"center\"]";
	line += "firewallguide.com##td[width=\"300\"][height=\"250\"]";
	line += "flashgot.net##.tla";
	line += "flashscore.com##div[style=\"height: 240px ! important;\"]";
	line += "flashscore.com##div[style=\"height: 90px ! important;\"]";
	line += "flixster.com##div[style=\"position: relative; height: 270px;\"]";
	line += "flvz.com###additional_plugins_bar";
	line += "flvz.com##a[href^=\"http://www.flvpro.com/movies/?aff=\"]";
	line += "fontstock.net##.mediaBox";
	line += "foodnetwork.ca##.bannerContainer";
	line += "foodnetwork.ca##.bboxContainer";
	line += "foodnetwork.co.uk###pre-header-banner";
	line += "foodnetwork.co.uk##.right_header_row";
	line += "foodnetwork.com##.mrec";
	line += "fool.com###promoAndLeaderboard";
	line += "footylatest.com###leaderboardspace";
	line += "forbes.com##.fifthN";
	line += "forbes.com##.top_banner";
	line += "forbes.com##div[style=\"width: 125px; height: 125px; padding: 20px 20px 20px 25px; float: left;\"]";
	line += "forbes.com##div[style=\"width: 125px; height: 125px; padding: 20px 25px 20px 20px; float: right;\"]";
	line += "forrst.com##.ad";
	line += "forum.notebookreview.com##td[width=\"400\"][height=\"280\"]";
	line += "forum.rpg.net##img[border=\"0\"][style=\"outline: medium none;\"]";
	line += "forums.battle.net##td[align=\"center\"][width=\"130\"]";
	line += "forums.scifi.com###flippingBanner";
	line += "forums.vr-zone.com##.perm_announcement";
	line += "forums.worldofwarcraft.com##td[align=\"center\"][width=\"130\"]";
	line += "forums.worldofwarcraft.com##td[width=\"130px\"][valign=\"top\"][align=\"center\"]:last-child";
	line += "forums.wow-europe.com##td[align=\"center\"][width=\"130\"]";
	line += "forums.wow-europe.com##td[width=\"130px\"][valign=\"top\"][align=\"center\"]:last-child";
	line += "forumserver.twoplustwo.com##td[width=\"120\"][valign=\"top\"][style=\"padding-left: 10px;\"]";
	line += "fox.com##.ad";
	line += "foxbusiness.com###cb_medrect1_div";
	line += "foxnews.com###console300x100";
	line += "foxnews.com###marketplace";
	line += "foxnews.com##.ad";
	line += "foxnews.com##.quigo";
	line += "fpsbanana.com##a[href^=\"http://www.xfactorservers.com/clients/link.php?id=\"]";
	line += "free-tv-video-online.info##a[style=\"margin-left: auto; font-size: 14px; padding: 3px; margin-right: auto; width: 640px; display: block; text-decoration: none;\"]";
	line += "freecodesource.com###banner";
	line += "freedict.com##.partners";
	line += "freeiconsdownload.com###LeftBanner";
	line += "freestreamtube.com###ad";
	line += "freshmeat.net##.banner-imu";
	line += "freshmeat.net##.banner1";
	line += "friday-ad.co.uk##.PlateFood";
	line += "ft.com###leaderboard";
	line += "ft.com##.marketing";
	line += "ftv.com###hdr_c";
	line += "ftv.com###hdr_r";
	line += "fudzilla.com###showcase";
	line += "fudzilla.com##.artbannersxtd";
	line += "funnyexam.com###top-leaderboard";
	line += "funnyexam.com##.leaderboard";
	line += "funnytipjars.com###top-leaderboard";
	line += "funnytipjars.com##.leaderboard";
	line += "fxnetworks.com###adBlock";
	line += "gadgetzone.com.au###Leaderboard-placeholder";
	line += "gadgetzone.com.au##td[style=\"width: 300px;\"]";
	line += "gadling.com##.medrect";
	line += "gadling.com##.topleader";
	line += "gamebanshee.com##.banner";
	line += "gamegrep.com##.leaderboard_unit";
	line += "gamepro.com.au##.rhs_300x250";
	line += "gamerevolution.com##td[height=\"100\"][style=\"padding-left: 5px; padding-top: 5px; padding-right: 5px;\"]";
	line += "gamernode.com##.ad";
	line += "gamerstemple.com###banner";
	line += "gamerstemple.com###tower1";
	line += "gamerstemple.com###tower2";
	line += "gamersyde.com##.placeholder-top";
	line += "games.com##.ad";
	line += "gamesindustry.biz###leader";
	line += "gamesindustry.biz###leader-container";
	line += "gamesradar.com##.tablets";
	line += "gawker.com###skySpacer";
	line += "gawker.com###skyscraper";
	line += "gbrej.com###bottom_banner";
	line += "gearlive.com###google";
	line += "gearlive.com##.wellvert";
	line += "geek.com##.leaderboard";
	line += "geek.com##.picksBox";
	line += "geek.com##a[href^=\"http://www.geek.com/partners?\"]";
	line += "geek.com##td[width=\"170\"]";
	line += "geekologie.com###leaderboard";
	line += "generation-nt.com##.innerpub125";
	line += "generation-nt.com##.innerpub250";
	line += "generation-nt.com##.pub125";
	line += "generation-nt.com##.pub2";
	line += "generation-nt.com##.pub3";
	line += "genesisowners.com##.tborder[width=\"160\"]";
	line += "genuineforextrading.com###clickbank";
	line += "get-ip.de##div[style=\"display: block; background: none repeat scroll 0% 0% rgb(238, 238, 238); width: 300px; height: 250px; margin-top: 10px;\"]";
	line += "get-ip.de##div[style=\"display: block; background: none repeat scroll 0% 0% rgb(238, 238, 238); width: 300px; height: 250px;\"]";
	line += "getfoxyproxy.org###ad";
	line += "gethuman.com##td[style=\"width: 200px;\"]";
	line += "getprice.com.au##li[style=\"clear: both; padding-left: 0pt; padding-bottom: 0pt; width: 580px;\"]";
	line += "ghacks.net##.gutterlink";
	line += "gigabyteupload.com##input[onclick^=\"window.location.href='http://www.affbuzzads.com/affiliate/\"]";
	line += "gigasize.com##.topbanner";
	line += "gigwise.com###skyscraper";
	line += "giveawayoftheday.com##.before_hot_tags";
	line += "givesmehope.com###droitetop";
	line += "gizmocrunch.com##div[style=\"background-color: rgb(235, 242, 247); width: 560px;\"]";
	line += "gizmodo.com###marquee-frame";
	line += "gizmodo.com###skySpacer";
	line += "gizmodo.com###skyscraper";
	line += "gizmodo.com###spacer160";
	line += "gmanews.tv##div[style=\"width: 250px; height: 280px; border-top: 1px solid rgb(204, 204, 204);\"]";
	line += "goal.com###marketplaceModule";
	line += "goal.com##.betting200x120";
	line += "goal.com##.betting364x80";
	line += "goauto.com.au###leftnavcontainer + table[width=\"130\"]";
	line += "gocurrency.com###gosense";
	line += "goldcoastunited.com.au##.promotion_wrapper";
	line += "golivewire.com##div[style=\"height: 292px; margin-left: 10px; background-image: url(http://img.golivewire.com/stickynote-gray.gif); background-repeat: no-repeat; background-position: 0px 3px; padding-left: 26px; padding-top: 26px;\"]";
	line += "good.is##.ad";
	line += "goodhopefm.co.za##.mrec";
	line += "goodhousekeeping.com###hpV2_728x90";
	line += "google.co.uk##.ts[style=\"margin: 0pt 0pt 12px; height: 92px;\"]";
	line += "google.com##.ts[style=\"margin: 0pt 0pt 12px; height: 92px;\"]";
	line += "googletutor.com##div[style=\"width: 125px; text-align: center;\"]";
	line += "googlewatch.eweek.com###topBanner";
	line += "governmentvideo.com##table[width=\"665\"]";
	line += "gpsreview.net###lead";
	line += "grapevine.is##.ad";
	line += "grapevine.is##div[style=\"padding: 12px 0pt; text-align: center;\"]";
	line += "grindtv.com###LREC";
	line += "grindtv.com###SKY";
	line += "grindtv.com###hLREC";
	line += "growingbusiness.co.uk##.siteLeaderBoard";
	line += "gtplanet.net###a2";
	line += "gtplanet.net###a3";
	line += "guardian.co.uk###commercial-partners";
	line += "guardian.co.uk##.kelkoo";
	line += "guitaretab.com##.ring_link";
	line += "guru3d.com##a[href^=\"http://driveragent.com/?ref=\"]";
	line += "guruji.com###SideBar";
	line += "guruji.com##div[style=\"border: 1px solid rgb(250, 239, 209); margin: 0px 4px; padding: 4px; background-color: rgb(255, 248, 221);\"]";
	line += "h-online.com##.bcadv";
	line += "haaretz.com##.affiliates";
	line += "haaretz.com##.buttonBanners";
	line += "halifaxcourier.co.uk###banner01";
	line += "hardocp.com##.ad";
	line += "harpers.org##.topbanner";
	line += "hdtvtest.co.uk##.deal";
	line += "healthboards.com##td[\\!valign=\"top\"]";
	line += "healthboards.com##td[align=\"left\"][width=\"300\"]:first-child";
	line += "hebdenbridgetimes.co.uk###banner01";
	line += "helenair.com##table.bordered[align=\"center\"][width=\"728\"]";
	line += "hellmode.com###header";
	line += "hellomagazine.com###publi";
	line += "help.com###bwp";
	line += "helpwithwindows.com###ad";
	line += "helpwithwindows.com###desc";
	line += "heraldscotland.com###leaderboard";
	line += "heraldsun.com.au##.multi-promo";
	line += "hi5.com###hi5-common-header-banner";
	line += "hi5.com##.hi5-common-header-banner-ad";
	line += "highdefdigest.com##table[width=\"300\"][cellspacing=\"0\"][cellpadding=\"0\"]";
	line += "hilarious-pictures.com###block-block-1";
	line += "hilarious-pictures.com###block-block-12";
	line += "hilarious-pictures.com###block-block-8";
	line += "hilarious-pictures.com##.horizontal";
	line += "hindustantimes.com##.story_lft_wid";
	line += "hitfix.com##.googlewide";
	line += "hollywoodreporter.com##.ad";
	line += "holyfragger.com##.ad";
	line += "hotfrog.com##.search-middle-adblock";
	line += "hotfroguk.co.uk##.search-middle-adblock";
	line += "hotjobs.yahoo.com###sponsorResults";
	line += "hotlinkfiles.com###leaderboard";
	line += "hotshare.net##.publi_videos1";
	line += "howstuffworks.com###MedRectHome";
	line += "howstuffworks.com###SponLogo";
	line += "howstuffworks.com##.adv";
	line += "howstuffworks.com##.ch";
	line += "howstuffworks.com##.search-span";
	line += "howstuffworks.com##td[width=\"980\"][height=\"90\"]";
	line += "howtoforge.com##div[style=\"margin-top: 10px; font-size: 11px;\"]";
	line += "howtogeek.com##body > div[style=\"height: 90px;\"]:first-child";
	line += "howtogeek.com##div[style=\"padding-top: 20px; margin-top: 10px; margin-bottom: 10px; min-height: 115px; text-align: center; width: 750px; margin-left: 113px;\"]";
	line += "howtogeek.com##div[style=\"padding-top: 20px; margin-top: 210px; margin-bottom: 10px; min-height: 115px; text-align: center; width: 750px; margin-left: -15px;\"]";
	line += "hplusmagazine.com###bottom";
	line += "huffingtonpost.com##.contin_below";
	line += "hvac-talk.com##td[align=\"center\"][valign=\"top\"][style=\"padding-left: 10px;\"]";
	line += "i-comers.com###headerfix";
	line += "i-programmer.info###iProgrammerAmazoncolum";
	line += "i-programmer.info##.bannergroup";
	line += "i4u.com###topBanner > div#right";
	line += "iafrica.com###c_row1_bannerHolder";
	line += "iafrica.com##.article_Banner";
	line += "iamdisappoint.com###top-leaderboard";
	line += "iamdisappoint.com##.leaderboard";
	line += "iberia.com##.bannerGiraffe";
	line += "ibtimes.com###DHTMLSuite_modalBox_contentDiv";
	line += "ibtimes.com##.modalDialog_contentDiv_shadow";
	line += "ibtimes.com##.modalDialog_transparentDivs";
	line += "icenews.is###topbanner";
	line += "idg.com.au###skin_bump";
	line += "idg.com.au###top_adblock_fix";
	line += "ign.com###boards_medrec_relative";
	line += "illimitux.net###screens";
	line += "illimitux.net##.pub_bot";
	line += "illimitux.net##.pub_top";
	line += "iloubnan.info###bann";
	line += "iloveim.com###closeAdsDiv";
	line += "imagebanana.com##.ad";
	line += "images.search.yahoo.com###r-n";
	line += "imageshack.us###add_frame";
	line += "imdb.com###top_rhs_1_wrapper";
	line += "imdb.com###top_rhs_wrapper";
	line += "imtranslator.net##td[align=\"right\"][valign=\"bottom\"][height=\"96\"]";
	line += "inbox.com###r";
	line += "inbox.com##.slinks";
	line += "indeed.co.uk##.sjl";
	line += "independent.co.uk###article > .box";
	line += "independent.co.uk###bottom_link";
	line += "independent.co.uk###yahooLinks";
	line += "independent.co.uk##.commercialpromo";
	line += "independent.co.uk##.googleCols";
	line += "independent.co.uk##.homepagePartnerList";
	line += "independent.co.uk##.spotlight";
	line += "indianexpress.com###shopping_deals";
	line += "indiatimes.com###jobsbox";
	line += "indiatimes.com##.hover2bg";
	line += "indiatimes.com##.tpgrynw > .topbrnw:first-child + div";
	line += "indiatimes.com##td[valign=\"top\"][height=\"110\"][align=\"center\"]";
	line += "indiewire.com###promo_book";
	line += "indyposted.com##.ad";
	line += "indystar.com##.ad";
	line += "info.co.uk##.p";
	line += "info.co.uk##.spon";
	line += "infoplease.com###gob";
	line += "infoplease.com###ssky";
	line += "infoplease.com##.tutIP-infoarea";
	line += "informationmadness.com###ja-topsl";
	line += "informationweek.com###buylink";
	line += "informationweek.com##.ad";
	line += "informit.com###leaderboard";
	line += "infoworld.com##.recRes_head";
	line += "inquirer.net###bottom_container";
	line += "inquirer.net###leaderboard_frame";
	line += "inquirer.net###marketplace_vertical_container";
	line += "inquirer.net##.bgadgray10px";
	line += "inquirer.net##.fontgraysmall";
	line += "inquirer.net##.padtopbot5";
	line += "inquirer.net##table[width=\"780\"][height=\"90\"]";
	line += "inquisitr.com###topx2";
	line += "insanelymac.com##.ad";
	line += "instapaper.com###deckpromo";
	line += "intelligencer.ca###banZone";
	line += "intelligencer.ca##.bnr";
	line += "interfacelift.com##.ad";
	line += "internet.com###contentmarketplace";
	line += "internet.com###mbEnd";
	line += "internet.com##.ch";
	line += "internetevolution.com##div[style=\"border: 2px solid rgb(230, 230, 230); margin-top: 30px;\"]";
	line += "investopedia.com###leader";
	line += "investopedia.com##.mainbodyleftcolumntrade";
	line += "investopedia.com##div[style=\"float: left; width: 250px; height: 250px; margin-right: 5px;\"]";
	line += "io9.com###marquee";
	line += "io9.com###marquee-frame";
	line += "io9.com###skyscraper";
	line += "io9.com##.highlite";
	line += "iol.co.za###sp_links";
	line += "iol.co.za###weatherbox-bottom";
	line += "iol.co.za##.lead_sp_links";
	line += "iol.co.za##table[width=\"120\"]";
	line += "iomtoday.co.im###banner01";
	line += "ipmart-forum.com###table1";
	line += "irishtimes.com###banner-area";
	line += "israelnationalnews.com##.leftColumn";
	line += "itnews.com.au##div[style=\"width: 300px; height: 250px;\"]";
	line += "itnews.com.au##div[style=\"width: 728px; height: 90px; margin-left: auto; margin-right: auto; padding-bottom: 20px;\"]";
	line += "itnewsonline.com##table[width=\"300\"][height=\"250\"]";
	line += "itnewsonline.com##td[width=\"120\"]";
	line += "itp.net##.top_bit";
	line += "itpro.co.uk###skyScraper";
	line += "itproportal.com###hp-accordion";
	line += "itproportal.com##.se_left";
	line += "itproportal.com##.se_right";
	line += "itproportal.com##.teaser";
	line += "itreviews.co.uk###bmmBox";
	line += "itweb.co.za###cosponsor";
	line += "itweb.co.za###cosponsor-logo";
	line += "itweb.co.za###cosponsorTab";
	line += "itweb.co.za###highlight-on";
	line += "itweb.co.za###sponsor";
	line += "itweb.co.za###sponsor-logo";
	line += "itweb.co.za###top-banner";
	line += "itweb.co.za##.hidden";
	line += "itweb.co.za##div[style=\"width: 300px; height: 266px; overflow: hidden; margin: 0pt;\"]";
	line += "itworld.com###more_resources";
	line += "itworld.com###partner_strip";
	line += "iwebtool.com##table[cellspacing=\"0\"][cellpadding=\"0\"][border=\"1\"]";
	line += "ixquick.com##td[bgcolor=\"#f7f9ff\"]";
	line += "jacarandafm.com###thebug";
	line += "jacarandafm.com##.block_120x600";
	line += "jakeludington.com###ablock";
	line += "jalopnik.com###skyscraper";
	line += "jame-world.com###adv_top";
	line += "jame-world.com##.adv_right";
	line += "jamendo.com###ad";
	line += "jamendo.com##.col_extra";
	line += "japanator.com##.gutters";
	line += "japanator.com##div[style=\"background-color: rgb(176, 176, 176); width: 320px; height: 260px; padding: 20px 10px 10px;\"]";
	line += "japanisweird.com###top-leaderboard";
	line += "japanisweird.com##.leaderboard";
	line += "japannewsreview.com##div[style=\"width: 955px; height: 90px; margin-bottom: 10px;\"]";
	line += "japantimes.co.jp###FooterAdBlock";
	line += "japantimes.co.jp###HeaderAdsBlockFront";
	line += "japantimes.co.jp##.RealEstateAdBlock";
	line += "japantimes.co.jp##.SmallBanner";
	line += "japantimes.co.jp##.UniversitySearchAdBlock";
	line += "japantimes.co.jp##table[height=\"250\"][width=\"250\"]";
	line += "japanvisitor.com###sponsor";
	line += "jarrowandhebburngazette.com###banner01";
	line += "javalobby.org###topLeaderboard";
	line += "jayisgames.com##.bfg-feature";
	line += "jdownloader.org##a[href^=\"http://fileserve.com/signup.php?reff=\"]";
	line += "jeuxvideo-flash.com###pub_header";
	line += "jewtube.com###adv";
	line += "jewtube.com##div[style=\"display: block; width: 468px; height: 60px; padding: 5px; border: 1px solid rgb(221, 221, 221); text-align: left;\"]";
	line += "jezebel.com###skyscraper";
	line += "johnbridge.com###header_right_cell";
	line += "johnbridge.com##td[valign=\"top\"] > table.tborder[width=\"140\"][cellspacing=\"1\"][cellpadding=\"6\"][border=\"0\"]";
	line += "joomla.org##div[style=\"margin: 0pt auto; width: 728px; height: 100px;\"]";
	line += "joox.net###body-sidebar";
	line += "joox.net##img[alt=\"Download FLV Direct\"]";
	line += "joystickdivision.com###Page_Header";
	line += "joystiq.com###medrect";
	line += "joystiq.com###medrectrb";
	line += "joystiq.com###topleader-wrap";
	line += "joystiq.com##.medrect";
	line += "jpost.com###topBanner";
	line += "jpost.com##.jp-grid-oppsidepane";
	line += "jpost.com##.padtopblubar";
	line += "jpost.com##[id=\"ads.gbox.1\"]";
	line += "jumptags.com##div[style=\"background: none repeat scroll 0% 0% rgb(255, 255, 255); padding: 5px; border-bottom: 1px solid rgb(170, 170, 170); height: 95px;\"]";
	line += "justhungry.com##a[href^=\"http://affiliates.jlist.com/\"]";
	line += "justin.tv###iphone_banner";
	line += "kaldata.net###header2";
	line += "katu.com###mrktplace_tabbed";
	line += "katu.com##.callout";
	line += "katz.cd###spon";
	line += "katzforums.com###aff";
	line += "kayak.co.in##[height=\"330\"][width=\"270\"][bgcolor=\"#fff8dd\"]";
	line += "kayak.co.uk##[height=\"330\"][width=\"270\"][bgcolor=\"#fff8dd\"]";
	line += "kayak.com##[height=\"330\"][width=\"270\"][bgcolor=\"#fff8dd\"]";
	line += "keepvid.com##.sponsors";
	line += "keepvid.com##.sponsors-s";
	line += "kewlshare.com###rollAdRKLA";
	line += "kibagames.com##.adv_default_box_container";
	line += "kibagames.com##.category_adv_container";
	line += "kibagames.com##.dc_color_lightgreen.dc_bg_for_adv";
	line += "kibagames.com##.search_adv_container";
	line += "kibagames.com##.start_overview_adv_container";
	line += "kibagames.com##div[style=\"border: 0px solid rgb(0, 0, 0); width: 160px; height: 600px;\"]";
	line += "kibagames.com##div[style=\"margin-bottom: 10px; border: 1px solid rgb(0, 0, 0); height: 90px;\"]";
	line += "kids-in-mind.com##td[valign=\"top\"][style=\"padding-left: 5px; padding-right: 5px;\"]";
	line += "kidsinmind.com##td[valign=\"top\"][style=\"padding-left: 5px; padding-right: 5px;\"]";
	line += "kidzworld.com##div[style=\"width: 160px; height: 617px; margin: auto;\"]";
	line += "kidzworld.com##div[style=\"width: 300px; height: 117px; margin: auto;\"]";
	line += "kidzworld.com##div[style=\"width: 300px; height: 267px; margin: auto;\"]";
	line += "king-mag.com###banner_468";
	line += "king-mag.com###leaderboard";
	line += "king-mag.com###mediumrec";
	line += "king-mag.com###skyscraper";
	line += "kino.to###LeftFull";
	line += "kino.to###RightFull";
	line += "kino.to##.Special";
	line += "kioskea.net###topContent";
	line += "kissfm961.com##div[style=\"padding-top: 10px; padding-left: 10px; height: 250px;\"]";
	line += "kizna-blog.com##a[href$=\".clickbank.net\"]";
	line += "knowfree.net###mta_bar";
	line += "knowfree.net##.web_link";
	line += "knowfree.net##a[href^=\"http://kvors.com/click/\"]";
	line += "knowyourmeme.com##.a160x600";
	line += "knowyourmeme.com##.a250x250";
	line += "knowyourmeme.com##.a728x90";
	line += "knoxnews.com###leaderboard";
	line += "knoxnews.com##.big_box";
	line += "kohit.net##.banner_468";
	line += "kohit.net##.top_banner";
	line += "kohit.net##div[style=\"width: 300px; height: 250px; background-color: rgb(0, 0, 0);\"]";
	line += "kotaku.com###skySpacer";
	line += "kotaku.com###skyscraper";
	line += "kovideo.net##.h-728";
	line += "kovideo.net##.right-def-160";
	line += "kovideo.net##.search-728";
	line += "krapps.com###header";
	line += "krapps.com##a[href^=\"index.php?adclick=\"]";
	line += "krebsonsecurity.com###sidebar-b";
	line += "krebsonsecurity.com###sidebar-box";
	line += "krillion.com###sponCol";
	line += "ksl.com##div[style=\"float: left; width: 300px; margin: 5px 0px 13px;\"]";
	line += "ksl.com##table[style=\"width: 635px; padding: 0pt; margin: 0pt; background-color: rgb(230, 239, 255);\"]";
	line += "kstp.com###siteHeaderLeaderboard";
	line += "ktvu.com###leaderboard-sticky";
	line += "kuklaskorner.com###ultimate";
	line += "kval.com###mrktplace_tabbed";
	line += "kval.com##.callout";
	line += "langmaker.com##table[width=\"120\"]";
	line += "lastminute.com###sponsoredFeature";
	line += "latimes.com###article-promo";
	line += "law.com###leaderboard";
	line += "layoutstreet.com##.ad";
	line += "lbc.co.uk###topbanner";
	line += "learninginfo.org##table[align=\"left\"][width=\"346\"]";
	line += "lemondrop.com##.sidebarBanner";
	line += "lemondrop.com##.topBanner";
	line += "licensing.biz##.newsinsert";
	line += "life.com##.ad";
	line += "lifehack.org##.offer";
	line += "lifehacker.com###skySpacer";
	line += "lifehacker.com###skyscraper";
	line += "lifespy.com##.SRR";
	line += "lightreading.com##div[align=\"center\"][style=\"height: 114px;\"]";
	line += "linux-mag.com##.sponsor-widget";
	line += "linuxforums.org###rightColumn";
	line += "linuxforums.org##div[style=\"margin: 2px; float: right; width: 300px; height: 250px;\"]";
	line += "linuxinsider.com###welcome-box";
	line += "linuxinsider.com##.content-block-slinks";
	line += "linuxinsider.com##.content-tab-slinks";
	line += "linuxquestions.org##div[style=\"margin: -3px -3px 5px 5px; float: right;\"]";
	line += "linuxtopia.org###bookcover_sky";
	line += "lionsdenu.com###banner300-top-right";
	line += "lionsdenu.com###sidebar-bottom-left";
	line += "lionsdenu.com###sidebar-bottom-right";
	line += "live365.com##.ad";
	line += "livejournal.com##.ljad";
	line += "liverpooldailypost.co.uk##.promotop";
	line += "livestream.com##.ad";
	line += "livevss.net###ad";
	line += "livevss.tv###floatLayer1";
	line += "living.aol.co.uk##.wide.horizontal_promo_HPHT";
	line += "lmgtfy.com###sponsor";
	line += "lmgtfy.com###sponsor_wrapper";
	line += "load.to##.download_right";
	line += "loaded.it###apDiv1";
	line += "loaded.it###bottomcorner";
	line += "loaded.it###ppad1";
	line += "loaded.it##img[style=\"border: 0px none; width: 750px;\"]";
	line += "local.co.uk###borderTab";
	line += "local.yahoo.com##.yls-rs-paid";
	line += "londonstockexchange.com##.banner";
	line += "londonstockexchange.com##.bannerTop";
	line += "loombo.com##.ad";
	line += "lotro-lore.com###banner";
	line += "lowbird.com##.teaser";
	line += "lowyat.net##img[border=\"1\"]";
	line += "lowyat.net##tr[style=\"cursor: pointer;\"]";
	line += "luxist.com###topleader-wrap";
	line += "luxist.com##.medrect";
	line += "lyrics007.com##td[bgcolor=\"#ffcc00\"][width=\"770\"][height=\"110\"]";
	line += "lyricsfreak.com###ticketcity";
	line += "lyricsfreak.com##.ad";
	line += "lyricsfreak.com##.ringtone";
	line += "lyricsmode.com##div[style=\"text-align: center; margin-top: 15px; height: 90px;\"]";
	line += "lyricwiki.org###p-navigation + .portlet";
	line += "lyrster.com##.el_results";
	line += "m-w.com###google_creative_3";
	line += "m-w.com###skyscraper_creative_2";
	line += "maannews.net##td[style=\"border: 1px solid rgb(204, 204, 204); width: 250px; height: 120px;\"]";
	line += "maannews.net##td[style=\"border: 1px solid rgb(204, 204, 204); width: 640px; height: 80px;\"]";
	line += "macdailynews.com###right";
	line += "macintouch.com###yellows";
	line += "macintouch.com##img[width=\"125\"][height=\"125\"]";
	line += "macintouch.com##img[width=\"468\"]";
	line += "macleans.ca###leaderboard";
	line += "maclife.com###top-banner";
	line += "macnewsworld.com##.content-block-slinks";
	line += "macnewsworld.com##.content-tab-slinks";
	line += "macnn.com###leaderboard";
	line += "macnn.com###supportbod";
	line += "macobserver.com##.dealsontheweb";
	line += "macobserver.com##.specials";
	line += "macosxhints.com##div[style=\"border-bottom: 2px solid rgb(123, 123, 123); padding-bottom: 8px; margin-bottom: 5px;\"]";
	line += "macrumors.com###googleblock300";
	line += "macrumors.com###mr_banner_topad";
	line += "macstories.net###ad";
	line += "macsurfer.com##.text_top_box";
	line += "macsurfer.com##table[width=\"300\"][height=\"250\"]";
	line += "macthemes2.net###imagelinks";
	line += "macupdate.com###promoSidebar";
	line += "macupdate.com##div[style=\"width: 728px; height: 90px; margin: 0px auto; display: block;\"]";
	line += "macworld.co.uk###footer";
	line += "macworld.co.uk###topBannerSpot";
	line += "macworld.com###shopping";
	line += "madeformums.com###contentbanner";
	line += "magic.co.uk###headerRowOne";
	line += "magme.com###top_banner";
	line += "mail.google.com###\\:lq";
	line += "mail.live.com###SkyscraperContent";
	line += "mail.yahoo.com###MNW";
	line += "mail.yahoo.com###MON > div[style=\"color: rgb(0, 0, 0); font-size: 10px; font-family: Verdana,arial,sans-serif; text-align: center;\"]";
	line += "mail.yahoo.com###SKY";
	line += "mail.yahoo.com###northbanner";
	line += "mail.yahoo.com###nwPane";
	line += "mail.yahoo.com###slot_LREC";
	line += "majorgeeks.com##.Outlines";
	line += "majorgeeks.com##a[href^=\"http://www.pctools.com/registry-mechanic/?ref=\"]";
	line += "majorgeeks.com##a[href^=\"https://secure.avangate.com/affiliate.php\"]";
	line += "majorgeeks.com##a[target=\"1\"]";
	line += "majorgeeks.com##a[target=\"top\"]";
	line += "majorgeeks.com##table[align=\"right\"][width=\"336\"][style=\"padding-left: 5px;\"]";
	line += "maketecheasier.com##a[href=\"http://maketecheasier.com/advertise\"]";
	line += "makeuseof.com##a[href=\"http://www.makeuseof.com/advertise/\"]";
	line += "makeuseof.com##div[style=\"margin-bottom: 15px; margin-top: 15px; padding: 5px; border: 1px solid rgb(198, 215, 225); background-color: rgb(216, 234, 242);\"]";
	line += "makezine.com##.ad";
	line += "malaysiastory.com##.box2";
	line += "maltonmercury.co.uk###banner01";
	line += "mangafox.com##a[href^=\"http://fs.game321.com/\"]";
	line += "map24.com###cont_m24up";
	line += "mapquest.com###offers";
	line += "marketingmag.ca###leaderboard_container";
	line += "marketingpilgrim.com###ad";
	line += "mashable.com##.header-banner";
	line += "massively.com###topleader-wrap";
	line += "mcvuk.com##.newsinsert";
	line += "mediabistro.com##.right-column-boxes-content-partners";
	line += "mediacoderhq.com##.gg1";
	line += "mediafire.com###catfish_div";
	line += "mediafire.com##.download_banner_container";
	line += "mediafire.com##.ninesixty_container:last-child td[align=\"right\"][valign=\"top\"]:first-child";
	line += "mediafiresearch.net##a[href^=\"http://mediafiresearch.net/adv1.php\"]";
	line += "mediaite.com###magnify_widget_rect_handle";
	line += "mediaite.com###supertop";
	line += "medicineandtechnology.com##div[style=\"width: 728px; height: 90px; position: relative; margin: 0pt; padding: 0pt; text-align: left;\"]";
	line += "megafileupload.com##.banner300";
	line += "megafileupload.com##.big_banner";
	line += "megauploadsearch.net##a[href^=\"http://megauploadsearch.net/adv.php\"]";
	line += "megavideo.com##div[style=\"position: relative; width: 355px; height: 299px; margin-top: 2px;\"]";
	line += "megavideo.com##div[style=\"position: relative; width: 359px; height: 420px; margin-left: -3px; margin-top: 1px;\"]";
	line += "melbourneheartfc.com.au##.promotion_wrapper";
	line += "melbournevictory.com.au##.promotion_wrapper";
	line += "mercurynews.com###mn_SP_Links";
	line += "merriam-webster.com###Dictionary-MW_DICT_728_BOT";
	line += "merriam-webster.com###google_creative_3";
	line += "metacafe.com###Billboard";
	line += "metadivx.com##.ad";
	line += "metblogs.com###a_medrect";
	line += "metblogs.com###a_widesky";
	line += "metro.co.uk##.google-sky";
	line += "metro.co.uk##.sky";
	line += "metro.us##div[style=\"width: 300px; height: 250px; float: right;\"]";
	line += "metrolyrics.com###cee_box";
	line += "metrolyrics.com###cee_overlay";
	line += "metrolyrics.com###ipod";
	line += "metromix.com###leaderboard";
	line += "mg.co.za###masthead > table[style=\"padding-right: 5px;\"]:first-child";
	line += "mg.co.za###miway-creative";
	line += "mg.co.za##.articlecontinues";
	line += "mg.co.za##div[style=\"width: 300px; height: 250px;\"]";
	line += "miamiherald.com###leaderboard";
	line += "microsoft-watch.com###topBannerContainer";
	line += "miloyski.com##a.button[target=\"_blank\"]";
	line += "mindspark.com##.desc";
	line += "miniclip.com##.block_300x250";
	line += "miniclip.com##.letterbox";
	line += "minnpost.com##.topleader";
	line += "missoulian.com###yahoo-contentmatch";
	line += "mlfat4arab.com##img[width=\"234\"][height=\"60\"]";
	line += "mmosite.com##.c_gg";
	line += "mmosite.com##.mmo_gg";
	line += "mmosite.com##.mmo_gg2";
	line += "mmosite.com##.mmo_textsponsor";
	line += "mobile-ent.biz##.newsinsert";
	line += "mobilecrunch.com##.ad";
	line += "mobilemoviezone.com##a[href^=\"http://adsalvo.com/\"]";
	line += "mobilemoviezone.com##a[href^=\"http://clk.mobgold.com/\"]";
	line += "mobilust.net##a[href^=\"http://nicevid.net/?af=\"]";
	line += "modernhealthcare.com##.mh_topshade_b";
	line += "mofunzone.com###ldrbrd_td";
	line += "money.co.uk###topBar";
	line += "morefailat11.com###top-leaderboard";
	line += "morefailat11.com##.leaderboard";
	line += "morningstar.com##.LeaderWrap";
	line += "morningstar.com##.aadsection_b1";
	line += "morningstar.com##.aadsection_b2";
	line += "mortgageguide101.com###ppc";
	line += "mosnews.com##.right_pop";
	line += "motherboard.tv##.banner";
	line += "motherboard.tv##.moreFromVice";
	line += "motherjones.com##.post-continues";
	line += "motherproof.com###leader";
	line += "motionempire.com##div[style=\"width: 728px; margin-top: 3px; margin-bottom: 3px; height: 90px; overflow: hidden; margin-left: 113px;\"]";
	line += "motorcycle-usa.com##.bannergoogle";
	line += "movie2k.com###ball";
	line += "movie2k.com##a[href^=\"http://www.affbuzzads.com/affiliate/\"]";
	line += "movie2k.com##a[style=\"color: rgb(255, 0, 0); font-size: 14px;\"]";
	line += "moviecritic.com.au###glinks";
	line += "moviefone.com###WIAModule";
	line += "moviefone.com##.ent_promo_sidetexttitle";
	line += "movies.yahoo.com###banner";
	line += "movies.yahoo.com##.lrec";
	line += "moviesfoundonline.com###banner";
	line += "moviesmobile.net##a[href*=\".amobee.com\"]";
	line += "moviesmobile.net##a[href*=\".mojiva.com\"]";
	line += "moviesplanet.com##.Banner468X60";
	line += "moviesplanet.com##.gb";
	line += "movshare.net##.ad";
	line += "movstore.com##.overlayVid";
	line += "mp3-shared.net##a[href^=\"http://click.yottacash.com?PID=\"]";
	line += "mp3lyrics.org###bota";
	line += "mp3raid.com##td[align=\"left\"]";
	line += "mpfour.net##.overlayVid";
	line += "msn.com###Sales1";
	line += "msn.com###Sales2";
	line += "msn.com###Sales3";
	line += "msn.com###Sales4";
	line += "msn.com###ad";
	line += "msn.com##.abs";
	line += "msn.com##.ad";
	line += "msnbc.msn.com###Dcolumn";
	line += "msnbc.msn.com###marketplace";
	line += "msnbc.msn.com##.w460";
	line += "mstar.com##.MPFBannerWrapper";
	line += "mtv.co.uk###mtv-shop";
	line += "mtv.com###gft-sponsors";
	line += "multiupload.com##div[style=\"position: relative; width: 701px; height: 281px; background-image: url(\\\"img/ad_bgr.gif\\\");\"]";
	line += "mumbaimirror.com##.bottombanner";
	line += "mumbaimirror.com##.topbanner";
	line += "music.yahoo.com###YMusicRegion_T3_R2C2_R1";
	line += "music.yahoo.com###lrec";
	line += "music.yahoo.com###lrecTop";
	line += "musicradar.com##.shopping_partners";
	line += "musicsonglyrics.com###adv_bg";
	line += "musicsonglyrics.com##td[width=\"300\"][valign=\"top\"]";
	line += "muskogeephoenix.com##div[style=\"height: 240px; width: 350px; background-color: rgb(238, 238, 238);\"]";
	line += "my360.com.au##div[style=\"height: 250px;\"]";
	line += "myfoxny.com##.marketplace";
	line += "myfoxphoenix.com###leaderboard";
	line += "myfoxphoenix.com##.marketplace";
	line += "myfoxphoenix.com##.module.horizontal";
	line += "myfoxphoenix.com##.vert.expanded";
	line += "mygaming.co.za##.banner_300";
	line += "mygaming.co.za##.banner_468";
	line += "mylifeisaverage.com##.ad";
	line += "myoutsourcedbrain.com###HTML2";
	line += "myretrotv.com##img[width=\"875\"][height=\"110\"]";
	line += "mysearch.com##a.desc > div";
	line += "myspace.com###marketing";
	line += "myspace.com###medRec";
	line += "myspace.com###music_googlelinks";
	line += "myspace.com###music_medrec";
	line += "myspace.com###tkn_medrec";
	line += "myspace.com##.SitesMedRecModule";
	line += "mystream.to###adv";
	line += "mystream.to###sysbar";
	line += "mystream.to##a[href^=\"out/\"]";
	line += "myway.com##.desc";
	line += "mywebsearch.com##.desc";
	line += "narutofan.com###right-spon";
	line += "nasdaq.com##div[style=\"vertical-align: middle; width: 336px; height: 284px;\"]";
	line += "nation.co.ke##.c15r";
	line += "nationalgeographic.com###headerboard";
	line += "nationalpost.com##.ad";
	line += "naukri.com##.collMTp";
	line += "nbc.com###nbc-300";
	line += "nbcbayarea.com##.ad";
	line += "nbcbayarea.com##.promo";
	line += "nbcconnecticut.com###marketingPromo";
	line += "nbcsandiego.com###partnerBar";
	line += "nbcsandiego.com##.ad";
	line += "nbcsports.com###top_90h";
	line += "ncrypt.in##a[title=\"HIGHSPEED Download\"]";
	line += "ndtv.com##div[style=\"position: relative; height: 260px; width: 300px;\"]";
	line += "nearlygood.com###abf";
	line += "necn.com###main_117";
	line += "necn.com###main_121";
	line += "necn.com###main_175";
	line += "necn.com###right_generic_117";
	line += "necn.com###right_generic_121";
	line += "necn.com###right_generic_175";
	line += "neopets.com###ban_bottom";
	line += "neopets.com##a[style=\"display: block; margin-left: auto; margin-right: auto; width: 996px; height: 94px;\"]";
	line += "neowin.net###special-steve";
	line += "neowin.net##.unspecific";
	line += "neowin.net##div[style=\"background: url(\\\"/images/atlas/aww2.png\\\") no-repeat scroll center center transparent ! important; height: 250px; width: 300px;\"]";
	line += "neowin.net##div[style=\"background:url(/images/atlas/aww2.png) no-repeat center center !important;height:250px;width:300px\"]";
	line += "nerej.com###bottom_banner";
	line += "nerve.com###topBanner";
	line += "netchunks.com###af_adblock";
	line += "netchunks.com###m_top_adblock";
	line += "netchunks.com###sponsorsM";
	line += "netmag.co.uk##div[style=\"margin: 0px auto; padding-right: 0px; float: left; padding-bottom: 0px; width: 320px; padding-top: 0px; height: 290px; background-color: rgb(255, 255, 255);\"]";
	line += "networkworld.com###lb_container_top";
	line += "networkworld.com###promoslot";
	line += "networkworld.com##.sponsor";
	line += "nevadaappeal.com##.youradhere";
	line += "newcastlejets.com.au##.promotion_wrapper";
	line += "newgrounds.com##.wide_storepromo";
	line += "newgrounds.com##.wide_storepromobot";
	line += "news.aol.co.uk###tdiv60";
	line += "news.aol.co.uk###tdiv71";
	line += "news.aol.co.uk###tdiv74";
	line += "news.cnet.com###bottom-leader";
	line += "news.com.au##.ad";
	line += "news.com.au##.sponsors";
	line += "news.yahoo.com###ymh-invitational-recs";
	line += "newsarama.com##.marketplace";
	line += "newsday.co.zw##.articlecontinues";
	line += "newsday.co.zw##div[style=\"width: 300px; height: 250px;\"]";
	line += "newsfactor.com##td[style=\"border-left: 1px solid rgb(192, 192, 192); padding-top: 3px; padding-bottom: 3px;\"]";
	line += "newsmax.com###noprint1";
	line += "newsmax.com##.sponsors_spacer";
	line += "newsnet5.com##.ad";
	line += "newsnet5.com##.marketplace";
	line += "newsniche.com##a[style=\"font-size: 12px; color: rgb(255, 166, 23);\"]";
	line += "newsonjapan.com###squarebanner300x250";
	line += "newsroomamerica.com###promotional";
	line += "newstatesman.com###footerbanner";
	line += "newsweek.com##.sponsor";
	line += "newsweek.com##.sponsorship";
	line += "nicknz.co.nz##.lrec";
	line += "nicknz.co.nz##.top-banner";
	line += "ninemsn.com.au###ad";
	line += "ninemsn.com.au###bannerTop";
	line += "ninemsn.seek.com.au###msnhd_div3";
	line += "nintendolife.com##.the300x250";
	line += "nitrome.com###banner_box";
	line += "nitrome.com###banner_description";
	line += "nitrome.com###banner_shadow";
	line += "nitrome.com###skyscraper_box";
	line += "nitrome.com###skyscraper_description";
	line += "nitrome.com###skyscraper_shadow";
	line += "nmap.org##img[height=\"90\"][width=\"120\"]";
	line += "nme.com###editorial_sky";
	line += "nme.com###skyscraper";
	line += "northjersey.com##.detail_boxwrap";
	line += "northjersey.com##.detail_pane_text";
	line += "notdoppler.com##table[width=\"312\"][height=\"252\"]";
	line += "notdoppler.com##td[background=\"/img/topad_1a.gif\"]";
	line += "notdoppler.com##td[background=\"/img/topad_1b.gif\"]";
	line += "notdoppler.com##td[background=\"/img/topad_1c.gif\"]";
	line += "notdoppler.com##td[background=\"/img/topad_2a.gif\"]";
	line += "notdoppler.com##td[height=\"100\"][rowspan=\"3\"]";
	line += "notdoppler.com##td[style=\"background-image: url(\\\"img/main_topshadow-light.gif\\\"); background-repeat: repeat-x; background-color: rgb(243, 243, 243);\"]";
	line += "notdoppler.com##td[width=\"728\"][height=\"90\"]";
	line += "notebooks.com##.efbleft";
	line += "noupe.com##.ad";
	line += "novamov.com##.ad";
	line += "novamov.com##.top_banner";
	line += "nqfury.com.au##.promotion_wrapper";
	line += "nullscript.info##div[style=\"border: 2px solid red; margin: 10px; padding: 10px; text-align: left; height: 80px; background-color: rgb(255, 247, 182);\"]";
	line += "nwanime.com###iwarn";
	line += "nwsource.com###skyscraperwide";
	line += "nwsource.com##.adblock";
	line += "nwsource.com##.googlemiddle";
	line += "ny1.com##.bannerSidebar";
	line += "ny1.com##.bannerTop";
	line += "nyaatorrents.org##a[href^=\"http://www.nyaatorrents.org/a?\"]";
	line += "nydailynews.com###nydn-topbar";
	line += "nydailynews.com##.z_sponsor";
	line += "nymag.com###partner-feeds";
	line += "nymag.com##.google-bottom";
	line += "nypost.com##.ad";
	line += "nyrej.com###bottom_banner";
	line += "nytimes.com##.ad";
	line += "nzgamer.com###premierholder";
	line += "nzgamer.com##.article_banner_holder";
	line += "nzherald.co.nz##.marketPlace";
	line += "o2cinemas.com##.links";
	line += "objectiface.com###top-leaderboard";
	line += "objectiface.com##.leaderboard";
	line += "ocregister.com###bannertop2";
	line += "ocworkbench.com##.shopwidget1";
	line += "offshore-mag.com##.sponsoredBy";
	line += "offshore-mag.com##.webcast-promo-box-sponsorname";
	line += "oldgames.sk###r_TopBar";
	line += "omg-facts.com###droitetop";
	line += "omg-facts.com##table[border=\"0\"][width=\"330px\"][height=\"270px\"]";
	line += "omg.yahoo.com###omg-lrec";
	line += "omgili.com###ad";
	line += "oneindia.in##.deal_lists";
	line += "oneindia.in##.fotfont";
	line += "oneindia.in##td[width=\"300\"][height=\"250\"]";
	line += "onjava.com###leaderboard";
	line += "online-literature.com##.leader-wrap-bottom";
	line += "online-literature.com##.leader-wrap-middle";
	line += "online-literature.com##.leader-wrap-top";
	line += "onlineathens.com##.story-insert";
	line += "onlineathens.com##.yahoo_hoz";
	line += "opendiary.com##div[style=\"width: 300px; height: 250px; border: 1px solid black; margin: 0px; padding: 0px;\"]";
	line += "opendiary.com##div[style=\"width: 728px; height: 90px; margin: 0px auto; padding: 0px;\"]";
	line += "opendrivers.com###google336x280";
	line += "orange.co.uk###home_leaderboard";
	line += "orange.co.uk###home_mpu";
	line += "orange.co.uk###home_partnerlinks";
	line += "orange.co.uk###home_shoppinglinks";
	line += "orange.co.uk##.spon_sored";
	line += "oreillynet.com###leaderboard";
	line += "osnews.com##.ad";
	line += "ourfamilygenes.ca##div[style=\"width: 100%; display: block; margin-bottom: 10px; height: 90px;\"]";
	line += "ovguide.com##.banner-rectangleMedium";
	line += "p2pnet.net###sidebar > ul:first-child + table[width=\"19%\"]";
	line += "p2pnet.net###sidebar2";
	line += "p2pnet.net##td[align=\"center\"][width=\"100%\"] > a[style=\"border: 0px none ; margin: 0px;\"][target=\"_blank\"] > img";
	line += "pagead2.googlesyndication.com##html";
	line += "passedoutphotos.com###top-leaderboard";
	line += "passedoutphotos.com##.leaderboard";
	line += "pbs.org###corp-sponsor-sec";
	line += "pbs.org###masthead1";
	line += "pbs.org###masthead2";
	line += "pbs.org##.newshour-support-wrap";
	line += "pc-freak.net##div[style=\"position: absolute; left: 740px; top: 240px; width: 0px;\"]";
	line += "pcadvisor.co.uk###broadbandchoices_frm";
	line += "pcadvisor.co.uk###mastHeadTopLeft";
	line += "pcauthority.com.au##.featured-retailers";
	line += "pcgamer.com##.ad";
	line += "pcmag.com###special_offers_trio";
	line += "pcmag.com##.content-links";
	line += "pcmag.com##.partners";
	line += "pcmag.com##.sp-links";
	line += "pcmag.com##.special-offers";
	line += "pcmag.com##.spotlight";
	line += "pcpro.co.uk###skin";
	line += "pcpro.co.uk###skyScrapper";
	line += "pcpro.co.uk##.leaderBoard";
	line += "pcr-online.biz##.newsinsert";
	line += "pcstats.com##table[cellpadding=\"2\"][align=\"right\"][width=\"300\"][style=\"border: 1px solid ;\"]";
	line += "pctipsbox.com###daikos-text-4";
	line += "pcworld.co.nz###sponsor_div";
	line += "pcworld.com###bizPromo";
	line += "pcworld.com###ciscoOOSBlog";
	line += "pcworld.com###industryWebcasts";
	line += "pcworld.com###resourceCenters";
	line += "pcworld.com###resourceLinks";
	line += "pcworld.com###specialOffers";
	line += "pcworld.com##.msReminderBadgeBanner";
	line += "pcworld.com##.skyscraper";
	line += "pdfmyurl.com##.banner";
	line += "pdfzone.com##.Skyscraper_BG";
	line += "pdfzone.com##.sponsors_container";
	line += "pdfzone.com##div[style=\"float: left; width: 336px; margin-right: 16px; margin-bottom: 5px;\"]";
	line += "pedulum.com###header_top";
	line += "penny-arcade.com###funding-h";
	line += "people.com##.quigo";
	line += "perfectlytimedphotos.com###top-leaderboard";
	line += "perfectlytimedphotos.com##.leaderboard";
	line += "perl.com###leaderboard";
	line += "perthglory.com.au##.promotion_wrapper";
	line += "pettube.com###ca";
	line += "phazeddl.com##a[href^=\"http://www.mydownloader.net/pr/\"]";
	line += "phazeddl.com##table#searchResult:first-child";
	line += "phazemp3.com##a[href^=\"http://www.mydownloader.net/pr/\"]";
	line += "phazemp3.com##table#searchResult:first-child";
	line += "phonescoop.com###botlink";
	line += "phonescoop.com###promob";
	line += "phoronix.com###welcome_screen";
	line += "photobucket.com##.bannerContainer";
	line += "phpbb.com##a[rel=\"external affiliate\"]";
	line += "phpbb.com##a[rel=\"external sponsor\"]";
	line += "phpbbhacks.com##div[style=\"height: 90px;\"]";
	line += "picapp.com##.ipad_300_250";
	line += "picapp.com##.ipad_728_90";
	line += "ping.eu##td[height=\"9\"][bgcolor=\"white\"][style=\"padding: 10px 25px 0px;\"]";
	line += "pingtest.net##.ad";
	line += "pinknews.co.uk##a[href^=\"http://www.pinknews.co.uk/clicks/\"]";
	line += "pitchero.com###clubSponsor";
	line += "planetxbox360.com###rightCol3gameHome";
	line += "planetxbox360.com##div[style=\"margin: 0px 0pt; padding: 2px; width: 300px; height: 250px;\"]";
	line += "planetxbox360.com##td#rightCol1[align=\"right\"][valign=\"top\"]";
	line += "planetxbox360.com##td[align=\"center\"][height=\"100\"][bgcolor=\"#3f3f3f\"]";
	line += "play.tm###lbc";
	line += "play.tm###sky";
	line += "playkidsgames.com##table[bgcolor=\"#333333\"][width=\"320\"][height=\"219\"]";
	line += "playkidsgames.com##table[width=\"100%\"][height=\"105\"]";
	line += "plusnetwork.com##.more_links";
	line += "plussports.com##.midBanner";
	line += "pmptoday.com##div[style=\"background-color: rgb(255, 255, 255); border: 1px solid rgb(51, 0, 0); font-family: Verdana,Arial,Sans-serif; font-size: 10px; padding: 0px; line-height: 11px; color: rgb(0, 0, 0); width: 728px; height: 90px;\"]";
	line += "politico.com##.in-story-banner";
	line += "politics.co.uk###top-banner";
	line += "politifact.com##.pfad";
	line += "ponged.com##.adv";
	line += "popbytes.com##div[align=\"left\"][style=\"padding-top: 0px; padding-bottom: 4px; width: 230px; background: none repeat scroll 0% 0% rgb(255, 255, 255);\"]";
	line += "popbytes.com##div[align=\"left\"][style=\"width: 230px; background: none repeat scroll 0% 0% rgb(255, 255, 255);\"]";
	line += "popbytes.com##table[cellspacing=\"1\"][cellpadding=\"0\"][border=\"0\"][bgcolor=\"#b9e70c\"]";
	line += "popbytes.com##table[width=\"229\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][bgcolor=\"#000000\"]";
	line += "popbytes.com##table[width=\"230\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][bgcolor=\"#000000\"]";
	line += "popbytes.com##table[width=\"230\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][bgcolor=\"#ffffff\"]";
	line += "popbytes.com##table[width=\"230\"][cellspacing=\"0\"][cellpadding=\"4\"][border=\"0\"][bgcolor=\"#ffffff\"]";
	line += "popbytes.com##table[width=\"230\"][cellspacing=\"5\"][cellpadding=\"3\"][style=\"overflow: hidden; border: 0px solid rgb(204, 204, 204); background-color: rgb(44, 161, 200);\"]";
	line += "popeater.com##.sidebarBanner";
	line += "popularmechanics.com###circ300x100";
	line += "popularmechanics.com###circ300x200";
	line += "popularmechanics.com###circ620x100";
	line += "post-trib.com###zip2save_link_widget";
	line += "press-citizen.com##.rightrail-promo";
	line += "pressf1.co.nz###sponsor_div";
	line += "pri.org###amazonBox180";
	line += "pricegrabber.co.uk###spl";
	line += "pricegrabber.com##.topBanner";
	line += "pricespy.co.nz##.ad";
	line += "prisonplanet.com###bottombanners";
	line += "prisonplanet.com###efoods";
	line += "proaudioreview.com##table[width=\"665\"]";
	line += "productreview.com.au##td[width=\"160\"][valign=\"top\"]";
	line += "projectw.org##a[href^=\"http://uploading.com/partners/\"]";
	line += "ps3news.com###bglink";
	line += "ps3news.com###sidebar > div > div > table[cellspacing=\"5px\"]:first-child";
	line += "psu.com###ad";
	line += "psx-scene.com##tr[valign=\"top\"]:first-child:last-child > td[width=\"125\"][valign=\"top\"][style=\"padding-left: 5px;\"]:last-child";
	line += "ptinews.com##.fullstoryadd";
	line += "ptinews.com##.fullstorydivright";
	line += "publicradio.org###amzContainer";
	line += "punjabimob.org##a[href*=\".smaato.net\"]";
	line += "pureoverclock.com###adblock1";
	line += "pureoverclock.com###adblock2";
	line += "pureoverclock.com###mainbanner";
	line += "qj.net###shoppingapi";
	line += "qj.net##.square";
	line += "quackit.com###rightColumn";
	line += "quackit.com##div[style=\"margin: auto; width: 180px; height: 250px; text-align: center; background: url(\\\"/pix/ads/ad_zappyhost_search_box_180x250.gif\\\") no-repeat scroll left top rgb(255, 255, 255);\"]";
	line += "quickload.to##a[href^=\"http://www.quickload.to/click.php?id=\"]";
	line += "quizlet.com##.googlewrap";
	line += "radaronline.com###videoExternalBanner";
	line += "radaronline.com###videoSkyscraper";
	line += "rapidlibrary.com##table[cellspacing=\"1\"][cellpadding=\"3\"][border=\"0\"][width=\"98%\"]";
	line += "ratemyprofessors.com##.rmp_leaderboard";
	line += "rawstory.com##td[width=\"101\"][align=\"center\"][style][margin=\"0\"]";
	line += "readmetro.com##.header";
	line += "readwriteweb.com###ad_block";
	line += "readwriteweb.com###fm_conversationalist_zone";
	line += "readwriteweb.com###rwcloud_promo";
	line += "readwriteweb.com###rwwpartners";
	line += "readwriteweb.com###vmware-trial";
	line += "realitytvobsession.com###glinks";
	line += "realworldtech.com##.leaderboard_wrapper";
	line += "receeve.it##.carousel";
	line += "redbookmag.com###special_offer_300x100";
	line += "reddit.com##.promotedlink";
	line += "rediff.com###world_right1";
	line += "rediff.com###world_top";
	line += "redmondmag.com##.ad";
	line += "reference.com###Resource_Center";
	line += "reference.com###abvFold";
	line += "reference.com###bannerTop";
	line += "reference.com###bnrTop";
	line += "reference.com###centerbanner_game";
	line += "reference.com###rc";
	line += "reference.com##.spl_unshd";
	line += "reference.com##.spl_unshd_NC";
	line += "rejournal.com##img[style=\"border-width: 0px;\"]";
	line += "rejournal.com##img[width=\"200\"][height=\"100\"]";
	line += "reminderfox.mozdev.org###promotion3";
	line += "restaurants.com##.latad";
	line += "reverso.net##.columnBanner2";
	line += "rhylfc.co.uk##.bannergroup";
	line += "rinkworks.com##table[style=\"float: right; border: 1px solid red; width: 250px; padding: 10px; margin: 10px;\"]";
	line += "rivals.com###thecontainer";
	line += "roadfly.com###leaderboardHead";
	line += "roadfly.com##.adv";
	line += "roadrunner.com##.leaderboard";
	line += "robotswithfeelings.com##div[style=\"height: 250px; width: 300px; background-color: rgb(0, 0, 0);\"]";
	line += "robotswithfeelings.com##div[style=\"height: 90px; width: 728px; margin-left: auto; margin-right: auto; background-color: rgb(0, 0, 0);\"]";
	line += "robtex.com##div[style=\"width: 728px; height: 90px; margin-left: auto; margin-right: auto;\"]";
	line += "rockpapershotgun.com##.marketing";
	line += "rollcall.com##.ad";
	line += "rollingstone.com##.ad";
	line += "rotoruadailypost.co.nz##.marketPlace";
	line += "rottentomatoes.com###afc_sidebar";
	line += "roughlydrafted.com###banner";
	line += "roulettereactions.com###top-leaderboard";
	line += "roulettereactions.com##.leaderboard";
	line += "royalgazette.com##div[style=\"height: 60px; width: 468px;\"]";
	line += "rr.com##.leaderboard";
	line += "rr.com##.leaderboardTop";
	line += "rs-catalog.com##div[onmouseout=\"this.style.backgroundColor='#fff7b6'\"]";
	line += "rte.ie###island300x250-inside";
	line += "rte.ie###story_island";
	line += "rte.ie###tilesHolder";
	line += "rte.ie##div[style=\"background-color: rgb(239, 238, 234); text-align: center; width: 728px; height: 92px; padding-top: 2px;\"]";
	line += "rubbernews.com##td[width=\"250\"]";
	line += "runescape.com###tb";
	line += "rushlimbaugh.com###top_leaderboard";
	line += "rwonline.com##table[width=\"665\"]";
	line += "sacbee.com###leaderboard";
	line += "satelliteguys.us##div[style=\"width: 300px; float: right; height: 250px; margin-left: 10px; margin-right: 10px; margin-bottom: 10px;\"]";
	line += "satelliteguys.us##td[width=\"160\"][valign=\"top\"][align=\"left\"]";
	line += "schlockmercenary.com##td[colspan=\"3\"]";
	line += "sci-tech-today.com##td[style=\"border-left: 1px dashed rgb(192, 192, 192); padding: 5px;\"]";
	line += "sci-tech-today.com##td[style=\"border-left: 1px solid rgb(192, 192, 192); padding-top: 3px; padding-bottom: 3px;\"]";
	line += "scienceblogs.com###leaderboard";
	line += "scienceblogs.com##.skyscraper";
	line += "sciencedaily.com##.rectangle";
	line += "sciencedaily.com##.skyscraper";
	line += "sciencedirect.com###leaderboard";
	line += "scientificamerican.com##a[href^=\"/ad-sections/\"]";
	line += "scientificamerican.com##div[style=\"height: 275px; margin: 0pt;\"]";
	line += "scoop.co.nz###top-banner";
	line += "scoop.co.nz###top-banner-base";
	line += "scoop.co.nz###topHeader";
	line += "scotsman.com###banner01";
	line += "search.aol.ca##.SLL";
	line += "search.aol.ca##.WOL";
	line += "search.aol.co.uk##.PMB";
	line += "search.aol.co.uk##.SLL";
	line += "search.aol.co.uk##.WOL";
	line += "search.aol.com##.PMB";
	line += "search.aol.com##.SLL";
	line += "search.aol.com##.WOL";
	line += "search.aol.in##.SLL";
	line += "search.cnbc.com###ms_aur";
	line += "search.com##.citeurl";
	line += "search.com##.dtext";
	line += "search.com##a[href^=\"http://shareware.search.com/click?\"]";
	line += "search.excite.co.uk###results11_container";
	line += "search.excite.co.uk##td[width=\"170\"][valign=\"top\"]";
	line += "search.icq.com##.more_sp";
	line += "search.icq.com##.more_sp_end";
	line += "search.icq.com##.res_sp";
	line += "search.netscape.com##.SLL";
	line += "search.netscape.com##.SWOL";
	line += "search.virginmedia.com##.s-links";
	line += "search.winamp.com##.SLL";
	line += "search.winamp.com##.SWOL";
	line += "search.winamp.com##.WOL";
	line += "search.yahoo.com###east";
	line += "search.yahoo.com###sec-col";
	line += "search.yahoo.com##.bbox";
	line += "search.yahoo.com##.overture";
	line += "searchalot.com##td[onmouseout=\"cs()\"]";
	line += "searchenginejournal.com##.even";
	line += "searchenginejournal.com##.odd";
	line += "searchenginesuggestions.com###top-leaderboard";
	line += "searchenginesuggestions.com##.leaderboard";
	line += "seattlepi.com##.wingadblock";
	line += "secretmaryo.org##div[style=\"width: 728px; height: 90px; margin-left: 6px;\"]";
	line += "securityfocus.com##td[width=\"160\"][bgcolor=\"#eaeaea\"]";
	line += "securityweek.com###banner";
	line += "seekfind.org##table[width=\"150\"]";
	line += "sensis.com.au##.pfpRightParent";
	line += "sensis.com.au##.pfplist";
	line += "serialnumber.in##div[style^=\"display: block; position: absolute;\"]";
	line += "serialnumber.in##div[style^=\"display: block; text-align: center; line-height: normal; visibility: visible; position: absolute;\"]";
	line += "sevenload.com###superbaannerContainer";
	line += "sevenload.com###yahoo-container";
	line += "sfgate.com##.kaango";
	line += "sfgate.com##.z-sponsored-block";
	line += "sfx.co.uk###banner";
	line += "shine.yahoo.com###ylf-ysm-side";
	line += "shinyshiny.tv##.leaderboard";
	line += "shitbrix.com###top-leaderboard";
	line += "shitbrix.com##.leaderboard";
	line += "shopping.com###featListingSection";
	line += "shopping.findtarget.com##div[style=\"background: none repeat scroll 0% 0% rgb(255, 255, 255); padding: 0pt 0.4em 0.1em 0pt; margin: 0.3em 0pt;\"]";
	line += "shopping.net##table[border=\"1\"][width=\"580\"]";
	line += "shopping.yahoo.com##.shmod-ysm";
	line += "sify.com##div[style=\"width: 250px; height: 250px;\"]";
	line += "siliconchip.com.au##td[align=\"RIGHT\"][width=\"50%\"][valign=\"BOTTOM\"]";
	line += "siliconvalley.com##.blogBox";
	line += "siliconvalley.com##.lnbbgcolor";
	line += "silverlight.net##.banner_header";
	line += "simplyassist.co.uk##.std_BottomLine";
	line += "simplyhired.com##.featured";
	line += "siteadvisor.com##.midPageSmallOuterDiv";
	line += "sitepoint.com##.industrybrains";
	line += "siteseer.ca###banZone";
	line += "sixbillionsecrets.com###droitetop";
	line += "sk-gaming.com###pts";
	line += "sk-gaming.com###ptsf";
	line += "skins.be##.shortBioShadowB.w240";
	line += "skyrock.com###pub_up";
	line += "slashfood.com##.quigo";
	line += "slate.com##.bizbox_promo";
	line += "slideshare.net##.medRecBottom2";
	line += "sloughobserver.co.uk###buttons-mpu-box";
	line += "slyck.com##div[style=\"width: 295px; border: 1px solid rgb(221, 221, 221); text-align: center; background: none repeat scroll 0% 0% rgb(255, 255, 255); padding: 5px; font: 12px verdana;\"]";
	line += "smarter.com##.favboxmiddlesearch";
	line += "smarter.com##.favwrapper";
	line += "smash247.com###RT1";
	line += "smashingmagazine.com###commentsponsortarget";
	line += "smashingmagazine.com###mediumrectangletarget";
	line += "smashingmagazine.com###sidebaradtarget";
	line += "smashingmagazine.com###sponsorlisttarget";
	line += "smashingmagazine.com##.ed";
	line += "smh.com.au##.ad";
	line += "snapfiles.com###bannerbar";
	line += "snapfiles.com###borderbar";
	line += "snapfiles.com###prodmsg";
	line += "snow.co.nz###content-footer-wrap";
	line += "snow.co.nz###header-banner";
	line += "snowtv.co.nz###header-banner";
	line += "soccerphile.com###midbanners";
	line += "soccerphile.com###topbanners";
	line += "socialmarker.com###ad";
	line += "soft32.com##a[href=\"http://p.ly/regbooster\"]";
	line += "softonic.com##.topbanner";
	line += "softonic.com##.topbanner_program";
	line += "softpedia.com##.logotable[align=\"right\"] > a[target=\"_blank\"]";
	line += "softpedia.com##.pagehead_op2";
	line += "softpedia.com##img[width=\"600\"][height=\"90\"]";
	line += "softpedia.com##td[align=\"right\"][style=\"padding-bottom: 5px; padding-left: 22px; padding-right: 17px;\"]";
	line += "solarmovie.com###l_35061";
	line += "someecards.com###shop";
	line += "someecards.com###some-ads";
	line += "someecards.com###some-more-ads";
	line += "someecards.com###store";
	line += "somethingawful.com##.oma_pal";
	line += "songlyrics.com##.content-bottom-banner";
	line += "songs.pk##img[width=\"120\"][height=\"60\"]";
	line += "songs.pk##table[width=\"149\"][height=\"478\"]";
	line += "songs.pk##td[width=\"100%\"][height=\"20\"]";
	line += "space.com###expandedBanner";
	line += "space.com##table[width=\"321\"][height=\"285\"][bgcolor=\"#000000\"]";
	line += "space.com##td[colspan=\"2\"]:first-child > table[width=\"968\"]:first-child";
	line += "sparesomelol.com###top-leaderboard";
	line += "sparesomelol.com##.leaderboard";
	line += "spectator.org##.ad";
	line += "spectrum.ieee.org###whtpprs";
	line += "speedtest.net##.ad";
	line += "spikedhumor.com###ctl00_CraveBanners";
	line += "spikedhumor.com##.ad";
	line += "spoiledphotos.com###top-leaderboard";
	line += "spoiledphotos.com##.leaderboard";
	line += "spokesman.com##.ad";
	line += "squidoo.com###header_banner";
	line += "stagevu.com##.ad";
	line += "start64.com##td[height=\"92\"][colspan=\"2\"]";
	line += "startpage.com###inlinetable";
	line += "startribune.com###bottomLeaderboard";
	line += "startribune.com###topLeaderboard";
	line += "staticice.com.au##table[rules=\"none\"][style=\"border: 1px solid rgb(135, 185, 245);\"]";
	line += "staticice.com.au##td[align=\"center\"][valign=\"middle\"][height=\"80\"]";
	line += "sternfannetwork.com##[align=\"center\"] > .tborder[width=\"728\"][cellspacing=\"1\"][cellpadding=\"0\"][border=\"0\"][align=\"center\"]";
	line += "stickam.com###f_BottomBanner";
	line += "stickam.com###h_TopBanner";
	line += "stopdroplol.com###top-leaderboard";
	line += "stopdroplol.com##.leaderboard";
	line += "storagereview.com##td[width=\"410\"]:first-child + td[align=\"right\"]";
	line += "stormfront.org##img[border=\"0\"][rel=\"nofollow\"]";
	line += "stormfront.org##table[width=\"863\"]";
	line += "streamingmedia.com##.sponlinkbox";
	line += "stripes.com##.ad";
	line += "stumblehere.com##td[width=\"270\"][height=\"110\"]";
	line += "stv.tv###collapsedBanner";
	line += "stv.tv###expandedBanner";
	line += "stv.tv###google";
	line += "stylelist.com###cod-promo";
	line += "stylelist.com##.fromsponsor";
	line += "stylelist.com##.partnerPromo";
	line += "stylelist.com##div[style=\"position: relative; border: 1px solid rgb(191, 191, 191); background: none repeat scroll 0% 0% white; width: 424px; display: block;\"]";
	line += "sunderlandecho.com###banner01";
	line += "sunshinecoastdaily.com.au###localOffers";
	line += "supernovatube.com##a[href^=\"http://preview.licenseacquisition.org/\"]";
	line += "superpages.com##.sponsreulst";
	line += "swamppolitics.com###leaderboard";
	line += "switched.com###topleader-wrap";
	line += "switched.com##.medrect";
	line += "swns.com##.story_mpu";
	line += "sydneyfc.com##.promotion_wrapper";
	line += "sydneyolympicfc.com###horiz_image_rotation";
	line += "sys-con.com###elementDiv";
	line += "sys-con.com##td[width=\"180\"][valign=\"top\"][rowspan=\"3\"]";
	line += "talkingpointsmemo.com##.seventwentyeight";
	line += "talkxbox.com###features-sub";
	line += "tarot.com###leaderboardOuter";
	line += "tattoofailure.com###top-leaderboard";
	line += "tattoofailure.com##.leaderboard";
	line += "tcmagazine.com###bannerfulltext";
	line += "tcmagazine.com###topbanner";
	line += "tcmagazine.info###bannerfulltext";
	line += "tcmagazine.info###topbanner";
	line += "tcpalm.com##.bigbox_wrapper";
	line += "teamliquid.net##div[style=\"width: 472px; height: 64px; overflow: hidden; padding: 0px; margin: 0px;\"]";
	line += "tech-recipes.com###first-300-ad";
	line += "tech-recipes.com###leaderboard";
	line += "techcrunch.com###post_unit_medrec";
	line += "techcrunch.com##.ad";
	line += "techcrunchit.com##.ad";
	line += "techdigest.tv##.leaderboard";
	line += "techdirt.com##.ad";
	line += "techguy.org##div[style=\"height: 100px; width: 100%; text-align: center;\"]";
	line += "techhamlet.com###text-32";
	line += "technewsworld.com##.content-block-slinks";
	line += "technewsworld.com##.content-tab-slinks";
	line += "technologyreview.com##div[style=\"padding-bottom: 8px;\"]";
	line += "technologyreview.com##div[style=\"text-align: center; background: url(\\\"/images/divider_horiz.gif\\\") repeat-x scroll left bottom transparent; padding: 10px;\"]";
	line += "technologyreview.com##p[style=\"clear: both; text-align: center; background: url(\\\"/images/divider_horiz.gif\\\") repeat-x scroll left bottom transparent; font-size: 11px; padding: 0pt; margin: 0pt;\"]";
	line += "technorati.com###ad";
	line += "technorati.com##.ad";
	line += "techrepublic.com.com###medusa";
	line += "techrepublic.com.com###ppeHotspot";
	line += "techrepublic.com.com###spotlight";
	line += "techrepublic.com.com###wpPromo";
	line += "techrepublic.com.com##.essentialTopics";
	line += "techrepublic.com.com##.hotspot";
	line += "techwatch.co.uk##table[width=\"250\"][height=\"300\"]";
	line += "techweb.com###h_banner";
	line += "tectonic.co.za##.tdad125";
	line += "teenhut.net##td[align=\"left\"][width=\"160\"][valign=\"top\"]";
	line += "teesoft.info###footer-800";
	line += "teesoft.info###uniblue";
	line += "telecompaper.com##.side_banner";
	line += "telegramcommunications.com###leftBanner";
	line += "telegramcommunications.com###rightBanner";
	line += "telegraph.co.uk###gafsslot1";
	line += "telegraph.co.uk###gafsslot2";
	line += "telegraph.co.uk##.comPuff";
	line += "telegraph.co.uk##a[href^=\"http://www.telegraph.co.uk/sponsored/\"]";
	line += "telegraphindia.com##.Caption";
	line += "televisionbroadcast.com##table[width=\"665\"]";
	line += "tesco.com###dartLeftSkipper";
	line += "tesco.com###dartRightSkipper";
	line += "tesco.com##.dart";
	line += "tf2maps.net##a[href=\"http://forums.tf2maps.net/payments.php\"]";
	line += "tf2maps.net##form[name=\"search\"] + div + fieldset";
	line += "tf2maps.net##form[name=\"search\"] + div + fieldset + br + br + fieldset";
	line += "tfportal.net###snt_wrapper";
	line += "tgdaily.com###right-banner";
	line += "thatvideogameblog.com##table[width=\"310\"][height=\"260\"]";
	line += "thatvideosite.com##div[style=\"padding-bottom: 15px; height: 250px;\"]";
	line += "the217.com###textpromo";
	line += "theaa.com###unanimis1";
	line += "theage.com.au##.ad";
	line += "thebizzare.com##.adblock";
	line += "thecelebritycafe.com##table[width=\"135\"][height=\"240\"]";
	line += "thecourier.co.uk###sidebarMiddleCol";
	line += "theeagle.com##.SectionRightRail300x600Box";
	line += "theeastafrican.co.ke##.c15r";
	line += "thefashionspot.com###roadblock";
	line += "thefreedictionary.com###Ov";
	line += "thefreedictionary.com##.Ov";
	line += "thefrisky.com##.partner-link-boxes-container";
	line += "thegameslist.com##.leader";
	line += "thegauntlet.ca##div[style=\"width: 170px; height: 620px; background: url(\\\"/advertisers/your-ad-here-160x600.gif\\\") repeat scroll 0% 0% rgb(204, 204, 204); vertical-align: top; text-align: center;\"]";
	line += "thegauntlet.ca##div[style=\"width: 190px; height: 110px; background: url(\\\"/advertisers/your-ad-here-180x90.gif\\\") repeat scroll 0% 0% rgb(204, 204, 204); vertical-align: top; text-align: center;\"]";
	line += "thegauntlet.ca##div[style=\"width: 190px; height: 170px; background: url(\\\"/advertisers/your-ad-here-180x150.gif\\\") repeat scroll 0% 0% rgb(204, 204, 204); vertical-align: top; text-align: center;\"]";
	line += "thegauntlet.ca##div[style=\"width: 738px; height: 110px; background: url(\\\"/advertisers/your-ad-here-728x90.gif\\\") repeat scroll 0% 0% rgb(204, 204, 204); vertical-align: top; text-align: center;\"]";
	line += "theglobeandmail.com##.ad";
	line += "thegrumpiest.com##td[align=\"left\"][width=\"135px\"]";
	line += "thegrumpiest.com##td[align=\"left\"][width=\"135px\"] + td#table1";
	line += "thehill.com###topbanner";
	line += "thehill.com##.banner";
	line += "thehill.com##.lbanner";
	line += "thehill.com##.vbanner";
	line += "thelocalweb.net##.verdana9green";
	line += "themaineedge.com##td[height=\"80\"][style=\"background-color: rgb(0, 0, 0);\"]";
	line += "themaineedge.com##td[width=\"180\"][style=\"background-color: rgb(51, 95, 155); text-align: center;\"]";
	line += "themoscowtimes.com##.adv_block";
	line += "themoscowtimes.com##.top_banner";
	line += "thenation.com##.ad";
	line += "thenation.com##.modalContainer";
	line += "thenation.com##.modalOverlay";
	line += "thenextweb.com##.promo";
	line += "thenextweb.com##.promotion_frame";
	line += "theonion.com##.ad";
	line += "thepittsburghchannel.com##.MS";
	line += "thepspblog.com###featured";
	line += "thepspblog.com###mta_bar";
	line += "theregister.co.uk###jobs-promo";
	line += "theregister.co.uk###msdn-promo";
	line += "theregister.co.uk##.papers-promo";
	line += "theregister.co.uk##.wptl";
	line += "thesaurus.com###abvFold";
	line += "thesaurus.com##.spl_unshd";
	line += "theserverside.com###leaderboard";
	line += "thesixthaxis.com##.map-header-mainblock";
	line += "thesixthaxis.com##.map-main-right-takeover";
	line += "thesixtyone.com##div[style=\"width: 968px; text-align: center; margin-top: 12px; clear: both; float: left;\"]";
	line += "thesmokinggun.com###skyscraper";
	line += "thestandard.com###leaderboard_banner";
	line += "thestates.fm###banbo";
	line += "thestreet.com###brokerage";
	line += "thestreet.com###textLinks";
	line += "thestreet.com###textLinksContainer";
	line += "thesun.co.uk###takeoverleft";
	line += "thesun.co.uk###takeoverright";
	line += "thesun.co.uk##.float-right.padding-left-10.width-300.padding-bottom-10.padding-top-10";
	line += "thesun.co.uk##.srch_cont";
	line += "thesuperficial.com###leaderboard";
	line += "thetandd.com##.yahoo_content_match";
	line += "thevarguy.com###middlebannerwrapper";
	line += "thevarguy.com##.squarebanner160x160";
	line += "thinkpads.com###sponsorbar";
	line += "thisisbath.co.uk###mast-head";
	line += "thisisbristol.co.uk###mast-head";
	line += "thisisleicestershire.co.uk###mast-head";
	line += "thisisleicestershire.co.uk##.banner-extButton";
	line += "thisismoney.co.uk###Sky";
	line += "thisisplymouth.co.uk##.leaderboard";
	line += "threatpost.com###partners";
	line += "tidbits.com###top_banner";
	line += "tigerdirect.ca##div[style=\"width: 936px; clear: both; margin-top: 2px; height: 90px;\"]";
	line += "tigerdroppings.com##td[height=\"95\"][bgcolor=\"#dedede\"]";
	line += "time.com##.sep";
	line += "timeanddate.com##fieldset[style=\"float: right; width: 180px;\"]";
	line += "timeout.com##.MD_textLinks01";
	line += "timeoutdubai.com###tleaderb";
	line += "timesdispatch.com###dealoftheday";
	line += "timesnewsline.com##div[style=\"border: 1px solid rgb(227, 227, 227); background: none repeat scroll 0% 0% rgb(255, 248, 221); padding: 5px; width: 95%;\"]";
	line += "timesnewsline.com##table[width=\"300\"][height=\"250\"][align=\"left\"]";
	line += "timesofindia.indiatimes.com##div[style=\"float: left; padding-left: 5px;\"]";
	line += "timesofindia.indiatimes.com##div[style=\"height: 100px;\"]";
	line += "timesonline.co.uk##.bg-f0eff5.padding-left-right-9.padding-top-6.link-width.word-wrap";
	line += "timesonline.co.uk##.bg-f0eff5.padding-left-right-9.padding-top-6.padding-bottom-7.word-wrap";
	line += "timesonline.co.uk##.classifieds-long-container";
	line += "tinypic.com##.ad";
	line += "tinypic.com##.medrec";
	line += "tips.net###googlebig";
	line += "titantv.com##.leaderboard";
	line += "tmz.com###leaderboard";
	line += "tmz.com###skyscraper";
	line += "tnt.tv###right300x250";
	line += "todaystmj4.com###leaderboard1";
	line += "todaytechnews.com##.advText";
	line += "tomsgames.com###pub_header";
	line += "tomsgames.it###pub_header";
	line += "tomsguide.com##.sideOffers";
	line += "tomwans.com##a.big_button[target=\"_blank\"]";
	line += "toofab.com###leaderboard";
	line += "top4download.com##div[style=\"float: left; width: 620px; height: 250px; clear: both;\"]";
	line += "top4download.com##div[style=\"width: 450px; height: 205px; clear: both;\"]";
	line += "topgear.com###skyscraper";
	line += "topix.com###freecredit";
	line += "topix.com###krillion_container";
	line += "topsocial.info##a[href^=\"http://click.search123.uk.com/\"]";
	line += "toptechnews.com##.regtext[style=\"border: 1px solid rgb(192, 192, 192); padding: 5px;\"]";
	line += "toptechnews.com##table[width=\"370\"][cellpadding=\"10\"][style=\"border: 1px solid rgb(204, 204, 204); border-collapse: collapse;\"]";
	line += "toptechnews.com##table[width=\"990\"][cellpadding=\"5\"]";
	line += "toptenreviews.com##.google_add_container";
	line += "toptut.com##.af-form";
	line += "torontosun.com###buttonRow";
	line += "torrent-finder.com##.cont_lb";
	line += "torrents.to##.da-top";
	line += "torrentz.com##div[style=\"width: 1000px; margin: 0pt auto;\"]";
	line += "totalfark.com###rightSideRightMenubar";
	line += "totalfilm.com###mpu_container";
	line += "totalfilm.com###skyscraper_container";
	line += "tothepc.com##.sidebsa";
	line += "toynews-online.biz##.newsinsert";
	line += "travel.yahoo.com##.spon";
	line += "travel.yahoo.com##.tgl-block";
	line += "treatmentforbruises.net##.fltlft";
	line += "treatmentforbruises.net##.fltrt";
	line += "treehugger.com##.google-indiv-box2";
	line += "treehugger.com##.leaderboard";
	line += "tripadvisor.ca##.commerce";
	line += "tripadvisor.co.uk##.commerce";
	line += "tripadvisor.com##.commerce";
	line += "tripadvisor.ie##.commerce";
	line += "tripadvisor.in##.commerce";
	line += "trovit.co.uk##.wrapper_trovit_ppc";
	line += "trucknetuk.com###page-body > div[style=\"margin: 0pt auto; text-align: center;\"]";
	line += "trucknetuk.com##table[width=\"100%\"][bgcolor=\"#cecbce\"] > tbody > tr > #sidebarright[valign=\"top\"]:last-child";
	line += "trucknetuk.com##table[width=\"620\"][cellspacing=\"3\"][bgcolor=\"#ffffff\"][align=\"center\"][style=\"border: thin solid black;\"]";
	line += "trueslant.com##.bot_banner";
	line += "trustedreviews.com###bottom-sky";
	line += "trustedreviews.com###top-sky";
	line += "trutv.com##.banner";
	line += "tsviewer.com###layer";
	line += "tuaw.com##.medrect";
	line += "tuaw.com##.topleader";
	line += "tucows.com##.w952.h85";
	line += "tucsoncitizen.com##.bigbox_container";
	line += "tucsoncitizen.com##.leaderboard_container_top";
	line += "tucsoncitizen.com##.skyscraper_container";
	line += "tutsplus.com###AdobeBanner";
	line += "tutsplus.com##.leader_board";
	line += "tutzone.net###bigBox";
	line += "tv.yahoo.com##.spons";
	line += "tvgolo.com##.inner2";
	line += "tvgolo.com##.title-box4";
	line += "tvgolo.com##.title-box5";
	line += "tvguide.co.uk##table[width=\"160\"][height=\"620\"]";
	line += "tvsquad.com###tvsquad_topBanner";
	line += "tvsquad.com##.banner";
	line += "tvtechnology.com##table[width=\"665\"]";
	line += "twcenter.net##div[style=\"width: 728px; height: 90px; margin: 1em auto 0pt;\"]";
	line += "twilightwap.com##.ahblock2";
	line += "twitter.com##.promoted-account";
	line += "twitter.com##.promoted-trend";
	line += "twitter.com##.promoted-tweet";
	line += "twitter.com##li[data*=\"advertiser_id\"]";
	line += "u-file.net##.spottt_tb";
	line += "ucas.com##a[href^=\"http://eva.ucas.com/s/redirect.php?ad=\"]";
	line += "ucoz.com##[id^=\"adBar\"]";
	line += "ugotfile.com##a[href=\"https://www.astrill.com/\"]";
	line += "ugotfile.com##a[href^=\"http://ugotfile.com/affiliate?\"]";
	line += "ukclimbing.com##img[width=\"250\"][height=\"350\"]";
	line += "ultimate-guitar.com##.pca";
	line += "ultimate-guitar.com##.pca2";
	line += "ultimate-guitar.com##td[align=\"center\"][width=\"160\"]";
	line += "ultimate-guitar.com##td[style=\"height: 110px; vertical-align: middle; text-align: center;\"]";
	line += "ultimate-guitar.com##td[width=\"100%\"][valign=\"middle\"][height=\"110\"]";
	line += "ultimate-rihanna.com###ad";
	line += "uncoached.com###sidebar300X250";
	line += "united-ddl.com##table[width=\"435\"][bgcolor=\"#575e57\"]";
	line += "unknown-horizons.org###akct";
	line += "unrealitymag.com###header";
	line += "unrealitymag.com###sidebar300X250";
	line += "uploaded.to##div[style=\"background-repeat: no-repeat; width: 728px; height: 90px; margin-left: 0px;\"]";
	line += "uploading.com##div[style=\"background: rgb(246, 246, 246) none repeat scroll 0% 0%; width: 35%; -moz-background-clip: border; -moz-background-origin: padding; -moz-background-inline-policy: continuous; height: 254px;\"]";
	line += "uploading.com##div[style=\"margin: -2px auto 19px; display: block; position: relative;\"]";
	line += "uploadville.com##a[href^=\"http://www.flvpro.com/movies/?aff=\"]";
	line += "uploadville.com##a[href^=\"http://www.gygan.com/affiliate/\"]";
	line += "urbandictionary.com###dfp_define_rectangle";
	line += "urbandictionary.com###dfp_homepage_medium_rectangle";
	line += "urbandictionary.com###dfp_skyscraper";
	line += "urbandictionary.com###rollup";
	line += "urbandictionary.com##.zazzle_links";
	line += "url.org###resspons1";
	line += "url.org###resspons2";
	line += "urlesque.com##.sidebarBanner";
	line += "urlesque.com##.topBanner";
	line += "usatoday.com###expandedBanner";
	line += "usatoday.com###footerSponsorOne";
	line += "usatoday.com###footerSponsorTwo";
	line += "usatoday.com##.ad";
	line += "usautoparts.net##td[height=\"111\"][align=\"center\"][valign=\"top\"]";
	line += "userscripts.org##.sponsor";
	line += "userstyles.org##.ad";
	line += "usnews.com##.ad";
	line += "usniff.com###bottom";
	line += "usniff.com##.top-usniff-torrents";
	line += "v3.co.uk###superSky";
	line += "v3.co.uk##.ad";
	line += "v3.co.uk##.hpu";
	line += "v3.co.uk##.leaderboard";
	line += "v8x.com.au##td[align=\"RIGHT\"][width=\"50%\"][valign=\"BOTTOM\"]";
	line += "variety.com###googlesearch";
	line += "variety.com###w300x250";
	line += "variety.com##.sponsor";
	line += "veehd.com##.isad";
	line += "venturebeat.com###leader";
	line += "venturebeat.com##div[style=\"height: 300px; text-align: center;\"]";
	line += "verizon.net##.sponsor";
	line += "vg247.com###leader";
	line += "vg247.com###rightbar > #halfpage";
	line += "vidbox.net##.overlayVid";
	line += "vidbux.com##a[href=\"http://www.vidbux.com/ccount/click.php?id=4\"]";
	line += "video.foxnews.com###cb_medrect1_div";
	line += "video2mp3.net###ad";
	line += "videogamer.com##.skinClick";
	line += "videogamer.com##.widesky";
	line += "videography.com##table[width=\"665\"]";
	line += "videohelp.com###leaderboard";
	line += "videohelp.com##.stylenormal[width=\"24%\"][valign=\"top\"][align=\"left\"]";
	line += "videohelp.com##td[valign=\"top\"][height=\"200\"][style=\"background-color: rgb(255, 255, 255);\"]";
	line += "videojug.com##.forceMPUSize";
	line += "videoweed.com##.ad";
	line += "videoweed.com##div[style=\"width: 460px; height: 60px; border: 1px solid rgb(204, 204, 204); margin: 0px auto 10px;\"]";
	line += "videoweed.com##div[style^=\"width: 160px; height: 600px; border: 1px solid rgb(204, 204, 204); float:\"]";
	line += "vidreel.com##.overlayVid";
	line += "vidxden.com###divxshowboxt > a[target=\"_blank\"] > img[width=\"158\"]";
	line += "vidxden.com##.ad";
	line += "vidxden.com##.header_greenbar";
	line += "vimeo.com##.ad";
	line += "vioku.com##.ad";
	line += "virginmedia.com##.s-links";
	line += "virtualnights.com###head-banner";
	line += "virus.gr###block-block-19";
	line += "viz.com##div[style^=\"position: absolute; width: 742px; height: 90px;\"]";
	line += "vladtv.com###banner-bottom";
	line += "w2c.in##[href^=\"http://c.admob.com/\"]";
	line += "w3schools.com##a[rel=\"nofollow\"]";
	line += "w3schools.com##div[style=\"width: 890px; height: 94px; position: relative; margin: 0px; padding: 0px; overflow: hidden;\"]";
	line += "walesonline.co.uk##.promobottom";
	line += "walesonline.co.uk##.promotop";
	line += "walletpop.com###attic";
	line += "walletpop.com##.medrect";
	line += "walletpop.com##.sponsWidget";
	line += "walyou.com##.ad";
	line += "warez-files.com##.premium_results";
	line += "warezchick.com##div.top > p:last-child";
	line += "warezchick.com##img[border=\"0\"]";
	line += "wareznova.com##img[width=\"298\"][height=\"53\"]";
	line += "wareznova.com##img[width=\"468\"]";
	line += "wareznova.com##input[value=\"Download from DLP\"]";
	line += "wareznova.com##input[value=\"Start Premium Downloader\"]";
	line += "washingtonexaminer.com###header_top";
	line += "washingtonpost.com###textlinkWrapper";
	line += "washingtonscene.thehill.com##.top";
	line += "wasterecyclingnews.com##.bigbanner";
	line += "watoday.com.au##.ad";
	line += "wattpad.com##div[style=\"width: 100%; height: 90px; text-align: center;\"]";
	line += "weather.ninemsn.com.au###msnhd_div3";
	line += "weatherbug.com##.wXcds1";
	line += "weatherbug.com##.wXcds2";
	line += "webdesignerwall.com##.ad";
	line += "webdesignstuff.com###headbanner";
	line += "webopedia.com##.bstext";
	line += "webpronews.com##.articleleftcol";
	line += "webresourcesdepot.com##.Banners";
	line += "webresourcesdepot.com##img[width=\"452px\"][height=\"60px\"]";
	line += "webworldindex.com##table[bgcolor=\"#ceddf0\"]";
	line += "weddingmuseum.com##a[href^=\"http://click.linksynergy.com/\"]";
	line += "weeklyworldnews.com##.top-banner";
	line += "wefindads.co.uk##div.posts-holder[style=\"margin-top: 10px;\"]";
	line += "wefollow.com##.ad";
	line += "wenn.com###topbanner";
	line += "weselectmodels.com##div[style=\"width: 728px; height: 90px; background-color: black; text-align: center;\"]";
	line += "westlothianhp.co.uk###banner01";
	line += "westsussextoday.co.uk###banner01";
	line += "wftv.com###leaderboard-sticky";
	line += "whatismyip.com##.gotomypc";
	line += "whatismyip.com##span[style=\"margin: 2px; float: left; width: 301px; height: 251px;\"]";
	line += "wheels.ca##div[style=\"color: rgb(153, 153, 153); font-size: 9px; clear: both; border-top: 1px solid rgb(238, 238, 238); padding-top: 15px;\"]";
	line += "wheels.ca##div[style=\"float: left; width: 237px; height: 90px; margin-right: 5px;\"]";
	line += "wheels.ca##div[style=\"float: left; width: 728px; height: 90px; z-index: 200000;\"]";
	line += "whistlestopper.com##td[align=\"left\"][width=\"160\"][valign=\"top\"]";
	line += "widescreengamingforum.com###banner-content";
	line += "wikia.com###HOME_LEFT_SKYSCRAPER_1";
	line += "wikia.com###HOME_TOP_LEADERBOARD";
	line += "wikia.com###LEFT_SKYSCRAPER_1";
	line += "wikia.com###LEFT_SKYSCRAPER_2";
	line += "wikia.com###TOP_LEADERBOARD";
	line += "winamp.com###subheader";
	line += "windows7download.com##div[style=\"width: 336px; height: 280px;\"]";
	line += "windows7download.com##div[style=\"width: 680px; height: 280px; clear: both;\"]";
	line += "windowsbbs.com##span[style=\"margin: 2px; float: left; width: 337px; height: 281px;\"]";
	line += "windowsitpro.com###dnn_pentonRoadblock_pnlRoadblock";
	line += "windowsxlive.net##div[style=\"width: 160px; height: 600px; margin-left: 12px; margin-top: 16px;\"]";
	line += "windowsxlive.net##div[style=\"width: 336px; height: 380px; float: right; margin: 8px;\"]";
	line += "winsupersite.com###footerLinks > table[width=\"100%\"]:first-child";
	line += "winsupersite.com##td[style=\"border-top: 1px none rgb(224, 224, 224); color: rgb(0, 0, 0); font-weight: normal; font-style: normal; font-family: sans-serif; font-size: 8pt; padding-right: 3px; padding-bottom: 3px; padding-top: 3px; text-align: left;\"]";
	line += "wired.co.uk##.banner-top";
	line += "wired.co.uk##.banner1";
	line += "wired.com###featured";
	line += "wirelessforums.org##td[width=\"160\"][valign=\"top\"]";
	line += "wisegeek.com##[action=\"/the-best-schools-for-you.htm\"]";
	line += "wishtv.com###leaderboard";
	line += "wlfi.com###leaderboard";
	line += "wordreference.com##.bannertop";
	line += "workforce.com##td[width=\"970\"][height=\"110\"]";
	line += "worksopguardian.co.uk###banner01";
	line += "worldmag.com##div[style=\"padding: 8px 0px; text-align: center;\"]";
	line += "worldmag.com##div[style=\"text-align: center; padding: 8px 0px; clear: both;\"]";
	line += "worthdownloading.com##tr:first-child:last-child > td:first-child:last-child > .small_titleGrey[align=\"center\"]:first-child";
	line += "worthingherald.co.uk###banner01";
	line += "worthplaying.com##.ad";
	line += "wow.com###topleader-wrap";
	line += "wow.com##.medrect";
	line += "wowwiki.com###HOME_LEFT_SKYSCRAPER_1";
	line += "wowwiki.com###HOME_TOP_LEADERBOARD";
	line += "wowwiki.com###LEFT_SKYSCRAPER_1";
	line += "wowwiki.com###TOP_LEADERBOARD";
	line += "wpbt2.org##.home_banners";
	line += "wphostingdiscount.com##.ad";
	line += "wptv.com##.module.horizontal";
	line += "wsj.com##.spn_links_box";
	line += "wwl.com###BannerXGroup";
	line += "wwtdd.com###showpping";
	line += "wwtdd.com##.post_insert";
	line += "wwtdd.com##.s728x90";
	line += "www.google.co.in##table[cellpadding=\"0\"][width=\"100%\"][style^=\"border: 1px solid\"]";
	line += "www.google.com##table[cellpadding=\"0\"][width=\"100%\"][style^=\"border: 1px solid\"]";
	line += "wypr.org###leaderboard";
	line += "xbox360rally.com###topbanner";
	line += "xe.com###HomePage_Slot1";
	line += "xe.com###HomePage_Slot2";
	line += "xe.com###HomePage_Slot3";
	line += "xe.com###UCCInputPage_Slot1";
	line += "xe.com###UCCInputPage_Slot2";
	line += "xe.com###UCCInputPage_Slot3";
	line += "xe.com###leaderB";
	line += "xe.com##.wa_leaderboard";
	line += "xfm.co.uk###commercial";
	line += "xml.com###leaderboard";
	line += "xml.com##.recommended_div2";
	line += "xml.com##.secondary[width=\"153\"][bgcolor=\"#efefef\"]";
	line += "xtremesystems.org##embed[width=\"728\"]";
	line += "xtremesystems.org##img[width=\"728\"]";
	line += "xtshare.com##.overlayVid";
	line += "xxlmag.com###medium-rec";
	line += "yahoo.com###ad";
	line += "yahoo.com###marketplace";
	line += "yahoo.com###mw-ysm-cm";
	line += "yahoo.com###y_provider_promo";
	line += "yahoo.com###ygmapromo";
	line += "yahoo.com###ylf-ysm";
	line += "yahoo.com###yn-gmy-promo-answers";
	line += "yahoo.com###yn-gmy-promo-groups";
	line += "yahoo.com##.fpad";
	line += "yahoo.com##.marketplace";
	line += "yahoo.com##.y708-commpartners";
	line += "yahoo.com##.yschspns";
	line += "yatsoba.com##.sponsors";
	line += "yauba.com###sidebar > .block_result:first-child";
	line += "yauba.com##.resultscontent:first-child";
	line += "yesasia.com##.advHr";
	line += "yfrog.com##.promo-area";
	line += "yodawgpics.com###top-leaderboard";
	line += "yodawgpics.com##.leaderboard";
	line += "yoimaletyoufinish.com###top-leaderboard";
	line += "yoimaletyoufinish.com##.leaderboard";
	line += "yorkshireeveningpost.co.uk###banner01";
	line += "yorkshirepost.co.uk###banner01";
	line += "yourmindblown.com##div[style=\"float: right; width: 300px; height: 600px; padding: 10px 0px;\"]";
	line += "yourmindblown.com##div[style=\"width: 300px; min-height: 250px; padding: 10px 0px; background: none repeat scroll 0% 0% rgb(255, 255, 255);\"]";
	line += "yourtomtom.com##.bot";
	line += "yourtomtom.com##div[style=\"height: 600px; padding: 6px 0pt; border: 1px solid rgb(180, 195, 154); background: none repeat scroll 0% 0% rgb(249, 252, 241); margin: 0pt;\"]";
	line += "youtube.com###feedmodule-PRO";
	line += "youtube.com###homepage-chrome-side-promo";
	line += "youtube.com###search-pva";
	line += "youtube.com###watch-branded-actions";
	line += "youtube.com###watch-buy-urls";
	line += "youtube.com##.promoted-videos";
	line += "youtube.com##.watch-extra-info-column";
	line += "youtube.com##.watch-extra-info-right";
	line += "ytmnd.com###please_dont_block_me";
	line += "ytmnd.com##td[colspan=\"5\"]";
	line += "zalaa.com##.left_iframe";
	line += "zalaa.com##.overlayVid";
	line += "zalaa.com##a[href^=\"http://www.graboid.com/affiliates/\"]";
	line += "zambiz.co.zm##td[width=\"130\"][height=\"667\"]";
	line += "zambiz.co.zm##td[width=\"158\"][height=\"667\"]";
	line += "zath.co.uk##.ad";
	line += "zdnet.co.uk##.sponsor";
	line += "zdnet.com###pplayLinks";
	line += "zdnet.com##.dirListSuperSpons";
	line += "zdnet.com##.hotspot";
	line += "zdnet.com##.promoBox";
	line += "zedomax.com##.entry > div[style=\"width: 100%; height: 280px;\"]";
	line += "zedomax.com##.entry > div[style=\"width: 336px; height: 280px;\"]";
	line += "zeenews.com##.ban-720-container";
	line += "zippyshare.com##.center_reklamy";
	line += "zomganime.com##a[href=\"http://fs.game321.com/?utm_source=zomganime&utm_medium=skin_banner&utm_term=free&utm_campaign=fs_zomg_skin\"]";
	line += "zomganime.com##div[style=\"background-color: rgb(153, 153, 153); width: 300px; height: 250px; overflow: hidden; margin: 0pt auto;\"]";
	line += "zomganime.com##div[style=\"background-color: rgb(239, 239, 239); width: 728px; height: 90px; overflow: hidden;\"]";
	line += "zomganime.com##marquee[width=\"160\"]";
	line += "zone.msn.com##.SuperBannerTVMain";
	line += "zonelyrics.net###panelRng";
	line += "zoozle.org###search_right";
	line += "zoozle.org###search_topline";
	line += "zoozle.org##a[onclick^=\"downloadFile('download_big', null,\"]";
	line += "zoozle.org##a[onclick^=\"downloadFile('download_related', null,\"]";
	line += "zuploads.com###buttoncontainer";
	line += "zuploads.com##.hispeed";
	line += "zuploads.net###buttoncontainer";
	line += "zuula.com##.sponsor";
	line += "zxxo.net##a[href^=\"http://www.linkbucks.com/referral/\"]";
	line += "!-----------------Whitelists-----------------!";
	line += "! *** easylist_whitelist.txt ***";
	line += "@@&adname=$script,domain=sankakucomplex.com";
	line += "@@||2mdn.net/*/dartshell*.swf";
	line += "@@||2mdn.net/*_ecw_$image,domain=wwe.com";
	line += "@@||2mdn.net/crossdomain.xml$object_subrequest";
	line += "@@||2mdn.net/instream/ads_sdk_config.xml$object_subrequest,domain=youtube.com";
	line += "@@||2mdn.net/instream/adsapi_$object_subrequest,domain=youtube.com";
	line += "@@||2mdn.net/viewad/817-grey.gif$object_subrequest,domain=imdb.com";
	line += "@@||a.ads2.msads.net^*.swf$domain=msnbc.msn.com";
	line += "@@||a.giantrealm.com/assets/vau/grplayer*.swf";
	line += "@@||abc.vad.go.com/dynamicvideoad?$object_subrequest";
	line += "@@||ad.103092804.com/st?ad_type=$subdocument,domain=wizard.mediacoderhq.com";
	line += "@@||ad.doubleclick.net/adx/nbcu.nbc/rewind$object_subrequest";
	line += "@@||ad.doubleclick.net/adx/vid.age/$object_subrequest";
	line += "@@||ad.doubleclick.net/pfadx/nbcu.nbc/rewind$object_subrequest";
	line += "@@||ad.zanox.com/ppc/$subdocument,domain=wisedock.at|wisedock.co.uk|wisedock.com|wisedock.de|wisedock.eu";
	line += "@@||ad3.liverail.com^$object_subrequest,domain=breitbart.tv|seesaw.com";
	line += "@@||adhostingsolutions.com/crossdomain.xml$object_subrequest,domain=novafm.com.au";
	line += "@@||adjuggler.com^$script,domain=videodetective.com";
	line += "@@||adm.fwmrm.net^*/admanager.swf?";
	line += "@@||admin.brightcove.com/viewer/*/advertisingmodule.swf$domain=guardian.co.uk|slate.com";
	line += "@@||adnet.twitvid.com/crossdomain.xml$object_subrequest";
	line += "@@||ads.adap.tv/control?$object_subrequest";
	line += "@@||ads.adap.tv/crossdomain.xml$object_subrequest";
	line += "@@||ads.adap.tv/redir/client/adplayer.swf$domain=xxlmag.com";
	line += "@@||ads.adultswim.com/js.ng/site=toonswim&toonswim_pos=600x400_ctr&toonswim_rollup=games$script";
	line += "@@||ads.belointeractive.com/realmedia/ads/adstream_mjx.ads/www.kgw.com/video/$script";
	line += "@@||ads.cnn.com/js.ng/*&cnn_intl_subsection=download$script";
	line += "@@||ads.cricbuzz.com/adserver/units/microsites/faststats.leaderboard.customcode.php$subdocument";
	line += "@@||ads.forbes.com/realmedia/ads/*@videopreroll$script";
	line += "@@||ads.fox.com/fox/black_2sec_600.flv";
	line += "@@||ads.foxnews.com/api/*-slideshow-data.js?";
	line += "@@||ads.foxnews.com/js/ad.js";
	line += "@@||ads.foxnews.com/js/omtr_code.js";
	line += "@@||ads.hulu.com^*.flv";
	line += "@@||ads.hulu.com^*.swf";
	line += "@@||ads.jetpackdigital.com.s3.amazonaws.com^$image,domain=vibe.com";
	line += "@@||ads.jetpackdigital.com/jquery.tools.min.js?$domain=vibe.com";
	line += "@@||ads.jetpackdigital.com^*/_uploads/$image,domain=vibe.com";
	line += "@@||ads.monster.com/html.ng/$background,image,subdocument,domain=monster.com";
	line += "@@||ads.morningstar.com/realmedia/ads/adstream_lx.ads/www.morningstar.com/video/$object_subrequest";
	line += "@@||ads.revsci.net/adserver/ako?$script,domain=foxbusiness.com|foxnews.com";
	line += "@@||ads.trutv.com/crossdomain.xml$object_subrequest";
	line += "@@||ads.trutv.com/html.ng/tile=*&site=trutv&tru_tv_pos=preroll&$object_subrequest";
	line += "@@||ads.yimg.com/ev/eu/any/$object";
	line += "@@||ads.yimg.com/ev/eu/any/vint/videointerstitial*.js";
	line += "@@||ads.yimg.com^*/any/yahoologo$image";
	line += "@@||ads.yimg.com^*/search/b/syc_logo_2.gif";
	line += "@@||ads.yimg.com^*videoadmodule*.swf";
	line += "@@||ads1.msn.com/ads/pronws/$image,domain=live.com";
	line += "@@||ads1.msn.com/library/dap.js$domain=msnbc.msn.com|wowarmory.com";
	line += "@@||adserver.bigwigmedia.com/ingamead3.swf";
	line += "@@||adserver.tvcatchup.com/crossdomain.xml$object_subrequest";
	line += "@@||adserver.tvcatchup.com/|$object_subrequest";
	line += "@@||adserver.yahoo.com/a?*&l=head&$script,domain=yahoo.com";
	line += "@@||adserver.yahoo.com/a?*=headr$script,domain=mail.yahoo.com";
	line += "@@||adtech.de/crossdomain.xml$object_subrequest,domain=deluxetelevision.com|gigwise.com|nelonen.fi|tv2.dk";
	line += "@@||app.promo.tubemogul.com/feed/placement.html?id=$script,domain=comedy.com";
	line += "@@||apple.com^*/video-ad.html";
	line += "@@||applevideo.edgesuite.net/admedia/*.flv";
	line += "@@||ar.atwola.com/file/adswrapper.js$script,domain=gasprices.mapquest.com";
	line += "@@||as.webmd.com/html.ng/transactionid=$object_subrequest";
	line += "@@||as.webmd.com/html.ng/transactionid=*&frame=$subdocument";
	line += "@@||assets.idiomag.com/flash/adverts/yume_$object_subrequest";
	line += "@@||att.com/images/*/admanager/";
	line += "@@||auctiva.com/listings/checkcustomitemspecifics.aspx?*&adtype=$script";
	line += "@@||autotrader.co.nz/data/adverts/$image";
	line += "@@||avclub.com/ads/av-video-ad/$xmlhttprequest";
	line += "@@||bing.com/images/async?q=$xmlhttprequest";
	line += "@@||bing.net/images/thumbnail.aspx?q=$image";
	line += "@@||bitgravity.com/revision3/swf/player/admanager.swf?$object_subrequest,domain=area5.tv";
	line += "@@||break.com/ads/preroll/$object_subrequest,domain=videosift.com";
	line += "@@||brothersoft.com/gads/coop_show_download.php?soft_id=$script";
	line += "@@||burotime.*/xml_*/reklam.xml$object_subrequest";
	line += "@@||campusfood.com/css/ad.css?";
	line += "@@||candystand.com/assets/images/ads/$image";
	line += "@@||cbs.com/sitecommon/includes/cacheable/combine.php?*/adfunctions.";
	line += "@@||cdn.last.fm/adserver/video/adroll/*/adroll.swf$domain=last.fm";
	line += "@@||cdn.springboard.gorillanation.com/storage/lightbox_code/static/companion_ads.js$domain=comingsoon.net|gamerevolution.com";
	line += "@@||channel4.com/media/scripts/oasconfig/siteads.js";
	line += "@@||chibis.adotube.com/appruntime/player/$object,object_subrequest";
	line += "@@||chloe.videogamer.com/data/*/videos/adverts/$object_subrequest";
	line += "@@||cisco.com/html.ng/site=cdc&concept=products$script";
	line += "@@||clustrmaps.com/images/clustrmaps-back-soon.jpg$third-party";
	line += "@@||cms.myspacecdn.com/cms/js/ad_wrapper*.js";
	line += "@@||cnet.com/ads/common/adclient/*.swf";
	line += "@@||creative.ak.fbcdn.net/ads3/creative/$image,domain=facebook.com";
	line += "@@||cubeecraft.com/openx/www/";
	line += "@@||dart.clearchannel.com/html.ng/$object_subrequest,domain=kissfm961.com|radio1045.com";
	line += "@@||deviantart.com/global/difi/?*&ad_frame=$subdocument";
	line += "@@||direct.fairfax.com.au/hserver/*/site=vid.*/adtype=embedded/$script";
	line += "@@||discovery.com/components/consolidate-static/?files=*/adsense-";
	line += "@@||disneyphotopass.com/adimages/";
	line += "@@||doubleclick.net/ad/*smartclip$script,domain=last.fm";
	line += "@@||doubleclick.net/adi/amzn.*;ri=digital-music-track;$subdocument";
	line += "@@||doubleclick.net/adi/dhd/homepage;sz=728x90;*;pos=top;$subdocument,domain=deadline.com";
	line += "@@||doubleclick.net/adj/*smartclip$script,domain=last.fm";
	line += "@@||doubleclick.net/adj/imdb2.consumer.video/*;sz=320x240,$script";
	line += "@@||doubleclick.net/adj/nbcu.nbc/videoplayer-$script";
	line += "@@||doubleclick.net/adj/pong.all/*;dcopt=ist;$script";
	line += "@@||doubleclick.net/pfadx/channel.video.crn/;*;cue=pre;$object_subrequest";
	line += "@@||doubleclick.net/pfadx/slate.v.video/*;cue=pre;$object_subrequest";
	line += "@@||doubleclick.net/pfadx/umg.*;sz=10x$script";
	line += "@@||doubleclick.net^*/adj/wwe.shows/ecw_ecwreplay;*;sz=624x325;$script";
	line += "@@||doubleclick.net^*/listen/*;sz=$script,domain=last.fm";
	line += "@@||doubleclick.net^*/ndm.tcm/video;$script,domain=player.video.news.com.au";
	line += "@@||doubleclick.net^*/videoplayer*=worldnow$subdocument,domain=ktiv.com|wflx.com";
	line += "@@||dstw.adgear.com/crossdomain.xml$domain=hot899.com|nj1015.com|streamtheworld.com";
	line += "@@||dstw.adgear.com/impressions/int/as=*.json?ag_r=$object_subrequest,domain=hot899.com|nj1015.com|streamtheworld.com";
	line += "@@||dyncdn.buzznet.com/catfiles/?f=dojo/*.googleadservices.$script";
	line += "@@||ebayrtm.com/rtm?rtmcmd&a=json&cb=parent.$script";
	line += "@@||edgar.pro-g.co.uk/data/*/videos/adverts/$object_subrequest";
	line += "@@||edmontonjournal.com/js/adsync/adsynclibrary.js";
	line += "@@||emediate.eu/crossdomain.xml$object_subrequest,domain=tv3play.se";
	line += "@@||emediate.eu/eas?cu_key=*;ty=playlist;$object_subrequest,domain=tv3play.se";
	line += "@@||emediate.se/eas_tag.1.0.js$domain=tv3play.se";
	line += "@@||espn.go.com^*/espn360/banner?$subdocument";
	line += "@@||eyewonder.com^$object,script,domain=last.fm";
	line += "@@||eyewonder.com^*/video/$object_subrequest,domain=last.fm";
	line += "@@||fdimages.fairfax.com.au^*/ffxutils.js$domain=thevine.com.au";
	line += "@@||feeds.videogamer.com^*/videoad.xml?$object_subrequest";
	line += "@@||fifa.com/flash/videoplayer/libs/advert_$object_subrequest";
	line += "@@||fwmrm.net/ad/p/1?$object_subrequest";
	line += "@@||fwmrm.net/crossdomain.xml$object_subrequest";
	line += "@@||gannett.gcion.com/addyn/3.0/*/adtech;alias=pluck_signin$script";
	line += "@@||garrysmod.org/ads/$background,image,script,stylesheet";
	line += "@@||go.com/dynamicvideoad?$object_subrequest,domain=disney.go.com";
	line += "@@||google.*/complete/search?$script";
	line += "@@||google.com/uds/?file=ads&$script,domain=guardian.co.uk";
	line += "@@||google.com/uds/api/ads/$script,domain=guardian.co.uk";
	line += "@@||gpacanada.com/img/sponsors/";
	line += "@@||gr.burstnet.com/crossdomain.xml$object_subrequest,domain=filefront.com";
	line += "@@||gstatic.com/images?q=$image";
	line += "@@||guim.co.uk^*/styles/wide/google-ads.css";
	line += "@@||gws.ign.com/ws/search?*&google_adpage=$script";
	line += "@@||hp.com/ad-landing/";
	line += "@@||huffingtonpost.com/images/v/etp_advert.png";
	line += "@@||i.cdn.turner.com^*/adserviceadapter.swf";
	line += "@@||i.real.com/ads/*.swf?clicktag=$domain=rollingstone.com";
	line += "@@||identity-us.com/ads/ads.html";
	line += "@@||ign.com/js.ng/size=headermainad&site=teamxbox$script,domain=teamxbox.com";
	line += "@@||ikea.com/ms/img/ads/";
	line += "@@||img.thedailywtf.com/images/ads/";
	line += "@@||img.weather.weatherbug.com^*/stickers/$background,image,stylesheet";
	line += "@@||imgag.com/product/full/el/adaptvadplayer.swf$domain=egreetings.com";
	line += "@@||imwx.com/js/adstwo/adcontroller.js$domain=weather.com";
	line += "@@||itv.com^*.adserver.js";
	line += "@@||itweb.co.za/banners/en-cdt*.gif";
	line += "@@||jdn.monster.com/render/adservercontinuation.aspx?$subdocument,domain=monster.com";
	line += "@@||jobs.wa.gov.au/images/advertimages/";
	line += "@@||js.revsci.net/gateway/gw.js?$domain=foxbusiness.com|foxnews.com";
	line += "@@||ksl.com/resources/classifieds/graphics/ad_";
	line += "@@||last.fm/ads.php?zone=*listen$subdocument";
	line += "@@||lightningcast.net/servlets/getplaylist?*&responsetype=asx&$object";
	line += "@@||live365.com/mini/blank300x250.html";
	line += "@@||live365.com/scripts/liveads.js";
	line += "@@||liverail.com/crossdomain.xml$object_subrequest";
	line += "@@||liverail.com/swf/*/plugins/flowplayer/";
	line += "@@||ltassrv.com/crossdomain.xml$object_subrequest,domain=animecrazy.net|gamepro.com";
	line += "@@||ltassrv.com/yume.swf$domain=animecrazy.net|gamepro.com";
	line += "@@||ltassrv.com/yume/yume_$object_subrequest,domain=animecrazy.net|gamepro.com";
	line += "@@||mads.cbs.com/mac-ad?$object_subrequest";
	line += "@@||mads.com.com/ads/common/faith/*.xml$object_subrequest";
	line += "@@||marines.com/videos/commercials/$object_subrequest";
	line += "@@||maxmind.com/app/geoip.js$domain=incgamers.com";
	line += "@@||media.abc.com/streaming/ads/preroll_$object_subrequest,domain=abc.go.com";
	line += "@@||media.monster.com/ads/$background,image,domain=monster.com";
	line += "@@||media.newjobs.com/ads/$background,image,object,domain=monster.com";
	line += "@@||media.salemwebnetwork.com/js/admanager/swfobject.js$domain=christianity.com";
	line += "@@||media.scanscout.com/ads/ss_ads3.swf$domain=failblog.org|icanhascheezburger.com";
	line += "@@||media.washingtonpost.com/wp-srv/ad/ad_v2.js";
	line += "@@||media.washingtonpost.com/wp-srv/ad/tiffany_manager.js";
	line += "@@||medrx.sensis.com.au/images/sensis/afl/util.js$domain=afl.com.au";
	line += "@@||meduniwien.ac.at/homepage/uploads/tx_macinabanners/$image";
	line += "@@||mercurial.selenic.com/images/sponsors/";
	line += "@@||mircscripts.org/advertisements.js";
	line += "@@||mlb.mlb.com/scripts/dc_ads.js";
	line += "@@||monster.com/services/bannerad.asmx/getadsrc$xmlhttprequest,domain=monster.com";
	line += "@@||mozilla.com/img/tignish/plugincheck/*/728_90/loading.png$domain=mozilla.com";
	line += "@@||msads.net/*.swf|$domain=msnbc.msn.com";
	line += "@@||msads.net/crossdomain.xml$object_subrequest,domain=msnbc.msn.com";
	line += "@@||msads.net^*.flv|$domain=msnbc.msn.com";
	line += "@@||mscommodin.webege.com/images/inicio/sponsors/$image";
	line += "@@||mxtabs.net/ads/interstitial$subdocument";
	line += "@@||newgrounds.com/ads/ad_medals.gif";
	line += "@@||newsarama.com/common/js/advertisements.js";
	line += "@@||newsweek.com/ads/adscripts/prod/*_$script";
	line += "@@||nick.com/js/ads.jsp";
	line += "@@||o.aolcdn.com/ads/adswrapper.js$domain=photos.tmz.com";
	line += "@@||oas.absoluteradio.co.uk/realmedia/ads/$object_subrequest";
	line += "@@||oas.bigflix.com/realmedia/ads/$object_subrequest";
	line += "@@||oas.five.tv/realmedia/ads/adstream_sx.ads/demand.five.tv/$object_subrequest";
	line += "@@||oascentral.feedroom.com/realmedia/ads/adstream_sx.ads/$script,domain=businessweek.com|economist.com|feedroom.com|stanford.edu";
	line += "@@||oascentral.surfline.com/realmedia/ads/adstream_sx.ads/www.surfline.com/articles$object_subrequest";
	line += "@@||objects.tremormedia.com/embed/js/$domain=bostonherald.com|deluxetelevision.com";
	line += "@@||objects.tremormedia.com/embed/swf/acudeoplayer.swf$domain=bostonherald.com|deluxetelevision.com";
	line += "@@||objects.tremormedia.com/embed/swf/admanager*.swf";
	line += "@@||objects.tremormedia.com/embed/xml/*.xml?r=$object_subrequest,domain=mydamnchannel.com";
	line += "@@||omgili.com/ads.search?";
	line += "@@||omnikool.discovery.com/realmedia/ads/adstream_mjx.ads/dsc.discovery.com/$script";
	line += "@@||onionstatic.com^*/videoads.js";
	line += "@@||pagead2.googlesyndication.com/pagead/ads?client=$subdocument,domain=artificialvision.com|metamodal.com|seeingwithsound.com";
	line += "@@||pagead2.googlesyndication.com/pagead/expansion_embed.js$domain=artificialvision.com|gameserver.n4cer.de|gpxplus.net|metamodal.com|myspace.com|seeingwithsound.com";
	line += "@@||pagead2.googlesyndication.com/pagead/scache/show_invideo_ads.js$domain=sciencedaily.com";
	line += "@@||pagead2.googlesyndication.com/pagead/show_ads.js$domain=articlewagon.com|artificialvision.com|gameserver.n4cer.de|gpxplus.net|metamodal.com|myspace.com|omegadrivers.net|seeingwithsound.com|spreadlink.us|warp2search.net";
	line += "@@||pagead2.googlesyndication.com/pagead/static?format=in_video_ads&$elemhide,subdocument";
	line += "@@||partner.googleadservices.com/gampad/google_ads.js$domain=avclub.com";
	line += "@@||partner.googleadservices.com/gampad/google_service.js$domain=avclub.com";
	line += "@@||partners.thefilter.com/crossdomain.xml$object_subrequest,domain=dailymotion.com|dailymotion.virgilio.it";
	line += "@@||partners.thefilter.com/dailymotionservice/$image,object_subrequest,script,domain=dailymotion.com|dailymotion.virgilio.it";
	line += "@@||pix04.revsci.net^*/pcx.js?$script,domain=foxbusiness.com|foxnews.com";
	line += "@@||pressdisplay.com/advertising/showimage.aspx?";
	line += "@@||promo2.tubemogul.com/adtags/slim_no_iframe.js$domain=comedy.com";
	line += "@@||promo2.tubemogul.com/flash/youtube.swf$domain=comedy.com";
	line += "@@||promo2.tubemogul.com/lib/tubemoguldisplaylib.js$domain=comedy.com";
	line += "@@||quit.org.au/images/images/ad/";
	line += "@@||redir.adap.tv/redir/client/adplayer.swf$domain=cracked.com|egreetings.com|ehow.com|imgag.com|videosift.com|xxlmag.com";
	line += "@@||redir.adap.tv/redir/client/static/as3adplayer.swf$domain=king5.com|newsinc.com|stickam.com|videosift.com|wkbw.com";
	line += "@@||redir.adap.tv/redir/javascript/adaptvadplayer.js$object_subrequest,domain=imgag.com";
	line += "@@||rosauers.com/locations/ads.html";
	line += "@@||rotate.infowars.com/www/delivery/fl.js";
	line += "@@||rotate.infowars.com/www/delivery/spcjs.php";
	line += "@@||sam.itv.com/xtserver/acc_random=*.video.preroll/seg=$object_subrequest";
	line += "@@||sankakucomplex.com^$script";
	line += "@@||sankakustatic.com^$script";
	line += "@@||scorecardresearch.com/beacon.js$domain=deviantart.com";
	line += "@@||search.excite.co.uk/minify.php?files*/css/feed/adsearch.css";
	line += "@@||seesaw.com/cp/c4/realmedia/ads/adstream_sx.ads/$xmlhttprequest";
	line += "@@||serve.vdopia.com/adserver/ad*.php$object_subrequest,script,xmlhttprequest";
	line += "@@||server.cpmstar.com/adviewas3.swf?contentspotid=$object_subrequest,domain=armorgames.com|freewebarcade.com|gamesforwork.com";
	line += "@@||server.cpmstar.com/view.aspx?poolid=$domain=newgrounds.com";
	line += "@@||sfx-images.mozilla.org^$image,domain=spreadfirefox.com";
	line += "@@||smartadserver.com/call/pubj/*/affiliate_id$script,domain=deezer.com";
	line += "@@||smartadserver.com/def/def/showdef.asp$domain=deezer.com";
	line += "@@||smartclip.net/delivery/tag?sid=$script,domain=last.fm";
	line += "@@||sonicstate.com/video/hd/hdconfig-geo.cfm?*/www/delivery/$object_subrequest";
	line += "@@||southparkstudios.com/layout/common/js/reporting/mtvi_ads_reporting.js";
	line += "@@||southparkstudios.com/layout/common/js/reporting/mtvi_ads_reporting_config.js";
	line += "@@||spotrails.com/crossdomain.xml$object_subrequest";
	line += "@@||spotrails.com^*/flowplayeradplayerplugin.swf";
	line += "@@||static.2mdn.net^*.xml$object_subrequest,domain=photoradar.com|youtube.com";
	line += "@@||static.ak.fbcdn.net^*/ads/$script";
	line += "@@||static.linkbucks.com^$script,stylesheet,domain=zxxo.net";
	line += "@@||static.scanscout.com/ads/are3.swf$domain=failblog.org|icanhascheezburger.com";
	line += "@@||superfundo.org/advertisement.js";
	line += "@@||telegraphcouk.skimlinks.com/api/telegraph.skimlinks.js";
	line += "@@||thefrisky.com/js/adspaces.min.js";
	line += "@@||thenewsroom.com^*/advertisement.xml$object_subrequest";
	line += "@@||theonion.com/ads/video-ad/$object_subrequest,xmlhttprequest";
	line += "@@||theonion.com^*/videoads.js";
	line += "@@||thestreet.com/js/ads/adplacer.js";
	line += "@@||timeinc.net/people/static/i/advertising/getpeopleeverywhere-*$background,domain=people.com|peoplestylewatch.com";
	line += "@@||timeinc.net^*/tii_ads.js$domain=ew.com";
	line += "@@||trutv.com/includes/banners/de/video/*.ad|$object_subrequest";
	line += "@@||turner.com^*/advertisement/cnnmoney_sponsors.gif$domain=money.cnn.com";
	line += "@@||tvgorge.com^*/adplayer.swf";
	line += "@@||tvnz.co.nz/stylesheets/tvnz/lib/js/advertisement.js";
	line += "@@||twitvid.com/mediaplayer_*.swf?";
	line += "@@||ultrabrown.com/images/adheader.jpg";
	line += "@@||upload.wikimedia.org/wikipedia/";
	line += "@@||utarget.co.uk/crossdomain.xml$object_subrequest,domain=tvcatchup.com";
	line += "@@||vancouversun.com/js/adsync/adsynclibrary.js";
	line += "@@||video-cdn.abcnews.com/ad_$object_subrequest";
	line += "@@||videoads.washingtonpost.com^$object_subrequest,domain=slatev.com";
	line += "@@||vidtech.cbsinteractive.com/plugins/*_adplugin.swf";
	line += "@@||vortex.accuweather.com/adc2004/pub/ads/js/ads-2006_vod.js";
	line += "@@||vox-static.liverail.com/swf/*/admanager.swf";
	line += "@@||vtstage.cbsinteractive.com/plugins/*_adplugin.swf";
	line += "@@||we7.com/api/streaming/advert-info?*&playsource=$object_subrequest";
	line += "@@||weather.com/common/a2/oasadframe.html?position=pagespon";
	line += "@@||weather.com/common/a2/oasadframe.html?position=pointspon";
	line += "@@||widget.slide.com^*/ads/*/preroll.swf";
	line += "@@||wikimedia.org^$elemhide";
	line += "@@||wikipedia.org^$elemhide";
	line += "@@||wrapper.teamxbox.com/a?size=headermainad&altlocdir=teamxbox$script";
	line += "@@||www.google.*/search?$subdocument";
	line += "@@||yallwire.com/pl_ads.php?$object_subrequest";
	line += "@@||yimg.com^*&yat/js/ads_";
	line += "@@||yimg.com^*/java/promotions/js/ad_eo_1.1.js";
	line += "@@||zedo.com/*.swf$domain=rajshri.com";
	line += "@@||zedo.com/*.xml$object_subrequest,domain=rajshri.com";
	line += "@@||zedo.com//$object_subrequest,domain=rajshri.com";
	line += "!Anti-Adblock";
	line += "@@/_468.gif$domain=seeingwithsound.com";
	line += "@@/_728.gif$domain=seeingwithsound.com";
	line += "@@/_728_90.$image,domain=seeingwithsound.com";
	line += "@@/_728x90.$image,domain=seeingwithsound.com";
	line += "@@_728by90.$image,domain=seeingwithsound.com";
	line += "@@||195.241.77.82^$image,domain=seeingwithsound.com";
	line += "@@||212.115.192.168^$image,domain=seeingwithsound.com";
	line += "@@||216.97.231.225^$domain=seeingwithsound.com";
	line += "@@||84.243.214.232^$image,domain=seeingwithsound.com";
	line += "@@||ads.clicksor.com/showad.php?*&adtype=7&$script,domain=rapid8.com";
	line += "@@||ads.gtainside.com/openx/ad.js";
	line += "@@||akihabaranews.com/images/ad/";
	line += "@@||artificialvision.com^$elemhide,image,script";
	line += "@@||arto.com/includes/js/adtech.de/script.axd/adframe.js?";
	line += "@@||avforums.com/forums/adframe.js";
	line += "@@||cinshare.com/js/embed.js?*=http://adserving.cpxinteractive.com/?";
	line += "@@||content.ytmnd.com/assets/js/a/adx.js";
	line += "@@||dailykos.com/ads/adblocker.blogads.css";
	line += "@@||dropbox.com^$image,script,domain=seeingwithsound.com";
	line += "@@||eq2flames.com/adframe.js";
	line += "@@||funkyfun.altervista.org/adsense.js$domain=livevss.net";
	line += "@@||gdataonline.com/exp/textad.js";
	line += "@@||googlepages.com^$image,script,domain=artificialvision.com|metamodal.com|seeingwithsound.com";
	line += "@@||gpxplus.net^$elemhide";
	line += "@@||hackers.co.id/adframe/adframe.js";
	line += "@@||home.tiscali.nl^$domain=seeingwithsound.com";
	line += "@@||livevss.net/adsense.js";
	line += "@@||lunarpages.com^$image,script,domain=artificialvision.com|metamodal.com|seeingwithsound.com";
	line += "@@||macobserver.com/js/adlink.js";
	line += "@@||metamodal.com^$elemhide,image,script";
	line += "@@||multi-load.com/peel.js$domain=multi-load.com";
	line += "@@||multiup.org/advertisement.js";
	line += "@@||ninjaraider.com/ads/$script";
	line += "@@||ninjaraider.com/adsense/$script";
	line += "@@||novamov.com/ads.js?*&ad_url=/adbanner";
	line += "@@||nwanime.com^$script";
	line += "@@||onlinevideoconverter.com/scripts/advertisement.js";
	line += "@@||pagead2.googlesyndication.com/pagead/render_ads.js$domain=seeingwithsound.com";
	line += "@@||photobucket.com^$image,domain=seeingwithsound.com";
	line += "@@||playtv.fr/img/design/adbg.jpg";
	line += "@@||pub.clicksor.net/newserving/js/show_ad.js$domain=rapid8.com";
	line += "@@||ratebeer.com/javascript/advertisement.js";
	line += "@@||seeingwithsound.cn^$image,script,domain=artificialvision.com|metamodal.com|seeingwithsound.com";
	line += "@@||seeingwithsound.com^$elemhide,image,script";
	line += "@@||serw.clicksor.com/newserving/getkey.php?$script,domain=rapid8.com";
	line += "@@||sharejunky.com/adserver/$script";
	line += "@@||sites.google.com/site/$image,script,domain=artificialvision.com|metamodal.com|seeingwithsound.com";
	line += "@@||sportsm8.com/adsense.js";
	line += "@@||spreadlink.us/advertisement.js";
	line += "@@||succesfactoren.nl^$image,domain=seeingwithsound.com";
	line += "@@||teknogods.com/advertisement.js";
	line += "@@||theteacherscorner.net/adiframe/$script";
	line += "@@||tpmrpg.net/adframe.js";
	line += "@@||video2mp3.net/img/ad*.js";
	line += "@@||visualprosthesis.com^$image,script,domain=artificialvision.com|metamodal.com|seeingwithsound.com";
	line += "@@||zshare.net/ads.js?*&ad_url=/adbanner";
	line += "!Non-English";
	line += "@@||24ur.com/adserver/adall.php?*&video_on_page=1";
	line += "@@||ads.globo.com/crossdomain.xml$object_subrequest";
	line += "@@||ads.globo.com/realmedia/ads/adstream_jx.ads/$object_subrequest,domain=globo.com";
	line += "@@||adser.localport.it/banman.asp?zoneid=71$subdocument";
	line += "@@||adswizz.com/www/delivery/$object_subrequest,domain=alloclips.com|video.belga.be";
	line += "@@||adtech.de/?adrawdata/3.0/*;|$object_subrequest,domain=nelonen.fi|tv2.dk";
	line += "@@||adtech.panthercustomer.com^*.flv$domain=tv3.ie";
	line += "@@||afterdark-nfs.com/ad/$background,image,script,stylesheet";
	line += "@@||aka-cdn-ns.adtech.de^*.flv$domain=tv3.ie";
	line += "@@||alimama.cn/taobaocdn/css/s8.css$domain=taobao.com";
	line += "@@||amarillas.cl/advertise.do?$xmlhttprequest";
	line += "@@||amarillas.cl/js/advertise/$script";
	line += "@@||autoscout24.*/all.js.aspx?m=css&*=/stylesheets/adbanner.css";
	line += "@@||banneradmin.rai.it/js.ng/sezione_rai=barramenu$script";
	line += "@@||bnrs.ilm.ee/www/delivery/fl.js";
	line += "@@||cpalead.com/mygateway.php?pub=$script,domain=serialnumber.in|spotifyripping.com|stumblehere.com|videodownloadx.com|yourpcmovies.net";
	line += "@@||e-planning.net/eb/*?*fvp=2&$object_subrequest,domain=clarin.com|emol.com";
	line += "@@||ebayrtm.com/rtm?$script,domain=annonces.ebay.fr|ebay.it";
	line += "@@||fotojorgen.no/images/*/webadverts/";
	line += "@@||hry.cz/ad/adcode.js";
	line += "@@||img.deniksport.cz/css/reklama.css?";
	line += "@@||mail.bg/mail/index/getads/$xmlhttprequest";
	line += "@@||nextmedia.com/admedia/$object_subrequest";
	line += "@@||ninjaraider.com^*/adsense.js";
	line += "@@||openx.motomedia.nl/live/www/delivery/$script";
	line += "@@||openx.zomoto.nl/live/www/delivery/fl.js";
	line += "@@||openx.zomoto.nl/live/www/delivery/spcjs.php?id=";
	line += "@@||pagead2.googlesyndication.com/pagead/abglogo/abg-da-100c-000000.png$domain=janno.dk|nielco.dk";
	line += "@@||ping.indieclicktv.com/www/delivery/ajs.php?zoneid=$object_subrequest,domain=penny-arcade.com";
	line += "@@||ring.bg/adserver/adall.php?*&video_on_page=1";
	line += "@@||static.mobile.eu^*/resources/images/ads/superteaser_$image,domain=automobile.fr|automobile.it|mobile.eu|mobile.ro";
	line += "@@||style.seznam.cz/ad/im.js";
	line += "@@||uol.com.br/html.ng/*&affiliate=$object_subrequest";
	line += "@@||video1.milanofinanza.it/movie/movie/adserver_$object,object_subrequest";
	line += "@@||virpl.ru^*_advert.php$xmlhttprequest,domain=virpl.ru";


            // Write file
            DWORD dwBytesWritten = 0;
            if (::WriteFile(hFile, line.GetBuffer(), line.GetLength(), &dwBytesWritten, NULL) && dwBytesWritten == line.GetLength())
            {
		        // Set correct version
                CPluginSettings* settings = CPluginSettings::GetInstance();

                settings->AddFilterUrl(CString(FILTERS_PROTOCOL) + CString(FILTERS_HOST) + "/easylist.txt", 1);
		        settings->Write();
            }
            else
            {
		        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_CREATE_FILE_WRITE, "Filter::Create - WriteFile");
            }

            // Close file
            if (!::CloseHandle(hFile))
            {
		        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_CREATE_FILE_CLOSE, "Filter::Create - CloseHandle");
	        }
	    }
	}
}

#endif // PRODUCT_SIMPLEADBLOCK


#ifdef PRODUCT_SIMPLEADBLOCK

bool CPluginFilter::IsAlive() const
{
    bool isAlive;

    s_criticalSectionFilterMap.Lock();
    {
        isAlive = !m_filterMap[0][0].empty();
    }
    s_criticalSectionFilterMap.Unlock();

    return isAlive;
}

#endif
