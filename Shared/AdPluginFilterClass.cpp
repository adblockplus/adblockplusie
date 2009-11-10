#include "AdPluginStdAfx.h"

#include "AdPluginFilterClass.h"

#if (defined PRODUCT_ADBLOCKER)
 #include "AdPluginSettings.h"
 #include "AdPluginClient.h"
 #include "AdPluginClientFactory.h"
#endif

#include "AdPluginMutex.h"


#if (defined PRODUCT_ADBLOCKER)

class CAdPluginFilterLock : public CAdPluginMutex
{

private:

    static CComAutoCriticalSection s_criticalSectionFilterLock;

public:

    CAdPluginFilterLock(const CStringA& filterFile) : CAdPluginMutex("FilterFile" + CString(filterFile), PLUGIN_ERROR_MUTEX_FILTER_FILE)
    {
        s_criticalSectionFilterLock.Lock();
    }

    ~CAdPluginFilterLock()
    {
        s_criticalSectionFilterLock.Unlock();
    }
};

CComAutoCriticalSection CAdPluginFilterLock::s_criticalSectionFilterLock;

#endif


// The filters are described at http://adblockplus.org/en/filters

CComAutoCriticalSection CAdPluginFilter::s_criticalSectionFilterMap;


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

CFilterElementHide::CFilterElementHide(const CStringA& filterText, const CStringA& filterFile) : m_filterText(filterText), m_filterFile(filterFile)
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
// CAdPluginFilter
// ============================================================================

CAdPluginFilter::CAdPluginFilter(const TFilterFileList& list, const CStringA& dataPath) : m_dataPath(dataPath)
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


CAdPluginFilter::CAdPluginFilter(const CStringA& dataPath) : m_dataPath(dataPath)
{
}


bool CAdPluginFilter::AddFilterElementHide(CStringA filterText, CStringA filterFile)
{
    int delimiterPos = filterText.Find("#");
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

        CStringA filterDomains = filterText.Left(delimiterPos).MakeLower();
        CStringA filterString  = filterText.Right(filterText.GetLength() - delimiterPos - (isOldFormat ? 1 : 2));

        bool isDomainSpecific = delimiterPos > 0 && filterDomains.Find('~') < 0;
        
        // Add not-domains to filter
        if (!isDomainSpecific && delimiterPos > 0)
        {
            int endPos = 0;

            while ((endPos = filterDomains.Find(',')) >= 0 || !filterDomains.IsEmpty())
	        {
                CStringA domain;

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
        CStringA tag;

        char firstTag = filterString.GetAt(0);
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
            int pos = filterString.FindOneOf(".#[(");
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
        CStringA id;
        CStringA className;
        
        // In old format, id/class is part of attributes
        if (isOldFormat == false && !filterString.IsEmpty())
        {
            char firstId = filterString.GetAt(0);
            
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

                CStringA arg = filterString.Mid(startPos + 1, endPos - startPos - 1);

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
                    
                    if (CStringA(attrSelector.m_bstrAttr) == "style")
                    {
                        attrSelector.m_isStyle = true;
                        attrSelector.m_value.MakeLower();
                    }
                    else if (CStringA(attrSelector.m_bstrAttr) == "id")
                    {
                        attrSelector.m_isId = true;
                    }
                    else if (CStringA(attrSelector.m_bstrAttr) == "class")
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
                CStringA domain;

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


bool CAdPluginFilter::IsMatchFilterElementHide(const CFilterElementHide& filter, IHTMLElement* pEl, const CStringA& domain) const
{
    bool isHidden = true;

    // Check is not domains
    if (!filter.m_domainsNot.empty())
    {
        for (std::set<CStringA>::const_iterator it = filter.m_domainsNot.begin(); isHidden && it != filter.m_domainsNot.end(); ++it)
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
        CStringA style;
        
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

            CStringA value;

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
                    value.Format("%u", vAttr.iVal);
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


bool CAdPluginFilter::IsElementHidden(const CStringA& tag, IHTMLElement* pEl, const CStringA& domain, const CStringA& indent) const
{
    bool isHidden = false;

    CStringA id;
    CComBSTR bstrId;
    if (SUCCEEDED(pEl->get_id(&bstrId)) && bstrId)
    {
        id = bstrId;
    }

    CStringA classNames;
    CComBSTR bstrClassNames;
    if (SUCCEEDED(pEl->get_className(&bstrClassNames)) && bstrClassNames)
    {
        classNames = bstrClassNames;
    }

    s_criticalSectionFilterMap.Lock();
    {
        CStringA domainTest = domain;
        
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
                                    CAdPluginDebug::DebugResultHiding(tag, "id:" + id + " class:" + classNames, filterIt->second.m_filterText, filterIt->second.m_filterFile);
                                }
                                else if (!id.IsEmpty())
                                {
                                    DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                                    CAdPluginDebug::DebugResultHiding(tag, "id:" + id, filterIt->second.m_filterText, filterIt->second.m_filterFile);
                                }
                                else if (!classNames.IsEmpty())
                                {
                                    DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                                    CAdPluginDebug::DebugResultHiding(tag, "class:" + classNames, filterIt->second.m_filterText, filterIt->second.m_filterFile);
                                }
                                else
                                {
                                    DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                                    CAdPluginDebug::DebugResultHiding(tag, "-" + classNames, filterIt->second.m_filterText, filterIt->second.m_filterFile);
                                }
                            }
#endif
                        }

                        // Iterate through class names
                        if (isHidden == false && !classNames.IsEmpty() && !filterIt->second.m_tagClassName.IsEmpty() && classNames.Find(' ') > 0)
                        {
                            int pos = 0;
                            CStringA className = classNames.Tokenize(" \t\n\r", pos);

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
                                            CAdPluginDebug::DebugResultHiding(tag, "id:" + id + " class:" + classNames, filterIt->second.m_filterText, filterIt->second.m_filterFile);
                                        }
                                        else
                                        {
                                            DEBUG_HIDE_EL(indent + "HideEl::Found (domain) filter:" + filterIt->second.m_filterText)
                                            CAdPluginDebug::DebugResultHiding(tag, "-" + classNames, filterIt->second.m_filterText, filterIt->second.m_filterFile);
                                        }
                                    }
#endif
                                }

                                // Next class name
                                className = classNames.Tokenize(" \t\n\r", pos);
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
                    CAdPluginDebug::DebugResultHiding(tag, "id:" + id, idIt->second.m_filterText, idIt->second.m_filterFile);
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
                        CAdPluginDebug::DebugResultHiding(tag, "id:" + id, idIt->second.m_filterText, idIt->second.m_filterFile);
                    }
#endif
                }
            }
        }

        // Search tag/className filters
        if (isHidden == false && !classNames.IsEmpty())
        {
            int pos = 0;
            CStringA className = classNames.Tokenize(" \t\n\r", pos);

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
                        CAdPluginDebug::DebugResultHiding(tag, "class:" + className, classIt->second.m_filterText, classIt->second.m_filterFile);
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
                            CAdPluginDebug::DebugResultHiding(tag, "class:" + className, classIt->second.m_filterText, classIt->second.m_filterFile);
                        }
#endif
                    }
                }

                // Next class name
                className = classNames.Tokenize(" \t\n\r", pos);
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
                    CAdPluginDebug::DebugResultHiding(tag, "-", tagIt->second.m_filterText, tagIt->second.m_filterFile);
                }
#endif
            }
        }
    }
    s_criticalSectionFilterMap.Unlock();

    return isHidden;
}


void CAdPluginFilter::AddFilter(CStringA filterString, CStringA filterFile, int filterType)
{
    CStringA raw = filterString;
    
	// Here we should find a key for the filter
	// We find a string of max 8 characters that does not contain any wildcards and which are unique for the filter

    // Find settings part, identified by $
    CStringA filterSettings;

    int pos = filterString.Find('$');
    if (pos > 0)
    {
        filterSettings = filterString.Right(filterString.GetLength() - pos - 1);
        filterString = filterString.Left(pos);
    }

    // Split filterString to parts

    bool bCheckFromStartDomain = false;
    if (filterString.Find("||") == 0)
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

    std::vector<CStringA> filterParts;
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

    CStringA filterPart = filterParts[0];
    filterPart.MakeLower();

    int nFilterParts = filterParts.size();
    int nFilterPart = 0;

    int filterPartLength = filterPart.GetLength();

    if (filterPartLength >= 7)
    {
        if (filterPart.Find("http://") == 0)
        {
            startCharacter = 7;
        }
        else if (filterPart.Find("https://") == 0)
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
            if (filterPart.Find("http://") == 0)
            {
                startCharacter = 7;
            }
            else if (filterPart.Find("https://") == 0)
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
            CStringA setting = pos >= 0 ? filterSettings.Left(pos) : filterSettings;
            filterSettings = pos >= 0 ? filterSettings.Right(filterSettings.GetLength() - pos - 1) : "";

            // Is content type negated
            bool bNegate = false;
            if (setting.GetAt(0) == '~')
            {
                bNegate = true;
                setting = setting.Right(setting.GetLength() - 1);
            }

            // Apply content type
            std::map<CStringA, int>::iterator it = m_contentMap.find(setting);
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
                    CStringA domain = posDomain >= 0 ? setting.Left(posDomain) : setting;
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

#ifdef PRODUCT_ADBLOCKER

bool CAdPluginFilter::DownloadFilterFile(const CStringA& url, const CStringA& filename)
{
    CStringA tempFile = CAdPluginSettings::GetTempFile(TEMP_FILE_PREFIX);

    DEBUG_GENERAL("*** Downloading filter file:" + filename + " (to " + tempFile + ")");

    bool bResult = !tempFile.IsEmpty();
    if (bResult)
    {
	    // if new filter urls are found download them and update the persistent data
	    HRESULT hr = ::URLDownloadToFileA(NULL, url, tempFile, 0, NULL);
        if (SUCCEEDED(hr))
        {
            CAdPluginFilterLock lock(filename);
            if (lock.IsLocked())
            {
                // Move the temporary file to the new text file.
                if (!::MoveFileExA(tempFile, CAdPluginSettings::GetDataPath(filename), MOVEFILE_REPLACE_EXISTING))
                {
                    DWORD dwError = ::GetLastError();

                    // Not same device? copy/delete instead
                    if (dwError == ERROR_NOT_SAME_DEVICE)
                    {
                        if (!::CopyFileA(tempFile, CAdPluginSettings::GetDataPath(filename), FALSE))
                        {
                            DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_COPY_FILE, "Filter::Unable to copy file:" + filename)

                            bResult = false;
                        }

                        ::DeleteFileA(tempFile);
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

bool CAdPluginFilter::ReadFilter(const CStringA& filename, const CStringA& downloadPath)
{
    bool isRead = false;

#ifdef PRODUCT_ADBLOCKER
    LocalClient* client = CAdPluginClientFactory::GetLazyClientInstance();
    if (client)
#endif
	{
        CStringA fileContent;

#ifdef PRODUCT_ADBLOCKER
        CAdPluginFilterLock lock(filename);
        if (lock.IsLocked())
        {
#endif
	        DEBUG_GENERAL("*** Loading filter:" + m_dataPath + filename);

            // Read file
            HANDLE hFile = ::CreateFileA(m_dataPath + filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);  
            if (hFile == INVALID_HANDLE_VALUE)
            {
                DWORD dwError = ::GetLastError();
#ifdef PRODUCT_ADBLOCKER
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
                        HANDLE hPersonalFile = ::CreateFileA(CAdPluginSettings::GetDataPath(PERSONAL_FILTER_FILE), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);  
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

#ifdef PRODUCT_ADBLOCKER
        }
#endif
        if (isRead)
        {
            // Parse file string
            int pos = 0;
            CStringA filter = fileContent.Tokenize("\n\r", pos);
        
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
					    if (filter.Find("@@") == 0)
					    {
						    filterType = CFilter::filterTypeWhiteList;

						    filter.Delete(0, 2);
					    }
					    // If a filter contains ## then it is a element hiding rule
					    else if (filter.Find("#") >= 0)
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

                    filter = fileContent.Tokenize("\n\r", pos);
			    }
		    }
		    s_criticalSectionFilterMap.Unlock();
        } 
	} // client

    return isRead;
}

void CAdPluginFilter::ParseFilters(const TFilterFileList& list)
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
#ifdef PRODUCT_ADBLOCKER
    LocalClient* client = CAdPluginClientFactory::GetLazyClientInstance();
    if (client)
#endif
	{
		for (TFilterFileList::const_iterator it = list.begin(); it != list.end(); ++it) 
		{
		    ReadFilter(it->first, it->second);
	    }

        ReadFilter(PERSONAL_FILTER_FILE);

#ifdef ENABLE_DEBUG_SELFTEST
        CStringA sCount;
        s_criticalSectionFilterMap.Lock();
        {
            sCount.Format("Block:%d/%d - BlockDef:%d - White:%d - WhiteDef:%d - Hide:%d/%d", m_filterMap[0][0].size(), m_filterMap[0][1].size(), m_filterMapDefault[0].size(), m_filterMap[1][0].size(), m_filterMapDefault[1].size(), m_elementHideTags.size() + m_elementHideTagsClass.size() + m_elementHideTagsId.size(), m_elementHideDomains.size());
        }
        s_criticalSectionFilterMap.Unlock();
        DEBUG_GENERAL("*** Filter count:" + sCount);
#endif
	} // client
}


bool CAdPluginFilter::IsMatchFilter(const CFilter& filter, CStringA src, const CStringA& srcDomain, const CStringA& domain) const
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

        for (std::set<CStringA>::const_iterator it = filter.m_domains.begin(); !bFound && it != filter.m_domains.end(); ++it)
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
        for (std::set<CStringA>::const_iterator it = filter.m_domainsNot.begin(); it != filter.m_domainsNot.end(); ++it)
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


const CFilter* CAdPluginFilter::MatchFilter(int filterType, const CStringA& src, int contentType, const CStringA& domain) const
{
	const CFilter* filter = NULL;

	int startCharacter = 0;
	int keyLength = 4;

    CStringA srcLower = src;
    srcLower.MakeLower();
    int srcLowerLength = srcLower.GetLength();

    // Extract src domain
    DWORD length = 2048;
    CStringA srcDomain;
    
    if (SUCCEEDED(::UrlGetPartA(src, srcDomain.GetBufferSetLength(2048), &length, URL_PART_HOSTNAME, 0)))
    {
        srcDomain.ReleaseBuffer();

        if (srcDomain.Left(4) == "www.")
        {
            srcDomain = srcDomain.Right(srcDomain.GetLength() - 4);
        }
        else if (srcDomain.Left(5) == "www2." || srcDomain.Left(5) == "www3.")
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
            if (srcLower.Find("http://") == 0)
            {
                startCharacter = 7;
            }
            else if (srcLower.Find("https://") == 0)
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
                if (srcLower.Find("http://") == 0)
                {
                    startCharacter = 7;
                }
                else if (srcLower.Find("https://") == 0)
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


bool CAdPluginFilter::ShouldWhiteList(CStringA src) const
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


bool CAdPluginFilter::ShouldBlock(CStringA src, int contentType, const CStringA& domain, bool addDebug) const
{
    // We should not block the empty string, so all filtering does not make sense
	// Therefore we just return
	if (src.Trim().IsEmpty())
	{
		return false;
	}

	CStringA type;
	if (addDebug)
	{
	    type = "???";
	    
	    std::map<int,CStringA>::const_iterator it = m_contentMapText.find(contentType);
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
            CAdPluginDebug::DebugResultBlocking(type, src, blockFilter->m_filterText, blockFilter->m_filterFile);
#endif
		}
	}
	else if (addDebug)
	{
		DEBUG_FILTER("Filter::ShouldBlock " + type + " NO  src:" + src)
	}

	return blockFilter ? true : false;
}

int CAdPluginFilter::FindMatch(const CStringA& src, CStringA filterPart, int srcStartPos) const
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
        CStringA test = filterCurrentPos >= 0 ? filterPart.Left(filterCurrentPos) : filterPart;
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

bool CAdPluginFilter::IsSpecialChar(char testChar) const
{
    if (isalnum(testChar) || testChar == '.' || testChar == '-' || testChar == '%')
    {
        return false;
    }

    return true;
}

bool CAdPluginFilter::IsSubdomain(const CStringA& subdomain, const CStringA& domain) const
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

#ifdef PRODUCT_ADBLOCKER

void CAdPluginFilter::CreateFilters()
{
    CAdPluginFilterLock lock("easylist.txt");
    if (lock.IsLocked())
    {
        // Check file existence
        std::ifstream is;
	    is.open(CAdPluginSettings::GetDataPath("easylist.txt"), std::ios_base::in);
	    if (is.is_open())
	    {
            is.close();
            return;
        }

        // Open file
        HANDLE hFile = ::CreateFileA(CAdPluginSettings::GetDataPath("easylist.txt"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);  
        if (hFile == INVALID_HANDLE_VALUE)
        {
		    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_CREATE_FILE_OPEN, "Filter::Create - CreateFile");
        }
        else
        {
            // Build filter string
            CStringA line;
            
		    line += "[Adblock Plus 1.0.2]\r\n";
		    line += "! Checksum: fYLzkO02cKTpYXnz0IE2sg\r\n";
		    line += "!\r\n";
		    line += "! Rick752's EasyList - Global English ad blocking subscription\r\n";
		    line += "! https://easylist.adblockplus.org/\r\n";
		    line += "! License: http://creativecommons.org/licenses/by-sa/3.0/\r\n";
		    line += "! Last modified: 27 Jul 2009  0:30 UTC\r\n";
		    line += "! Expires: 5 days (updates automatically every 5 days)\r\n";
		    line += "!\r\n";
		    line += "! Please report unblocked ads or mistakenly blocked content:\r\n";
		    line += "! Forum: http://forums.lanik.us/\r\n";
		    line += "! E-Mail: ares2mail -at- gmail.com\r\n";
		    line += "!\r\n";
		    line += "!-------------------------Ad blocking rules--------------------------!\r\n";
		    line += ".1100i.com/\r\n";
		    line += ".188server.com/\r\n";
		    line += ".247realmedia.com/\r\n";
		    line += ".2mdn.net/\r\n";
		    line += ".360ads.com/\r\n";
		    line += ".43plc.com/\r\n";
		    line += ".600z.com/\r\n";
		    line += ".abmr.net/\r\n";
		    line += ".about.com/0g/$subdocument\r\n";
		    line += ".accuserveadsystem.com/\r\n";
		    line += ".acronym.com/\r\n";
		    line += ".ad-flow.com/\r\n";
		    line += ".ad20.net/\r\n";
		    line += ".adaction.se/\r\n";
		    line += ".adbard.net/\r\n";
		    line += ".adblade.com/\r\n";
		    line += ".adbrite.com/\r\n";
		    line += ".adbureau.net/\r\n";
		    line += ".adbutler.com/\r\n";
		    line += ".adcde.com/\r\n";
		    line += ".adcentriconline.com/\r\n";
		    line += ".adchap.com/\r\n";
		    line += ".adecn.com/\r\n";
		    line += ".adengage.com/\r\n";
		    line += ".adf01.net/\r\n";
		    line += ".adfactory88.com/\r\n";
		    line += ".adfrontiers.com/\r\n";
		    line += ".adfusion.com/\r\n";
		    line += ".adgardener.com/\r\n";
		    line += ".adgine.net/\r\n";
		    line += ".adgroups.com/\r\n";
		    line += ".adhese.be/\r\n";
		    line += ".adhese.net/\r\n";
		    line += ".adicate.com/\r\n";
		    line += ".adinterax.com/\r\n";
		    line += ".adireland.com/\r\n";
		    line += ".adisn.com/\r\n";
		    line += ".adition.com/\r\n";
		    line += ".adjug.com/\r\n";
		    line += ".adjuggler.com/$domain=~videodetective.com\r\n";
		    line += ".adlink.net/\r\n";
		    line += ".admarketplace.net/\r\n";
		    line += ".adnet.biz/\r\n";
		    line += ".adnet.com/\r\n";
		    line += ".adnet.ru/\r\n";
		    line += ".adnxs.com/\r\n";
		    line += ".adocean.pl/\r\n";
		    line += ".adoperator.com/\r\n";
		    line += ".adotube.com/\r\n";
		    line += ".adpinion.com/\r\n";
		    line += ".adpionier.de/\r\n";
		    line += ".adsdk.com/\r\n";
		    line += ".adserver.yahoo.com/\r\n";
		    line += ".adservinginternational.com/\r\n";
		    line += ".adsforindians.com/\r\n";
		    line += ".adshopping.com/\r\n";
		    line += ".adshuffle.com/\r\n";
		    line += ".adsmarket.com/\r\n";
		    line += ".adsonar.com/\r\n";
		    line += ".adspeed.com/\r\n";
		    line += ".adtology3.com/\r\n";
		    line += ".adtoma.com/\r\n";
		    line += ".adtrgt.com/\r\n";
		    line += ".adtrix.com/\r\n";
		    line += ".adultadworld.com/\r\n";
		    line += ".adultfriendfinder.com/banners/\r\n";
		    line += ".adversalservers.com/\r\n";
		    line += ".adverserve.net/\r\n";
		    line += ".advertarium.com.ua/\r\n";
		    line += ".adverticum.net/\r\n";
		    line += ".advertising-department.com/\r\n";
		    line += ".advertising.com/\r\n";
		    line += ".advertlets.com/\r\n";
		    line += ".advertserve.com/\r\n";
		    line += ".advg.jp/\r\n";
		    line += ".adviva.net/\r\n";
		    line += ".adxpower.com/\r\n";
		    line += ".afcyhf.com/\r\n";
		    line += ".affiliate.com/\r\n";
		    line += ".affiliatefuel.com/\r\n";
		    line += ".affiliatefuture.com/\r\n";
		    line += ".affiliatesensor.com/\r\n";
		    line += ".affiliproducts.com/\r\n";
		    line += ".affinity.com/\r\n";
		    line += ".agentcenters.com/\r\n";
		    line += ".aggregateknowledge.com/\r\n";
		    line += ".aim4media.com/\r\n";
		    line += ".alimama.cn/\r\n";
		    line += ".allthelyrics.com:*/popup.js\r\n";
		    line += ".alphagodaddy.com/\r\n";
		    line += ".amgdgt.com\r\n";
		    line += ".anrdoezrs.net/\r\n";
		    line += ".arcadebanners.com/\r\n";
		    line += ".arti-mediagroup.com/\r\n";
		    line += ".as5000.com/\r\n";
		    line += ".aspx?zoneid=*&task=$domain=~juventus.com\r\n";
		    line += ".assoc-amazon.com/\r\n";
		    line += ".atdmt.com/\r\n";
		    line += ".atwola.com/$domain=~gasprices.mapquest.com|~photos.tmz.com\r\n";
		    line += ".au/_ads/\r\n";
		    line += ".au/ads/\r\n";
		    line += ".audienceprofiler.com/\r\n";
		    line += ".auspipe.com/\r\n";
		    line += ".avads.co.uk/\r\n";
		    line += ".awaps.net/\r\n";
		    line += ".awin1.com/\r\n";
		    line += ".awltovhc.com/\r\n";
		    line += ".axill.com/\r\n";
		    line += ".azads.com/\r\n";
		    line += ".azjmp.com/\r\n";
		    line += ".azlyrics.com/*_az.js\r\n";
		    line += ".azoogleads.com/\r\n";
		    line += ".bannerbank.ru/\r\n";
		    line += ".bannerconnect.com/\r\n";
		    line += ".bannerconnect.net/\r\n";
		    line += ".bannersmania.com/\r\n";
		    line += ".bbelements.com/\r\n";
		    line += ".begun.ru/\r\n";
		    line += ".belointeractive.com/\r\n";
		    line += ".bestofferdirect.com/\r\n";
		    line += ".bfast.com/\r\n";
		    line += ".bidvertiser.com/\r\n";
		    line += ".bimedia.net/video/$object\r\n";
		    line += ".bit-tech.net/style/skin/$stylesheet\r\n";
		    line += ".blogad.com.tw/\r\n";
		    line += ".blogads.com/\r\n";
		    line += ".bloggerads.net/\r\n";
		    line += ".bloomberg.com/jscommon/banner.js\r\n";
		    line += ".bluekai.com/\r\n";
		    line += ".bluestreak.com/\r\n";
		    line += ".bmanpn.com/\r\n";
		    line += ".bnetworx.com/\r\n";
		    line += ".br/ads/\r\n";
		    line += ".bravenetmedianetwork.com/\r\n";
		    line += ".break.com/*-ad.html\r\n";
		    line += ".bridgetrack.com/\r\n";
		    line += ".btrll.com/\r\n";
		    line += ".burstnet.com/\r\n";
		    line += ".buysellads.com/\r\n";
		    line += ".buzzparadise.com/\r\n";
		    line += ".c-on-text.com/\r\n";
		    line += ".c8.net.ua/\r\n";
		    line += ".ca/ads/\r\n";
		    line += ".captainad.com/\r\n";
		    line += ".casalemedia.com/\r\n";
		    line += ".cc-dt.com/\r\n";
		    line += ".cdmediaworld.com*/!\r\n";
		    line += ".cgecwm.org/\r\n";
		    line += ".checkm8.com/\r\n";
		    line += ".checkmystats.com.au/\r\n";
		    line += ".checkoutfree.com/$third-party\r\n";
		    line += ".chitika.net/\r\n";
		    line += ".ciao.co.uk/load_file.php?\r\n";
		    line += ".cjt1.net/\r\n";
		    line += ".clash-media.com/\r\n";
		    line += ".claxonmedia.com/\r\n";
		    line += ".clickad.pl/\r\n";
		    line += ".clickbooth.com/\r\n";
		    line += ".clickexperts.net/\r\n";
		    line += ".clickintext.net/\r\n";
		    line += ".clickthrucash.com/\r\n";
		    line += ".clixgalore.com/\r\n";
		    line += ".co.uk/ads.pl\r\n";
		    line += ".codeproject.com/script/adm/\r\n";
		    line += ".cogsdigital.com/\r\n";
		    line += ".collective-media.net/\r\n";
		    line += ".com/2cpu/*side*.html\r\n";
		    line += ".com.com/*prompu300\r\n";
		    line += ".com/ads.pl\r\n";
		    line += ".com/ads/*$domain=~adhack.com|~cnet.com|~espn.go.com|~nbc.com|~newgrounds.com|~newsweek.com|~photos.tmz.com|~rollingstone.com\r\n";
		    line += ".com/adx/\r\n";
		    line += ".com/sideads|\r\n";
		    line += ".com/topads|\r\n";
		    line += ".commission-junction.com/\r\n";
		    line += ".commissionmonster.com/\r\n";
		    line += ".computerworld.com/*/jobroll/\r\n";
		    line += ".connextra.com/\r\n";
		    line += ".consumerreports.org/cro/resources/js/sx.js\r\n";
		    line += ".contextuads.com/\r\n";
		    line += ".contextweb.com/\r\n";
		    line += ".covertarget.com/*_*.php$subdocument\r\n";
		    line += ".cpaclicks.com/\r\n";
		    line += ".cpalead.com/\r\n";
		    line += ".cpays.com/\r\n";
		    line += ".cpmstar.com/\r\n";
		    line += ".cpuim.com/\r\n";
		    line += ".cpxinteractive.com/\r\n";
		    line += ".crispads.com/\r\n";
		    line += ".crowdgravity.com/\r\n";
		    line += ".cubics.com/\r\n";
		    line += ".decisionmark.com/\r\n";
		    line += ".decisionnews.com/\r\n";
		    line += ".deepmetrix.com/\r\n";
		    line += ".demonoid.com/cached/*.html?r=\r\n";
		    line += ".destinationurl.com/\r\n";
		    line += ".dgmaustralia.com/\r\n";
		    line += ".dl-rms.com/\r\n";
		    line += ".domainsponsor.com/\r\n";
		    line += ".doubleclick.net/adi/\r\n";
		    line += ".doubleclick.net/adj/$domain=~sbs.com.au\r\n";
		    line += ".doubleclick.net/*;sz=$object_subrequest,other,domain=digitaltrends.com|heavy.com|myspace.com|nbc.com\r\n";
		    line += ".doubleclick.net/pfadx/*.mtvi/$object_subrequest,other\r\n";
		    line += ".dpbolvw.net/\r\n";
		    line += ".dynw.com/banner\r\n";
		    line += ".earthlink.net/*/promos/\r\n";
		    line += ".ebayobjects.com/\r\n";
		    line += ".ebayrtm.com/rtm?rtmcmd&a=json*&c=1\r\n";
		    line += ".edge.ru4.com/\r\n";
		    line += ".egamingonline.com/\r\n";
		    line += ".emediate.eu/\r\n";
		    line += ".emediate.se/\r\n";
		    line += ".enticelabs.com/el/\r\n";
		    line += ".episodic.com/*/logos/player-\r\n";
		    line += ".etology.com/\r\n";
		    line += ".euroclick.com/\r\n";
		    line += ".exponential.com/\r\n";
		    line += ".exelator.com/\r\n";
		    line += ".excite.com/gca_iframe.html?\r\n";
		    line += ".exitexplosion.com/\r\n";
		    line += ".eyereturn.com/\r\n";
		    line += ".eyewonder.com/\r\n";
		    line += ".fairfax.com.au/$~stylesheet\r\n";
		    line += ".falkag.net/\r\n";
		    line += ".fastclick.net/\r\n";
		    line += ".feedburner.com/~a/\r\n";
		    line += ".fimserve.com/\r\n";
		    line += ".firstadsolution.com/\r\n";
		    line += ".firstlightera.com/\r\n";
		    line += ".fixionmedia.com/\r\n";
		    line += ".fluxads.com/\r\n";
		    line += ".flyordie.com/games/free/b/*--?p=\r\n";
		    line += ".fmpub.net/\r\n";
		    line += ".forrestersurveys.com/\r\n";
		    line += ".friendlyduck.com/\r\n";
		    line += ".ftjcfx.com/\r\n";
		    line += ".fudzilla.com/*/banners/\r\n";
		    line += ".funklicks.com/\r\n";
		    line += ".fwmrm.net/*.flv\r\n";
		    line += ".fwmrm.net/ad/*&\r\n";
		    line += ".g.doubleclick.net/\r\n";
		    line += ".game-advertising-online.com/\r\n";
		    line += ".gameads.com/\r\n";
		    line += ".gamecetera.com/\r\n";
		    line += ".gamecopyworld.com*/!\r\n";
		    line += ".gamersbanner.com/\r\n";
		    line += ".gametrailers.com/css/gt6_siteskin_$stylesheet\r\n";
		    line += "||gannett.gcion.com^$third-party\r\n";
		    line += ".geocities.com/js_source/\r\n";
		    line += ".geocities.yahoo.*/js/sq.\r\n";
		    line += ".geopromos.com/\r\n";
		    line += ".gestionpub.com/\r\n";
		    line += ".getprice.com.au/searchwidget.aspx?$subdocument\r\n";
		    line += ".gklmedia.com/\r\n";
		    line += ".globaladsales.com/\r\n";
		    line += ".goauto.com.au/mellor/mellor.nsf/toy$subdocument\r\n";
		    line += ".googleadservices.com/\r\n";
		    line += ".grabmyads.com/\r\n";
		    line += ".gumgum.com/\r\n";
		    line += ".gwinnettdailypost.com/1.iframe.asp?\r\n";
		    line += ".havamedia.net/\r\n";
		    line += ".hit-now.com/\r\n";
		    line += ".hopfeed.com/\r\n";
		    line += ".hosticanaffiliate.com/\r\n";
		    line += ".httpool.com/\r\n";
		    line += ".hummy.org.uk/*/brotator/\r\n";
		    line += ".hypemakers.net/\r\n";
		    line += ".hypervre.com/\r\n";
		    line += ".i.com.com/*/ads/\r\n";
		    line += ".i.com.com/*/vendor_bg_\r\n";
		    line += ".ic-live.com/\r\n";
		    line += ".icdirect.com/\r\n";
		    line += ".idg.com.au/images/*_promo$image\r\n";
		    line += ".imagesatlantic.com/\r\n";
		    line += ".imedia.co.il/\r\n";
		    line += ".imglt.com/\r\n";
		    line += ".imiclk.com/\r\n";
		    line += ".impresionesweb.com/\r\n";
		    line += ".indiads.com/\r\n";
		    line += ".industrybrains.com/\r\n";
		    line += ".inetinteractive.com/$third-party\r\n";
		    line += ".infinite-ads.\r\n";
		    line += ".infolinks.com/\r\n";
		    line += ".infoseek.co.jp/isweb/clip.html\r\n";
		    line += ".insightexpressai.com/\r\n";
		    line += ".intellitxt.com/\r\n";
		    line += ".interclick.com/\r\n";
		    line += ".interpolls.com/\r\n";
		    line += ".ipromote.com/\r\n";
		    line += ".iselectmedia.com/*/banners/$third-party\r\n";
		    line += ".jangonetwork.com/\r\n";
		    line += ".japan-guide.com/ad/\r\n";
		    line += ".jdoqocy.\r\n";
		    line += ".jumboaffiliates.\r\n";
		    line += ".kallout.com/*.php?id=$third-party\r\n";
		    line += ".kanoodle.com/\r\n";
		    line += ".kerg.net/\r\n";
		    line += ".ketoo.com/\r\n";
		    line += ".kitz.co.uk/files/jump2/\r\n";
		    line += ".klipmart.\r\n";
		    line += ".kontera.com/\r\n";
		    line += ".kqzyfj.\r\n";
		    line += ".labtimes.org/banner/\r\n";
		    line += ".lakequincy.com/\r\n";
		    line += ".lduhtrp.net/\r\n";
		    line += ".leadacceptor.\r\n";
		    line += ".ligatus.com/\r\n";
		    line += ".lightningcast.net/$~object_subrequest,~other\r\n";
		    line += ".lightningcast.net/*/getplaylist?$domain=reuters.com\r\n";
		    line += ".lingospot.com/\r\n";
		    line += ".linkads.*?\r\n";
		    line += ".linkexchange.\r\n";
		    line += ".linkworth.\r\n";
		    line += ".linkz.net/\r\n";
		    line += ".litres.ru/static/banner/\r\n";
		    line += ".liverail.com/\r\n";
		    line += ".ltassrv\r\n";
		    line += ".marketingsolutions.yahoo.com/\r\n";
		    line += ".match.com/*/prm/$subdocument\r\n";
		    line += ".maxserving.\r\n";
		    line += ".mb01.com/\r\n";
		    line += ".mbn.com.ua/\r\n";
		    line += ".media6degrees.com/\r\n";
		    line += ".mediafire.com/*/linkto/default-$subdocument\r\n";
		    line += ".mediafire.com/*/remove_ads.gif\r\n";
		    line += ".mediagridwork.com/mx.js\r\n";
		    line += ".medialand.ru/\r\n";
		    line += ".mediaonenetwork.net/\r\n";
		    line += ".mediaplex.com/\r\n";
		    line += ".mediatarget.\r\n";
		    line += ".mediavantage.\r\n";
		    line += ".megaclick.com/\r\n";
		    line += ".mercuras.\r\n";
		    line += ".merriam-webster.com/*/accipiter.js\r\n";
		    line += ".metaffiliation.\r\n";
		    line += ".mgnetwork.com/dealtaker/\r\n";
		    line += ".microsoftaffiliates.*.aspx?\r\n";
		    line += ".mirago.com/\r\n";
		    line += ".miva.com/\r\n";
		    line += ".mochiads.com/srv/\r\n";
		    line += ".mootermedia.\r\n";
		    line += ".mp3mediaworld.com*/!\r\n";
		    line += ".msn.com/?adunitid\r\n";
		    line += ".musictarget.com*/!\r\n";
		    line += ".myway.com/gca_iframe.\r\n";
		    line += ".nearlygood.com/*/_aff.php?\r\n";
		    line += ".neoseeker.com/*_pc.html\r\n";
		    line += ".net3media.\r\n";
		    line += ".netavenir.\r\n";
		    line += ".newanglemedia.com/clients/\r\n";
		    line += ".news.com.au/*-promo$image\r\n";
		    line += ".news.com.au/news/vodafone/$object\r\n";
		    line += ".newsadstream.\r\n";
		    line += ".nexac.com/\r\n";
		    line += ".nicheads.com/\r\n";
		    line += ".northmay.com/\r\n";
		    line += ".nvero.net/\r\n";
		    line += ".nyadmcncserve-\r\n";
		    line += ".nytimes.com/video/ads/$object_subrequest,other\r\n";
		    line += ".nz/ads/\r\n";
		    line += ".obibanners.\r\n";
		    line += ".objectservers.com/\r\n";
		    line += ".ocforums.com/adj/\r\n";
		    line += ".onenetworkdirect.\r\n";
		    line += ".openx.org/$third-party\r\n";
		    line += ".org/ad.html\r\n";
		    line += ".org/ads/\r\n";
		    line += ".othersonline.com/partner/scripts/*?\r\n";
		    line += ".overture.com/\r\n";
		    line += ".oxado.com/\r\n";
		    line += ".p-advg.com/\r\n";
		    line += ".partypartners.com/\r\n";
		    line += ".pc-ads.com/\r\n";
		    line += ".perfb.com/\r\n";
		    line += ".pgpartner.\r\n";
		    line += ".pheedo.*/img.phdo?\r\n";
		    line += ".php?adclass\r\n";
		    line += ".php?bannerid$domain=~fatwallet.com\r\n";
		    line += ".php?zoneid=*&loc=\r\n";
		    line += ".picadmedia.com/\r\n";
		    line += ".platinumadvertisement.\r\n";
		    line += ".playertraffic.\r\n";
		    line += ".plsthx.com/newaff/\r\n";
		    line += ".pointroll.\r\n";
		    line += ".precisionclick.com/\r\n";
		    line += ".predictad.com/\r\n";
		    line += ".pricegrabber.com/$subdocument\r\n";
		    line += ".pricespy.co.nz/adds/\r\n";
		    line += ".primaryads.\r\n";
		    line += ".pro-advertising.\r\n";
		    line += ".pro-market.net/\r\n";
		    line += ".probannerswap.\r\n";
		    line += ".profitpeelers.com/\r\n";
		    line += ".projectwonderful.com/$third-party\r\n";
		    line += ".proximic.com/js/widget.js\r\n";
		    line += ".ps3news.com/banner/\r\n";
		    line += ".pulse360.com/\r\n";
		    line += ".qj.net/a$s.php?\r\n";
		    line += ".qksrv.net/\r\n";
		    line += ".qksz.net/\r\n";
		    line += ".questionmarket.com/\r\n";
		    line += ".questus.com/\r\n";
		    line += ".realmatch.com/widgets/js/\r\n";
		    line += ".reklamz.com/\r\n";
		    line += ".reuters.com/*/video_ad.\r\n";
		    line += ".revfusion.net/\r\n";
		    line += ".revresda.\r\n";
		    line += ".ringtonematcher.com/\r\n";
		    line += ".rmxads.com/\r\n";
		    line += ".roirocket.com/\r\n";
		    line += ".rotatingad.com/\r\n";
		    line += ".rottentomatoes.*size=*x*&dechannel\r\n";
		    line += ".rovion.*?affid=\r\n";
		    line += ".rubiconproject.com/\r\n";
		    line += ".rwpads.\r\n";
		    line += ".scanscout.com/\r\n";
		    line += ".shareasale.com/\r\n";
		    line += ".shareresults.\r\n";
		    line += ".smartadserver.com/\r\n";
		    line += ".smarttargetting.\r\n";
		    line += ".snap.com/\r\n";
		    line += ".snopes.com/*/*ad$subdocument\r\n";
		    line += ".socialmedia.com/\r\n";
		    line += ".socialvibe.com/\r\n";
		    line += ".sonnerie.\r\n";
		    line += ".space.com/promo/\r\n";
		    line += ".sparkstudios.\r\n";
		    line += ".specificclick.net/\r\n";
		    line += ".specificmedia.\r\n";
		    line += ".speedsuccess.net/\r\n";
		    line += ".sponsorpalace.\r\n";
		    line += ".spotplex.*widget\r\n";
		    line += ".srtk.net/\r\n";
		    line += ".sta-ads.\r\n";
		    line += ".stlyrics.com/*_az.js\r\n";
		    line += ".sublimemedia.net/\r\n";
		    line += ".survey-poll.\r\n";
		    line += ".swf?clicktag=$domain=~rollingstone.com\r\n";
		    line += ".tacoda.net/\r\n";
		    line += ".tailsweep.com/\r\n";
		    line += ".targetnet.com/\r\n";
		    line += ".targetpoint.com/\r\n";
		    line += ".targetspot.com/\r\n";
		    line += ".techcrunch.com/*&blockcampaign=1\r\n";
		    line += ".techtree.com/js/jquery.catfish.js\r\n";
		    line += ".testnet.nl/\r\n";
		    line += ".thebigchair.com.au/egnonline/\r\n";
		    line += ".themis-media.com/media/global/images/cskins/\r\n";
		    line += ".themis.yahoo.com/\r\n";
		    line += ".tiser.com/\r\n";
		    line += ".tkqlhce.\r\n";
		    line += ".total-media.net/\r\n";
		    line += ".tqlkg.com/\r\n";
		    line += ".tradedoubler.com/\r\n";
		    line += ".trafficmasterz.\r\n";
		    line += ".tribalfusion.com/\r\n";
		    line += ".trigami.com/$third-party\r\n";
		    line += ".triggertag.gorillanation.com/\r\n";
		    line += ".tv/ads/\r\n";
		    line += ".twinplan.com/af_\r\n";
		    line += ".twittad.com/\r\n";
		    line += ".typepad.com/sponsors/\r\n";
		    line += ".tyroo.com/\r\n";
		    line += ".uimserv.net/\r\n";
		    line += ".uk/ads/\r\n";
		    line += ".unicast.\r\n";
		    line += ".universalhub.com/bban/\r\n";
		    line += ".us/ads/\r\n";
		    line += ".usercash.com/\r\n";
		    line += ".usnews.com/*/ad-welcome.js\r\n";
		    line += ".usnews.com/*/ad.js\r\n";
		    line += ".utarget.co.uk/$domain=~tvcatchup.com\r\n";
		    line += ".valuead.\r\n";
		    line += ".valueclick.\r\n";
		    line += ".vibrantmedia.\r\n";
		    line += ".videoegg.com/*/init.js?\r\n";
		    line += ".videosift.com/bnr.php?\r\n";
		    line += ".vpico.com/\r\n";
		    line += ".vsservers.net/\r\n";
		    line += ".w3schools.com/banners/\r\n";
		    line += ".webads.co.nz/\r\n";
		    line += ".webmasterplan.com/\r\n";
		    line += ".widgetbucks.com/\r\n";
		    line += ".worlddatinghere.\r\n";
		    line += ".worthathousandwords.com/\r\n";
		    line += ".xchangebanners.\r\n";
		    line += ".xgaming.com/rotate*.php?\r\n";
		    line += ".yahoo.*/serv?s=\r\n";
		    line += ".yahoo.com/*.ads.darla.\r\n";
		    line += ".yahoo.com/*/eyc-themis?\r\n";
		    line += ".yahoo.com/ads?*=mrec_ad&\r\n";
		    line += ".yahoo.com/darla/\r\n";
		    line += ".yceml.net/\r\n";
		    line += ".yfrog.com/ym.php?\r\n";
		    line += ".yieldbuild.com/\r\n";
		    line += ".yieldmanager.com/\r\n";
		    line += ".yieldmanager.net/\r\n";
		    line += ".yimg.com/*/fairfax/$image\r\n";
		    line += ".yimg.com/a/1-$~stylesheet\r\n";
		    line += ".yimg.com/adv/$image\r\n";
		    line += ".ytimg.com/yt/swf/ad-*.swf\r\n";
		    line += ".za/ads/\r\n";
		    line += ".zangocash.*/detectenvironment\r\n";
		    line += ".zanox.com/\r\n";
		    line += ".zeads.com/\r\n";
		    line += ".zedo.com/\r\n";
		    line += ".zoomdirect.com.au/\r\n";
		    line += ".zoomin.tv/*/amalia*?\r\n";
		    line += ".zxxds.net/\r\n";
		    line += "/.adserv/*\r\n";
		    line += "/204.2.168.8/*$domain=gmanews.tv\r\n";
		    line += "/;*;cue=pre;$object_subrequest,other,domain=~crn.com\r\n";
		    line += "/a.clearlightdigital.\r\n";
		    line += "/a.giantrealm.com\r\n";
		    line += "/aa.voice2page.com/*\r\n";
		    line += "/ab.vcmedia.\r\n";
		    line += "/aamsz=*/acc_random=\r\n";
		    line += "/aamsz=*/pageid=\r\n";
		    line += "/aamsz=*/position=\r\n";
		    line += "/abmw.aspx\r\n";
		    line += "/acc_random=*/aamsz=\r\n";
		    line += "/ad-1.5.\r\n";
		    line += "/ad-frame.\r\n";
		    line += "/ad-iframe-wrapper.\r\n";
		    line += "/ad-server/*\r\n";
		    line += "/ad.asp?\r\n";
		    line += "/ad.aspx?\r\n";
		    line += "/ad.cgi?\r\n";
		    line += "/ad.html?\r\n";
		    line += "/ad.jsp?\r\n";
		    line += "/ad.php?\r\n";
		    line += "/ad.pl?z\r\n";
		    line += "/ad/?id=\r\n";
		    line += "/ad/banner_*?\r\n";
		    line += "/ad/code$script\r\n";
		    line += "/ad/frame\r\n";
		    line += "/ad/google_\r\n";
		    line += "/ad/header_\r\n";
		    line += "/ad/init*.php\r\n";
		    line += "/ad/init.\r\n";
		    line += "/ad/lrec/*\r\n";
		    line += "/ad/mercury$object\r\n";
		    line += "/ad/mrec/*\r\n";
		    line += "/ad/serve\r\n";
		    line += "/ad/skyscraper.\r\n";
		    line += "/ad/sponsors/*\r\n";
		    line += "/ad/textlinks/*\r\n";
		    line += "/ad/top_\r\n";
		    line += "/ad/view/*\r\n";
		    line += "/ad2.aspx?\r\n";
		    line += "/ad2games.\r\n";
		    line += "/ad?count=\r\n";
		    line += "/ad_configuration\r\n";
		    line += "/ad_creatives.\r\n";
		    line += "/ad_feed.js?\r\n";
		    line += "/ad_frame.\r\n";
		    line += "/ad_functions\r\n";
		    line += "/ad_holder/*\r\n";
		    line += "/ad_insert.\r\n";
		    line += "/ad_manager.\r\n";
		    line += "/ad_refresher.\r\n";
		    line += "/ad_reloader_\r\n";
		    line += "/ad_serv.\r\n";
		    line += "/ad_sky.\r\n";
		    line += "/ad_sizes=\r\n";
		    line += "/ad_top.\r\n";
		    line += "/ad_v2.js\r\n";
		    line += "/ad_vert.gif\r\n";
		    line += "/ad_wrapper\r\n";
		    line += "/adbanner\r\n";
		    line += "/adbrite$subdocument\r\n";
		    line += "/adbrite.\r\n";
		    line += "/adbureau.\r\n";
		    line += "/adchannel_\r\n";
		    line += "/adclick\r\n";
		    line += "/adclient$domain=~cnet.com\r\n";
		    line += "/adcode.\r\n";
		    line += "/adcodes/*\r\n";
		    line += "/adconfig.xml?$image\r\n";
		    line += "/adconfig/*\r\n";
		    line += "/adcontent.$~object_subrequest,~other\r\n";
		    line += "/adcreative.\r\n";
		    line += "/adcycle/*\r\n";
		    line += "/addyn|*|adtech;\r\n";
		    line += "/addyn/3.0/*\r\n";
		    line += "/adengage_\r\n";
		    line += "/adf.cgi?\r\n";
		    line += "/adfarm.\r\n";
		    line += "/adfetch?\r\n";
		    line += "/adframe.$domain=~arto.com|~avforums.com|~clickandload.net|~coldmirror.de|~eq2flames.com|~pwinsider.com\r\n";
		    line += "/adframe/*\r\n";
		    line += "/adframe_\r\n";
		    line += "/adfshow?\r\n";
		    line += "/adfunction\r\n";
		    line += "/adgraphics/*\r\n";
		    line += "/adguru.\r\n";
		    line += "/adheader\r\n";
		    line += "/adhtml/*\r\n";
		    line += "/adiframe.\r\n";
		    line += "/adiframe/*\r\n";
		    line += "/adimages.\r\n";
		    line += "/adimages/*\r\n";
		    line += "/adinsert.\r\n";
		    line += "/adinterax.\r\n";
		    line += "/adisfy.com/*\r\n";
		    line += "/adjs.php?\r\n";
		    line += "/adjsmp.php?\r\n";
		    line += "/adlabel\r\n";
		    line += "/adlayer.php?\r\n";
		    line += "/adlayer/*\r\n";
		    line += "/adlink.\r\n";
		    line += "/adlink_\r\n";
		    line += "/adlinks.\r\n";
		    line += "/adman/www/*\r\n";
		    line += "/admanagement/*\r\n";
		    line += "/admanager\r\n";
		    line += "/admaster?\r\n";
		    line += "/admatch-syndication.\r\n";
		    line += "/admedia.\r\n";
		    line += "/adn.fusionads.\r\n";
		    line += "/adnetwork.$domain=~facebook.com\r\n";
		    line += "/adonline.\r\n";
		    line += "/adpage.\r\n";
		    line += "/adpeeps.php\r\n";
		    line += "/adpeeps/*\r\n";
		    line += "/adplayer/*\r\n";
		    line += "/adpoint.\r\n";
		    line += "/adpopup.\r\n";
		    line += "/adproducts/*\r\n";
		    line += "/adproxy/*\r\n";
		    line += "/adrelated.\r\n";
		    line += "/adreload?\r\n";
		    line += "/adremote.\r\n";
		    line += "/adrevolver/*\r\n";
		    line += "/adroot/*\r\n";
		    line += "/adrot.js\r\n";
		    line += "/ads-leader\r\n";
		    line += "/ads-rec\r\n";
		    line += "/ads-service.\r\n";
		    line += "/ads-sky\r\n";
		    line += "/ads.htm\r\n";
		    line += "/ads.php?\r\n";
		    line += "/ads/frame\r\n";
		    line += "/ads/java\r\n";
		    line += "/ads/paid\r\n";
		    line += "/ads/right\r\n";
		    line += "/ads/text\r\n";
		    line += "/ads/top\r\n";
		    line += "/ads2.php?$third-party\r\n";
		    line += "/ads_iframe.$domain=~buy.com\r\n";
		    line += "/ads_php/*\r\n";
		    line += "/ads_reporting/*\r\n";
		    line += "/ads_v2.\r\n";
		    line += "/ads_yahoo.\r\n";
		    line += "/adsadclient31.dll?$domain=~movies.msn.de\r\n";
		    line += "/adsadview.\r\n";
		    line += "/adsatt.\r\n";
		    line += "/adsearch.\r\n";
		    line += "/adscript$domain=~newsweek.com\r\n";
		    line += "/adsense.$domain=~thecodingstudio.com\r\n";
		    line += "/adsense/*$domain=~google.com\r\n";
		    line += "/adsense_\r\n";
		    line += "/adsense2\r\n";
		    line += "/adserve/*\r\n";
		    line += "/adserver.$domain=~tv.derstandard.at\r\n";
		    line += "/adserver/*$domain=~drei.at|~usps.com\r\n";
		    line += "/adserver2/*\r\n";
		    line += "/adserver?\r\n";
		    line += "/adservice$script\r\n";
		    line += "/adsfac.\r\n";
		    line += "/adsfolder/*\r\n";
		    line += "/adshow?\r\n";
		    line += "/adsiframe/*\r\n";
		    line += "/adsimage/*\r\n";
		    line += "/adsinclude.\r\n";
		    line += "/adsinsert.\r\n";
		    line += "/adsmanagement/*?\r\n";
		    line += "/adsmanager/*\r\n";
		    line += "/adsnew.\r\n";
		    line += "/adsonar.\r\n";
		    line += "/adspace$subdocument\r\n";
		    line += "/adspaces.\r\n";
		    line += "/adspro/*\r\n";
		    line += "/adsremote.\r\n";
		    line += "/adsreporting/*\r\n";
		    line += "/adssrv.\r\n";
		    line += "/adstorage.\r\n";
		    line += "/adstream.\r\n";
		    line += "/adswap.$domain=~apple.com\r\n";
		    line += "/adswap/*\r\n";
		    line += "/adsyndication.\r\n";
		    line += "/adsystem/*\r\n";
		    line += "/adtags.\r\n";
		    line += "/adtags/*\r\n";
		    line += "/adtech;\r\n";
		    line += "/adtech.$domain=~arto.com|~tv3.ie\r\n";
		    line += "/adtech/*\r\n";
		    line += "/adtech_\r\n";
		    line += "/adtext.\r\n";
		    line += "/adtext_\r\n";
		    line += "/adtology\r\n";
		    line += "/adtop.js\r\n";
		    line += "/adtrack/*\r\n";
		    line += "/adtraff.\r\n";
		    line += "/adtype.php?\r\n";
		    line += "/adunits/*\r\n";
		    line += "/advert.\r\n";
		    line += "/advert/*\r\n";
		    line += "/advert_\r\n";
		    line += "/advert?\r\n";
		    line += "/advertisementview/*\r\n";
		    line += "/advertising/*$subdocument\r\n";
		    line += "/advertpro/*\r\n";
		    line += "/adverts.\r\n";
		    line += "/adverts/*$domain=~videogamer.com\r\n";
		    line += "/adverts_\r\n";
		    line += "/adview.\r\n";
		    line += "/advision.\r\n";
		    line += "/adwords.$third-party\r\n";
		    line += "/adworks.\r\n";
		    line += "/adworks/*\r\n";
		    line += "/adwrapper/*\r\n";
		    line += "/adwrapperiframe.\r\n";
		    line += "/adx_remote.\r\n";
		    line += "/adxx.php?\r\n";
		    line += "/adyoz.com/*\r\n";
		    line += "/aff_frame.\r\n";
		    line += "/affad?q=\r\n";
		    line += "/affads/*\r\n";
		    line += "/affiliate*.php?\r\n";
		    line += "/affiliate_$image\r\n";
		    line += "/affilatebanner.\r\n";
		    line += "/affiliatebanners/*\r\n";
		    line += "/affiliates.*.aspx?\r\n";
		    line += "/affiliates.babylon.\r\n";
		    line += "/affiliates/banner\r\n";
		    line += "/affiliatewiz/*\r\n";
		    line += "/affiliationcash.\r\n";
		    line += "/afimages.\r\n";
		    line += "/afr.php?\r\n";
		    line += "/ah.pricegrabber.com/cb_table.php\r\n";
		    line += "/ajrotator/*$domain=~videodetective.com\r\n";
		    line += "/ajs.php?\r\n";
		    line += "/armorgames.com/misc/banners/*\r\n";
		    line += "/article_ad.\r\n";
		    line += "/annonser/*\r\n";
		    line += "/api/ads/*\r\n";
		    line += "/aseadnshow.\r\n";
		    line += "/aserve.directorym.\r\n";
		    line += "/audsci.js\r\n";
		    line += "/auspipe.com/*\r\n";
		    line += "/autopromo\r\n";
		    line += "/athena-ads.\r\n";
		    line += "/avpa.javalobby.org/*\r\n";
		    line += "/ban_m.php?\r\n";
		    line += "/banimpress.\r\n";
		    line += "/banman.asp?\r\n";
		    line += "/banner.play-asia.com\r\n";
		    line += "/banner_ad.\r\n";
		    line += "/banner_ads.\r\n";
		    line += "/banner_control.php?\r\n";
		    line += "/banner_db.php?\r\n";
		    line += "/banner_file.php?\r\n";
		    line += "/banner_js.*?\r\n";
		    line += "/banner_management/*\r\n";
		    line += "/banner_skyscraper.\r\n";
		    line += "/bannercode.php\r\n";
		    line += "/bannerfarm/*$third-party\r\n";
		    line += "/bannerframe.*?\r\n";
		    line += "/bannermanager/*\r\n";
		    line += "/bannermedia/*\r\n";
		    line += "/bannerrotation.\r\n";
		    line += "/banners.*&iframe=\r\n";
		    line += "/banners.adultfriendfinder\r\n";
		    line += "/banners.expressindia.com/*\r\n";
		    line += "/banners.sys-con.com/*\r\n";
		    line += "/banners/affiliate/*\r\n";
		    line += "/banners_rotation.\r\n";
		    line += "/bannerscript/*\r\n";
		    line += "/bannerserver/*\r\n";
		    line += "/bannersyndication.\r\n";
		    line += "/bannerview.*?\r\n";
		    line += "/bannery/*?banner=\r\n";
		    line += "/baselinead.\r\n";
		    line += "/bbccom.js?\r\n";
		    line += "/behaviorads/*\r\n";
		    line += "/beta.down2crazy.com/*$third-party\r\n";
		    line += "/bin-layer.\r\n";
		    line += "/blogad_\r\n";
		    line += "/blogads\r\n";
		    line += "/bnrs.ilm.ee/*\r\n";
		    line += "/bnrsrv.*?\r\n";
		    line += "/body.imho.ru/*\r\n";
		    line += "/boomad.\r\n";
		    line += "/box.anchorfree.net/*\r\n";
		    line += "/boylesportsreklame.*?\r\n";
		    line += "/bs.yandex.ru\r\n";
		    line += "/butler.php?type=\r\n";
		    line += "/burnsoftware.info*/!\r\n";
		    line += "/buyclicks/*\r\n";
		    line += "/c.adroll.\r\n";
		    line += "/cas.clickability.com/*\r\n";
		    line += "/cashad.\r\n";
		    line += "/cashad2.\r\n";
		    line += "/cbanners.\r\n";
		    line += "/cgi-bin/ads/*\r\n";
		    line += "/circads.\r\n";
		    line += "/clickserv\r\n";
		    line += "/cm8adam\r\n";
		    line += "/cm8space_call$script\r\n";
		    line += "/cms/profile_display/*\r\n";
		    line += "/cnnslads.\r\n";
		    line += "/cnwk.*widgets.js\r\n";
		    line += "/commercial_top.\r\n";
		    line += "/commercials/splash\r\n";
		    line += "/common/ads/*\r\n";
		    line += "/content.4chan.org/tmp/*\r\n";
		    line += "/contextad.\r\n";
		    line += "/csdynamic\r\n";
		    line += "/ctamlive160x160.\r\n";
		    line += "/ctxtlink/*\r\n";
		    line += "/cubics.com/*\r\n";
		    line += "/customad.\r\n";
		    line += "/d.m3.net/*\r\n";
		    line += "/da.feedsportal.com/r/*\r\n";
		    line += "/data.resultlinks.\r\n";
		    line += "/dart_ads/*\r\n";
		    line += "/dbbsrv.com/*\r\n";
		    line += "/dc.tremormedia.com/*\r\n";
		    line += "/dc_ads.\r\n";
		    line += "/dcloadads/*\r\n";
		    line += "/delivery.3rdads.\r\n";
		    line += "/delivery/ag.php\r\n";
		    line += "/descpopup.js\r\n";
		    line += "/destacados/*$subdocument\r\n";
		    line += "/digdug.divxnetworks.com/*\r\n";
		    line += "/direct_ads.$domain=~bbc.co.uk\r\n";
		    line += "/directads.\r\n";
		    line += "/displayad.\r\n";
		    line += "/displayads\r\n";
		    line += "/dnsads.html?\r\n";
		    line += "/dontblockthis/*\r\n";
		    line += "/doubleclick/iframe.\r\n";
		    line += "/drawad.php?\r\n";
		    line += "/dsg/bnn/*\r\n";
		    line += "/dyn_banner.\r\n";
		    line += "/dyn_banners_\r\n";
		    line += "/dynamicad?\r\n";
		    line += "/dynamiccsad?\r\n";
		    line += "/dynbanner/flash/*\r\n";
		    line += "/ebay_ads/*\r\n";
		    line += "/ebayisapi.dll?ekserver&\r\n";
		    line += "/ecustomeropinions.com/popup/*\r\n";
		    line += "/edge-dl.andomedia.com/*\r\n";
		    line += "/ekmas.com\r\n";
		    line += "/emailads/*\r\n";
		    line += "/eralinks/*$subdocument\r\n";
		    line += "/export_feeds.php?*&banner$domain=~storagereview.com\r\n";
		    line += "/external/ad.js\r\n";
		    line += "/eyoob.com/elayer/*\r\n";
		    line += "/fairadsnetwork.\r\n";
		    line += "/fatads.\r\n";
		    line += "/featuredads\r\n";
		    line += "/files/ads/*\r\n";
		    line += "/filetarget.com*/!\r\n";
		    line += "/filetarget.com/*_*.php$subdocument\r\n";
		    line += "/flashads.\r\n";
		    line += "/flashads/*\r\n";
		    line += "/flipmedia\r\n";
		    line += "/forms.aweber.com/*\r\n";
		    line += "/freetrafficbar.\r\n";
		    line += "/fuseads/*\r\n";
		    line += "/gamecast/ads\r\n";
		    line += "/gamersad.\r\n";
		    line += "/gampad/google_service.js|\r\n";
		    line += "/get_ad.php?\r\n";
		    line += "/get_player_ads_\r\n";
		    line += "/getad.php?\r\n";
		    line += "/getad.php|\r\n";
		    line += "/getbanner.cfm?$subdocument\r\n";
		    line += "/getmdhlink.\r\n";
		    line += "/getsponslinks\r\n";
		    line += "/gfx/ads/*\r\n";
		    line += "/glam_ads.\r\n";
		    line += "/google-ad?\r\n";
		    line += "/google-adsense$subdocument\r\n";
		    line += "/google_ad_\r\n";
		    line += "/google_ads.\r\n";
		    line += "/google_ads/*\r\n";
		    line += "/googlead.\r\n";
		    line += "/googleads-\r\n";
		    line += "/googleads_\r\n";
		    line += "/googleads2\r\n";
		    line += "/googleadsense\r\n";
		    line += "/googleafc\r\n";
		    line += "/googleframe.\r\n";
		    line += "/hera.hardocp.com/*\r\n";
		    line += "/hits.europuls.\r\n";
		    line += "/hits4pay.\r\n";
		    line += "/homepage_ads/*\r\n";
		    line += "/hotjobs_module.js\r\n";
		    line += "/houseads/*\r\n";
		    line += "/html.ng/*\r\n";
		    line += "/httpads/*\r\n";
		    line += "/ibanners.empoweredcomms.com.au/*\r\n";
		    line += "/iframe-ads/*\r\n";
		    line += "/iframe_ad.\r\n";
		    line += "/iframe_ads/*\r\n";
		    line += "/iframead.\r\n";
		    line += "/iframed_*sessionid=\r\n";
		    line += "/images/ad/*$domain=~quit.org.au\r\n";
		    line += "/images/ad_\r\n";
		    line += "/images/ads-\r\n";
		    line += "/images/bnnrs/*\r\n";
		    line += "/img.shopping.com/sc/pac/shopwidget_\r\n";
		    line += "/img/ad_*.gif\r\n";
		    line += "/includes/ad_\r\n";
		    line += "/includes/ads/*\r\n";
		    line += "/indianrailways/*\r\n";
		    line += "/internet.ziffdavis.com/*\r\n";
		    line += "/intext.js\r\n";
		    line += "/invideoad.\r\n";
		    line += "/itunesaffiliate\r\n";
		    line += "/job_ticker.\r\n";
		    line += "/jobs.thedailywtf.com/?pubid=\r\n";
		    line += "/js.*.yahoo.net/iframe.php?\r\n";
		    line += "/js.ng/c\r\n";
		    line += "/js.ng/s\r\n";
		    line += "/js/ad.js?$domain=~thecomedynetwork.ca|~tsn.ca\r\n";
		    line += "/js/ads.js$domain=~imdb.com\r\n";
		    line += "/js/ads/*\r\n";
		    line += "/js/adserver\r\n";
		    line += "/js/interstitial_space.js\r\n";
		    line += "/js/ysc_csc_\r\n";
		    line += "/kermit.macnn.\r\n";
		    line += "/kestrel.ospreymedialp.\r\n";
		    line += "/l.yimg.com/a/a/1-/flash/promotions/*/0*\r\n";
		    line += "/l.yimg.com/a/a/1-/java/promotions/*.swf\r\n";
		    line += "/launch/testdrive.gif\r\n";
		    line += "/layer-ad.\r\n";
		    line += "/layer-ads.\r\n";
		    line += "/layer.php\r\n";
		    line += "/layer/?\r\n";
		    line += "/layer/layer.js\r\n";
		    line += "/layer/s*.php?\r\n";
		    line += "/layer2.php\r\n";
		    line += "/layer3.php\r\n";
		    line += "/layerads_\r\n";
		    line += "/layers/layer.js\r\n";
		    line += "/layout/ads/*\r\n";
		    line += "/linkexchange/*\r\n";
		    line += "/linkreplacer.js\r\n";
		    line += "/links_sponsored_\r\n";
		    line += "/linkshare/*\r\n";
		    line += "/listings.*/iframe/dir\r\n";
		    line += "/loadad.aspx?\r\n";
		    line += "/logos/adlogo\r\n";
		    line += "/lw/ysc_csc_$script\r\n";
		    line += "/mac-ad?\r\n";
		    line += "/magic-ads/*\r\n";
		    line += "/marbachadverts.\r\n";
		    line += "/marketing*partner$image\r\n";
		    line += "/mazda.com.au/banners/*$third-party\r\n";
		    line += "/media.funpic.*/layer.\r\n";
		    line += "/media/ads/*\r\n";
		    line += "/mediamgr.ugo.\r\n";
		    line += "/medrx.sensis.com.au/*\r\n";
		    line += "/miva_ads.\r\n";
		    line += "/mnetorfad.js\r\n";
		    line += "/mod_ad/*\r\n";
		    line += "/modules/ads/*\r\n";
		    line += "/mtvmusic_ads_reporting.js\r\n";
		    line += "/nascar/*/defector.js\r\n";
		    line += "/nascar/*/promos/*$script,object\r\n";
		    line += "/nbjmp.com/*\r\n";
		    line += "/netspiderads\r\n";
		    line += "/network.sportsyndicator.\r\n";
		    line += "/network.triadmedianetwork.\r\n";
		    line += "/nextad/*\r\n";
		    line += "/oascentral.\r\n";
		    line += "/oasdefault/*\r\n";
		    line += "/oasisi-*.php?\r\n";
		    line += "/oasisi.php?\r\n";
		    line += "/obeus.com/initframe/*\r\n";
		    line += "/oddbanner.\r\n";
		    line += "/oiopub-direct/*\r\n";
		    line += "/openads.\r\n";
		    line += "/openads/*\r\n";
		    line += "/openads2/*\r\n";
		    line += "/openx/www/*\r\n";
		    line += "/openx_fl.\r\n";
		    line += "/outsidebanners/*\r\n";
		    line += "/overture/*$script,subdocument\r\n";
		    line += "/overture_\r\n";
		    line += "/ovt_show.asp?\r\n";
		    line += "/ox.bit-tech.net/delivery/*\r\n";
		    line += "/ox/www/*\r\n";
		    line += "/page-ads.\r\n";
		    line += "/pagead/ads?*_start_delay=1\r\n";
		    line += "/pagead/ads?*_fetch=\r\n";
		    line += "/pagead/imgad?\r\n";
		    line += "/pagead2.$~object_subrequest,~other\r\n";
		    line += "/pageads/*\r\n";
		    line += "/pageear.js\r\n";
		    line += "/pageear/*\r\n";
		    line += "/pagepeel$script\r\n";
		    line += "/pagepeel-*.gif\r\n";
		    line += "/paidlisting/*\r\n";
		    line += "/partner*rotate\r\n";
		    line += "/partner.sbaffiliates.\r\n";
		    line += "/partner.video.syndication.msn.com/*$~object_subrequest,~other\r\n";
		    line += "/partnerbanner.\r\n";
		    line += "/partnerbanner/*\r\n";
		    line += "/partnership/*affiliate\r\n";
		    line += "/peel.js\r\n";
		    line += "/peel1.js\r\n";
		    line += "/peelad/*$script\r\n";
		    line += "/perfads.\r\n";
		    line += "/performancingads/*$image\r\n";
		    line += "/phpads/*\r\n";
		    line += "/phpads2\r\n";
		    line += "/phpadserver/*\r\n";
		    line += "/phpadsnew$~object_subrequest,~other\r\n";
		    line += "/pilot_ad.\r\n";
		    line += "/pitattoad.\r\n";
		    line += "/pl.yumenetworks.com/*\r\n";
		    line += "/play/ad/*$object_subrequest,other\r\n";
		    line += "/player/ad.htm\r\n";
		    line += "/popads.\r\n";
		    line += "/popunder.\r\n";
		    line += "/popupjs.\r\n";
		    line += "/printads/*\r\n";
		    line += "/processads.\r\n";
		    line += "/processing/impressions.asp?\r\n";
		    line += "/promos.fling.\r\n";
		    line += "/psclicks.asp?\r\n";
		    line += "/pub.betclick.com/*\r\n";
		    line += "/pub/ads/*\r\n";
		    line += "/public.zangocash.\r\n";
		    line += "/public/ad?\r\n";
		    line += "/public/bannerjs.*?*=\r\n";
		    line += "/publicidad$~object_subrequest,~other\r\n";
		    line += "/publisher.shopzilla.$subdocument\r\n";
		    line += "/pubs.hiddennetwork.com/*\r\n";
		    line += "/r.mail.ru$object\r\n";
		    line += "/rad.*?getsad=\r\n";
		    line += "/railads.\r\n";
		    line += "/random=*/aamsz=\r\n";
		    line += "/realmedia/ads/*$domain=~absoluteradio.co.uk|~demand.five.tv\r\n";
		    line += "/reklam.\r\n";
		    line += "/reklama.\r\n";
		    line += "/reclame/*\r\n";
		    line += "/redvase.bravenet.com/*\r\n";
		    line += "/requestadvertisement.\r\n";
		    line += "/richmedia.yimg.com/*\r\n";
		    line += "/rok.com.com/*\r\n";
		    line += "/rotateads.\r\n";
		    line += "/rotating.php$subdocument\r\n";
		    line += "/rotating_banner\r\n";
		    line += "/rotation/*.php?\r\n";
		    line += "/rover.ebay.*&adtype=\r\n";
		    line += "/rtq.careerbuilder.com/*\r\n";
		    line += "/sa.entireweb.com/*\r\n";
		    line += "/scripts.snowball.com/clinkscontent/*\r\n";
		    line += "/scripts/ads/*\r\n";
		    line += "/search.spotxchange.com/*$domain=~supernovatube.com\r\n";
		    line += "/servedbyadbutler.\r\n";
		    line += "/sev4ifmxa.com/*\r\n";
		    line += "/share/ads/*\r\n";
		    line += "/shared/ads/*\r\n";
		    line += "/shopping.trustedreviews.com/box/*$subdocument\r\n";
		    line += "/shops.tgdaily.com/*&widget=\r\n";
		    line += "/show.asp?*_sky$subdocument\r\n";
		    line += "/show_ad.\r\n";
		    line += "/show_ad_\r\n";
		    line += "/show_ads.js?\r\n";
		    line += "/show_ads_\r\n";
		    line += "/show_afs_ads.\r\n";
		    line += "/show_deals.js\r\n";
		    line += "/show_i.php?\r\n";
		    line += "/showad.$subdocument\r\n";
		    line += "/showad/*\r\n";
		    line += "/showads.\r\n";
		    line += "/showbanner.php?\r\n";
		    line += "/showflashad.\r\n";
		    line += "/showlayer.\r\n";
		    line += "/skyad.php\r\n";
		    line += "/slideinad.\r\n";
		    line += "/small_ad.\r\n";
		    line += "/smart.allocine.fr/*\r\n";
		    line += "/smartad.\r\n";
		    line += "/smartlinks.epl?partner=\r\n";
		    line += "/sochr.com/*\r\n";
		    line += "/socialads.\r\n";
		    line += "/softsale/*$subdocument\r\n";
		    line += "/spac_adx.\r\n";
		    line += "/spc.php?\r\n";
		    line += "/special_ads/*\r\n";
		    line += "/spinbox.freedom.\r\n";
		    line += "/splash/page_header/*\r\n";
		    line += "/spo_show.asp?\r\n";
		    line += "/sponsimages/*\r\n";
		    line += "/sponsorad.\r\n";
		    line += "/sponsored$subdocument\r\n";
		    line += "/sponsored.gif\r\n";
		    line += "/squaread.\r\n";
		    line += "/static.zangocash.\r\n";
		    line += "/static/ad_$subdocument\r\n";
		    line += "/static/ads/*\r\n";
		    line += "/stickyad.\r\n";
		    line += "/storage/ads/*\r\n";
		    line += "/support.biemedia.\r\n";
		    line += "/surveycookie.js\r\n";
		    line += "/svgn.com/*\r\n";
		    line += "/systemad.\r\n";
		    line += "/systemad_\r\n";
		    line += "/tcode3.html\r\n";
		    line += "/testingad.\r\n";
		    line += "/textad.\r\n";
		    line += "/textad?\r\n";
		    line += "/textads/*\r\n";
		    line += "/tii_ads.$domain=~ew.com\r\n";
		    line += "/tikilink?\r\n";
		    line += "/tmz-adblock/*\r\n";
		    line += "/toolkitads.\r\n";
		    line += "/top_ads/*\r\n";
		    line += "/tradedoubler/*\r\n";
		    line += "/trusearch.net/affblock/*\r\n";
		    line += "/ttz_ad.\r\n";
		    line += "/tx_macinabanners/*\r\n";
		    line += "/ua.badongo.com/js.php?\r\n";
		    line += "/udmserve.net/*\r\n";
		    line += "/unicast.ign.com/assets/*\r\n";
		    line += "/upsellitjs2.jsp?\r\n";
		    line += "/userbanners/*\r\n";
		    line += "/vs20060817.com/*\r\n";
		    line += "/valueclick.\r\n";
		    line += "/vclkads.\r\n";
		    line += "/vendshow/*\r\n";
		    line += "/video-cdn.*_ad_\r\n";
		    line += "/video-cdn.*_promo_\r\n";
		    line += "/video.ap.org/*/ad_js.\r\n";
		    line += "/videoad.$domain=~videogamer.com\r\n";
		    line += "/videoads.\r\n";
		    line += "/videoads/*\r\n";
		    line += "/view/banner/*/zone?zid=\r\n";
		    line += "/vindicoasset.*/instreamad/*\r\n";
		    line += "/visit.homepagle.\r\n";
		    line += "/vista.pl/*/banners/*\r\n";
		    line += "/vtextads.\r\n";
		    line += "/w1.buysub.$~image\r\n";
		    line += "/webadimg/*\r\n";
		    line += "/webad?a\r\n";
		    line += "/webads.\r\n";
		    line += "/webads_\r\n";
		    line += "/webadverts/*\r\n";
		    line += "/whiteglove.jsp?\r\n";
		    line += "/widget.blogrush.com/show.js\r\n";
		    line += "/widgets.fccinteractive.com/*\r\n";
		    line += "/wipeads/*\r\n";
		    line += "/wp-srv/ad/*$third-party\r\n";
		    line += "/wp-srv/ad/*feed\r\n";
		    line += "/writelayerad.\r\n";
		    line += "/ws.amazon.*/widgets/q?\r\n";
		    line += "/www.cb.cl/*/banner\r\n";
		    line += "/www/delivery/*$domain=~video.belga.be\r\n";
		    line += "/www2.sys-con.com/*.cfm\r\n";
		    line += "/x4300tiz.com/*\r\n";
		    line += "/xbanner.php?\r\n";
		    line += "/xcelsiusadserver.com/*\r\n";
		    line += "/ygames_e/embed/src/embedplayer.js\r\n";
		    line += "/ysmads.html\r\n";
		    line += "=adiframe\r\n";
		    line += "=ads.ascx\r\n";
		    line += "=viewadjs\r\n";
		    line += "?*&ad_keyword=\r\n";
		    line += "?*&adspace=\r\n";
		    line += "?*&clientype=*&adid=\r\n";
		    line += "?*&google_adpage=$domain=~ign.com\r\n";
		    line += "?ad_ids=*&referer=\r\n";
		    line += "?ad_type=\r\n";
		    line += "?adtype=\r\n";
		    line += "?getad=&$~object_subrequest,~other\r\n";
		    line += "_ad.aspx\r\n";
		    line += "_ad_template_\r\n";
		    line += "_adbrite\r\n";
		    line += "_adfunction\r\n";
		    line += "_ads.php?\r\n";
		    line += "_adspace$domain=~adultswim.com|~cnn.com\r\n";
		    line += "_advertisement*.gif\r\n";
		    line += "_banner_ad\r\n";
		    line += "_bannerid*random\r\n";
		    line += "_box_ads/\r\n";
		    line += "_companionad.\r\n";
		    line += "_dynamicads/\r\n";
		    line += "_homepage_ad.\r\n";
		    line += "_homepage_ads.\r\n";
		    line += "_js/ad.js\r\n";
		    line += "_overlay_ad.\r\n";
		    line += "_skyscraper.$third-party\r\n";
		    line += "_skyscraper_\r\n";
		    line += "_videoad.$object_subrequest,other\r\n";
		    line += "8080/ads/\r\n";
		    line += "a/adx.js\r\n";
		    line += "all/ads/\r\n";
		    line += "banner-ad.\r\n";
		    line += "bannerad.\r\n";
		    line += "blackberryforums.net/banners/\r\n";
		    line += "central/ads/\r\n";
		    line += "crushorflush.com/html/promoframe.html\r\n";
		    line += "d/adx.js\r\n";
		    line += "dinclinx.com/\r\n";
		    line += "dynamicad.\r\n";
		    line += "e/adx.js\r\n";
		    line += "e/js.ng/\r\n";
		    line += "e/js_ng/\r\n";
		    line += "expedia_ad.\r\n";
		    line += "freeworldgroup.com/banner\r\n";
		    line += "friendfinder.com/*?*&iframe=1\r\n";
		    line += "http://stocker.bonnint.\r\n";
		    line += "http://ttsrc.aroq.\r\n";
		    line += "http://video.flashtalking.\r\n";
		    line += "images/ads/\r\n";
		    line += "include/ads/\r\n";
		    line += "interfacelift.com/inc_new/*$subdocument\r\n";
		    line += "k/adx.js\r\n";
		    line += "leftsidead.\r\n";
		    line += "m/adx.js\r\n";
		    line += "n/adx.js\r\n";
		    line += "m/js.ng/\r\n";
		    line += "majorgeeks.com/*/banners/\r\n";
		    line += "news.com.au/*/promos/\r\n";
		    line += "popinads.\r\n";
		    line += "popup_ad.\r\n";
		    line += "retrevo.com/*/pcwframe.jsp?\r\n";
		    line += "rightsidead.\r\n";
		    line += "s/adx.js\r\n";
		    line += "services/ads/\r\n";
		    line += "space.com/*interstitial_space.js\r\n";
		    line += "sponsor.gif\r\n";
		    line += "sponsorads.\r\n";
		    line += "t/adx.js\r\n";
		    line += "u/adx.js\r\n";
		    line += "z/adx.js\r\n";
		    line += "|http://a.ads.\r\n";
		    line += "|http://ad-uk.\r\n";
		    line += "|http://ad.$~object_subrequest,~other\r\n";
		    line += "|http://ad0.\r\n";
		    line += "|http://ad1.\r\n";
		    line += "|http://ad2.\r\n";
		    line += "|http://ad3.\r\n";
		    line += "|http://ad4.\r\n";
		    line += "|http://ad5.\r\n";
		    line += "|http://adimg.\r\n";
		    line += "|http://adnet.\r\n";
		    line += "|http://adq.\r\n";
		    line += "|http://ads.\r\n";
		    line += "|http://ads0.\r\n";
		    line += "|http://ads1.\r\n";
		    line += "|http://ads2.\r\n";
		    line += "|http://ads3.\r\n";
		    line += "|http://ads4.\r\n";
		    line += "|http://ads5.\r\n";
		    line += "|http://adserv$domain=~usps.com\r\n";
		    line += "|http://adstil.\r\n";
		    line += "|http://adsvr.\r\n";
		    line += "|http://adsys.\r\n";
		    line += "|http://adt.\r\n";
		    line += "|http://adv.$domain=~juventus.com\r\n";
		    line += "|http://adx.\r\n";
		    line += "|http://bwp.*/search?\r\n";
		    line += "|http://feeds.*/~a/\r\n";
		    line += "|http://getad.\r\n";
		    line += "|http://jazad.\r\n";
		    line += "|http://phpads\r\n";
		    line += "|http://rcm*.amazon.\r\n";
		    line += "|http://rss.*/~a/\r\n";
		    line += "|http://synad\r\n";
		    line += "|http://wrapper.*/a?\r\n";
		    line += "|http://xban.\r\n";
		    line += "*-120x600*\r\n";
		    line += "*-160x600*\r\n";
		    line += "*-300x250*\r\n";
		    line += "*-336x280*\r\n";
		    line += "*-468x60-*\r\n";
		    line += "*-468x60.*\r\n";
		    line += "*-468x60/*\r\n";
		    line += "*-468x60_*\r\n";
		    line += "*-468x80-*\r\n";
		    line += "*-468x80.*\r\n";
		    line += "*-468x80/*\r\n";
		    line += "*-468x80_*\r\n";
		    line += "*-728x90-*\r\n";
		    line += "*-728x90.*\r\n";
		    line += "*-728x90/*\r\n";
		    line += "*-728x90_*\r\n";
		    line += "*.120x600*\r\n";
		    line += "*.160x600*\r\n";
		    line += "*.300x250*\r\n";
		    line += "*.336x280*\r\n";
		    line += "*.468x60-*\r\n";
		    line += "*.468x60.*\r\n";
		    line += "*.468x60/*\r\n";
		    line += "*.468x60_*\r\n";
		    line += "*.468x80-*\r\n";
		    line += "*.468x80.*\r\n";
		    line += "*.468x80/*\r\n";
		    line += "*.468x80_*\r\n";
		    line += "*.728x90-*\r\n";
		    line += "*.728x90.*\r\n";
		    line += "*.728x90/*\r\n";
		    line += "*.728x90_*\r\n";
		    line += "*/120x600*\r\n";
		    line += "*/160x600*\r\n";
		    line += "*/300x250*\r\n";
		    line += "*/336x280*\r\n";
		    line += "*/468x60-*\r\n";
		    line += "*/468x60.*\r\n";
		    line += "*/468x60/*\r\n";
		    line += "*/468x60_*\r\n";
		    line += "*/468x80-*\r\n";
		    line += "*/468x80.*\r\n";
		    line += "*/468x80/*\r\n";
		    line += "*/468x80_*\r\n";
		    line += "*/728x90-*\r\n";
		    line += "*/728x90.*\r\n";
		    line += "*/728x90/*\r\n";
		    line += "*/728x90_*\r\n";
		    line += "*_120x600*\r\n";
		    line += "*_160x600*\r\n";
		    line += "*_300x250*$domain=~deviantart.com\r\n";
		    line += "*_336x280*\r\n";
		    line += "*_468x60-*\r\n";
		    line += "*_468x60.*$domain=~gacktworlddears.com\r\n";
		    line += "*_468x60/*\r\n";
		    line += "*_468x60_*\r\n";
		    line += "*_468x80-*\r\n";
		    line += "*_468x80.*\r\n";
		    line += "*_468x80/*\r\n";
		    line += "*_468x80_*\r\n";
		    line += "*_728x90-*\r\n";
		    line += "*_728x90.*\r\n";
		    line += "*_728x90/*\r\n";
		    line += "*_728x90_*\r\n";
		    line += "*_768x90_*\r\n";
		    line += "*-468_60.*\r\n";
		    line += "*_468.htm*\r\n";
		    line += "*_468_60.*\r\n";
		    line += "*_728_90.*\r\n";
		    line += "*468x60.g*\r\n";
		    line += "*728x90.g*\r\n";
		    line += "*728x90.h*\r\n";
		    line += "*728x90.s*\r\n";
		    line += "*n728x90_*\r\n";
		    line += "!-------------------------Whitelisting rules-------------------------!\r\n";
		    line += "@@.2mdn.net/*_ecw_$image,domain=wwe.com\r\n";
		    line += "@@.adap.tv/*/adplayer.swf?*vreel$object_subrequest,other\r\n";
		    line += "@@.adserver.yahoo.com/a?*=headr$script\r\n";
		    line += "@@.adserver.yahoo.com/a?*=mov&l=lrec&bg=ffffff&no_expandable=1$subdocument\r\n";
		    line += "@@.adserver.yahoo.com/a?*mail&l=head&$script\r\n";
		    line += "@@.belointeractive.com/*/ads/*@*?|$script\r\n";
		    line += "@@.doubleclick.net/adj/*smartclip$script,domain=last.fm|lastfm.de\r\n";
		    line += "@@.doubleclick.net/*/adj/wwe.shows/ecw_ecwreplay;*;sz=624x325;$script\r\n";
		    line += "@@.doubleclick.net/adj/nbcu.nbc/videoplayer-$script\r\n";
		    line += "@@.e-planning.net/eb/*?*fvp=2&$object_subrequest,other,domain=clarin.com|emol.com\r\n";
		    line += "@@.eyewonder.com/$object,script,domain=last.fm|lastfm.de\r\n";
		    line += "@@.style.com/*?*.doubleclick.net/adj/|$object\r\n";
		    line += "@@.yimg.com/*/videoadmodule2.swf$object_subrequest,other\r\n";
		    line += "@@.yimg.com/us.yimg.com/a/1-/java/promotions/js/ad_eo_1.1.js$script\r\n";
		    line += "@@.zedo.com/*.swf$object_subrequest,other,domain=rajshri.com\r\n";
		    line += "@@.zedo.com/*.xml$object_subrequest,other,domain=rajshri.com\r\n";
		    line += "@@//mail.google.com/mail/uploader/*$object\r\n";
		    line += "@@/adfinder.jsp?*/oascentral.feedroom.com/realmedia/ads/*$subdocument,domain=businessweek.com|economist.com|feedroom.com|stanford.edu\r\n";
		    line += "@@/admatch-syndication.mochila.com/viewer/*&articleid=$script\r\n";
		    line += "@@/ads.adultswim.com/js.ng/site=toonswim&toonswim_pos=600x400_ctr&toonswim_rollup=games$script\r\n";
		    line += "@@/ads.hulu.com/*.swf$object_subrequest,other\r\n";
		    line += "@@/ads.imeem.com/*$object_subrequest,other\r\n";
		    line += "@@/ads.js?*/adbanner$script,~third-party,domain=novamov.com|zshare.net\r\n";
		    line += "@@/ads.php?zone=*listen$subdocument,domain=last.fm|lastfm.de\r\n";
		    line += "@@/ads/adstream_mjx.ads/video.eurosport.$script\r\n";
		    line += "@@/assets.heavy.com/ads/*_int$object_subrequest,other\r\n";
		    line += "@@/assets.idiomag.com/flash/adverts/yume_$object_subrequest,other\r\n";
		    line += "@@/dyncdn.buzznet.com/catfiles/?f=dojo/*.googleadservices.$script\r\n";
		    line += "@@/img.weather.weatherbug.com/*/stickers/*\r\n";
		    line += "@@/ll.static.abc.go.com/streaming/move/*\r\n";
		    line += "@@/media.scanscout.com/ads/*.swf$object_subrequest,other\r\n";
		    line += "@@/omnikool.discovery.com/realmedia/ads/adstream_mjx.ads/dsc.discovery.com/*$script\r\n";
		    line += "@@/pagead/s*video$script,object,subdocument\r\n";
		    line += "@@/static.ak.fbcdn.net/*/ads/*$script\r\n";
		    line += "@@/video/shareplayer.swf?videoid=\r\n";
		    line += "@@/widget.slide.com/*/ads/*/preroll.swf$object_subrequest,other\r\n";
		    line += "@@/widgets.nbcuni.com/nbcplayers/*\r\n";
		    line += "@@bannerad$domain=rai.tv\r\n";
		    line += "@@cdn.fastclick.net/fastclick.net/video/\r\n";
		    line += "@@espn.go.com/*/espn360/banner?$subdocument\r\n";
		    line += "@@flyordie.com/*&affiliate$subdocument\r\n";
		    line += "@@fox.com.au/player|$document\r\n";
		    line += "@@int1.fp.sandpiper.net/$object_subrequest,other\r\n";
		    line += "@@|http://ad.doubleclick.net/adj/imdb2.consumer.video/*;tile=7;sz=320x240,$script\r\n";
		    line += "@@|http://ad.doubleclick.net/pfadx/umg.*;sz=10x$script\r\n";
		    line += "@@|http://ad.gogopin.com/*$~document\r\n";
		    line += "@@|http://ads.forbes.com/realmedia/ads/*@videopreroll$script\r\n";
		    line += "@@|http://ads.yimg.com/*/search/b/syc_logo_2.gif|$image\r\n";
		    line += "@@|http://adserver.bigwigmedia.com/ingamead3.swf$object_subrequest,other\r\n";
		    line += "@@|http://bnrs.ilm.ee/www/delivery/fl.js$script\r\n";
		    line += "@@|http://img.thedailywtf.com/images/ads/$image\r\n";
		    line += "@@|http://media.salemwebnetwork.com/js/admanager/swfobject.js$script,domain=christianity.com\r\n";
		    line += "@@|http://oas.bigflix.com/*$object_subrequest,other\r\n";
		    line += "@@|http://oascentral.feedroom.com/RealMedia/ads/adstream_sx.ads/$script,domain=businessweek.com|economist.com|feedroom.com|stanford.edu\r\n";
		    line += "@@|http://objects.tremormedia.com/embed/swf/admanager.swf$object_subrequest,other\r\n";
		    line += "@@|http://pagead2.googlesyndication.com/pagead/abglogo/abg-da-100c-000000.png$domain=janno.dk|nielco.dk\r\n";
		    line += "@@|http://serve.vdopia.com/adserver/ad*.php$script,object_subrequest,other,xmlhttprequest\r\n";
		    line += "@@|http://sfx-images.mozilla.org/$image,domain=spreadfirefox.com\r\n";
		    line += "@@|http://vox-static.liverail.com/crossdomain.xml$object_subrequest,other,domain=atom.com\r\n";
		    line += "@@|http://www.ngads.com/checkabp.js?$script\r\n";
		    line += "@@|http://www.theonion.com/ads/video-ad$object_subrequest,other\r\n";
		    line += "@@|http://simple-adblock.com/plugin/welcome\r\n";

            // Write file
            DWORD dwBytesWritten = 0;
            if (::WriteFile(hFile, line.GetBuffer(), line.GetLength(), &dwBytesWritten, NULL) && dwBytesWritten == line.GetLength())
            {
		        // Set correct version
                CAdPluginSettings* settings = CAdPluginSettings::GetInstance();

                settings->AddFilterUrl(CStringA(FILTERS_PROTOCOL) + CStringA(FILTERS_HOST) + "/easylist.txt", 1);
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

#endif // PRODUCT_ADBLOCKER


#ifdef PRODUCT_ADBLOCKER

bool CAdPluginFilter::IsAlive() const
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
