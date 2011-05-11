#include "PluginStdAfx.h"

#include "PluginFilter.h"

#if (defined PRODUCT_SIMPLEADBLOCK)
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
	if (filterParts.size() < 1)
	{
		return;
	}
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

		//Rename old filter file if exists
		if (filename == PERSONAL_FILTER_FILE)
		{
	        HANDLE hFile = ::CreateFile(m_dataPath + PERSONAL_FILTER_FILE_OLD, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);  
			if (hFile != INVALID_HANDLE_VALUE)
			{
				CloseHandle(hFile);
				int res = _wrename(m_dataPath + PERSONAL_FILTER_FILE_OLD, m_dataPath + filename);
				DWORD err = GetLastError();
				err = 0;
			}
		}

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

			//Init MLang
			CoInitialize(NULL);
			HRESULT hr = S_OK;
			CComPtr<IMultiLanguage2> pMultiLanguage;
			hr = CoCreateInstance(CLSID_CMultiLanguage, NULL, CLSCTX_INPROC_SERVER, IID_IMultiLanguage2, (void**)&pMultiLanguage); 


			bool codepageAvailable = false;
			DetectEncodingInfo encodingInfos[10];
			int scores = 10;
            while ((bRead = ::ReadFile(hFile, pBuffer, 8192, &dwBytesRead, NULL)) == TRUE && dwBytesRead > 0)
            {
				//detect codepage based on first buffer
				if (!codepageAvailable)
				{
					unsigned int srcLength = fileContent.GetLength();
					char* buf = (char*)fileContent.GetBufferSetLength(fileContent.GetLength());
					hr = pMultiLanguage->DetectInputCodepage(0, 0, (char*)pBuffer, (int*)&dwBytesRead, encodingInfos, &scores);
					codepageAvailable = true;
				}



				//Unicode
				if ((encodingInfos[0].nCodePage == 1200) || (encodingInfos[0].nCodePage == 1201))
				{
					fileContent += CString((wchar_t*)buffer, dwBytesRead / 2);
				}
				else
				{
					pByteBuffer[dwBytesRead] = 0;
	                fileContent += buffer;
				}
            }

			//Remove the BOM for UTF-8
			if (((BYTE)fileContent.GetAt(0) == 0x3F) && ((BYTE)fileContent.GetAt(1) == 0xBB) && ((BYTE)fileContent.GetAt(2) == 0x57))
			{
				fileContent.Delete(0, 3);
			}
            // Read error        
            if (!bRead)
            {
		        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_READ_FILE, "Filter::ParseFilters - Read")
            }
            else
            {
				isRead = true;

				UINT dstSize = 0;
				BYTE* buf = (BYTE*)fileContent.GetString();
				UINT srcLength = fileContent.GetLength() * 2;
				hr = pMultiLanguage->ConvertString(NULL, encodingInfos[scores - 1].nCodePage, 1252, (BYTE*)buf, &srcLength, NULL, &dstSize);
				char* bufferTmp = new char[dstSize + 1];
				hr = pMultiLanguage->ConvertString(NULL, encodingInfos[scores - 1].nCodePage, 1252, (BYTE*)buf, &srcLength, (BYTE*)bufferTmp, &dstSize);
				bufferTmp[dstSize] = 0;
				//Unicode 
				if ((encodingInfos[0].nCodePage == 1200) || (encodingInfos[0].nCodePage == 1201))
				{
					//remove BOM for Unicode
					fileContent = (char*)bufferTmp;
					fileContent.Delete(0, 1);

				}
				else 
				{
					wchar_t* fileContentBuffer = fileContent.GetBufferSetLength(dstSize);
					memcpy(fileContentBuffer, bufferTmp, dstSize);
				}
				delete [] bufferTmp;

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
					//Anything we do not support here
					else if (filter.Find(L"*") == 0)
					{
						filterType = CFilter::filterTypeUnknown;
					}
				    // Else, it is a general rule
				    else
				    {
					    filterType = CFilter::filterTypeBlocking;
				    }

					try
					{
						// Element hiding not supported yet
						if (filterType == CFilter::filterTypeElementHide)
						{ 
//							if ((filter.Find('[') < 0) && (filter.Find('^') < 0))
//							{
								AddFilterElementHide(filter, filename);
//							}
						}
						else if (filterType != CFilter::filterTypeUnknown)
						{
//							if ((filter.Find('[') < 0) && (filter.Find('^') < 0))
//							{
								AddFilter(filter, filename, filterType);
//							}
						}
					}
					catch(...)
					{
						//just ignore all errors we might get when adding filters
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
//    ReadFilter(PERSONAL_FILTER_FILE);
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
			CString loweredDomain = srcDomain;
			int domainPos = src.Find(loweredDomain.MakeLower());
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


	CPluginSettings* settings = CPluginSettings::GetInstance();

 
	if (!settings->GetBool(SETTING_PLUGIN_REGISTRATION, false))
	{
		//is the limit exceeded?
		if ((settings->GetValue(SETTING_PLUGIN_ADBLOCKCOUNT, 0) >= settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0)) 
			&& (settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0) > 0))
		{
			return false;
		} 
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


	if (blockFilter)
	{
		//is plugin registered
		if (!settings->GetBool(SETTING_PLUGIN_REGISTRATION, false))
		{
			//is the limit exceeded? When the upgrade dialog is displayed adblockcount is set to 1000000
			if ((settings->GetValue(SETTING_PLUGIN_ADBLOCKCOUNT, 0) >= settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0))
				&& (settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0) > 0))
			{
				return false;
			} 
		}
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
    CPluginFilterLock lock("filter1.txt");
    if (lock.IsLocked())
    {
        // Check file existence
        std::ifstream is;
	    is.open(CPluginSettings::GetDataPath("filter1.txt"), std::ios_base::in);
	    if (is.is_open())
	    {
            is.close();
            return;
        }

        // Open file
        HANDLE hFile = ::CreateFile(CPluginSettings::GetDataPath("filter1.txt"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);  
        if (hFile == INVALID_HANDLE_VALUE)
        {
		    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_FILTER, PLUGIN_ERROR_FILTER_CREATE_FILE_OPEN, "Filter::Create - CreateFile");
        }
        else
        {
            // Build filter string
            CStringA line;
	line += "[Adblock Plus 1.1]\r\n";
	line += "! Checksum: 0xN4e5SegJGeZuYZQGhShQ\r\n";
	line += "! EasyList - https://easylist.adblockplus.org/\r\n";
	line += "! Last modified:  4 Nov 2010 16:30 UTC\r\n";
	line += "! Expires: 5 days (update frequency)\r\n";
	line += "! Licence: https://easylist-downloads.adblockplus.org/COPYING\r\n";
	line += "!\r\n";
	line += "! Please report any unblocked adverts or problems\r\n";
	line += "! in the forums (http://forums.lanik.us/)\r\n";
	line += "! or via e-mail (easylist.subscription@gmail.com).\r\n";
	line += "!\r\n";
	line += "!-----------------General advert blocking filters-----------------!\r\n";
	line += "! *** easylist_general_block.txt ***\r\n";
	line += "&ad_keyword=\r\n";
	line += "&ad_type_\r\n";
	line += "&adname=\r\n";
	line += "&adspace=\r\n";
	line += "&adtype=\r\n";
	line += "&advertiserid=\r\n";
	line += "&clientype=*&adid=\r\n";
	line += "&googleadword=\r\n";
	line += "&program=revshare&\r\n";
	line += "+adverts/\r\n";
	line += "-ad-large.\r\n";
	line += "-ad-loading.\r\n";
	line += "-ad-manager/\r\n";
	line += "-ad-util-\r\n";
	line += "-ad1.jpg\r\n";
	line += "-adhelper.\r\n";
	line += "-banner-ads/\r\n";
	line += "-bin/ad_\r\n";
	line += "-content/adsys/\r\n";
	line += "-contest-ad.\r\n";
	line += "-leaderboard-ad-\r\n";
	line += "-rebels-ads/\r\n";
	line += "-text-ads.\r\n";
	line += "-third-ad.\r\n";
	line += ".ad.footer.\r\n";
	line += ".ad.page.\r\n";
	line += ".ad.premiere.\r\n";
	line += ".adplacement=\r\n";
	line += ".adriver.\r\n";
	line += ".ads.darla.\r\n";
	line += ".adserv/*\r\n";
	line += ".adserver.\r\n";
	line += ".aspx?ad=\r\n";
	line += ".aspx?zoneid=*&task=\r\n";
	line += ".au/_ads/\r\n";
	line += ".au/ads/\r\n";
	line += ".ca/ads/\r\n";
	line += ".cc/ads/\r\n";
	line += ".com/ad-\r\n";
	line += ".com/ad.\r\n";
	line += ".com/ad/\r\n";
	line += ".com/ad1.\r\n";
	line += ".com/ad2.\r\n";
	line += ".com/ad2/\r\n";
	line += ".com/ad?\r\n";
	line += ".com/ad_\r\n";
	line += ".com/adlib/\r\n";
	line += ".com/adops/\r\n";
	line += ".com/ads-\r\n";
	line += ".com/ads.\r\n";
	line += ".com/ads/$image,object,subdocument\r\n";
	line += ".com/ads?\r\n";
	line += ".com/ads_\r\n";
	line += ".com/advt/\r\n";
	line += ".com/adx/\r\n";
	line += ".com/gad/\r\n";
	line += ".com/gads/\r\n";
	line += ".com/openx/\r\n";
	line += ".com/ss/ad/\r\n";
	line += ".html?ad=\r\n";
	line += ".in/ads/\r\n";
	line += ".info/ads/\r\n";
	line += ".jp/ads/\r\n";
	line += ".net/_ads/\r\n";
	line += ".net/ad/$~object_subrequest\r\n";
	line += ".net/ads/\r\n";
	line += ".net/ads2/\r\n";
	line += ".net/ads3/\r\n";
	line += ".net/ads_\r\n";
	line += ".nz/ads/\r\n";
	line += ".org/ad/\r\n";
	line += ".org/ad2_\r\n";
	line += ".org/ad_\r\n";
	line += ".org/ads/\r\n";
	line += ".org/gads/\r\n";
	line += ".php?bannerid=\r\n";
	line += ".php?zoneid=*&loc=\r\n";
	line += ".swf?clicktag=\r\n";
	line += ".to/ad.php|\r\n";
	line += ".to/ads/\r\n";
	line += ".tv/ads/\r\n";
	line += ".uk/ads/\r\n";
	line += ".us/ads/\r\n";
	line += ".za/ads/\r\n";
	line += "/*;cue=pre;$object_subrequest\r\n";
	line += "/120ad.gif\r\n";
	line += "/468ad.gif\r\n";
	line += "/468xads.\r\n";
	line += "/?addyn|*\r\n";
	line += "/?advideo/*\r\n";
	line += "/a2/ads/*\r\n";
	line += "/aamsz=*/acc_random=\r\n";
	line += "/aamsz=*/pageid=\r\n";
	line += "/aamsz=*/position=\r\n";
	line += "/abm.asp?z=\r\n";
	line += "/abmw.asp?z=\r\n";
	line += "/abmw.aspx\r\n";
	line += "/acc_random=*/aamsz=\r\n";
	line += "/ad-468-\r\n";
	line += "/ad-amz.\r\n";
	line += "/ad-banner-\r\n";
	line += "/ad-box-\r\n";
	line += "/ad-cdn.\r\n";
	line += "/ad-frame.\r\n";
	line += "/ad-header.\r\n";
	line += "/ad-hug.\r\n";
	line += "/ad-iframe-\r\n";
	line += "/ad-inject/*\r\n";
	line += "/ad-leaderboard.\r\n";
	line += "/ad-letter.\r\n";
	line += "/ad-loader-\r\n";
	line += "/ad-local.\r\n";
	line += "/ad-managment/*\r\n";
	line += "/ad-server/*\r\n";
	line += "/ad-template/*\r\n";
	line += "/ad-top-\r\n";
	line += "/ad-topbanner-\r\n";
	line += "/ad-vert.\r\n";
	line += "/ad-vertical-\r\n";
	line += "/ad.asp?\r\n";
	line += "/ad.cgi?\r\n";
	line += "/ad.css?\r\n";
	line += "/ad.epl?\r\n";
	line += "/ad.jsp?\r\n";
	line += "/ad.mason?\r\n";
	line += "/ad.php?\r\n";
	line += "/ad/files/*\r\n";
	line += "/ad/iframe/*\r\n";
	line += "/ad/img/*\r\n";
	line += "/ad/script/*\r\n";
	line += "/ad/side_\r\n";
	line += "/ad/takeover/*\r\n";
	line += "/ad1.html\r\n";
	line += "/ad160.php\r\n";
	line += "/ad160x600.\r\n";
	line += "/ad1place.\r\n";
	line += "/ad1x1home.\r\n";
	line += "/ad2.aspx\r\n";
	line += "/ad2.html\r\n";
	line += "/ad2border.\r\n";
	line += "/ad300.htm\r\n";
	line += "/ad300x145.\r\n";
	line += "/ad350.html\r\n";
	line += "/ad728.php\r\n";
	line += "/ad728x15.\r\n";
	line += "/ad?count=\r\n";
	line += "/ad_agency/*\r\n";
	line += "/ad_area.\r\n";
	line += "/ad_banner.\r\n";
	line += "/ad_banner/*\r\n";
	line += "/ad_banner_\r\n";
	line += "/ad_bottom.\r\n";
	line += "/ad_bsb.\r\n";
	line += "/ad_campaigns/*\r\n";
	line += "/ad_code.\r\n";
	line += "/ad_configuration.\r\n";
	line += "/ad_content.\r\n";
	line += "/ad_count.\r\n";
	line += "/ad_creatives.\r\n";
	line += "/ad_editorials_\r\n";
	line += "/ad_engine?\r\n";
	line += "/ad_feed.js?\r\n";
	line += "/ad_footer.\r\n";
	line += "/ad_forum_\r\n";
	line += "/ad_frame.\r\n";
	line += "/ad_function.\r\n";
	line += "/ad_gif/*\r\n";
	line += "/ad_google.\r\n";
	line += "/ad_header_\r\n";
	line += "/ad_holder/*\r\n";
	line += "/ad_horizontal.\r\n";
	line += "/ad_html/*\r\n";
	line += "/ad_iframe.\r\n";
	line += "/ad_iframe_\r\n";
	line += "/ad_insert.\r\n";
	line += "/ad_jnaught/*\r\n";
	line += "/ad_label_\r\n";
	line += "/ad_leader.\r\n";
	line += "/ad_left.\r\n";
	line += "/ad_legend_\r\n";
	line += "/ad_link.\r\n";
	line += "/ad_load.\r\n";
	line += "/ad_manager.\r\n";
	line += "/ad_mpu.\r\n";
	line += "/ad_notice.\r\n";
	line += "/ad_os.php?\r\n";
	line += "/ad_page_\r\n";
	line += "/ad_print.\r\n";
	line += "/ad_rectangle_\r\n";
	line += "/ad_refresher.\r\n";
	line += "/ad_reloader_\r\n";
	line += "/ad_right.\r\n";
	line += "/ad_rotation.\r\n";
	line += "/ad_rotator.\r\n";
	line += "/ad_rotator_\r\n";
	line += "/ad_script.\r\n";
	line += "/ad_serv.\r\n";
	line += "/ad_serve.\r\n";
	line += "/ad_server.\r\n";
	line += "/ad_server/*\r\n";
	line += "/ad_sizes=\r\n";
	line += "/ad_skin_\r\n";
	line += "/ad_sky.\r\n";
	line += "/ad_skyscraper.\r\n";
	line += "/ad_slideout.\r\n";
	line += "/ad_space.\r\n";
	line += "/ad_square.\r\n";
	line += "/ad_supertile/*\r\n";
	line += "/ad_tag.\r\n";
	line += "/ad_tag_\r\n";
	line += "/ad_tags_\r\n";
	line += "/ad_tile/*\r\n";
	line += "/ad_title_\r\n";
	line += "/ad_top.\r\n";
	line += "/ad_topgray2.\r\n";
	line += "/ad_tpl.\r\n";
	line += "/ad_upload/*\r\n";
	line += "/ad_vert.\r\n";
	line += "/ad_vertical.\r\n";
	line += "/ad_view_\r\n";
	line += "/adaffiliate_\r\n";
	line += "/adanim/*\r\n";
	line += "/adaptvadplayer.\r\n";
	line += "/adbanner.\r\n";
	line += "/adbanner/*\r\n";
	line += "/adbanner_\r\n";
	line += "/adbanners/*\r\n";
	line += "/adbar.aspx\r\n";
	line += "/adbg.jpg\r\n";
	line += "/adbot_promos/*\r\n";
	line += "/adbottom.\r\n";
	line += "/adbox.gif\r\n";
	line += "/adbox.js\r\n";
	line += "/adbox.php\r\n";
	line += "/adbrite.\r\n";
	line += "/adbureau.\r\n";
	line += "/adcde.js\r\n";
	line += "/adcell/*\r\n";
	line += "/adcentral.\r\n";
	line += "/adchannel_\r\n";
	line += "/adclick.\r\n";
	line += "/adclient.\r\n";
	line += "/adclient/*\r\n";
	line += "/adclutter.\r\n";
	line += "/adcode.\r\n";
	line += "/adcode/*\r\n";
	line += "/adcodes/*\r\n";
	line += "/adcollector.\r\n";
	line += "/adcomponent/*\r\n";
	line += "/adconfig.js\r\n";
	line += "/adconfig.xml?\r\n";
	line += "/adconfig/*\r\n";
	line += "/adcontent.$~object_subrequest\r\n";
	line += "/adcontroller.\r\n";
	line += "/adcreative.\r\n";
	line += "/adcycle.\r\n";
	line += "/adcycle/*\r\n";
	line += "/addeals/*\r\n";
	line += "/addelivery/*\r\n";
	line += "/addyn/3.0/*\r\n";
	line += "/addyn|*|adtech;\r\n";
	line += "/adengage1.\r\n";
	line += "/adengage_\r\n";
	line += "/adengine/*\r\n";
	line += "/adexclude/*\r\n";
	line += "/adf.cgi?\r\n";
	line += "/adfactory.\r\n";
	line += "/adfarm.\r\n";
	line += "/adfetch?\r\n";
	line += "/adfetcher?\r\n";
	line += "/adfever_\r\n";
	line += "/adfile/*\r\n";
	line += "/adfooter.\r\n";
	line += "/adframe.\r\n";
	line += "/adframe/*\r\n";
	line += "/adframe?\r\n";
	line += "/adframe_\r\n";
	line += "/adframebottom.\r\n";
	line += "/adframemiddle.\r\n";
	line += "/adframetop.\r\n";
	line += "/adfshow?\r\n";
	line += "/adfunction.\r\n";
	line += "/adfunctions.\r\n";
	line += "/adgitize-\r\n";
	line += "/adgraphics/*\r\n";
	line += "/adguru.\r\n";
	line += "/adhalfbanner.\r\n";
	line += "/adhandler.\r\n";
	line += "/adhandler/*\r\n";
	line += "/adheader.\r\n";
	line += "/adheadertxt.\r\n";
	line += "/adhomepage.\r\n";
	line += "/adhtml/*\r\n";
	line += "/adhug_jc.\r\n";
	line += "/adiframe.\r\n";
	line += "/adiframe/*\r\n";
	line += "/adify_box.\r\n";
	line += "/adify_leader.\r\n";
	line += "/adify_sky.\r\n";
	line += "/adimage.\r\n";
	line += "/adimages.\r\n";
	line += "/adimages/*$~subdocument\r\n";
	line += "/adindex/*\r\n";
	line += "/adinjector.\r\n";
	line += "/adinsert.\r\n";
	line += "/adinterax.\r\n";
	line += "/adjs.php?\r\n";
	line += "/adjsmp.\r\n";
	line += "/adlabel.\r\n";
	line += "/adlabel_\r\n";
	line += "/adlayer.\r\n";
	line += "/adlayer/*\r\n";
	line += "/adleader.\r\n";
	line += "/adlink-\r\n";
	line += "/adlink.\r\n";
	line += "/adlink_\r\n";
	line += "/adlinks.\r\n";
	line += "/adlist_\r\n";
	line += "/adloader.\r\n";
	line += "/adm/ad/*\r\n";
	line += "/adman.js\r\n";
	line += "/adman/image/*\r\n";
	line += "/adman/www/*\r\n";
	line += "/admanagement/*\r\n";
	line += "/admanagementadvanced.\r\n";
	line += "/admanager.$~object_subrequest\r\n";
	line += "/admanager/*$~object_subrequest\r\n";
	line += "/admanager3.\r\n";
	line += "/admanagers/*\r\n";
	line += "/admanagerstatus/*\r\n";
	line += "/admarket/*\r\n";
	line += "/admaster.\r\n";
	line += "/admaster?\r\n";
	line += "/admedia.\r\n";
	line += "/admedia/*\r\n";
	line += "/admega.\r\n";
	line += "/admentor/*\r\n";
	line += "/admicro2.\r\n";
	line += "/admicro_\r\n";
	line += "/admin/ad_\r\n";
	line += "/adnetmedia.\r\n";
	line += "/adnetwork.\r\n";
	line += "/adnews.\r\n";
	line += "/adnext.\r\n";
	line += "/adng.html\r\n";
	line += "/adnotice.\r\n";
	line += "/adonline.\r\n";
	line += "/adotubeplugin.\r\n";
	line += "/adp.htm\r\n";
	line += "/adpage.\r\n";
	line += "/adpage/*\r\n";
	line += "/adpartner.\r\n";
	line += "/adpeeps.\r\n";
	line += "/adpeeps/*\r\n";
	line += "/adplayer.\r\n";
	line += "/adplayer/*\r\n";
	line += "/adplugin.\r\n";
	line += "/adpoint.\r\n";
	line += "/adpool/*\r\n";
	line += "/adpopup.\r\n";
	line += "/adproducts/*\r\n";
	line += "/adproxy.\r\n";
	line += "/adproxy/*\r\n";
	line += "/adrelated.\r\n";
	line += "/adreload?\r\n";
	line += "/adremote.\r\n";
	line += "/adrevenue/*\r\n";
	line += "/adrevolver/*\r\n";
	line += "/adriver.\r\n";
	line += "/adriver_\r\n";
	line += "/adrolays.\r\n";
	line += "/adroller.\r\n";
	line += "/adroot/*\r\n";
	line += "/adrot.js\r\n";
	line += "/adrotator/*\r\n";
	line += "/adrotv2.\r\n";
	line += "/adruptive.\r\n";
	line += "/ads-banner.\r\n";
	line += "/ads-common.\r\n";
	line += "/ads-footer.\r\n";
	line += "/ads-leader|\r\n";
	line += "/ads-rectangle.\r\n";
	line += "/ads-rec|\r\n";
	line += "/ads-right.\r\n";
	line += "/ads-service.\r\n";
	line += "/ads-skyscraper.\r\n";
	line += "/ads-sky|\r\n";
	line += "/ads.asp?\r\n";
	line += "/ads.dll/*\r\n";
	line += "/ads.htm\r\n";
	line += "/ads.jsp\r\n";
	line += "/ads.php?\r\n";
	line += "/ads.pl?\r\n";
	line += "/ads/2010/*\r\n";
	line += "/ads/cnvideo/*\r\n";
	line += "/ads/common/*\r\n";
	line += "/ads/footer_\r\n";
	line += "/ads/freewheel/*\r\n";
	line += "/ads/home/*\r\n";
	line += "/ads/house/*\r\n";
	line += "/ads/images/*\r\n";
	line += "/ads/interstitial/*\r\n";
	line += "/ads/labels/*\r\n";
	line += "/ads/layer.\r\n";
	line += "/ads/leaderboard_\r\n";
	line += "/ads/preloader/*\r\n";
	line += "/ads/preroll_\r\n";
	line += "/ads/promo_\r\n";
	line += "/ads/rectangle_\r\n";
	line += "/ads/sponsor_\r\n";
	line += "/ads/square-\r\n";
	line += "/ads/third-party/*\r\n";
	line += "/ads09a/*\r\n";
	line += "/ads2.html\r\n";
	line += "/ads2.php\r\n";
	line += "/ads2_2.\r\n";
	line += "/ads2_header.\r\n";
	line += "/ads9.dll\r\n";
	line += "/ads?id=\r\n";
	line += "/ads_code.\r\n";
	line += "/ads_global.\r\n";
	line += "/ads_google.\r\n";
	line += "/ads_iframe.\r\n";
	line += "/ads_openx/*\r\n";
	line += "/ads_php/*\r\n";
	line += "/ads_reporting/*\r\n";
	line += "/ads_yahoo.\r\n";
	line += "/adsa468.\r\n";
	line += "/adsa728.\r\n";
	line += "/adsadview.\r\n";
	line += "/adsales/*\r\n";
	line += "/adsatt.\r\n";
	line += "/adsbanner.\r\n";
	line += "/adsbannerjs.\r\n";
	line += "/adscale_\r\n";
	line += "/adscluster.\r\n";
	line += "/adscontent.\r\n";
	line += "/adscontent2.\r\n";
	line += "/adscript.\r\n";
	line += "/adscript_\r\n";
	line += "/adscripts/*\r\n";
	line += "/adscroll.\r\n";
	line += "/adsdaqbanner_\r\n";
	line += "/adsdaqbox_\r\n";
	line += "/adsdaqsky_\r\n";
	line += "/adsearch.\r\n";
	line += "/adsense-\r\n";
	line += "/adsense.\r\n";
	line += "/adsense/*\r\n";
	line += "/adsense23.\r\n";
	line += "/adsense24.\r\n";
	line += "/adsense?\r\n";
	line += "/adsense_\r\n";
	line += "/adsensegb.\r\n";
	line += "/adsensegoogle.\r\n";
	line += "/adsensets.\r\n";
	line += "/adserv.\r\n";
	line += "/adserv2.\r\n";
	line += "/adserve.\r\n";
	line += "/adserve/*\r\n";
	line += "/adserver.\r\n";
	line += "/adserver/*\r\n";
	line += "/adserver2.\r\n";
	line += "/adserver2/*\r\n";
	line += "/adserver?\r\n";
	line += "/adserver_\r\n";
	line += "/adserversolutions/*\r\n";
	line += "/adservice|\r\n";
	line += "/adserving.\r\n";
	line += "/adsfac.\r\n";
	line += "/adsfetch.\r\n";
	line += "/adsfile.\r\n";
	line += "/adsfolder/*\r\n";
	line += "/adsframe.\r\n";
	line += "/adshandler.\r\n";
	line += "/adsheader.\r\n";
	line += "/adshow.\r\n";
	line += "/adshow?\r\n";
	line += "/adshow_\r\n";
	line += "/adsiframe/*\r\n";
	line += "/adsign.\r\n";
	line += "/adsimage/*\r\n";
	line += "/adsinclude.\r\n";
	line += "/adsinsert.\r\n";
	line += "/adsky.php\r\n";
	line += "/adskyright.\r\n";
	line += "/adskyscraper.\r\n";
	line += "/adslots.\r\n";
	line += "/adslug-\r\n";
	line += "/adslug_\r\n";
	line += "/adsmanagement/*\r\n";
	line += "/adsmanager/*\r\n";
	line += "/adsmedia_\r\n";
	line += "/adsnew.\r\n";
	line += "/adsonar.\r\n";
	line += "/adsopenx/*\r\n";
	line += "/adspace.\r\n";
	line += "/adspace/*\r\n";
	line += "/adspaces.\r\n";
	line += "/adsponsor.\r\n";
	line += "/adspro/*\r\n";
	line += "/adsquare.\r\n";
	line += "/adsremote.\r\n";
	line += "/adsreporting/*\r\n";
	line += "/adsrich.\r\n";
	line += "/adsright.\r\n";
	line += "/adsrule.\r\n";
	line += "/adsserv.\r\n";
	line += "/adssrv.\r\n";
	line += "/adstemplate/*\r\n";
	line += "/adstorage.\r\n";
	line += "/adstracking.\r\n";
	line += "/adstream.\r\n";
	line += "/adstream_\r\n";
	line += "/adstub.\r\n";
	line += "/adstubs/*\r\n";
	line += "/adswap.\r\n";
	line += "/adswap/*\r\n";
	line += "/adswide.\r\n";
	line += "/adswidejs.\r\n";
	line += "/adswrapper.\r\n";
	line += "/adsx728.\r\n";
	line += "/adsx_728.\r\n";
	line += "/adsync/*\r\n";
	line += "/adsyndication.\r\n";
	line += "/adsys/ads.\r\n";
	line += "/adsystem/*\r\n";
	line += "/adtag/type/*\r\n";
	line += "/adtago.\r\n";
	line += "/adtags.\r\n";
	line += "/adtags/*\r\n";
	line += "/adtagtc.\r\n";
	line += "/adtagtranslator.\r\n";
	line += "/adtech.\r\n";
	line += "/adtech/*\r\n";
	line += "/adtech;\r\n";
	line += "/adtech_\r\n";
	line += "/adtext.\r\n";
	line += "/adtext4.\r\n";
	line += "/adtext_\r\n";
	line += "/adtitle.\r\n";
	line += "/adtology.\r\n";
	line += "/adtonomy.\r\n";
	line += "/adtop.do\r\n";
	line += "/adtop.js\r\n";
	line += "/adtrack/*\r\n";
	line += "/adtraff.\r\n";
	line += "/adtvideo.\r\n";
	line += "/adtype.\r\n";
	line += "/adunit.\r\n";
	line += "/adunits/*\r\n";
	line += "/adv/ads/*\r\n";
	line += "/adverserve.\r\n";
	line += "/advert-$~stylesheet\r\n";
	line += "/advert.$domain=~kp.ru\r\n";
	line += "/advert/*\r\n";
	line += "/advert?\r\n";
	line += "/advert_\r\n";
	line += "/advertise-\r\n";
	line += "/advertise.\r\n";
	line += "/advertise/*\r\n";
	line += "/advertisehere.\r\n";
	line += "/advertisement-\r\n";
	line += "/advertisement.\r\n";
	line += "/advertisement/*\r\n";
	line += "/advertisement2.\r\n";
	line += "/advertisement_\r\n";
	line += "/advertisementheader.\r\n";
	line += "/advertisementrotation.\r\n";
	line += "/advertisements.\r\n";
	line += "/advertisements/*\r\n";
	line += "/advertisementview/*\r\n";
	line += "/advertiser/*\r\n";
	line += "/advertisers/*\r\n";
	line += "/advertising.\r\n";
	line += "/advertising/*$~object,~object_subrequest\r\n";
	line += "/advertising2.\r\n";
	line += "/advertising_\r\n";
	line += "/advertisingcontent/*\r\n";
	line += "/advertisingmanual.\r\n";
	line += "/advertisingmodule.\r\n";
	line += "/advertisment.\r\n";
	line += "/advertize_\r\n";
	line += "/advertmedia/*\r\n";
	line += "/advertorials/*\r\n";
	line += "/advertphp/*\r\n";
	line += "/advertpro/*\r\n";
	line += "/adverts.\r\n";
	line += "/adverts/*\r\n";
	line += "/adverts_\r\n";
	line += "/adview.\r\n";
	line += "/adview?\r\n";
	line += "/adviewer.\r\n";
	line += "/advision.\r\n";
	line += "/advolatility.\r\n";
	line += "/advpartnerinit.\r\n";
	line += "/adwords/*\r\n";
	line += "/adworks.\r\n";
	line += "/adworks/*\r\n";
	line += "/adwrapper/*\r\n";
	line += "/adwrapperiframe.\r\n";
	line += "/adxx.php?\r\n";
	line += "/adzone.\r\n";
	line += "/adzones/*\r\n";
	line += "/aff_frame.\r\n";
	line += "/affad?q=\r\n";
	line += "/affads/*\r\n";
	line += "/affclick/*\r\n";
	line += "/affilatebanner.\r\n";
	line += "/affiliate/banners/*\r\n";
	line += "/affiliate/script.php?\r\n";
	line += "/affiliate_banners/*\r\n";
	line += "/affiliate_resources/*\r\n";
	line += "/affiliatebanner/*\r\n";
	line += "/affiliatebanners/*\r\n";
	line += "/affiliateimages/*\r\n";
	line += "/affiliatelinks.\r\n";
	line += "/affiliates.*.aspx?\r\n";
	line += "/affiliates/banner\r\n";
	line += "/affiliatewiz/*\r\n";
	line += "/affiliation/*\r\n";
	line += "/affiliationcash.\r\n";
	line += "/affilinet/ads/*\r\n";
	line += "/afimages.\r\n";
	line += "/afr.php?\r\n";
	line += "/ajax/ads/*\r\n";
	line += "/ajrotator/*\r\n";
	line += "/ajs.php?\r\n";
	line += "/annonser/*\r\n";
	line += "/api/ads/*\r\n";
	line += "/app/ads.js\r\n";
	line += "/article_ad.\r\n";
	line += "/as3adplayer.\r\n";
	line += "/aseadnshow.\r\n";
	line += "/aspbanner_inc.asp?\r\n";
	line += "/assets/ads/*\r\n";
	line += "/audioads/*\r\n";
	line += "/auditudeadunit.\r\n";
	line += "/austria_ad.\r\n";
	line += "/auto_ad_\r\n";
	line += "/back-ad.\r\n";
	line += "/ban_m.php?\r\n";
	line += "/banimpress.\r\n";
	line += "/banman.asp?\r\n";
	line += "/banman/*\r\n";
	line += "/banner-ad-\r\n";
	line += "/banner-ad/*\r\n";
	line += "/banner/ad_\r\n";
	line += "/banner/affiliate/*\r\n";
	line += "/banner_468.\r\n";
	line += "/banner_ad.\r\n";
	line += "/banner_ad_\r\n";
	line += "/banner_ads.\r\n";
	line += "/banner_ads_\r\n";
	line += "/banner_adv/*\r\n";
	line += "/banner_advert/*\r\n";
	line += "/banner_control.php?\r\n";
	line += "/banner_db.php?\r\n";
	line += "/banner_file.php?\r\n";
	line += "/banner_js.*?\r\n";
	line += "/banner_management/*\r\n";
	line += "/banner_skyscraper.\r\n";
	line += "/bannerad.\r\n";
	line += "/bannerad_\r\n";
	line += "/bannerads-\r\n";
	line += "/bannerads/*\r\n";
	line += "/banneradviva.\r\n";
	line += "/bannercode.php\r\n";
	line += "/bannerconduit.swf?\r\n";
	line += "/bannerexchange/*\r\n";
	line += "/bannerfarm/*\r\n";
	line += "/bannerframe.*?\r\n";
	line += "/bannerframeopenads.\r\n";
	line += "/bannerframeopenads_\r\n";
	line += "/bannermanager/*\r\n";
	line += "/bannermedia/*\r\n";
	line += "/bannerrotation.\r\n";
	line += "/bannerrotation/*\r\n";
	line += "/banners.*&iframe=\r\n";
	line += "/banners/affiliate/*\r\n";
	line += "/banners/promo/*\r\n";
	line += "/banners_rotation.\r\n";
	line += "/bannerscript/*\r\n";
	line += "/bannerserver/*\r\n";
	line += "/bannersyndication.\r\n";
	line += "/bannerview.*?\r\n";
	line += "/bannery/*?banner=\r\n";
	line += "/bar-ad.\r\n";
	line += "/baselinead.\r\n";
	line += "/basic/ad/*\r\n";
	line += "/behaviorads/*\r\n";
	line += "/beta-ad.\r\n";
	line += "/bg/ads/*\r\n";
	line += "/bg_ads_\r\n";
	line += "/bi_affiliate.js\r\n";
	line += "/bigad.p\r\n";
	line += "/bigboxad.\r\n";
	line += "/bkgrndads/*\r\n";
	line += "/blogad_\r\n";
	line += "/blogads.\r\n";
	line += "/blogads/*\r\n";
	line += "/blogads3/*\r\n";
	line += "/bmndoubleclickad.\r\n";
	line += "/bnrsrv.*?\r\n";
	line += "/bodyads/*\r\n";
	line += "/boomad.\r\n";
	line += "/bottom_ad.\r\n";
	line += "/bottomad.\r\n";
	line += "/bottomad/*\r\n";
	line += "/btn_ad_\r\n";
	line += "/bucketads.\r\n";
	line += "/butler.php?type=\r\n";
	line += "/buttonads.\r\n";
	line += "/buyad.html\r\n";
	line += "/buyclicks/*\r\n";
	line += "/buysellads.\r\n";
	line += "/bw_adsys.\r\n";
	line += "/bytemark_ad.\r\n";
	line += "/campus/ads/*\r\n";
	line += "/cashad.\r\n";
	line += "/cashad2.\r\n";
	line += "/central/ads/*\r\n";
	line += "/cgi-bin/ads/*\r\n";
	line += "/channelblockads.\r\n";
	line += "/chinaadclient.\r\n";
	line += "/chitika-ad?\r\n";
	line += "/circads.\r\n";
	line += "/cms/js/ad_\r\n";
	line += "/cnnslads.\r\n";
	line += "/cnwk.1d/ads/*\r\n";
	line += "/coldseal_ad.\r\n";
	line += "/commercial_horizontal.\r\n";
	line += "/commercial_top.\r\n";
	line += "/commercials/*\r\n";
	line += "/common/ad/*\r\n";
	line += "/common/ads/*\r\n";
	line += "/companion_ads.\r\n";
	line += "/content/ad_\r\n";
	line += "/content_ad.\r\n";
	line += "/content_ad_\r\n";
	line += "/contentadxxl.\r\n";
	line += "/contentad|\r\n";
	line += "/contextad.\r\n";
	line += "/controller/ads/*\r\n";
	line += "/corner_ads/*\r\n";
	line += "/country_ad.\r\n";
	line += "/cpxads.\r\n";
	line += "/ctamlive160x160.\r\n";
	line += "/customad.\r\n";
	line += "/customadsense.\r\n";
	line += "/cvs/ads/*\r\n";
	line += "/cwggoogleadshow.\r\n";
	line += "/dart_ads/*\r\n";
	line += "/dartads.\r\n";
	line += "/dateads.\r\n";
	line += "/dclk_ads.\r\n";
	line += "/dclk_ads_\r\n";
	line += "/dcloadads/*\r\n";
	line += "/de/ads/*\r\n";
	line += "/defer_ads.\r\n";
	line += "/deferads.\r\n";
	line += "/deliver.nmi?\r\n";
	line += "/deliverad/*\r\n";
	line += "/deliverjs.nmi?\r\n";
	line += "/delivery/ag.php\r\n";
	line += "/delivery/al.php\r\n";
	line += "/delivery/apu.php\r\n";
	line += "/delivery/avw.php\r\n";
	line += "/descpopup.js\r\n";
	line += "/direct_ads.\r\n";
	line += "/directads.\r\n";
	line += "/display_ads.\r\n";
	line += "/displayad.\r\n";
	line += "/displayad?\r\n";
	line += "/displayads/*\r\n";
	line += "/dne_ad.\r\n";
	line += "/dnsads.html?\r\n";
	line += "/doors/ads/*\r\n";
	line += "/doubleclick.phpi?\r\n";
	line += "/doubleclick/iframe.\r\n";
	line += "/doubleclick_ads.\r\n";
	line += "/doubleclick_ads/*\r\n";
	line += "/doubleclickcontainer.\r\n";
	line += "/doubleclicktag.\r\n";
	line += "/download/ad.\r\n";
	line += "/download/ad/*\r\n";
	line += "/drawad.php?\r\n";
	line += "/drivingrevenue/*\r\n";
	line += "/dsg/bnn/*\r\n";
	line += "/dxd/ads/*\r\n";
	line += "/dyn_banner.\r\n";
	line += "/dyn_banners_\r\n";
	line += "/dynamic/ads/*\r\n";
	line += "/dynamicad?\r\n";
	line += "/dynamiccsad?\r\n";
	line += "/dynamicvideoad?\r\n";
	line += "/dynanews/ad-\r\n";
	line += "/dynbanner/flash/*\r\n";
	line += "/ebay_ads/*\r\n";
	line += "/emailads/*\r\n";
	line += "/emediatead.\r\n";
	line += "/ext_ads.\r\n";
	line += "/external/ad.\r\n";
	line += "/external/ads/*\r\n";
	line += "/external_ads.\r\n";
	line += "/eyewondermanagement.\r\n";
	line += "/eyewondermanagement28.\r\n";
	line += "/fastclick160.\r\n";
	line += "/fastclick728.\r\n";
	line += "/fatads.\r\n";
	line += "/featuredadshome.\r\n";
	line += "/file/ad.\r\n";
	line += "/files/ad/*\r\n";
	line += "/files/ads/*\r\n";
	line += "/fimserve.\r\n";
	line += "/flash/ad_\r\n";
	line += "/flash/ads/*\r\n";
	line += "/flashad.\r\n";
	line += "/flashads.\r\n";
	line += "/flashads/*\r\n";
	line += "/footad-\r\n";
	line += "/footer-ad-\r\n";
	line += "/footer_ad_\r\n";
	line += "/framead-\r\n";
	line += "/framead.\r\n";
	line += "/framead/*\r\n";
	line += "/framead_\r\n";
	line += "/frequencyads.\r\n";
	line += "/frnads.\r\n";
	line += "/fuseads/*\r\n";
	line += "/gads.html\r\n";
	line += "/gads.js\r\n";
	line += "/gafsads?\r\n";
	line += "/galleryad.\r\n";
	line += "/gamead/*\r\n";
	line += "/gamersad.\r\n";
	line += "/genericrichmediabannerad/*\r\n";
	line += "/geo-ads_\r\n";
	line += "/geo/ads.\r\n";
	line += "/get-ad.\r\n";
	line += "/getad.aspx\r\n";
	line += "/getad.js\r\n";
	line += "/getad.php?\r\n";
	line += "/getad.php|\r\n";
	line += "/getad?n=\r\n";
	line += "/getadframe.\r\n";
	line += "/getads/*\r\n";
	line += "/getadvertimageservlet?\r\n";
	line += "/getarticleadvertimageservlet?\r\n";
	line += "/getbanner.cfm?\r\n";
	line += "/getsponslinks.\r\n";
	line += "/getsponslinksauto.\r\n";
	line += "/getvdopiaads.\r\n";
	line += "/getvideoad.\r\n";
	line += "/gexternalad.\r\n";
	line += "/gfx/ads/*\r\n";
	line += "/glam_ads.\r\n";
	line += "/google-ad?\r\n";
	line += "/google-ads.\r\n";
	line += "/google-adsense-\r\n";
	line += "/google-adsense.\r\n";
	line += "/google_ad_\r\n";
	line += "/google_ads.\r\n";
	line += "/google_ads/*\r\n";
	line += "/google_ads_\r\n";
	line += "/google_adsense.\r\n";
	line += "/google_adsense_\r\n";
	line += "/google_afc.\r\n";
	line += "/googlead-\r\n";
	line += "/googlead.\r\n";
	line += "/googlead_\r\n";
	line += "/googleadhtml/*\r\n";
	line += "/googleadright.\r\n";
	line += "/googleads-\r\n";
	line += "/googleads.\r\n";
	line += "/googleads2.\r\n";
	line += "/googleads3widetext.\r\n";
	line += "/googleads_\r\n";
	line += "/googleadsafs_\r\n";
	line += "/googleadsense.\r\n";
	line += "/googleafs.\r\n";
	line += "/graphics/ad_\r\n";
	line += "/gt6skyadtop.\r\n";
	line += "/header_ads_\r\n";
	line += "/headerads.\r\n";
	line += "/headvert.\r\n";
	line += "/hitbar_ad_\r\n";
	line += "/homepage_ads/*\r\n";
	line += "/house-ads/*\r\n";
	line += "/house_ad-\r\n";
	line += "/house_ad_\r\n";
	line += "/housead/*\r\n";
	line += "/houseads.\r\n";
	line += "/houseads/*\r\n";
	line += "/houseads?\r\n";
	line += "/hoverad.\r\n";
	line += "/html.ng/*\r\n";
	line += "/htmlads/*\r\n";
	line += "/httpads/*\r\n";
	line += "/icon_ad.\r\n";
	line += "/idevaffiliate/*\r\n";
	line += "/iframe-ads/*\r\n";
	line += "/iframe/ad/*\r\n";
	line += "/iframe_ad.\r\n";
	line += "/iframe_ads/*\r\n";
	line += "/iframe_ads?\r\n";
	line += "/iframe_chitika_\r\n";
	line += "/iframead.\r\n";
	line += "/iframead/*\r\n";
	line += "/iframead_\r\n";
	line += "/iframeadsense.\r\n";
	line += "/iframeadsensewrapper.\r\n";
	line += "/iframedartad.\r\n";
	line += "/imads.js\r\n";
	line += "/image/ads/*\r\n";
	line += "/image_ads/*\r\n";
	line += "/images/ad-\r\n";
	line += "/images/ad.\r\n";
	line += "/images/ad/*\r\n";
	line += "/images/ad1.\r\n";
	line += "/images/ad125.\r\n";
	line += "/images/ad2.\r\n";
	line += "/images/ad4.\r\n";
	line += "/images/ad5.\r\n";
	line += "/images/ad_\r\n";
	line += "/images/ads-\r\n";
	line += "/images/ads/*\r\n";
	line += "/images/ads_\r\n";
	line += "/images/aff-\r\n";
	line += "/images/gads_\r\n";
	line += "/images/sponsored/*\r\n";
	line += "/images_jtads/*\r\n";
	line += "/img/ad/*\r\n";
	line += "/img/ad_\r\n";
	line += "/img/ads/*\r\n";
	line += "/img/sponsor/*\r\n";
	line += "/img/topad_\r\n";
	line += "/imgs/ads/*\r\n";
	line += "/inad.php\r\n";
	line += "/inc/ads/*\r\n";
	line += "/include/ads/*\r\n";
	line += "/include/boxad_\r\n";
	line += "/include/skyad_\r\n";
	line += "/included_ads/*\r\n";
	line += "/includes/ad_\r\n";
	line += "/includes/ads/*\r\n";
	line += "/incmpuad.\r\n";
	line += "/index-ad.\r\n";
	line += "/index_ads.\r\n";
	line += "/inline_ad.\r\n";
	line += "/inline_ad_\r\n";
	line += "/innerads.\r\n";
	line += "/insertads.\r\n";
	line += "/instreamad/*\r\n";
	line += "/intellitext.js\r\n";
	line += "/interad.\r\n";
	line += "/intextads.\r\n";
	line += "/introduction_ad.\r\n";
	line += "/invideoad.\r\n";
	line += "/inx-ad.\r\n";
	line += "/ipadad.\r\n";
	line += "/irc_ad_\r\n";
	line += "/ireel/ad*.jpg\r\n";
	line += "/ispy/ads/*\r\n";
	line += "/iwadsense.\r\n";
	line += "/j/ads.js\r\n";
	line += "/jivoxadplayer.\r\n";
	line += "/jlist-affiliates/*\r\n";
	line += "/js/ads-\r\n";
	line += "/js/ads/*\r\n";
	line += "/js/ads_\r\n";
	line += "/jsadscripts/*\r\n";
	line += "/jsfiles/ads/*\r\n";
	line += "/keyade.js\r\n";
	line += "/kredit-ad.\r\n";
	line += "/label-advertisement.\r\n";
	line += "/layer-ad.\r\n";
	line += "/layer-ads.\r\n";
	line += "/layer/ad.\r\n";
	line += "/layer/ads.\r\n";
	line += "/layerads_\r\n";
	line += "/layout/ads/*\r\n";
	line += "/lbl_ad.\r\n";
	line += "/leader_ad.\r\n";
	line += "/leftad_\r\n";
	line += "/linkads.\r\n";
	line += "/links_sponsored_\r\n";
	line += "/linkshare/*\r\n";
	line += "/liveads.\r\n";
	line += "/loadad.aspx?\r\n";
	line += "/loadadwiz.\r\n";
	line += "/local_ads_\r\n";
	line += "/lotto_ad_\r\n";
	line += "/lrec_ad.\r\n";
	line += "/mac-ad?\r\n";
	line += "/magic-ads/*\r\n";
	line += "/main/ad_\r\n";
	line += "/main_ad.\r\n";
	line += "/main_ad/*\r\n";
	line += "/mainad.\r\n";
	line += "/mbn_ad.\r\n";
	line += "/mcad.php\r\n";
	line += "/media/ads/*\r\n";
	line += "/megaad.\r\n";
	line += "/metaadserver/*\r\n";
	line += "/mini-ads/*\r\n";
	line += "/mini_ads.\r\n";
	line += "/mint/ads/*\r\n";
	line += "/misc/ad-\r\n";
	line += "/miva_ads.\r\n";
	line += "/mnetorfad.js\r\n";
	line += "/mobile_ad.\r\n";
	line += "/mobilephonesad/*\r\n";
	line += "/mod/adman/*\r\n";
	line += "/mod_ad/*\r\n";
	line += "/modalad.\r\n";
	line += "/modules/ad_\r\n";
	line += "/modules/ads/*\r\n";
	line += "/mpumessage.\r\n";
	line += "/msnadimg.\r\n";
	line += "/mstextad?\r\n";
	line += "/mtvi_ads_\r\n";
	line += "/mylayer-ad/*\r\n";
	line += "/mysimpleads/*\r\n";
	line += "/neoads.\r\n";
	line += "/new/ad/*\r\n";
	line += "/newads.\r\n";
	line += "/newrightcolad.\r\n";
	line += "/news_ad.\r\n";
	line += "/newtopmsgad.\r\n";
	line += "/nextad/*\r\n";
	line += "/o2contentad.\r\n";
	line += "/oas_ad_\r\n";
	line += "/oasadframe.\r\n";
	line += "/oascentral.$~object_subrequest\r\n";
	line += "/oasdefault/*\r\n";
	line += "/oasisi-*.php?\r\n";
	line += "/oasisi.php?\r\n";
	line += "/oiopub-direct/*$~stylesheet\r\n";
	line += "/omb-ad-\r\n";
	line += "/online/ads/*\r\n";
	line += "/openads-\r\n";
	line += "/openads.\r\n";
	line += "/openads/*\r\n";
	line += "/openads2/*\r\n";
	line += "/openx/www/*\r\n";
	line += "/openx_fl.\r\n";
	line += "/other/ads/*\r\n";
	line += "/overlay_ad_\r\n";
	line += "/ovt_show.asp?\r\n";
	line += "/page-ads.\r\n";
	line += "/pagead/ads?\r\n";
	line += "/pageadimg/*\r\n";
	line += "/pageads/*\r\n";
	line += "/pageear.\r\n";
	line += "/pageear/*\r\n";
	line += "/pageear_\r\n";
	line += "/pagepeel-\r\n";
	line += "/pagepeel.\r\n";
	line += "/pagepeel/*\r\n";
	line += "/pagepeel_banner/*\r\n";
	line += "/pagepeelads.\r\n";
	line += "/paidlisting/*\r\n";
	line += "/partnerads/*\r\n";
	line += "/partnerads_\r\n";
	line += "/partnerbanner.\r\n";
	line += "/partnerbanner/*\r\n";
	line += "/partners/ads/*\r\n";
	line += "/partnersadbutler/*\r\n";
	line += "/peel.js\r\n";
	line += "/peel/?webscr=\r\n";
	line += "/peel1.js\r\n";
	line += "/peelad.\r\n";
	line += "/peelad/*\r\n";
	line += "/peeljs.php\r\n";
	line += "/perfads.\r\n";
	line += "/performancingads/*\r\n";
	line += "/phpads.\r\n";
	line += "/phpads/*\r\n";
	line += "/phpads2/*\r\n";
	line += "/phpadserver/*\r\n";
	line += "/phpadsnew/*\r\n";
	line += "/pictures/ads/*\r\n";
	line += "/pilot_ad.\r\n";
	line += "/pitattoad.\r\n";
	line += "/pix/ad/*\r\n";
	line += "/pix/ads/*\r\n";
	line += "/pixelads/*\r\n";
	line += "/play/ad/*\r\n";
	line += "/player/ads/*\r\n";
	line += "/pool.ads.\r\n";
	line += "/pop_ad.\r\n";
	line += "/popads.\r\n";
	line += "/popads/*\r\n";
	line += "/popunder.\r\n";
	line += "/position=*/aamsz=\r\n";
	line += "/post_ads_\r\n";
	line += "/ppd_ads.\r\n";
	line += "/predictad.\r\n";
	line += "/premierebtnad/*\r\n";
	line += "/premium_ad.\r\n";
	line += "/premiumads/*\r\n";
	line += "/previews/ad/*\r\n";
	line += "/printad.\r\n";
	line += "/printad/*\r\n";
	line += "/printads/*\r\n";
	line += "/processads.\r\n";
	line += "/promo/ad_\r\n";
	line += "/promobuttonad.\r\n";
	line += "/promotions/ads.\r\n";
	line += "/promotions/ads?\r\n";
	line += "/protection/ad/*\r\n";
	line += "/pub/ad/*\r\n";
	line += "/pub/ads/*\r\n";
	line += "/public/ad/*\r\n";
	line += "/public/ad?\r\n";
	line += "/publicidad.$~object_subrequest\r\n";
	line += "/publicidad/*\r\n";
	line += "/qandaads/*\r\n";
	line += "/quadadvert.\r\n";
	line += "/questions/ads/*\r\n";
	line += "/radopenx?\r\n";
	line += "/radwindowclient.js\r\n";
	line += "/railad.\r\n";
	line += "/railads.\r\n";
	line += "/random=*/aamsz=\r\n";
	line += "/randomad.\r\n";
	line += "/randomad2.\r\n";
	line += "/rcolads1.\r\n";
	line += "/rcolads2.\r\n";
	line += "/rcom-ads.\r\n";
	line += "/realmedia/ads/*\r\n";
	line += "/rec_ad1.\r\n";
	line += "/reclame/*\r\n";
	line += "/rectangle_ad.\r\n";
	line += "/refreshads-\r\n";
	line += "/reklam.\r\n";
	line += "/reklama.\r\n";
	line += "/relatedads.\r\n";
	line += "/requestadvertisement.\r\n";
	line += "/requestmyspacead.\r\n";
	line += "/retrad.\r\n";
	line += "/richmedia.adv?\r\n";
	line += "/right-ad-\r\n";
	line += "/right_ads?\r\n";
	line += "/rightad.\r\n";
	line += "/rightnavads.\r\n";
	line += "/rightnavadsanswer.\r\n";
	line += "/rotads/*\r\n";
	line += "/rotateads.\r\n";
	line += "/rotatingpeels.js\r\n";
	line += "/rsads.js\r\n";
	line += "/rsads/c\r\n";
	line += "/rsads/r\r\n";
	line += "/rsads/u\r\n";
	line += "/rss/ads/*\r\n";
	line += "/satnetads.\r\n";
	line += "/satnetgoogleads.\r\n";
	line += "/scanscoutoverlayadrenderer.\r\n";
	line += "/scaradcontrol.\r\n";
	line += "/scripts/ad-\r\n";
	line += "/scripts/ad.\r\n";
	line += "/scripts/ad/*\r\n";
	line += "/scripts/ads/*\r\n";
	line += "/scripts/clickjs.php\r\n";
	line += "/search/ad/*\r\n";
	line += "/search/ads?\r\n";
	line += "/searchad.\r\n";
	line += "/searchads/*\r\n";
	line += "/secondads.\r\n";
	line += "/secondads_\r\n";
	line += "/serveads.\r\n";
	line += "/services/ads/*\r\n";
	line += "/sevenl_ad.\r\n";
	line += "/share/ads/*\r\n";
	line += "/shared/ads/*\r\n";
	line += "/show-ad.\r\n";
	line += "/show_ad.\r\n";
	line += "/show_ad_\r\n";
	line += "/show_ads.js\r\n";
	line += "/show_ads_\r\n";
	line += "/showad.\r\n";
	line += "/showad/*\r\n";
	line += "/showad_\r\n";
	line += "/showads.\r\n";
	line += "/showads/*\r\n";
	line += "/showadvertising.\r\n";
	line += "/showflashad.\r\n";
	line += "/showlayer.\r\n";
	line += "/showmarketingmaterial.\r\n";
	line += "/side-ad-\r\n";
	line += "/side-ads-\r\n";
	line += "/sidead.\r\n";
	line += "/sideads/*\r\n";
	line += "/sideads|\r\n";
	line += "/sidebar_ad.\r\n";
	line += "/sidebarad/*\r\n";
	line += "/sidecol_ad.\r\n";
	line += "/silver/ads/*\r\n";
	line += "/site_ads.\r\n";
	line += "/siteads.\r\n";
	line += "/siteads/*\r\n";
	line += "/siteafs.txt?\r\n";
	line += "/sites/ad_\r\n";
	line += "/skyad.php\r\n";
	line += "/skyadjs/*\r\n";
	line += "/skybar_ad.\r\n";
	line += "/skyframeopenads.\r\n";
	line += "/skyframeopenads_\r\n";
	line += "/skyscraperad.\r\n";
	line += "/slideadverts/*\r\n";
	line += "/slideinad.\r\n";
	line += "/small_ad.\r\n";
	line += "/smartad.\r\n";
	line += "/smartads.\r\n";
	line += "/smartlinks.\r\n";
	line += "/smb/ads/*\r\n";
	line += "/socialads.\r\n";
	line += "/socialads/*\r\n";
	line += "/someads.\r\n";
	line += "/spc.php?\r\n";
	line += "/spcjs.php?\r\n";
	line += "/special-ads/*\r\n";
	line += "/special_ads/*\r\n";
	line += "/specials/htads/*\r\n";
	line += "/spo_show.asp?\r\n";
	line += "/sponser.\r\n";
	line += "/sponsimages/*\r\n";
	line += "/sponslink_\r\n";
	line += "/sponsor-ad|\r\n";
	line += "/sponsor-right.\r\n";
	line += "/sponsor-top.\r\n";
	line += "/sponsor_images/*\r\n";
	line += "/sponsorad.\r\n";
	line += "/sponsoradds/*\r\n";
	line += "/sponsorads/*\r\n";
	line += "/sponsored_ad.\r\n";
	line += "/sponsored_links_\r\n";
	line += "/sponsored_text.\r\n";
	line += "/sponsored_top.\r\n";
	line += "/sponsoredcontent.\r\n";
	line += "/sponsoredlinks.\r\n";
	line += "/sponsoredlinks/*\r\n";
	line += "/sponsoredlinksiframe.\r\n";
	line += "/sponsoring/*\r\n";
	line += "/sponsors_box.\r\n";
	line += "/sponsorship_\r\n";
	line += "/sponsorstrips/*\r\n";
	line += "/square-ads/*\r\n";
	line += "/squaread.\r\n";
	line += "/srv/ad/*\r\n";
	line += "/static/ad_\r\n";
	line += "/static/ads/*\r\n";
	line += "/stickyad.\r\n";
	line += "/storage/ads/*\r\n";
	line += "/story_ad.\r\n";
	line += "/subad2_\r\n";
	line += "/swf/ad-\r\n";
	line += "/synad2.\r\n";
	line += "/system/ad/*\r\n";
	line += "/systemad.\r\n";
	line += "/systemad_\r\n";
	line += "/td_ads/*\r\n";
	line += "/tdlads/*\r\n";
	line += "/templates/_ads/*\r\n";
	line += "/testingad.\r\n";
	line += "/textad.\r\n";
	line += "/textad/*\r\n";
	line += "/textad?\r\n";
	line += "/textadrotate.\r\n";
	line += "/textads/*\r\n";
	line += "/textads_\r\n";
	line += "/thirdparty/ad/*\r\n";
	line += "/thirdpartyads/*\r\n";
	line += "/tii_ads.\r\n";
	line += "/tikilink?\r\n";
	line += "/tmo/ads/*\r\n";
	line += "/tmobilead.\r\n";
	line += "/toigoogleads.\r\n";
	line += "/toolkitads.\r\n";
	line += "/tools/ad.\r\n";
	line += "/top-ad-\r\n";
	line += "/top_ad.\r\n";
	line += "/top_ad_\r\n";
	line += "/top_ads/*\r\n";
	line += "/top_ads_\r\n";
	line += "/topad.php\r\n";
	line += "/topads.\r\n";
	line += "/topperad.\r\n";
	line += "/tracked_ad.\r\n";
	line += "/tradead_\r\n";
	line += "/tribalad.\r\n";
	line += "/ttz_ad.\r\n";
	line += "/tx_macinabanners/*\r\n";
	line += "/txt_ad.\r\n";
	line += "/ukc-ad.\r\n";
	line += "/unity/ad/*\r\n";
	line += "/update_ads/*\r\n";
	line += "/upload/ads/*\r\n";
	line += "/uploads/ads/*\r\n";
	line += "/us-adcentre.\r\n";
	line += "/valueclick.\r\n";
	line += "/vclkads.\r\n";
	line += "/video/ads/*\r\n";
	line += "/video_ad.\r\n";
	line += "/video_ad_\r\n";
	line += "/videoad.\r\n";
	line += "/videoads.\r\n";
	line += "/videoads/*\r\n";
	line += "/videowall-ad.\r\n";
	line += "/view/banner/*/zone?zid=\r\n";
	line += "/vtextads.\r\n";
	line += "/wallpaperads/*\r\n";
	line += "/webad?a\r\n";
	line += "/webadimg/*\r\n";
	line += "/webads.\r\n";
	line += "/webads_\r\n";
	line += "/webadverts/*\r\n";
	line += "/welcome_ad.\r\n";
	line += "/widget/ads.\r\n";
	line += "/wipeads/*\r\n";
	line += "/wp-content/ads/*\r\n";
	line += "/wp-content/plugins/fasterim-optin/*\r\n";
	line += "/wp-srv/ad/*\r\n";
	line += "/wpads/iframe.\r\n";
	line += "/writelayerad.\r\n";
	line += "/www/ads/*\r\n";
	line += "/www/delivery/*\r\n";
	line += "/xmladparser.\r\n";
	line += "/yahoo-ads/*\r\n";
	line += "/your-ad.\r\n";
	line += "/ysmads.\r\n";
	line += "/zanox.js\r\n";
	line += "/zanox/banner/*\r\n";
	line += "/zanox_ad/*\r\n";
	line += "2010/ads/\r\n";
	line += "8080/ads/\r\n";
	line += ";adsense_\r\n";
	line += ";iframeid=ad_\r\n";
	line += "=adtech_\r\n";
	line += "=advert/\r\n";
	line += "=advertorial&\r\n";
	line += "?ad_ids=\r\n";
	line += "?ad_type=\r\n";
	line += "?ad_width=\r\n";
	line += "?adarea=\r\n";
	line += "?adclass=\r\n";
	line += "?adpage=\r\n";
	line += "?adpartner=\r\n";
	line += "?adsize=\r\n";
	line += "?adslot=\r\n";
	line += "?adtype=\r\n";
	line += "?advertising=\r\n";
	line += "?advtile=\r\n";
	line += "?advurl=\r\n";
	line += "?file=ads&\r\n";
	line += "?getad=&$~object_subrequest\r\n";
	line += "?view=ad&\r\n";
	line += "_140x600_\r\n";
	line += "_160_ad_\r\n";
	line += "_acorn_ad_\r\n";
	line += "_ad.php?\r\n";
	line += "_ad120x120_\r\n";
	line += "_ad234x90-\r\n";
	line += "_ad_big.\r\n";
	line += "_ad_bsb.\r\n";
	line += "_ad_code.\r\n";
	line += "_ad_content.\r\n";
	line += "_ad_controller.\r\n";
	line += "_ad_count.\r\n";
	line += "_ad_courier.\r\n";
	line += "_ad_footer.\r\n";
	line += "_ad_homepage.\r\n";
	line += "_ad_iframe.\r\n";
	line += "_ad_images/\r\n";
	line += "_ad_label.\r\n";
	line += "_ad_leaderboard.\r\n";
	line += "_ad_placeholder-\r\n";
	line += "_ad_right.\r\n";
	line += "_ad_skyscraper.\r\n";
	line += "_ad_square.\r\n";
	line += "_ad_widesky.\r\n";
	line += "_adagency/\r\n";
	line += "_adbanner.\r\n";
	line += "_adbreak.\r\n";
	line += "_adcall_\r\n";
	line += "_adfunction.\r\n";
	line += "_adpage=\r\n";
	line += "_adpartner.\r\n";
	line += "_adplugin.\r\n";
	line += "_ads.html\r\n";
	line += "_ads.php?\r\n";
	line += "_ads/script.\r\n";
	line += "_ads1.asp\r\n";
	line += "_ads?pid=\r\n";
	line += "_ads_index_\r\n";
	line += "_ads_reporting.\r\n";
	line += "_ads_single_\r\n";
	line += "_adserve/\r\n";
	line += "_adshare.\r\n";
	line += "_adshow.\r\n";
	line += "_adsjs.php?\r\n";
	line += "_adsys.js\r\n";
	line += "_adtext_\r\n";
	line += "_adtitle.\r\n";
	line += "_advert.\r\n";
	line += "_advert1.\r\n";
	line += "_advertise.\r\n";
	line += "_advertise180.\r\n";
	line += "_advertisehere.\r\n";
	line += "_advertisement.\r\n";
	line += "_advertisement_\r\n";
	line += "_advertising/\r\n";
	line += "_advertisment.\r\n";
	line += "_advertorials/\r\n";
	line += "_adwrap.\r\n";
	line += "_afs_ads.\r\n";
	line += "_argus_ad_\r\n";
	line += "_assets/ads/\r\n";
	line += "_background_ad/\r\n";
	line += "_banner_adv_\r\n";
	line += "_bannerad.\r\n";
	line += "_blogads.\r\n";
	line += "_bottom_ads_\r\n";
	line += "_box_ads/\r\n";
	line += "_buttonad.\r\n";
	line += "_companionad.\r\n";
	line += "_contest_ad_\r\n";
	line += "_custom_ad.\r\n";
	line += "_custom_ad_\r\n";
	line += "_displaytopads.\r\n";
	line += "_dynamicads/\r\n";
	line += "_externalad.\r\n";
	line += "_fach_ad.\r\n";
	line += "_fd_adbg1a.\r\n";
	line += "_fd_adbg2.\r\n";
	line += "_fd_adbg2a.\r\n";
	line += "_fd_adtop.\r\n";
	line += "_feast_ad.\r\n";
	line += "_gads_bottom.\r\n";
	line += "_gads_footer.\r\n";
	line += "_gads_top.\r\n";
	line += "_headerad.\r\n";
	line += "_home_adrow-\r\n";
	line += "_images/ads/\r\n";
	line += "_inc/adsrc.\r\n";
	line += "_index_ad.\r\n";
	line += "_mainad.\r\n";
	line += "_media/ads/*\r\n";
	line += "_mmsadbanner/\r\n";
	line += "_org_ad.\r\n";
	line += "_overlay_ad.\r\n";
	line += "_paidadvert_\r\n";
	line += "_player_ads_\r\n";
	line += "_request_ad.\r\n";
	line += "_right_ad.\r\n";
	line += "_special_ads/\r\n";
	line += "_stack_ads/\r\n";
	line += "_temp/ad_\r\n";
	line += "_top_ad.\r\n";
	line += "_topad.php\r\n";
	line += "_tribalfusion.\r\n";
	line += "_videoad.\r\n";
	line += "a/adx.js\r\n";
	line += "couk/ads/\r\n";
	line += "d/adx.js\r\n";
	line += "e/adx.js\r\n";
	line += "m/adbox/\r\n";
	line += "m/adx.js\r\n";
	line += "m/topads|\r\n";
	line += "n/adx.js\r\n";
	line += "s/adx.js\r\n";
	line += "t/adx.js\r\n";
	line += "u/adx.js\r\n";
	line += "z/adx.js\r\n";
	line += "|http://a.ads.\r\n";
	line += "|http://ad-uk.\r\n";
	line += "|http://ad.$~object_subrequest,domain=~europa.eu|~gogopin.com|~sjsu.edu|~uitm.edu.my|~uni-freiburg.de\r\n";
	line += "|http://ad0.\r\n";
	line += "|http://ad1.\r\n";
	line += "|http://ad2.$domain=~ad2.zophar.net\r\n";
	line += "|http://ad3.\r\n";
	line += "|http://ad4.\r\n";
	line += "|http://ad5.\r\n";
	line += "|http://ad6.\r\n";
	line += "|http://ad7.\r\n";
	line += "|http://adbox.\r\n";
	line += "|http://adimg.\r\n";
	line += "|http://adnet.\r\n";
	line += "|http://ads.$domain=~ahds.ac.uk\r\n";
	line += "|http://ads0.\r\n";
	line += "|http://ads1.\r\n";
	line += "|http://ads18.\r\n";
	line += "|http://ads2.\r\n";
	line += "|http://ads3.\r\n";
	line += "|http://ads4.\r\n";
	line += "|http://ads5.\r\n";
	line += "|http://adserv\r\n";
	line += "|http://adseu.\r\n";
	line += "|http://adsrv.\r\n";
	line += "|http://adsvr.\r\n";
	line += "|http://adsys.\r\n";
	line += "|http://adtag.\r\n";
	line += "|http://adver.\r\n";
	line += "|http://advertiser.\r\n";
	line += "|http://bwp.*/search\r\n";
	line += "|http://feeds.*/~a/\r\n";
	line += "|http://getad.\r\n";
	line += "|http://jazad.\r\n";
	line += "|http://openx.\r\n";
	line += "|http://pubad.\r\n";
	line += "|http://rss.*/~a/\r\n";
	line += "|http://synad.\r\n";
	line += "|http://u-ads.\r\n";
	line += "|http://wrapper.*/a?\r\n";
	line += "|https://ads.\r\n";
	line += "||adserver1.\r\n";
	line += "!Dimensions\r\n";
	line += "-120x60-\r\n";
	line += "-120x60.\r\n";
	line += "-360x110.\r\n";
	line += "-468_60.\r\n";
	line += "-468x60-\r\n";
	line += "-468x60.\r\n";
	line += "-468x60/\r\n";
	line += "-468x60_\r\n";
	line += "-468x80-\r\n";
	line += "-468x80.\r\n";
	line += "-468x80/\r\n";
	line += "-468x80_\r\n";
	line += "-480x60-\r\n";
	line += "-480x60.\r\n";
	line += "-480x60/\r\n";
	line += "-480x60_\r\n";
	line += "-728x90-\r\n";
	line += "-728x90.\r\n";
	line += "-728x90/\r\n";
	line += "-728x90_\r\n";
	line += ".468x60-\r\n";
	line += ".468x60.\r\n";
	line += ".468x60/\r\n";
	line += ".468x60_\r\n";
	line += ".468x80-\r\n";
	line += ".468x80.\r\n";
	line += ".468x80/\r\n";
	line += ".468x80_\r\n";
	line += ".480x60-\r\n";
	line += ".480x60.\r\n";
	line += ".480x60/\r\n";
	line += ".480x60_\r\n";
	line += ".728x90-\r\n";
	line += ".728x90.\r\n";
	line += ".728x90/\r\n";
	line += ".728x90_\r\n";
	line += ".com/160_\r\n";
	line += ".com/728_\r\n";
	line += "/125x125_banner.\r\n";
	line += "/180x150-\r\n";
	line += "/180x150_\r\n";
	line += "/250x250.\r\n";
	line += "/300x250-\r\n";
	line += "/300x250.\r\n";
	line += "/300x250_\r\n";
	line += "/468-60.\r\n";
	line += "/468_60.\r\n";
	line += "/468x60-\r\n";
	line += "/468x60.\r\n";
	line += "/468x60/*\r\n";
	line += "/468x60_\r\n";
	line += "/468x80-\r\n";
	line += "/468x80.\r\n";
	line += "/468x80/*\r\n";
	line += "/468x80_\r\n";
	line += "/480x60-\r\n";
	line += "/480x60.\r\n";
	line += "/480x60/*\r\n";
	line += "/480x60_\r\n";
	line += "/600x160_\r\n";
	line += "/728_90/*\r\n";
	line += "/728x79_\r\n";
	line += "/728x90-\r\n";
	line += "/728x90.\r\n";
	line += "/728x90/*\r\n";
	line += "/728x90_\r\n";
	line += "/768x90-\r\n";
	line += "=300x250;\r\n";
	line += "_120_600\r\n";
	line += "_120x60.\r\n";
	line += "_120x600.\r\n";
	line += "_120x60_\r\n";
	line += "_160_600_\r\n";
	line += "_160x600_\r\n";
	line += "_300_250_\r\n";
	line += "_300x250-\r\n";
	line += "_300x250_\r\n";
	line += "_460x60.\r\n";
	line += "_468-60.\r\n";
	line += "_468.gif\r\n";
	line += "_468.htm\r\n";
	line += "_468_60.\r\n";
	line += "_468_60_\r\n";
	line += "_468x120.\r\n";
	line += "_468x60-\r\n";
	line += "_468x60.\r\n";
	line += "_468x60/\r\n";
	line += "_468x60_\r\n";
	line += "_468x80-\r\n";
	line += "_468x80.\r\n";
	line += "_468x80/\r\n";
	line += "_468x80_\r\n";
	line += "_468x90.\r\n";
	line += "_480x60-\r\n";
	line += "_480x60.\r\n";
	line += "_480x60/\r\n";
	line += "_480x60_\r\n";
	line += "_720x90.\r\n";
	line += "_728.gif\r\n";
	line += "_728.htm\r\n";
	line += "_728_90.\r\n";
	line += "_728_90_\r\n";
	line += "_728x90-\r\n";
	line += "_728x90.\r\n";
	line += "_728x90/\r\n";
	line += "_728x90_\r\n";
	line += "_768x90_\r\n";
	line += "!-----------------General element hiding rules-----------------!\r\n";
	line += "! *** easylist_general_hide.txt ***\r\n";
	line += "###A9AdsMiddleBoxTop\r\n";
	line += "###A9AdsOutOfStockWidgetTop\r\n";
	line += "###A9AdsServicesWidgetTop\r\n";
	line += "###ADSLOT_1\r\n";
	line += "###ADSLOT_2\r\n";
	line += "###ADSLOT_3\r\n";
	line += "###ADSLOT_4\r\n";
	line += "###AD_CONTROL_22\r\n";
	line += "###ADsmallWrapper\r\n";
	line += "~digitalhome.ca###Ad1\r\n";
	line += "###Ad160x600\r\n";
	line += "###Ad2\r\n";
	line += "###Ad300x250\r\n";
	line += "###Ad3Left\r\n";
	line += "###Ad3Right\r\n";
	line += "###Ad3TextAd\r\n";
	line += "###AdBanner_F1\r\n";
	line += "###AdBar\r\n";
	line += "###AdBar1\r\n";
	line += "###AdContainerTop\r\n";
	line += "###AdContentModule_F\r\n";
	line += "###AdDetails_GoogleLinksBottom\r\n";
	line += "###AdDetails_InsureWith\r\n";
	line += "###AdFrame4\r\n";
	line += "~ksl.com###AdHeader\r\n";
	line += "###AdMiddle\r\n";
	line += "###AdMobileLink\r\n";
	line += "###AdRectangle\r\n";
	line += "###AdSenseDiv\r\n";
	line += "###AdServer\r\n";
	line += "###AdShowcase_F1\r\n";
	line += "###AdSky23\r\n";
	line += "###AdSkyscraper\r\n";
	line += "###AdSponsor_SF\r\n";
	line += "###AdSubsectionShowcase_F1\r\n";
	line += "###AdTargetControl1_iframe\r\n";
	line += "###AdText\r\n";
	line += "###AdTop\r\n";
	line += "###Ad_Block\r\n";
	line += "###Ad_Center1\r\n";
	line += "###Ad_Right1\r\n";
	line += "###Ad_Top\r\n";
	line += "~cynamite.de###Adbanner\r\n";
	line += "###Adrectangle\r\n";
	line += "###Ads\r\n";
	line += "###AdsContent\r\n";
	line += "###AdsRight\r\n";
	line += "###AdsWrap\r\n";
	line += "###Ads_BA_CAD\r\n";
	line += "###Ads_BA_CAD2\r\n";
	line += "###Ads_BA_CAD_box\r\n";
	line += "###Ads_BA_SKY\r\n";
	line += "###AdvertMPU23b\r\n";
	line += "###AdvertPanel\r\n";
	line += "~designspotter.com###AdvertiseFrame\r\n";
	line += "~winload.de###Advertisement\r\n";
	line += "###Advertisements\r\n";
	line += "###Advertorial\r\n";
	line += "###Advertorials\r\n";
	line += "###BannerAdvert\r\n";
	line += "###BigBoxAd\r\n";
	line += "###BodyAd\r\n";
	line += "###ButtonAd\r\n";
	line += "###CompanyDetailsNarrowGoogleAdsPresentationControl\r\n";
	line += "###CompanyDetailsWideGoogleAdsPresentationControl\r\n";
	line += "###ContentAd\r\n";
	line += "###ContentAd1\r\n";
	line += "###ContentAd2\r\n";
	line += "###ContentAdPlaceHolder1\r\n";
	line += "###ContentAdPlaceHolder2\r\n";
	line += "###ContentAdXXL\r\n";
	line += "###ContentPolepositionAds_Result\r\n";
	line += "###DivAdEggHeadCafeTopBanner\r\n";
	line += "###FooterAd\r\n";
	line += "###FooterAdContainer\r\n";
	line += "###GoogleAd1\r\n";
	line += "###GoogleAd2\r\n";
	line += "###GoogleAd3\r\n";
	line += "###GoogleAdsPresentationControl\r\n";
	line += "###GoogleAdsense\r\n";
	line += "###Google_Adsense_Main\r\n";
	line += "###HEADERAD\r\n";
	line += "###HOME_TOP_RIGHT_BOXAD\r\n";
	line += "###HeaderAdsBlock\r\n";
	line += "###HeaderAdsBlockFront\r\n";
	line += "###HeaderBannerAdSpacer\r\n";
	line += "###HeaderTextAd\r\n";
	line += "###HeroAd\r\n";
	line += "###HomeAd1\r\n";
	line += "###HouseAd\r\n";
	line += "###ID_Ad_Sky\r\n";
	line += "###Journal_Ad_125\r\n";
	line += "###Journal_Ad_300\r\n";
	line += "###KH-contentAd\r\n";
	line += "###LeftAd\r\n";
	line += "###LeftAdF1\r\n";
	line += "###LeftAdF2\r\n";
	line += "###LftAd\r\n";
	line += "###LoungeAdsDiv\r\n";
	line += "###LowerContentAd\r\n";
	line += "###MainSponsoredLinks\r\n";
	line += "###Nightly_adContainer\r\n";
	line += "###PREFOOTER_LEFT_BOXAD\r\n";
	line += "###PREFOOTER_RIGHT_BOXAD\r\n";
	line += "###PageLeaderAd\r\n";
	line += "###RelevantAds\r\n";
	line += "###RgtAd1\r\n";
	line += "###RightAd\r\n";
	line += "###RightNavTopAdSpot\r\n";
	line += "###RightSponsoredAd\r\n";
	line += "###SectionAd300-250\r\n";
	line += "###SectionSponsorAd\r\n";
	line += "###SidebarAdContainer\r\n";
	line += "###SkyAd\r\n";
	line += "###SpecialAds\r\n";
	line += "###SponsoredAd\r\n";
	line += "###SponsoredLinks\r\n";
	line += "###TOP_ADROW\r\n";
	line += "###TOP_RIGHT_BOXAD\r\n";
	line += "~kids.t-online.de###Tadspacehead\r\n";
	line += "###TopAd\r\n";
	line += "###TopAdContainer\r\n";
	line += "###TopAdDiv\r\n";
	line += "###TopAdPos\r\n";
	line += "###VM-MPU-adspace\r\n";
	line += "###VM-footer-adspace\r\n";
	line += "###VM-header-adspace\r\n";
	line += "###VM-header-adwrap\r\n";
	line += "###XEadLeaderboard\r\n";
	line += "###XEadSkyscraper\r\n";
	line += "###_ads\r\n";
	line += "###about_adsbottom\r\n";
	line += "###ad-120x600-sidebar\r\n";
	line += "###ad-120x60Div\r\n";
	line += "###ad-160x600\r\n";
	line += "###ad-160x600-sidebar\r\n";
	line += "###ad-250\r\n";
	line += "###ad-250x300\r\n";
	line += "###ad-300\r\n";
	line += "###ad-300x250\r\n";
	line += "###ad-300x250-sidebar\r\n";
	line += "###ad-300x250Div\r\n";
	line += "###ad-728\r\n";
	line += "###ad-728x90-leaderboard-top\r\n";
	line += "###ad-article\r\n";
	line += "###ad-banner\r\n";
	line += "###ad-bottom\r\n";
	line += "###ad-bottom-wrapper\r\n";
	line += "###ad-boxes\r\n";
	line += "###ad-bs\r\n";
	line += "###ad-buttons\r\n";
	line += "###ad-colB-1\r\n";
	line += "###ad-column\r\n";
	line += "~madame.lefigaro.fr###ad-container\r\n";
	line += "###ad-content\r\n";
	line += "###ad-contentad\r\n";
	line += "###ad-footer\r\n";
	line += "###ad-footprint-160x600\r\n";
	line += "###ad-front-footer\r\n";
	line += "###ad-front-sponsoredlinks\r\n";
	line += "###ad-halfpage\r\n";
	line += "~ifokus.se###ad-header\r\n";
	line += "###ad-inner\r\n";
	line += "###ad-label\r\n";
	line += "###ad-leaderboard\r\n";
	line += "###ad-leaderboard-bottom\r\n";
	line += "###ad-leaderboard-container\r\n";
	line += "###ad-leaderboard-spot\r\n";
	line += "###ad-leaderboard-top\r\n";
	line += "###ad-left\r\n";
	line += "###ad-list-row\r\n";
	line += "###ad-lrec\r\n";
	line += "###ad-medium-rectangle\r\n";
	line += "###ad-medrec\r\n";
	line += "###ad-middlethree\r\n";
	line += "###ad-middletwo\r\n";
	line += "###ad-module\r\n";
	line += "###ad-mpu\r\n";
	line += "###ad-mpu1-spot\r\n";
	line += "###ad-mpu2\r\n";
	line += "###ad-mpu2-spot\r\n";
	line += "###ad-north\r\n";
	line += "###ad-one\r\n";
	line += "###ad-placard\r\n";
	line += "###ad-placeholder\r\n";
	line += "###ad-rectangle\r\n";
	line += "###ad-right\r\n";
	line += "###ad-righttop\r\n";
	line += "###ad-row\r\n";
	line += "###ad-side-text\r\n";
	line += "###ad-sky\r\n";
	line += "###ad-skyscraper\r\n";
	line += "###ad-slug-wrapper\r\n";
	line += "###ad-small-banner\r\n";
	line += "###ad-space\r\n";
	line += "###ad-splash\r\n";
	line += "###ad-spot\r\n";
	line += "###ad-target\r\n";
	line += "###ad-target-Leaderbord\r\n";
	line += "###ad-teaser\r\n";
	line += "###ad-text\r\n";
	line += "~gismeteo.com,~gismeteo.ru,~gismeteo.ua###ad-top\r\n";
	line += "###ad-top-banner\r\n";
	line += "###ad-top-text-low\r\n";
	line += "###ad-top-wrap\r\n";
	line += "###ad-tower\r\n";
	line += "###ad-trailerboard-spot\r\n";
	line += "###ad-typ1\r\n";
	line += "###ad-west\r\n";
	line += "###ad-wrap\r\n";
	line += "###ad-wrap-right\r\n";
	line += "~collegeslackers.com###ad-wrapper\r\n";
	line += "###ad-wrapper1\r\n";
	line += "###ad-yahoo-simple\r\n";
	line += "~tphousing.com,~tvgorge.com###ad1\r\n";
	line += "###ad1006\r\n";
	line += "###ad125BL\r\n";
	line += "###ad125BR\r\n";
	line += "###ad125TL\r\n";
	line += "###ad125TR\r\n";
	line += "###ad125x125\r\n";
	line += "###ad160x600\r\n";
	line += "###ad160x600right\r\n";
	line += "###ad1Sp\r\n";
	line += "###ad2\r\n";
	line += "###ad2Sp\r\n";
	line += "~absoluteradio.co.uk###ad3\r\n";
	line += "###ad300\r\n";
	line += "###ad300-250\r\n";
	line += "###ad300X250\r\n";
	line += "###ad300_x_250\r\n";
	line += "###ad300x150\r\n";
	line += "###ad300x250\r\n";
	line += "###ad300x250Module\r\n";
	line += "###ad300x60\r\n";
	line += "###ad300x600\r\n";
	line += "###ad300x600_callout\r\n";
	line += "###ad336\r\n";
	line += "###ad336x280\r\n";
	line += "###ad375x85\r\n";
	line += "###ad4\r\n";
	line += "###ad468\r\n";
	line += "###ad468x60\r\n";
	line += "###ad468x60_top\r\n";
	line += "###ad526x250\r\n";
	line += "###ad600\r\n";
	line += "###ad7\r\n";
	line += "###ad728\r\n";
	line += "###ad728Mid\r\n";
	line += "###ad728Top\r\n";
	line += "###ad728Wrapper\r\n";
	line += "~natgeo.tv###ad728x90\r\n";
	line += "###adBadges\r\n";
	line += "~campusdish.com###adBanner\r\n";
	line += "###adBanner120x600\r\n";
	line += "###adBanner160x600\r\n";
	line += "###adBanner336x280\r\n";
	line += "###adBannerTable\r\n";
	line += "###adBannerTop\r\n";
	line += "###adBar\r\n";
	line += "###adBlock125\r\n";
	line += "###adBlocks\r\n";
	line += "###adBox\r\n";
	line += "###adBox350\r\n";
	line += "###adBox390\r\n";
	line += "###adComponentWrapper\r\n";
	line += "~jobs.wa.gov.au,~pricewatch.com###adContainer\r\n";
	line += "###adContainer_1\r\n";
	line += "###adContainer_2\r\n";
	line += "###adContainer_3\r\n";
	line += "~remixshare.com###adDiv\r\n";
	line += "###adFps\r\n";
	line += "###adFtofrs\r\n";
	line += "###adGallery\r\n";
	line += "###adGroup1\r\n";
	line += "###adHeader\r\n";
	line += "###adIsland\r\n";
	line += "###adL\r\n";
	line += "###adLB\r\n";
	line += "###adLabel\r\n";
	line += "###adLayer\r\n";
	line += "###adLeaderTop\r\n";
	line += "###adLeaderboard\r\n";
	line += "###adMPU\r\n";
	line += "###adMiddle0Frontpage\r\n";
	line += "###adMiniPremiere\r\n";
	line += "###adP\r\n";
	line += "###adPlaceHolderRight\r\n";
	line += "###adPlacer\r\n";
	line += "###adRight\r\n";
	line += "###adSenseModule\r\n";
	line += "###adSenseWrapper\r\n";
	line += "###adServer_marginal\r\n";
	line += "###adSidebar\r\n";
	line += "###adSidebarSq\r\n";
	line += "###adSky\r\n";
	line += "###adSkyscraper\r\n";
	line += "###adSlider\r\n";
	line += "###adSpace\r\n";
	line += "###adSpace3\r\n";
	line += "###adSpace300_ifrMain\r\n";
	line += "###adSpace4\r\n";
	line += "###adSpace5\r\n";
	line += "###adSpace6\r\n";
	line += "###adSpace7\r\n";
	line += "###adSpace_footer\r\n";
	line += "###adSpace_right\r\n";
	line += "###adSpace_top\r\n";
	line += "###adSpacer\r\n";
	line += "###adSpecial\r\n";
	line += "###adSpot-Leader\r\n";
	line += "###adSpot-banner\r\n";
	line += "###adSpot-island\r\n";
	line += "###adSpot-mrec1\r\n";
	line += "###adSpot-sponsoredlinks\r\n";
	line += "###adSpot-textbox1\r\n";
	line += "###adSpot-widestrip\r\n";
	line += "###adSpotAdvertorial\r\n";
	line += "###adSpotIsland\r\n";
	line += "###adSpotSponsoredLinks\r\n";
	line += "###adSquare\r\n";
	line += "###adStaticA\r\n";
	line += "###adStrip\r\n";
	line += "###adSuperAd\r\n";
	line += "###adSuperPremiere\r\n";
	line += "###adSuperbanner\r\n";
	line += "###adTableCell\r\n";
	line += "###adTag1\r\n";
	line += "###adTag2\r\n";
	line += "###adText\r\n";
	line += "###adText_container\r\n";
	line += "###adTile\r\n";
	line += "~ran.de###adTop\r\n";
	line += "###adTopboxright\r\n";
	line += "###adTower\r\n";
	line += "###adUnit\r\n";
	line += "~bankenverband.de,~thisisleicestershire.co.uk###adWrapper\r\n";
	line += "###adZoneTop\r\n";
	line += "###ad_160x160\r\n";
	line += "###ad_160x600\r\n";
	line += "###ad_190x90\r\n";
	line += "###ad_300\r\n";
	line += "###ad_300_250\r\n";
	line += "###ad_300_250_1\r\n";
	line += "###ad_300x250\r\n";
	line += "###ad_300x250_content_column\r\n";
	line += "###ad_300x90\r\n";
	line += "###ad_468_60\r\n";
	line += "###ad_5\r\n";
	line += "###ad_728_foot\r\n";
	line += "###ad_728x90\r\n";
	line += "###ad_940\r\n";
	line += "###ad_984\r\n";
	line += "###ad_A\r\n";
	line += "###ad_B\r\n";
	line += "###ad_Banner\r\n";
	line += "###ad_C\r\n";
	line += "###ad_C2\r\n";
	line += "###ad_D\r\n";
	line += "###ad_E\r\n";
	line += "###ad_F\r\n";
	line += "###ad_G\r\n";
	line += "###ad_H\r\n";
	line += "###ad_I\r\n";
	line += "###ad_J\r\n";
	line += "###ad_K\r\n";
	line += "###ad_L\r\n";
	line += "###ad_M\r\n";
	line += "###ad_N\r\n";
	line += "###ad_O\r\n";
	line += "###ad_P\r\n";
	line += "###ad_YieldManager-300x250\r\n";
	line += "###ad_anchor\r\n";
	line += "###ad_area\r\n";
	line += "###ad_banner\r\n";
	line += "###ad_banner_top\r\n";
	line += "###ad_bar\r\n";
	line += "###ad_bellow_post\r\n";
	line += "###ad_block_1\r\n";
	line += "###ad_block_2\r\n";
	line += "###ad_bottom\r\n";
	line += "###ad_box_colspan\r\n";
	line += "###ad_branding\r\n";
	line += "###ad_bs_area\r\n";
	line += "###ad_center_monster\r\n";
	line += "###ad_cont\r\n";
	line += "~academics.de###ad_container\r\n";
	line += "###ad_container_marginal\r\n";
	line += "###ad_container_side\r\n";
	line += "###ad_container_top\r\n";
	line += "###ad_content_top\r\n";
	line += "###ad_content_wrap\r\n";
	line += "###ad_feature\r\n";
	line += "###ad_firstpost\r\n";
	line += "###ad_footer\r\n";
	line += "###ad_front_three\r\n";
	line += "###ad_fullbanner\r\n";
	line += "~arablionz.com,~djluv.in,~egymedicine.net###ad_global_below_navbar\r\n";
	line += "###ad_global_header\r\n";
	line += "###ad_haha_1\r\n";
	line += "###ad_haha_4\r\n";
	line += "###ad_halfpage\r\n";
	line += "###ad_head\r\n";
	line += "###ad_header\r\n";
	line += "###ad_horizontal\r\n";
	line += "###ad_horseshoe_left\r\n";
	line += "###ad_horseshoe_right\r\n";
	line += "###ad_horseshoe_spacer\r\n";
	line += "###ad_horseshoe_top\r\n";
	line += "###ad_hotpots\r\n";
	line += "###ad_in_arti\r\n";
	line += "###ad_island\r\n";
	line += "###ad_label\r\n";
	line += "###ad_lastpost\r\n";
	line += "###ad_layer2\r\n";
	line += "###ad_leader\r\n";
	line += "###ad_leaderBoard\r\n";
	line += "###ad_leaderboard\r\n";
	line += "###ad_leaderboard_top\r\n";
	line += "###ad_left\r\n";
	line += "###ad_lrec\r\n";
	line += "###ad_lwr_square\r\n";
	line += "###ad_main\r\n";
	line += "###ad_medium_rectangle\r\n";
	line += "###ad_medium_rectangular\r\n";
	line += "###ad_mediumrectangle\r\n";
	line += "###ad_menu_header\r\n";
	line += "###ad_middle\r\n";
	line += "###ad_most_pop_234x60_req_wrapper\r\n";
	line += "###ad_mpu\r\n";
	line += "###ad_mpuav\r\n";
	line += "###ad_mrcontent\r\n";
	line += "###ad_overlay\r\n";
	line += "###ad_play_300\r\n";
	line += "###ad_rect\r\n";
	line += "###ad_rect_body\r\n";
	line += "###ad_rect_bottom\r\n";
	line += "###ad_rectangle\r\n";
	line += "###ad_rectangle_medium\r\n";
	line += "###ad_related_links_div\r\n";
	line += "###ad_related_links_div_program\r\n";
	line += "###ad_replace_div_0\r\n";
	line += "###ad_replace_div_1\r\n";
	line += "###ad_report_leaderboard\r\n";
	line += "###ad_report_rectangle\r\n";
	line += "###ad_right\r\n";
	line += "###ad_right_main\r\n";
	line += "###ad_ros_tower\r\n";
	line += "###ad_rr_1\r\n";
	line += "###ad_sec\r\n";
	line += "###ad_sec_div\r\n";
	line += "###ad_sidebar\r\n";
	line += "###ad_sidebar1\r\n";
	line += "###ad_sidebar2\r\n";
	line += "###ad_sidebar3\r\n";
	line += "###ad_skyscraper\r\n";
	line += "###ad_skyscraper_text\r\n";
	line += "###ad_slot_leaderboard\r\n";
	line += "###ad_slot_livesky\r\n";
	line += "###ad_slot_sky_top\r\n";
	line += "~streetinsider.com###ad_space\r\n";
	line += "~wretch.cc###ad_square\r\n";
	line += "###ad_ss\r\n";
	line += "###ad_table\r\n";
	line += "###ad_term_bottom_place\r\n";
	line += "###ad_thread_first_post_content\r\n";
	line += "###ad_top\r\n";
	line += "###ad_top_holder\r\n";
	line += "###ad_tp_banner_1\r\n";
	line += "###ad_tp_banner_2\r\n";
	line += "###ad_unit\r\n";
	line += "###ad_vertical\r\n";
	line += "###ad_widget\r\n";
	line += "###ad_window\r\n";
	line += "~amnestyusa.org,~drownedinsound.com###ad_wrapper\r\n";
	line += "###adbanner\r\n";
	line += "###adbig\r\n";
	line += "###adbnr\r\n";
	line += "###adboard\r\n";
	line += "###adbody\r\n";
	line += "###adbottom\r\n";
	line += "~kalaydo.de###adbox\r\n";
	line += "###adbox1\r\n";
	line += "###adbox2\r\n";
	line += "###adclear\r\n";
	line += "###adcode\r\n";
	line += "###adcode1\r\n";
	line += "###adcode2\r\n";
	line += "###adcode3\r\n";
	line += "###adcode4\r\n";
	line += "###adcolumnwrapper\r\n";
	line += "###adcontainer\r\n";
	line += "###adcontainerRight\r\n";
	line += "###adcontainsm\r\n";
	line += "###adcontent\r\n";
	line += "###adcontrolPushSite\r\n";
	line += "###add_ciao2\r\n";
	line += "###addbottomleft\r\n";
	line += "###addiv-bottom\r\n";
	line += "###addiv-top\r\n";
	line += "###adfooter_728x90\r\n";
	line += "###adframe:not(frameset)\r\n";
	line += "###adhead\r\n";
	line += "###adhead_g\r\n";
	line += "###adheader\r\n";
	line += "###adhome\r\n";
	line += "###adiframe1_iframe\r\n";
	line += "###adiframe2_iframe\r\n";
	line += "###adiframe3_iframe\r\n";
	line += "###adimg\r\n";
	line += "###adition_content_ad\r\n";
	line += "###adlabel\r\n";
	line += "###adlabelFooter\r\n";
	line += "###adlayerad\r\n";
	line += "###adleaderboard\r\n";
	line += "###adleft\r\n";
	line += "###adlinks\r\n";
	line += "###adlinkws\r\n";
	line += "###adlrec\r\n";
	line += "###admid\r\n";
	line += "###admiddle3center\r\n";
	line += "###admiddle3left\r\n";
	line += "###adposition\r\n";
	line += "###adposition-C\r\n";
	line += "###adposition-FPMM\r\n";
	line += "###adposition2\r\n";
	line += "~contracostatimes.com,~mercurynews.com,~siliconvalley.com###adposition3\r\n";
	line += "###adposition4\r\n";
	line += "###adrectangle\r\n";
	line += "###adrectanglea\r\n";
	line += "###adrectangleb\r\n";
	line += "###adrig\r\n";
	line += "###adright\r\n";
	line += "###adright2\r\n";
	line += "###adrighthome\r\n";
	line += "~2gb-hosting.com,~addoway.com,~bash.org.ru,~block-adblock-plus.com,~dailykos.com,~divxhosting.net,~facebook.com,~harpers.org,~miloyski.com,~radio.de,~tomwans.com,~tonprenom.com,~www.google.com,~zuploads.com,~zuploads.net###ads\r\n";
	line += "###ads-468\r\n";
	line += "###ads-area\r\n";
	line += "###ads-block\r\n";
	line += "###ads-bot\r\n";
	line += "###ads-bottom\r\n";
	line += "###ads-col\r\n";
	line += "###ads-dell\r\n";
	line += "###ads-horizontal\r\n";
	line += "###ads-indextext\r\n";
	line += "###ads-leaderboard1\r\n";
	line += "###ads-lrec\r\n";
	line += "###ads-menu\r\n";
	line += "###ads-middle\r\n";
	line += "###ads-prices\r\n";
	line += "###ads-rhs\r\n";
	line += "###ads-right\r\n";
	line += "###ads-top\r\n";
	line += "###ads-vers7\r\n";
	line += "###ads160left\r\n";
	line += "###ads2\r\n";
	line += "###ads300\r\n";
	line += "###ads300Bottom\r\n";
	line += "###ads300Top\r\n";
	line += "###ads336x280\r\n";
	line += "###ads7\r\n";
	line += "###ads728bottom\r\n";
	line += "###ads728top\r\n";
	line += "###ads790\r\n";
	line += "###adsDisplay\r\n";
	line += "###adsID\r\n";
	line += "###ads_160\r\n";
	line += "###ads_300\r\n";
	line += "###ads_728\r\n";
	line += "###ads_banner\r\n";
	line += "###ads_belowforumlist\r\n";
	line += "###ads_belownav\r\n";
	line += "###ads_bottom_inner\r\n";
	line += "###ads_bottom_outer\r\n";
	line += "###ads_box\r\n";
	line += "###ads_button\r\n";
	line += "###ads_catDiv\r\n";
	line += "###ads_footer\r\n";
	line += "###ads_html1\r\n";
	line += "###ads_html2\r\n";
	line += "###ads_right\r\n";
	line += "###ads_right_sidebar\r\n";
	line += "###ads_sidebar_roadblock\r\n";
	line += "###ads_space\r\n";
	line += "###ads_top\r\n";
	line += "###ads_watch_top_square\r\n";
	line += "###ads_zone27\r\n";
	line += "###adsbottom\r\n";
	line += "###adsbox\r\n";
	line += "###adscolumn\r\n";
	line += "###adsd_contentad_r1\r\n";
	line += "###adsd_contentad_r2\r\n";
	line += "###adsd_contentad_r3\r\n";
	line += "###adsense\r\n";
	line += "###adsense-tag\r\n";
	line += "###adsense-text\r\n";
	line += "###adsenseOne\r\n";
	line += "###adsenseWrap\r\n";
	line += "~jeeppatriot.com###adsense_inline\r\n";
	line += "###adsense_leaderboard\r\n";
	line += "###adsense_overlay\r\n";
	line += "###adsense_placeholder_2\r\n";
	line += "###adsenseheader\r\n";
	line += "###adsensetopplay\r\n";
	line += "###adsensewidget-3\r\n";
	line += "###adserv\r\n";
	line += "###adsky\r\n";
	line += "###adskyscraper\r\n";
	line += "###adslot\r\n";
	line += "###adsonar\r\n";
	line += "~metblogs.com,~oreilly.com###adspace\r\n";
	line += "###adspace-300x250\r\n";
	line += "###adspace300x250\r\n";
	line += "###adspaceBox\r\n";
	line += "###adspaceBox300\r\n";
	line += "###adspace_header\r\n";
	line += "###adspot-1\r\n";
	line += "###adspot-149x170\r\n";
	line += "###adspot-1x4\r\n";
	line += "###adspot-2\r\n";
	line += "###adspot-295x60\r\n";
	line += "###adspot-2a\r\n";
	line += "###adspot-2b\r\n";
	line += "###adspot-300x250-pos-1\r\n";
	line += "###adspot-300x250-pos-2\r\n";
	line += "###adspot-468x60-pos-2\r\n";
	line += "###adspot-a\r\n";
	line += "###adspot300x250\r\n";
	line += "###adsright\r\n";
	line += "###adstop\r\n";
	line += "###adt\r\n";
	line += "###adtab\r\n";
	line += "###adtag_right_side\r\n";
	line += "###adtech_googleslot_03c\r\n";
	line += "###adtech_takeover\r\n";
	line += "###adtop\r\n";
	line += "###adtxt\r\n";
	line += "###adv-masthead\r\n";
	line += "###adv_google_300\r\n";
	line += "###adv_google_728\r\n";
	line += "###adv_top_banner_wrapper\r\n";
	line += "###adver1\r\n";
	line += "###adver2\r\n";
	line += "###adver3\r\n";
	line += "###adver4\r\n";
	line += "###adver5\r\n";
	line += "###adver6\r\n";
	line += "###adver7\r\n";
	line += "~finn.no,~gcrg.org,~kalaydo.de,~m424.com,~secondamano.it,~sepa.org.uk###advert\r\n";
	line += "###advert-1\r\n";
	line += "###advert-120\r\n";
	line += "###advert-boomer\r\n";
	line += "###advert-display\r\n";
	line += "###advert-header\r\n";
	line += "###advert-leaderboard\r\n";
	line += "###advert-links-bottom\r\n";
	line += "###advert-skyscraper\r\n";
	line += "###advert-top\r\n";
	line += "###advert1\r\n";
	line += "###advertBanner\r\n";
	line += "###advertRight\r\n";
	line += "###advert_250x250\r\n";
	line += "###advert_box\r\n";
	line += "###advert_leaderboard\r\n";
	line += "###advert_lrec_format\r\n";
	line += "###advert_mid\r\n";
	line += "###advert_mpu\r\n";
	line += "###advert_right_skyscraper\r\n";
	line += "###advertbox\r\n";
	line += "###advertbox2\r\n";
	line += "###advertbox3\r\n";
	line += "###advertbox4\r\n";
	line += "###adverthome\r\n";
	line += "~zlatestranky.cz###advertise\r\n";
	line += "###advertise-now\r\n";
	line += "###advertise1\r\n";
	line += "###advertiseHere\r\n";
	line += "~ping-timeout.de###advertisement\r\n";
	line += "###advertisement160x600\r\n";
	line += "###advertisement728x90\r\n";
	line += "###advertisementLigatus\r\n";
	line += "###advertisementPrio2\r\n";
	line += "###advertiser-container\r\n";
	line += "###advertiserLinks\r\n";
	line += "~uscbookstore.com###advertising\r\n";
	line += "###advertising-banner\r\n";
	line += "###advertising-caption\r\n";
	line += "###advertising-container\r\n";
	line += "###advertising-control\r\n";
	line += "###advertising-skyscraper\r\n";
	line += "###advertisingModule160x600\r\n";
	line += "###advertisingModule728x90\r\n";
	line += "###advertisment\r\n";
	line += "###advertismentElementInUniversalbox\r\n";
	line += "###advertorial\r\n";
	line += "~markt.de###adverts\r\n";
	line += "###adverts-top-container\r\n";
	line += "###adverts-top-left\r\n";
	line += "###adverts-top-middle\r\n";
	line += "###adverts-top-right\r\n";
	line += "###advertsingle\r\n";
	line += "###advt\r\n";
	line += "###adwhitepaperwidget\r\n";
	line += "###adwin_rec\r\n";
	line += "###adwith\r\n";
	line += "###adwords-4-container\r\n";
	line += "~musicstar.de###adwrapper\r\n";
	line += "###adxBigAd\r\n";
	line += "###adxMiddle5\r\n";
	line += "###adxSponLink\r\n";
	line += "###adxSponLinkA\r\n";
	line += "###adxtop\r\n";
	line += "###adzbanner\r\n";
	line += "###adzerk\r\n";
	line += "###adzoneBANNER\r\n";
	line += "###affinityBannerAd\r\n";
	line += "###agi-ad300x250\r\n";
	line += "###agi-ad300x250overlay\r\n";
	line += "###agi-sponsored\r\n";
	line += "###alert_ads\r\n";
	line += "###anchorAd\r\n";
	line += "###annoying_ad\r\n";
	line += "###ap_adframe\r\n";
	line += "###apiBackgroundAd\r\n";
	line += "###apiTopAdWrap\r\n";
	line += "###apmNADiv\r\n";
	line += "###araHealthSponsorAd\r\n";
	line += "###article-ad-container\r\n";
	line += "###article-box-ad\r\n";
	line += "###articleAdReplacement\r\n";
	line += "###articleLeftAdColumn\r\n";
	line += "###articleSideAd\r\n";
	line += "###article_ad\r\n";
	line += "###article_box_ad\r\n";
	line += "###asinglead\r\n";
	line += "###atlasAdDivGame\r\n";
	line += "###awds-nt1-ad\r\n";
	line += "###banner-300x250\r\n";
	line += "###banner-ad\r\n";
	line += "###banner-ad-container\r\n";
	line += "###banner-ads\r\n";
	line += "###banner250x250\r\n";
	line += "###banner468x60\r\n";
	line += "###banner728x90\r\n";
	line += "###bannerAd\r\n";
	line += "###bannerAdTop\r\n";
	line += "###bannerAd_ctr\r\n";
	line += "###banner_ad\r\n";
	line += "###banner_ad_footer\r\n";
	line += "###banner_admicro\r\n";
	line += "###banner_ads\r\n";
	line += "###banner_content_ad\r\n";
	line += "###banner_topad\r\n";
	line += "###bannerad\r\n";
	line += "###bannerad2\r\n";
	line += "###bbccom_mpu\r\n";
	line += "###bbccom_storyprintsponsorship\r\n";
	line += "###bbo_ad1\r\n";
	line += "###bg-footer-ads\r\n";
	line += "###bg-footer-ads2\r\n";
	line += "###bg_YieldManager-300x250\r\n";
	line += "###bigAd\r\n";
	line += "###bigBoxAd\r\n";
	line += "###bigad300outer\r\n";
	line += "###bigadbox\r\n";
	line += "###bigadspot\r\n";
	line += "###billboard_ad\r\n";
	line += "###block-ad_cube-1\r\n";
	line += "###block-openads-1\r\n";
	line += "###block-openads-3\r\n";
	line += "###block-openads-4\r\n";
	line += "###block-openads-5\r\n";
	line += "###block-thewrap_ads_250x300-0\r\n";
	line += "###block_advert\r\n";
	line += "###blog-ad\r\n";
	line += "###blog_ad_content\r\n";
	line += "###blog_ad_opa\r\n";
	line += "###blox-big-ad\r\n";
	line += "###blox-big-ad-bottom\r\n";
	line += "###blox-big-ad-top\r\n";
	line += "###blox-halfpage-ad\r\n";
	line += "###blox-tile-ad\r\n";
	line += "###blox-tower-ad\r\n";
	line += "###book-ad\r\n";
	line += "###botad\r\n";
	line += "###bott_ad2\r\n";
	line += "###bott_ad2_300\r\n";
	line += "###bottom-ad\r\n";
	line += "###bottom-ad-container\r\n";
	line += "###bottom-ads\r\n";
	line += "###bottomAd\r\n";
	line += "###bottomAdCCBucket\r\n";
	line += "###bottomAdContainer\r\n";
	line += "###bottomAdSense\r\n";
	line += "###bottomAdSenseDiv\r\n";
	line += "###bottomAds\r\n";
	line += "###bottomRightAd\r\n";
	line += "###bottomRightAdSpace\r\n";
	line += "###bottom_ad\r\n";
	line += "###bottom_ad_area\r\n";
	line += "###bottom_ads\r\n";
	line += "###bottom_banner_ad\r\n";
	line += "###bottom_overture\r\n";
	line += "###bottom_sponsor_ads\r\n";
	line += "###bottom_sponsored_links\r\n";
	line += "###bottom_text_ad\r\n";
	line += "###bottomad\r\n";
	line += "###bottomads\r\n";
	line += "###bottomadsense\r\n";
	line += "###bottomadwrapper\r\n";
	line += "###bottomleaderboardad\r\n";
	line += "###box-content-ad\r\n";
	line += "###box-googleadsense-1\r\n";
	line += "###box-googleadsense-r\r\n";
	line += "###box1ad\r\n";
	line += "###boxAd300\r\n";
	line += "###boxAdContainer\r\n";
	line += "###box_ad\r\n";
	line += "###box_mod_googleadsense\r\n";
	line += "###boxad1\r\n";
	line += "###boxad2\r\n";
	line += "###boxad3\r\n";
	line += "###boxad4\r\n";
	line += "###boxad5\r\n";
	line += "###bpAd\r\n";
	line += "###bps-header-ad-container\r\n";
	line += "###btr_horiz_ad\r\n";
	line += "###burn_header_ad\r\n";
	line += "###button-ads-horizontal\r\n";
	line += "###button-ads-vertical\r\n";
	line += "###buttonAdWrapper1\r\n";
	line += "###buttonAdWrapper2\r\n";
	line += "###buttonAds\r\n";
	line += "###buttonAdsContainer\r\n";
	line += "###button_ad_container\r\n";
	line += "###button_ad_wrap\r\n";
	line += "###buttonad\r\n";
	line += "###buy-sell-ads\r\n";
	line += "###c4ad-Middle1\r\n";
	line += "###caAdLarger\r\n";
	line += "###catad\r\n";
	line += "###cellAd\r\n";
	line += "###channel_ad\r\n";
	line += "###channel_ads\r\n";
	line += "###ciHomeRHSAdslot\r\n";
	line += "###circ_ad\r\n";
	line += "###cnnRR336ad\r\n";
	line += "###cnnTopAd\r\n";
	line += "###col3_advertising\r\n";
	line += "###colRightAd\r\n";
	line += "###collapseobj_adsection\r\n";
	line += "###column4-google-ads\r\n";
	line += "###commercial_ads\r\n";
	line += "###common_right_ad_wrapper\r\n";
	line += "###common_right_lower_ad_wrapper\r\n";
	line += "###common_right_lower_adspace\r\n";
	line += "###common_right_lower_player_ad_wrapper\r\n";
	line += "###common_right_lower_player_adspace\r\n";
	line += "###common_right_player_ad_wrapper\r\n";
	line += "###common_right_player_adspace\r\n";
	line += "###common_right_right_adspace\r\n";
	line += "###common_top_adspace\r\n";
	line += "###companion-ad\r\n";
	line += "###companionAdDiv\r\n";
	line += "###containerLocalAds\r\n";
	line += "###containerLocalAdsInner\r\n";
	line += "###containerMrecAd\r\n";
	line += "###content-ad-header\r\n";
	line += "###content-header-ad\r\n";
	line += "###contentAd\r\n";
	line += "###contentTopAds2\r\n";
	line += "~filestage.to###content_ad\r\n";
	line += "###content_ad_square\r\n";
	line += "###content_ad_top\r\n";
	line += "###content_ads_content\r\n";
	line += "###content_box_300body_sponsoredoffers\r\n";
	line += "###content_box_adright300_google\r\n";
	line += "###content_mpu\r\n";
	line += "###contentad\r\n";
	line += "###contentad_imtext\r\n";
	line += "###contentad_right\r\n";
	line += "###contentads\r\n";
	line += "###contentinlineAd\r\n";
	line += "###contextad\r\n";
	line += "###contextual-ads\r\n";
	line += "###contextual-ads-block\r\n";
	line += "###contextualad\r\n";
	line += "###coverads\r\n";
	line += "###ctl00_Adspace_Top_Height\r\n";
	line += "###ctl00_BottomAd\r\n";
	line += "###ctl00_ContentRightColumn_RightColumn_Ad1_BanManAd\r\n";
	line += "###ctl00_ContentRightColumn_RightColumn_PremiumAd1_ucBanMan_BanManAd\r\n";
	line += "###ctl00_LHTowerAd\r\n";
	line += "###ctl00_LeftHandAd\r\n";
	line += "###ctl00_MasterHolder_IBanner_adHolder\r\n";
	line += "###ctl00_TopAd\r\n";
	line += "###ctl00_TowerAd\r\n";
	line += "###ctl00_VBanner_adHolder\r\n";
	line += "###ctl00_abot_bb\r\n";
	line += "###ctl00_adFooter\r\n";
	line += "###ctl00_atop_bt\r\n";
	line += "###ctl00_cphMain_hlAd1\r\n";
	line += "###ctl00_cphMain_hlAd2\r\n";
	line += "###ctl00_cphMain_hlAd3\r\n";
	line += "###ctl00_ctl00_MainPlaceHolder_itvAdSkyscraper\r\n";
	line += "###ctl00_ctl00_ctl00_Main_Main_PlaceHolderGoogleTopBanner_MPTopBannerAd\r\n";
	line += "###ctl00_ctl00_ctl00_Main_Main_SideBar_MPSideAd\r\n";
	line += "###ctl00_ctl00_ctl00_tableAdsTop\r\n";
	line += "###ctl00_dlTilesAds\r\n";
	line += "###ctl00_m_skinTracker_m_adLBL\r\n";
	line += "###ctl00_phCrackerMain_ucAffiliateAdvertDisplayMiddle_pnlAffiliateAdvert\r\n";
	line += "###ctl00_phCrackerMain_ucAffiliateAdvertDisplayRight_pnlAffiliateAdvert\r\n";
	line += "###ctrlsponsored\r\n";
	line += "###cubeAd\r\n";
	line += "###cube_ads\r\n";
	line += "###cube_ads_inner\r\n";
	line += "###cubead\r\n";
	line += "###cubead-2\r\n";
	line += "###dItemBox_ads\r\n";
	line += "###dart_160x600\r\n";
	line += "###dc-display-right-ad-1\r\n";
	line += "###dcol-sponsored\r\n";
	line += "###defer-adright\r\n";
	line += "###detail_page_vid_topads\r\n";
	line += "~mtanyct.info###divAd\r\n";
	line += "###divAdBox\r\n";
	line += "###divMenuAds\r\n";
	line += "###divWNAdHeader\r\n";
	line += "###divWrapper_Ad\r\n";
	line += "###div_video_ads\r\n";
	line += "###dlads\r\n";
	line += "###dni-header-ad\r\n";
	line += "###dnn_ad_banner\r\n";
	line += "###download_ads\r\n";
	line += "###ds-mpu\r\n";
	line += "###editorsmpu\r\n";
	line += "###evotopTen_advert\r\n";
	line += "###ex-ligatus\r\n";
	line += "###exads\r\n";
	line += "~discuss.com.hk,~uwants.com###featuread\r\n";
	line += "###featured-advertisements\r\n";
	line += "###featuredAdContainer2\r\n";
	line += "###featuredAds\r\n";
	line += "###feed_links_ad_container\r\n";
	line += "###first-300-ad\r\n";
	line += "###first-adlayer\r\n";
	line += "###first_ad_unit\r\n";
	line += "###firstad\r\n";
	line += "###fl_hdrAd\r\n";
	line += "###flexiad\r\n";
	line += "###footer-ad\r\n";
	line += "###footer-advert\r\n";
	line += "###footer-adverts\r\n";
	line += "###footer-sponsored\r\n";
	line += "###footerAd\r\n";
	line += "###footerAdDiv\r\n";
	line += "###footerAds\r\n";
	line += "###footerAdvertisement\r\n";
	line += "###footerAdverts\r\n";
	line += "###footer_ad\r\n";
	line += "###footer_ad_01\r\n";
	line += "###footer_ad_block\r\n";
	line += "###footer_ad_container\r\n";
	line += "~investopedia.com###footer_ads\r\n";
	line += "###footer_adspace\r\n";
	line += "###footer_text_ad\r\n";
	line += "###footerad\r\n";
	line += "###fr_ad_center\r\n";
	line += "###frame_admain\r\n";
	line += "###frnAdSky\r\n";
	line += "###frnBannerAd\r\n";
	line += "###frnContentAd\r\n";
	line += "###from_our_sponsors\r\n";
	line += "###front_advert\r\n";
	line += "###front_mpu\r\n";
	line += "###ft-ad\r\n";
	line += "###ft-ad-1\r\n";
	line += "###ft-ad-container\r\n";
	line += "###ft_mpu\r\n";
	line += "###fusionad\r\n";
	line += "###fw-advertisement\r\n";
	line += "###g_ad\r\n";
	line += "###g_adsense\r\n";
	line += "###ga_300x250\r\n";
	line += "###gad\r\n";
	line += "###galleries-tower-ad\r\n";
	line += "###gallery-ad-m0\r\n";
	line += "###gallery_ads\r\n";
	line += "###game-info-ad\r\n";
	line += "###gasense\r\n";
	line += "###global_header_ad_area\r\n";
	line += "###gmi-ResourcePageAd\r\n";
	line += "###gmi-ResourcePageLowerAd\r\n";
	line += "###goads\r\n";
	line += "###google-ad\r\n";
	line += "###google-ad-art\r\n";
	line += "###google-ad-table-right\r\n";
	line += "###google-ad-tower\r\n";
	line += "###google-ads\r\n";
	line += "###google-ads-bottom\r\n";
	line += "###google-ads-header\r\n";
	line += "###google-ads-left-side\r\n";
	line += "###google-adsense-mpusize\r\n";
	line += "###googleAd\r\n";
	line += "###googleAds\r\n";
	line += "###googleAdsSml\r\n";
	line += "###googleAdsense\r\n";
	line += "###googleAdsenseBanner\r\n";
	line += "###googleAdsenseBannerBlog\r\n";
	line += "###googleAdwordsModule\r\n";
	line += "###googleAfcContainer\r\n";
	line += "###googleSearchAds\r\n";
	line += "###googleShoppingAdsRight\r\n";
	line += "###googleShoppingAdsTop\r\n";
	line += "###googleSubAds\r\n";
	line += "###google_ad\r\n";
	line += "###google_ad_container\r\n";
	line += "###google_ad_inline\r\n";
	line += "###google_ad_test\r\n";
	line += "###google_ads\r\n";
	line += "###google_ads_frame1\r\n";
	line += "###google_ads_frame1_anchor\r\n";
	line += "###google_ads_test\r\n";
	line += "###google_ads_top\r\n";
	line += "###google_adsense_home_468x60_1\r\n";
	line += "###googlead\r\n";
	line += "###googleadbox\r\n";
	line += "###googleads\r\n";
	line += "###googleadsense\r\n";
	line += "###googlesponsor\r\n";
	line += "###grid_ad\r\n";
	line += "###gsyadrectangleload\r\n";
	line += "###gsyadrightload\r\n";
	line += "###gsyadtop\r\n";
	line += "###gsyadtopload\r\n";
	line += "###gtopadvts\r\n";
	line += "###half-page-ad\r\n";
	line += "###halfPageAd\r\n";
	line += "###halfe-page-ad-box\r\n";
	line += "###hdtv_ad_ss\r\n";
	line += "~uwcu.org###head-ad\r\n";
	line += "###headAd\r\n";
	line += "###head_advert\r\n";
	line += "###headad\r\n";
	line += "###header-ad\r\n";
	line += "###header-ad-rectangle-container\r\n";
	line += "###header-ads\r\n";
	line += "###header-adspace\r\n";
	line += "###header-advert\r\n";
	line += "###header-advertisement\r\n";
	line += "###header-advertising\r\n";
	line += "###headerAd\r\n";
	line += "###headerAdBackground\r\n";
	line += "###headerAdContainer\r\n";
	line += "###headerAdWrap\r\n";
	line += "###headerAds\r\n";
	line += "###headerAdsWrapper\r\n";
	line += "###headerTopAd\r\n";
	line += "~cmt.com###header_ad\r\n";
	line += "###header_ad_728_90\r\n";
	line += "###header_ad_container\r\n";
	line += "###header_adcode\r\n";
	line += "###header_ads\r\n";
	line += "###header_advertisement_top\r\n";
	line += "###header_leaderboard_ad_container\r\n";
	line += "###header_publicidad\r\n";
	line += "###headerad\r\n";
	line += "###headeradbox\r\n";
	line += "###headerads\r\n";
	line += "###headeradwrap\r\n";
	line += "###headline_ad\r\n";
	line += "###headlinesAdBlock\r\n";
	line += "###hiddenadAC\r\n";
	line += "###hideads\r\n";
	line += "###hl-sponsored-results\r\n";
	line += "###homeTopRightAd\r\n";
	line += "###home_ad\r\n";
	line += "###home_bottom_ad\r\n";
	line += "###home_contentad\r\n";
	line += "###home_mpu\r\n";
	line += "###home_spensoredlinks\r\n";
	line += "###homepage-ad\r\n";
	line += "###homepageAdsTop\r\n";
	line += "###homepage_right_ad\r\n";
	line += "###homepage_right_ad_container\r\n";
	line += "###homepage_top_ads\r\n";
	line += "###hometop_234x60ad\r\n";
	line += "###hor_ad\r\n";
	line += "###horizontal-banner-ad\r\n";
	line += "###horizontal_ad\r\n";
	line += "###horizontal_ad_top\r\n";
	line += "###horizontalads\r\n";
	line += "###houseAd\r\n";
	line += "###hp-header-ad\r\n";
	line += "###hp-right-ad\r\n";
	line += "###hp-store-ad\r\n";
	line += "###hpV2_300x250Ad\r\n";
	line += "###hpV2_googAds\r\n";
	line += "###icePage_SearchLinks_AdRightDiv\r\n";
	line += "###icePage_SearchLinks_DownloadToolbarAdRightDiv\r\n";
	line += "###in_serp_ad\r\n";
	line += "###inadspace\r\n";
	line += "###indexad\r\n";
	line += "###inlinead\r\n";
	line += "###inlinegoogleads\r\n";
	line += "###inlist-ad-block\r\n";
	line += "###inner-advert-row\r\n";
	line += "###insider_ad_wrapper\r\n";
	line += "###instoryad\r\n";
	line += "###int-ad\r\n";
	line += "###interstitial_ad_wrapper\r\n";
	line += "###islandAd\r\n";
	line += "###j_ad\r\n";
	line += "###ji_medShowAdBox\r\n";
	line += "###jmp-ad-buttons\r\n";
	line += "###joead\r\n";
	line += "###joead2\r\n";
	line += "###ka_adRightSkyscraperWide\r\n";
	line += "###landing-adserver\r\n";
	line += "###largead\r\n";
	line += "###lateAd\r\n";
	line += "###layerTLDADSERV\r\n";
	line += "###lb-sponsor-left\r\n";
	line += "###lb-sponsor-right\r\n";
	line += "###leader-board-ad\r\n";
	line += "###leader-sponsor\r\n";
	line += "###leaderAdContainer\r\n";
	line += "###leader_board_ad\r\n";
	line += "###leaderad\r\n";
	line += "###leaderad_section\r\n";
	line += "###leaderboard-ad\r\n";
	line += "###leaderboard-bottom-ad\r\n";
	line += "###leaderboard_ad\r\n";
	line += "###left-ad-skin\r\n";
	line += "###left-lower-adverts\r\n";
	line += "###left-lower-adverts-container\r\n";
	line += "###leftAdContainer\r\n";
	line += "###leftAd_rdr\r\n";
	line += "###leftAdvert\r\n";
	line += "###leftSectionAd300-100\r\n";
	line += "###left_ad\r\n";
	line += "###left_adspace\r\n";
	line += "###leftad\r\n";
	line += "###leftads\r\n";
	line += "###lg-banner-ad\r\n";
	line += "###ligatus\r\n";
	line += "###linkAds\r\n";
	line += "###linkads\r\n";
	line += "###live-ad\r\n";
	line += "###longAdSpace\r\n";
	line += "###lowerAdvertisementImg\r\n";
	line += "###lowerads\r\n";
	line += "###lowerthirdad\r\n";
	line += "###lowertop-adverts\r\n";
	line += "###lowertop-adverts-container\r\n";
	line += "###lrecad\r\n";
	line += "###lsadvert-left_menu_1\r\n";
	line += "###lsadvert-left_menu_2\r\n";
	line += "###lsadvert-top\r\n";
	line += "###mBannerAd\r\n";
	line += "###main-ad\r\n";
	line += "###main-ad160x600\r\n";
	line += "###main-ad160x600-img\r\n";
	line += "###main-ad728x90\r\n";
	line += "###main-bottom-ad\r\n";
	line += "###mainAd\r\n";
	line += "###mainAdUnit\r\n";
	line += "###mainAdvert\r\n";
	line += "###main_ad\r\n";
	line += "###main_rec_ad\r\n";
	line += "###main_top_ad_container\r\n";
	line += "###marketing-promo\r\n";
	line += "###mastAdvert\r\n";
	line += "###mastad\r\n";
	line += "###mastercardAd\r\n";
	line += "###masthead_ad\r\n";
	line += "###masthead_topad\r\n";
	line += "###medRecAd\r\n";
	line += "###media_ad\r\n";
	line += "###mediumAdvertisement\r\n";
	line += "###medrectad\r\n";
	line += "###menuAds\r\n";
	line += "###mi_story_assets_ad\r\n";
	line += "###mid-ad300x250\r\n";
	line += "###mid-table-ad\r\n";
	line += "###midRightTextAds\r\n";
	line += "###mid_ad_div\r\n";
	line += "###mid_ad_title\r\n";
	line += "###mid_mpu\r\n";
	line += "###midadd\r\n";
	line += "###midadspace\r\n";
	line += "###middle-ad\r\n";
	line += "###middlead\r\n";
	line += "###middleads\r\n";
	line += "###midrect_ad\r\n";
	line += "###midstrip_ad\r\n";
	line += "###mini-ad\r\n";
	line += "###module-google_ads\r\n";
	line += "###module_ad\r\n";
	line += "###module_box_ad\r\n";
	line += "###module_sky_scraper\r\n";
	line += "###monsterAd\r\n";
	line += "###moogleAd\r\n";
	line += "###most_popular_ad\r\n";
	line += "###motionAd\r\n";
	line += "###mpu\r\n";
	line += "###mpu-advert\r\n";
	line += "###mpuAd\r\n";
	line += "###mpuDiv\r\n";
	line += "###mpuSlot\r\n";
	line += "###mpuWrapper\r\n";
	line += "###mpuWrapperAd\r\n";
	line += "###mpu_banner\r\n";
	line += "###mpu_holder\r\n";
	line += "###mpu_text_ad\r\n";
	line += "###mpuad\r\n";
	line += "###mrecAdContainer\r\n";
	line += "###ms_ad\r\n";
	line += "###msad\r\n";
	line += "###multiLinkAdContainer\r\n";
	line += "###myads_HeaderButton\r\n";
	line += "###n_sponsor_ads\r\n";
	line += "###namecom_ad_hosting_main\r\n";
	line += "###narrow_ad_unit\r\n";
	line += "###natadad300x250\r\n";
	line += "###national_microlink_ads\r\n";
	line += "###nationalad\r\n";
	line += "###navi_banner_ad_780\r\n";
	line += "###nba300Ad\r\n";
	line += "###nbaMidAds\r\n";
	line += "###nbaVid300Ad\r\n";
	line += "###new_topad\r\n";
	line += "###newads\r\n";
	line += "###ng_rtcol_ad\r\n";
	line += "###noresultsads\r\n";
	line += "###northad\r\n";
	line += "###oanda_ads\r\n";
	line += "###onespot-ads\r\n";
	line += "###online_ad\r\n";
	line += "###p-googleadsense\r\n";
	line += "###page-header-ad\r\n";
	line += "###pageAds\r\n";
	line += "###pageAdsDiv\r\n";
	line += "###page_content_top_ad\r\n";
	line += "###pagelet_adbox\r\n";
	line += "###panelAd\r\n";
	line += "###pb_report_ad\r\n";
	line += "###pcworldAdBottom\r\n";
	line += "###pcworldAdTop\r\n";
	line += "###pinball_ad\r\n";
	line += "###player-below-advert\r\n";
	line += "###player_ad\r\n";
	line += "###player_ads\r\n";
	line += "###pod-ad-video-page\r\n";
	line += "###populate_ad_bottom\r\n";
	line += "###populate_ad_left\r\n";
	line += "###portlet-advertisement-left\r\n";
	line += "###portlet-advertisement-right\r\n";
	line += "###post-promo-ad\r\n";
	line += "###post5_adbox\r\n";
	line += "###post_ad\r\n";
	line += "###premium_ad\r\n";
	line += "###priceGrabberAd\r\n";
	line += "###print_ads\r\n";
	line += "~bipbip.co.il###printads\r\n";
	line += "###product-adsense\r\n";
	line += "~flickr.com###promo-ad\r\n";
	line += "###promoAds\r\n";
	line += "###ps-vertical-ads\r\n";
	line += "###pub468x60\r\n";
	line += "###publicidad\r\n";
	line += "###pushdown_ad\r\n";
	line += "###qm-ad-big-box\r\n";
	line += "###qm-ad-sky\r\n";
	line += "###qm-dvdad\r\n";
	line += "###r1SoftAd\r\n";
	line += "###rail_ad1\r\n";
	line += "###rail_ad2\r\n";
	line += "###realEstateAds\r\n";
	line += "###rectAd\r\n";
	line += "###rect_ad\r\n";
	line += "###rectangle-ad\r\n";
	line += "###rectangle_ad\r\n";
	line += "###refine-300-ad\r\n";
	line += "###region-top-ad\r\n";
	line += "###rh-ad-container\r\n";
	line += "###rh_tower_ad\r\n";
	line += "###rhs_ads\r\n";
	line += "###rhsadvert\r\n";
	line += "###right-ad\r\n";
	line += "###right-ad-skin\r\n";
	line += "###right-ad-title\r\n";
	line += "###right-ads-3\r\n";
	line += "###right-box-ad\r\n";
	line += "###right-featured-ad\r\n";
	line += "###right-mpu-1-ad-container\r\n";
	line += "###right-uppder-adverts\r\n";
	line += "###right-uppder-adverts-container\r\n";
	line += "###rightAd\r\n";
	line += "###rightAd300x250\r\n";
	line += "###rightAdColumn\r\n";
	line += "###rightAd_rdr\r\n";
	line += "###rightColAd\r\n";
	line += "###rightColumnMpuAd\r\n";
	line += "###rightColumnSkyAd\r\n";
	line += "###right_ad\r\n";
	line += "###right_ad_wrapper\r\n";
	line += "###right_ads\r\n";
	line += "###right_advertisement\r\n";
	line += "###right_advertising\r\n";
	line += "###right_column_ads\r\n";
	line += "###rightad\r\n";
	line += "###rightadContainer\r\n";
	line += "###rightadvertbar-doubleclickads\r\n";
	line += "###rightbar-ad\r\n";
	line += "###rightside-ads\r\n";
	line += "###rightside_ad\r\n";
	line += "###righttop-adverts\r\n";
	line += "###righttop-adverts-container\r\n";
	line += "###rm_ad_text\r\n";
	line += "###ros_ad\r\n";
	line += "###rotatingads\r\n";
	line += "###row2AdContainer\r\n";
	line += "###rt-ad\r\n";
	line += "###rt-ad-top\r\n";
	line += "###rt-ad468\r\n";
	line += "###rtMod_ad\r\n";
	line += "###rtmod_ad\r\n";
	line += "###sAdsBox\r\n";
	line += "###sb-ad-sq\r\n";
	line += "###sb_advert\r\n";
	line += "###sb_sponsors\r\n";
	line += "###search-google-ads\r\n";
	line += "###searchAdSenseBox\r\n";
	line += "###searchAdSenseBoxAd\r\n";
	line += "###searchAdSkyscraperBox\r\n";
	line += "###search_ads\r\n";
	line += "###search_result_ad\r\n";
	line += "###second-adlayer\r\n";
	line += "###secondBoxAdContainer\r\n";
	line += "###section-container-ddc_ads\r\n";
	line += "###section-sponsors\r\n";
	line += "###section_advertorial_feature\r\n";
	line += "###servfail-ads\r\n";
	line += "###sew-ad1\r\n";
	line += "###shoppingads\r\n";
	line += "###show-ad\r\n";
	line += "###showAd\r\n";
	line += "###showad\r\n";
	line += "###side-ad\r\n";
	line += "###side-ad-container\r\n";
	line += "###sideAd\r\n";
	line += "###sideAdSub\r\n";
	line += "###sideBarAd\r\n";
	line += "###side_ad\r\n";
	line += "###side_ad_wrapper\r\n";
	line += "###side_ads_by_google\r\n";
	line += "###side_sky_ad\r\n";
	line += "###sidead\r\n";
	line += "###sideads\r\n";
	line += "###sidebar-125x125-ads\r\n";
	line += "###sidebar-125x125-ads-below-index\r\n";
	line += "###sidebar-ad\r\n";
	line += "###sidebar-ad-boxes\r\n";
	line += "###sidebar-ad-space\r\n";
	line += "###sidebar-ad-wrap\r\n";
	line += "###sidebar-ad3\r\n";
	line += "~gaelick.com###sidebar-ads\r\n";
	line += "###sidebar2ads\r\n";
	line += "###sidebar_ad_widget\r\n";
	line += "~facebook.com,~japantoday.com###sidebar_ads\r\n";
	line += "###sidebar_ads_180\r\n";
	line += "###sidebar_sponsoredresult_body\r\n";
	line += "###sidebarad\r\n";
	line += "###sideline-ad\r\n";
	line += "###single-mpu\r\n";
	line += "###singlead\r\n";
	line += "###site-leaderboard-ads\r\n";
	line += "###site_top_ad\r\n";
	line += "###sitead\r\n";
	line += "###sky-ad\r\n";
	line += "###skyAd\r\n";
	line += "###skyAdContainer\r\n";
	line += "###skyScrapperAd\r\n";
	line += "###skyWrapperAds\r\n";
	line += "###sky_ad\r\n";
	line += "###sky_advert\r\n";
	line += "###skyads\r\n";
	line += "###skyscraper-ad\r\n";
	line += "###skyscraperAd\r\n";
	line += "###skyscraperAdContainer\r\n";
	line += "###skyscraper_ad\r\n";
	line += "###skyscraper_advert\r\n";
	line += "###skyscraperad\r\n";
	line += "###sliderAdHolder\r\n";
	line += "###slideshow_ad_300x250\r\n";
	line += "###sm-banner-ad\r\n";
	line += "###small_ad\r\n";
	line += "###smallerAd\r\n";
	line += "###specials_ads\r\n";
	line += "###speeds_ads\r\n";
	line += "###speeds_ads_fstitem\r\n";
	line += "###speedtest_mrec_ad\r\n";
	line += "###sphereAd\r\n";
	line += "###splinks\r\n";
	line += "###sponLinkDiv_1\r\n";
	line += "###sponlink\r\n";
	line += "###sponsAds\r\n";
	line += "###sponsLinks\r\n";
	line += "###spons_left\r\n";
	line += "###sponseredlinks\r\n";
	line += "###sponsor-search\r\n";
	line += "###sponsorAd1\r\n";
	line += "###sponsorAd2\r\n";
	line += "###sponsorAdDiv\r\n";
	line += "###sponsorLinks\r\n";
	line += "###sponsorTextLink\r\n";
	line += "###sponsor_banderole\r\n";
	line += "###sponsor_box\r\n";
	line += "###sponsor_deals\r\n";
	line += "###sponsor_panSponsor\r\n";
	line += "###sponsor_recommendations\r\n";
	line += "###sponsorbar\r\n";
	line += "###sponsorbox\r\n";
	line += "~hollywood.com,~worldsbestbars.com###sponsored\r\n";
	line += "###sponsored-ads\r\n";
	line += "###sponsored-features\r\n";
	line += "###sponsored-links\r\n";
	line += "###sponsored-resources\r\n";
	line += "###sponsored1\r\n";
	line += "###sponsoredBox1\r\n";
	line += "###sponsoredBox2\r\n";
	line += "###sponsoredLinks\r\n";
	line += "###sponsoredList\r\n";
	line += "###sponsoredResults\r\n";
	line += "###sponsoredSiteMainline\r\n";
	line += "###sponsoredSiteSidebar\r\n";
	line += "###sponsored_ads_v4\r\n";
	line += "###sponsored_content\r\n";
	line += "###sponsored_game_row_listing\r\n";
	line += "###sponsored_links\r\n";
	line += "###sponsored_v12\r\n";
	line += "###sponsoredlinks\r\n";
	line += "###sponsoredlinks_cntr\r\n";
	line += "###sponsoredresults_top\r\n";
	line += "###sponsoredwellcontainerbottom\r\n";
	line += "###sponsoredwellcontainertop\r\n";
	line += "###sponsorlink\r\n";
	line += "###sponsors\r\n";
	line += "###sponsors_top_container\r\n";
	line += "###sponsorshipBadge\r\n";
	line += "###spotlightAds\r\n";
	line += "###spotlightad\r\n";
	line += "###sqAd\r\n";
	line += "###square-sponsors\r\n";
	line += "###squareAd\r\n";
	line += "###squareAdSpace\r\n";
	line += "###squareAds\r\n";
	line += "###square_ad\r\n";
	line += "###start_middle_container_advertisment\r\n";
	line += "###sticky-ad\r\n";
	line += "###stickyBottomAd\r\n";
	line += "###story-ad-a\r\n";
	line += "###story-ad-b\r\n";
	line += "###story-leaderboard-ad\r\n";
	line += "###story-sponsoredlinks\r\n";
	line += "###storyAd\r\n";
	line += "###storyAdWrap\r\n";
	line += "###storyad2\r\n";
	line += "###subpage-ad-right\r\n";
	line += "###subpage-ad-top\r\n";
	line += "###swads\r\n";
	line += "###synch-ad\r\n";
	line += "###systemad_background\r\n";
	line += "###tabAdvertising\r\n";
	line += "###takeoverad\r\n";
	line += "###tblAd\r\n";
	line += "###tbl_googlead\r\n";
	line += "###tcwAd\r\n";
	line += "###template_ad_leaderboard\r\n";
	line += "###tertiary_advertising\r\n";
	line += "###text-ad\r\n";
	line += "###text-ads\r\n";
	line += "###textAd\r\n";
	line += "###textAds\r\n";
	line += "###text_ad\r\n";
	line += "###text_ads\r\n";
	line += "###text_advert\r\n";
	line += "###textad\r\n";
	line += "###textad3\r\n";
	line += "###the-last-ad-standing\r\n";
	line += "###thefooterad\r\n";
	line += "###themis-ads\r\n";
	line += "###tile-ad\r\n";
	line += "###tmglBannerAd\r\n";
	line += "###top-ad\r\n";
	line += "###top-ad-container\r\n";
	line += "###top-ad-menu\r\n";
	line += "###top-ads\r\n";
	line += "###top-ads-tabs\r\n";
	line += "###top-advertisement\r\n";
	line += "###top-banner-ad\r\n";
	line += "###top-search-ad-wrapper\r\n";
	line += "###topAd\r\n";
	line += "###topAd728x90\r\n";
	line += "###topAdBanner\r\n";
	line += "###topAdContainer\r\n";
	line += "###topAdSenseDiv\r\n";
	line += "###topAdcontainer\r\n";
	line += "###topAds\r\n";
	line += "###topAdsContainer\r\n";
	line += "###topAdvert\r\n";
	line += "~neowin.net###topBannerAd\r\n";
	line += "###topNavLeaderboardAdHolder\r\n";
	line += "###topRightBlockAdSense\r\n";
	line += "~morningstar.se###top_ad\r\n";
	line += "###top_ad_area\r\n";
	line += "###top_ad_game\r\n";
	line += "###top_ad_wrapper\r\n";
	line += "###top_ads\r\n";
	line += "###top_advertise\r\n";
	line += "###top_advertising\r\n";
	line += "###top_right_ad\r\n";
	line += "###top_wide_ad\r\n";
	line += "~bumpshack.com###topad\r\n";
	line += "###topad_left\r\n";
	line += "###topad_right\r\n";
	line += "###topadblock\r\n";
	line += "###topaddwide\r\n";
	line += "###topads\r\n";
	line += "###topadsense\r\n";
	line += "###topadspace\r\n";
	line += "###topadzone\r\n";
	line += "###topcustomad\r\n";
	line += "###topleaderboardad\r\n";
	line += "###toprightAdvert\r\n";
	line += "###toprightad\r\n";
	line += "###topsponsored\r\n";
	line += "###toptextad\r\n";
	line += "###towerad\r\n";
	line += "###ttp_ad_slot1\r\n";
	line += "###ttp_ad_slot2\r\n";
	line += "###twogamesAd\r\n";
	line += "###txt_link_ads\r\n";
	line += "###undergameAd\r\n";
	line += "###upperAdvertisementImg\r\n";
	line += "###upperMpu\r\n";
	line += "###upperad\r\n";
	line += "###urban_contentad_1\r\n";
	line += "###urban_contentad_2\r\n";
	line += "###urban_contentad_article\r\n";
	line += "###v_ad\r\n";
	line += "###vert_ad\r\n";
	line += "###vert_ad_placeholder\r\n";
	line += "###vertical_ad\r\n";
	line += "###vertical_ads\r\n";
	line += "###videoAd\r\n";
	line += "###video_cnv_ad\r\n";
	line += "###video_overlay_ad\r\n";
	line += "###videoadlogo\r\n";
	line += "###viewportAds\r\n";
	line += "###walltopad\r\n";
	line += "###weblink_ads_container\r\n";
	line += "###welcomeAdsContainer\r\n";
	line += "###welcome_ad_mrec\r\n";
	line += "###welcome_advertisement\r\n";
	line += "###wf_ContentAd\r\n";
	line += "###wf_FrontSingleAd\r\n";
	line += "###wf_SingleAd\r\n";
	line += "###wf_bottomContentAd\r\n";
	line += "###wgtAd\r\n";
	line += "###whatsnews_top_ad\r\n";
	line += "###whitepaper-ad\r\n";
	line += "###whoisRightAdContainer\r\n";
	line += "###wide_ad_unit_top\r\n";
	line += "###widget_advertisement\r\n";
	line += "###wrapAdRight\r\n";
	line += "###wrapAdTop\r\n";
	line += "###y-ad-units\r\n";
	line += "###y708-ad-expedia\r\n";
	line += "###y708-ad-lrec\r\n";
	line += "###y708-ad-partners\r\n";
	line += "###y708-ad-ysm\r\n";
	line += "###y708-advertorial-marketplace\r\n";
	line += "###yahoo-ads\r\n";
	line += "###yahoo-sponsors\r\n";
	line += "###yahooSponsored\r\n";
	line += "###yahoo_ads\r\n";
	line += "###yahoo_ads_2010\r\n";
	line += "###yahooad-tbl\r\n";
	line += "###yan-sponsored\r\n";
	line += "###ybf-ads\r\n";
	line += "###yfi_fp_ad_mort\r\n";
	line += "###yfi_fp_ad_nns\r\n";
	line += "###yfi_pf_ad_mort\r\n";
	line += "###ygrp-sponsored-links\r\n";
	line += "###ymap_adbanner\r\n";
	line += "###yn-gmy-ad-lrec\r\n";
	line += "###yreSponsoredLinks\r\n";
	line += "###ysm_ad_iframe\r\n";
	line += "###zoneAdserverMrec\r\n";
	line += "###zoneAdserverSuper\r\n";
	line += "##.ADBAR\r\n";
	line += "##.ADPod\r\n";
	line += "##.AD_ALBUM_ITEMLIST\r\n";
	line += "##.AD_MOVIE_ITEM\r\n";
	line += "##.AD_MOVIE_ITEMLIST\r\n";
	line += "##.AD_MOVIE_ITEMROW\r\n";
	line += "##.Ad-MPU\r\n";
	line += "##.Ad120x600\r\n";
	line += "##.Ad160x600\r\n";
	line += "##.Ad160x600left\r\n";
	line += "##.Ad160x600right\r\n";
	line += "##.Ad247x90\r\n";
	line += "##.Ad300x250\r\n";
	line += "##.Ad300x250L\r\n";
	line += "##.Ad728x90\r\n";
	line += "##.AdBorder\r\n";
	line += "~co-operative.coop##.AdBox\r\n";
	line += "##.AdBox7\r\n";
	line += "##.AdContainerBox308\r\n";
	line += "##.AdHeader\r\n";
	line += "##.AdHere\r\n";
	line += "~backpage.com##.AdInfo\r\n";
	line += "##.AdMedium\r\n";
	line += "##.AdPlaceHolder\r\n";
	line += "##.AdRingtone\r\n";
	line += "##.AdSense\r\n";
	line += "##.AdSpace\r\n";
	line += "##.AdTextSmallFont\r\n";
	line += "~buy.com,~superbikeplanet.com##.AdTitle\r\n";
	line += "##.AdUnit\r\n";
	line += "##.AdUnit300\r\n";
	line += "##.Ad_C\r\n";
	line += "##.Ad_D_Wrapper\r\n";
	line += "##.Ad_E_Wrapper\r\n";
	line += "##.Ad_Right\r\n";
	line += "~thecoolhunter.net##.Ads\r\n";
	line += "##.AdsBoxBottom\r\n";
	line += "##.AdsBoxSection\r\n";
	line += "##.AdsBoxTop\r\n";
	line += "##.AdsLinks1\r\n";
	line += "##.AdsLinks2\r\n";
	line += "~swanseacity.net,~wrexhamafc.co.uk##.Advert\r\n";
	line += "##.AdvertMidPage\r\n";
	line += "##.AdvertiseWithUs\r\n";
	line += "##.AdvertisementTextTag\r\n";
	line += "##.ArticleAd\r\n";
	line += "##.ArticleInlineAd\r\n";
	line += "##.BannerAd\r\n";
	line += "##.BigBoxAd\r\n";
	line += "##.BlockAd\r\n";
	line += "##.BottomAdContainer\r\n";
	line += "##.BottomAffiliate\r\n";
	line += "##.BoxAd\r\n";
	line += "##.CG_adkit_leaderboard\r\n";
	line += "##.CG_details_ad_dropzone\r\n";
	line += "##.ComAread\r\n";
	line += "##.CommentAd\r\n";
	line += "##.ContentAd\r\n";
	line += "##.ContentAds\r\n";
	line += "##.DAWRadvertisement\r\n";
	line += "##.DeptAd\r\n";
	line += "##.DisplayAd\r\n";
	line += "##.FT_Ad\r\n";
	line += "##.FlatAds\r\n";
	line += "##.GOOGLE_AD\r\n";
	line += "##.GoogleAd\r\n";
	line += "##.HPNewAdsBannerDiv\r\n";
	line += "##.HPRoundedAd\r\n";
	line += "##.HomeContentAd\r\n";
	line += "##.IABAdSpace\r\n";
	line += "##.IndexRightAd\r\n";
	line += "##.LazyLoadAd\r\n";
	line += "##.LeftAd\r\n";
	line += "##.LeftTowerAd\r\n";
	line += "##.M2Advertisement\r\n";
	line += "##.MD_adZone\r\n";
	line += "##.MOS-ad-hack\r\n";
	line += "##.MPU\r\n";
	line += "##.MPUHolder\r\n";
	line += "##.MPUTitleWrapperClass\r\n";
	line += "##.MiddleAd\r\n";
	line += "##.MiddleAdContainer\r\n";
	line += "##.OpenXad\r\n";
	line += "##.PU_DoubleClickAdsContent\r\n";
	line += "##.Post5ad\r\n";
	line += "##.RBboxAd\r\n";
	line += "##.RectangleAd\r\n";
	line += "##.RelatedAds\r\n";
	line += "##.RightAd1\r\n";
	line += "##.RightGoogleAFC\r\n";
	line += "##.RightRailTop300x250Ad\r\n";
	line += "##.RightSponsoredAdTitle\r\n";
	line += "##.RightTowerAd\r\n";
	line += "##.SideAdCol\r\n";
	line += "##.SidebarAd\r\n";
	line += "##.SitesGoogleAdsModule\r\n";
	line += "##.SkyAdContainer\r\n";
	line += "##.SponsorCFrame\r\n";
	line += "##.SponsoredAdTitle\r\n";
	line += "##.SponsoredContent\r\n";
	line += "##.SponsoredLinks\r\n";
	line += "##.SponsoredLinksGrayBox\r\n";
	line += "##.SponsorshipText\r\n";
	line += "##.SquareAd\r\n";
	line += "##.StandardAdLeft\r\n";
	line += "##.StandardAdRight\r\n";
	line += "##.TextAd\r\n";
	line += "##.TheEagleGoogleAdSense300x250\r\n";
	line += "##.TopAd\r\n";
	line += "##.TopAdContainer\r\n";
	line += "##.TopAdL\r\n";
	line += "##.TopAdR\r\n";
	line += "##.TopBannerAd\r\n";
	line += "##.UIStandardFrame_SidebarAds\r\n";
	line += "##.UIWashFrame_SidebarAds\r\n";
	line += "##.UnderAd\r\n";
	line += "##.VerticalAd\r\n";
	line += "##.VideoAd\r\n";
	line += "##.WidgetAdvertiser\r\n";
	line += "##.a160x600\r\n";
	line += "##.a728x90\r\n";
	line += "##.ad-120x600\r\n";
	line += "##.ad-160\r\n";
	line += "##.ad-160x600\r\n";
	line += "##.ad-250\r\n";
	line += "##.ad-300\r\n";
	line += "##.ad-300-block\r\n";
	line += "##.ad-300-blog\r\n";
	line += "##.ad-300x100\r\n";
	line += "##.ad-300x250\r\n";
	line += "##.ad-300x250-right0\r\n";
	line += "##.ad-350\r\n";
	line += "##.ad-355x75\r\n";
	line += "##.ad-600\r\n";
	line += "##.ad-635x40\r\n";
	line += "##.ad-728\r\n";
	line += "##.ad-728x90\r\n";
	line += "##.ad-728x90-1\r\n";
	line += "##.ad-728x90_forum\r\n";
	line += "##.ad-above-header\r\n";
	line += "##.ad-adlink-bottom\r\n";
	line += "##.ad-adlink-side\r\n";
	line += "##.ad-background\r\n";
	line += "##.ad-banner\r\n";
	line += "##.ad-bigsize\r\n";
	line += "##.ad-block\r\n";
	line += "##.ad-blog2biz\r\n";
	line += "##.ad-bottom\r\n";
	line += "##.ad-box\r\n";
	line += "##.ad-break\r\n";
	line += "##.ad-btn\r\n";
	line += "##.ad-btn-heading\r\n";
	line += "~assetbar.com##.ad-button\r\n";
	line += "##.ad-cell\r\n";
	line += "~arbetsformedlingen.se##.ad-container\r\n";
	line += "##.ad-disclaimer\r\n";
	line += "##.ad-display\r\n";
	line += "##.ad-div\r\n";
	line += "##.ad-enabled\r\n";
	line += "##.ad-feedback\r\n";
	line += "##.ad-filler\r\n";
	line += "##.ad-footer\r\n";
	line += "##.ad-footer-leaderboard\r\n";
	line += "##.ad-google\r\n";
	line += "##.ad-graphic-large\r\n";
	line += "##.ad-gray\r\n";
	line += "##.ad-hdr\r\n";
	line += "##.ad-head\r\n";
	line += "##.ad-holder\r\n";
	line += "##.ad-homeleaderboard\r\n";
	line += "##.ad-img\r\n";
	line += "##.ad-island\r\n";
	line += "##.ad-label\r\n";
	line += "##.ad-leaderboard\r\n";
	line += "##.ad-links\r\n";
	line += "##.ad-lrec\r\n";
	line += "##.ad-medium\r\n";
	line += "##.ad-medium-two\r\n";
	line += "##.ad-mpu\r\n";
	line += "##.ad-note\r\n";
	line += "##.ad-notice\r\n";
	line += "##.ad-other\r\n";
	line += "##.ad-permalink\r\n";
	line += "##.ad-placeholder\r\n";
	line += "##.ad-postText\r\n";
	line += "##.ad-poster\r\n";
	line += "##.ad-priority\r\n";
	line += "##.ad-rect\r\n";
	line += "##.ad-rectangle\r\n";
	line += "##.ad-rectangle-text\r\n";
	line += "##.ad-related\r\n";
	line += "##.ad-rh\r\n";
	line += "##.ad-ri\r\n";
	line += "##.ad-right\r\n";
	line += "##.ad-right-header\r\n";
	line += "##.ad-right-txt\r\n";
	line += "##.ad-row\r\n";
	line += "##.ad-section\r\n";
	line += "~ifokus.se##.ad-sidebar\r\n";
	line += "##.ad-sidebar-outer\r\n";
	line += "##.ad-sidebar300\r\n";
	line += "##.ad-sky\r\n";
	line += "##.ad-slot\r\n";
	line += "##.ad-slot-234-60\r\n";
	line += "##.ad-slot-300-250\r\n";
	line += "##.ad-slot-728-90\r\n";
	line += "##.ad-space\r\n";
	line += "##.ad-space-mpu-box\r\n";
	line += "##.ad-spot\r\n";
	line += "##.ad-squares\r\n";
	line += "##.ad-statement\r\n";
	line += "##.ad-tabs\r\n";
	line += "##.ad-text\r\n";
	line += "##.ad-text-links\r\n";
	line += "##.ad-tile\r\n";
	line += "##.ad-title\r\n";
	line += "##.ad-top\r\n";
	line += "##.ad-top-left\r\n";
	line += "##.ad-unit\r\n";
	line += "##.ad-unit-300\r\n";
	line += "##.ad-unit-300-wrapper\r\n";
	line += "##.ad-unit-anchor\r\n";
	line += "##.ad-vert\r\n";
	line += "##.ad-vtu\r\n";
	line += "##.ad-wrap\r\n";
	line += "##.ad-wrapper\r\n";
	line += "##.ad-zone-s-q-l\r\n";
	line += "##.ad.super\r\n";
	line += "##.ad0\r\n";
	line += "##.ad1\r\n";
	line += "##.ad10\r\n";
	line += "##.ad120\r\n";
	line += "##.ad120x600\r\n";
	line += "##.ad125\r\n";
	line += "##.ad160\r\n";
	line += "##.ad160x600\r\n";
	line += "##.ad18\r\n";
	line += "##.ad19\r\n";
	line += "##.ad2\r\n";
	line += "##.ad21\r\n";
	line += "##.ad250\r\n";
	line += "##.ad250c\r\n";
	line += "##.ad3\r\n";
	line += "##.ad300\r\n";
	line += "##.ad300250\r\n";
	line += "##.ad300_250\r\n";
	line += "##.ad300x100\r\n";
	line += "##.ad300x250\r\n";
	line += "##.ad300x250-hp-features\r\n";
	line += "##.ad300x250Top\r\n";
	line += "##.ad300x250_container\r\n";
	line += "##.ad300x250box\r\n";
	line += "##.ad300x50-right\r\n";
	line += "##.ad300x600\r\n";
	line += "##.ad310\r\n";
	line += "##.ad336x280\r\n";
	line += "##.ad343x290\r\n";
	line += "##.ad4\r\n";
	line += "##.ad400right\r\n";
	line += "##.ad450\r\n";
	line += "~itavisen.no##.ad468\r\n";
	line += "##.ad468_60\r\n";
	line += "##.ad468x60\r\n";
	line += "##.ad6\r\n";
	line += "##.ad620x70\r\n";
	line += "##.ad626X35\r\n";
	line += "##.ad7\r\n";
	line += "##.ad728\r\n";
	line += "##.ad728_90\r\n";
	line += "##.ad728x90\r\n";
	line += "##.ad728x90_container\r\n";
	line += "##.ad8\r\n";
	line += "##.ad90x780\r\n";
	line += "##.adAgate\r\n";
	line += "##.adArea674x60\r\n";
	line += "##.adBanner\r\n";
	line += "##.adBanner300x250\r\n";
	line += "##.adBanner728x90\r\n";
	line += "##.adBannerTyp1\r\n";
	line += "##.adBannerTypSortableList\r\n";
	line += "##.adBannerTypW300\r\n";
	line += "##.adBar\r\n";
	line += "##.adBgBottom\r\n";
	line += "##.adBgMId\r\n";
	line += "##.adBgTop\r\n";
	line += "##.adBlock\r\n";
	line += "##.adBottomboxright\r\n";
	line += "~ksl.com##.adBox\r\n";
	line += "##.adBoxBody\r\n";
	line += "##.adBoxBorder\r\n";
	line += "##.adBoxContent\r\n";
	line += "##.adBoxInBignews\r\n";
	line += "##.adBoxSidebar\r\n";
	line += "##.adBoxSingle\r\n";
	line += "##.adCMRight\r\n";
	line += "##.adColumn\r\n";
	line += "##.adCont\r\n";
	line += "##.adContTop\r\n";
	line += "~mycareer.com.au,~nytimes.com##.adContainer\r\n";
	line += "##.adContour\r\n";
	line += "##.adCreative\r\n";
	line += "~superbikeplanet.com##.adDiv\r\n";
	line += "~contracostatimes.com,~mercurynews.com,~siliconvalley.com##.adElement\r\n";
	line += "##.adFender3\r\n";
	line += "##.adFrame\r\n";
	line += "##.adFtr\r\n";
	line += "##.adFullWidthMiddle\r\n";
	line += "##.adGoogle\r\n";
	line += "##.adHeader\r\n";
	line += "##.adHeadline\r\n";
	line += "~superhry.cz##.adHolder\r\n";
	line += "##.adHome300x250\r\n";
	line += "##.adHorisontal\r\n";
	line += "##.adInNews\r\n";
	line += "##.adLabel\r\n";
	line += "##.adLeader\r\n";
	line += "##.adLeaderForum\r\n";
	line += "##.adLeaderboard\r\n";
	line += "##.adLeft\r\n";
	line += "##.adLoaded\r\n";
	line += "##.adLocal\r\n";
	line += "##.adMastheadLeft\r\n";
	line += "##.adMastheadRight\r\n";
	line += "##.adMegaBoard\r\n";
	line += "##.adMkt2Colw\r\n";
	line += "~outspark.com##.adModule\r\n";
	line += "##.adMpu\r\n";
	line += "##.adNewsChannel\r\n";
	line += "##.adNoOutline\r\n";
	line += "##.adNotice\r\n";
	line += "##.adNoticeOut\r\n";
	line += "##.adObj\r\n";
	line += "##.adPageBorderL\r\n";
	line += "##.adPageBorderR\r\n";
	line += "##.adPanel\r\n";
	line += "##.adRect\r\n";
	line += "##.adRight\r\n";
	line += "##.adSelfServiceAdvertiseLink\r\n";
	line += "##.adServer\r\n";
	line += "##.adSkyscraperHolder\r\n";
	line += "##.adSlot\r\n";
	line += "##.adSpBelow\r\n";
	line += "~o2online.de##.adSpace\r\n";
	line += "##.adSpacer\r\n";
	line += "##.adSponsor\r\n";
	line += "##.adSpot\r\n";
	line += "##.adSpot-searchAd\r\n";
	line += "##.adSpot-textBox\r\n";
	line += "##.adSpot-twin\r\n";
	line += "##.adSpotIsland\r\n";
	line += "##.adSquare\r\n";
	line += "~marktplaats.nl##.adSummary\r\n";
	line += "##.adSuperboard\r\n";
	line += "##.adSupertower\r\n";
	line += "##.adTD\r\n";
	line += "##.adTab\r\n";
	line += "##.adTag\r\n";
	line += "~bipbip.co.il##.adText\r\n";
	line += "##.adTileWrap\r\n";
	line += "##.adTiler\r\n";
	line += "~ksl.com,~stadtlist.de,~superbikeplanet.com##.adTitle\r\n";
	line += "##.adTopboxright\r\n";
	line += "##.adTout\r\n";
	line += "##.adTxt\r\n";
	line += "##.adUnitHorz\r\n";
	line += "##.adUnitVert\r\n";
	line += "##.adUnitVert_noImage\r\n";
	line += "##.adWebBoard\r\n";
	line += "##.adWidget\r\n";
	line += "##.adWithTab\r\n";
	line += "##.adWrap\r\n";
	line += "##.adWrapper\r\n";
	line += "##.ad_0\r\n";
	line += "##.ad_1\r\n";
	line += "##.ad_120x90\r\n";
	line += "##.ad_125\r\n";
	line += "##.ad_130x90\r\n";
	line += "##.ad_160\r\n";
	line += "##.ad_160x600\r\n";
	line += "##.ad_2\r\n";
	line += "##.ad_200\r\n";
	line += "##.ad_200x200\r\n";
	line += "##.ad_250x250\r\n";
	line += "##.ad_250x250_w\r\n";
	line += "##.ad_3\r\n";
	line += "##.ad_300\r\n";
	line += "##.ad_300_250\r\n";
	line += "##.ad_300x250\r\n";
	line += "##.ad_300x250_box_right\r\n";
	line += "##.ad_336\r\n";
	line += "##.ad_336x280\r\n";
	line += "##.ad_350x100\r\n";
	line += "##.ad_350x250\r\n";
	line += "##.ad_400x200\r\n";
	line += "##.ad_468\r\n";
	line += "##.ad_468x60\r\n";
	line += "##.ad_600\r\n";
	line += "##.ad_728\r\n";
	line += "##.ad_728x90\r\n";
	line += "##.ad_Left\r\n";
	line += "~nirmaltv.com##.ad_Right\r\n";
	line += "##.ad_amazon\r\n";
	line += "##.ad_banner\r\n";
	line += "##.ad_banner_border\r\n";
	line += "##.ad_biz\r\n";
	line += "##.ad_block_338\r\n";
	line += "##.ad_body\r\n";
	line += "##.ad_border\r\n";
	line += "##.ad_botbanner\r\n";
	line += "##.ad_bottom_leaderboard\r\n";
	line += "##.ad_box\r\n";
	line += "##.ad_box2\r\n";
	line += "##.ad_box_ad\r\n";
	line += "##.ad_box_div\r\n";
	line += "##.ad_callout\r\n";
	line += "##.ad_caption\r\n";
	line += "##.ad_contain\r\n";
	line += "##.ad_container\r\n";
	line += "~salon.com##.ad_content\r\n";
	line += "##.ad_content_wide\r\n";
	line += "##.ad_contents\r\n";
	line += "##.ad_descriptor\r\n";
	line += "##.ad_eyebrow\r\n";
	line += "##.ad_footer\r\n";
	line += "##.ad_framed\r\n";
	line += "##.ad_front_promo\r\n";
	line += "##.ad_head\r\n";
	line += "~news.yahoo.com,~speurders.nl##.ad_header\r\n";
	line += "##.ad_hpm\r\n";
	line += "##.ad_info_block\r\n";
	line += "##.ad_inline\r\n";
	line += "##.ad_island\r\n";
	line += "##.ad_label\r\n";
	line += "##.ad_launchpad\r\n";
	line += "##.ad_leader\r\n";
	line += "##.ad_leaderboard\r\n";
	line += "##.ad_left\r\n";
	line += "~leboncoin.fr##.ad_links\r\n";
	line += "##.ad_linkunit\r\n";
	line += "##.ad_loc\r\n";
	line += "##.ad_lrec\r\n";
	line += "##.ad_main\r\n";
	line += "##.ad_medrec\r\n";
	line += "##.ad_medrect\r\n";
	line += "##.ad_middle\r\n";
	line += "##.ad_mpu\r\n";
	line += "##.ad_mr\r\n";
	line += "##.ad_mrec\r\n";
	line += "##.ad_mrec_title_article\r\n";
	line += "##.ad_mrect\r\n";
	line += "##.ad_news\r\n";
	line += "##.ad_notice\r\n";
	line += "##.ad_one\r\n";
	line += "##.ad_p360\r\n";
	line += "##.ad_partner\r\n";
	line += "##.ad_partners\r\n";
	line += "##.ad_plus\r\n";
	line += "##.ad_post\r\n";
	line += "##.ad_power\r\n";
	line += "##.ad_rectangle\r\n";
	line += "~didaktik-der-mathematik.de##.ad_right\r\n";
	line += "##.ad_right_col\r\n";
	line += "##.ad_row\r\n";
	line += "##.ad_sidebar\r\n";
	line += "##.ad_skyscraper\r\n";
	line += "##.ad_slug\r\n";
	line += "##.ad_slug_table\r\n";
	line += "~chinapost.com.tw##.ad_space\r\n";
	line += "##.ad_space_300_250\r\n";
	line += "##.ad_sponsor\r\n";
	line += "##.ad_sponsoredsection\r\n";
	line += "##.ad_spot_b\r\n";
	line += "##.ad_spot_c\r\n";
	line += "##.ad_square_r\r\n";
	line += "##.ad_square_top\r\n";
	line += "~bbs.newhua.com,~leboncoin.fr##.ad_text\r\n";
	line += "##.ad_text_w\r\n";
	line += "##.ad_title\r\n";
	line += "##.ad_top\r\n";
	line += "##.ad_top_leaderboard\r\n";
	line += "##.ad_topright\r\n";
	line += "##.ad_tower\r\n";
	line += "##.ad_unit\r\n";
	line += "##.ad_unit_rail\r\n";
	line += "##.ad_url\r\n";
	line += "##.ad_warning\r\n";
	line += "##.ad_wid300\r\n";
	line += "##.ad_wide\r\n";
	line += "##.ad_wrap\r\n";
	line += "##.ad_wrapper\r\n";
	line += "##.ad_wrapper_fixed\r\n";
	line += "##.ad_wrapper_top\r\n";
	line += "##.ad_zone\r\n";
	line += "##.adarea\r\n";
	line += "##.adarea-long\r\n";
	line += "##.adbanner\r\n";
	line += "##.adbannerbox\r\n";
	line += "##.adbannerright\r\n";
	line += "##.adbar\r\n";
	line += "##.adbg\r\n";
	line += "##.adborder\r\n";
	line += "##.adbot\r\n";
	line += "##.adbottom\r\n";
	line += "##.adbottomright\r\n";
	line += "~bodybuilding.com,~gametop.com,~lidovky.cz,~nordea.fi##.adbox\r\n";
	line += "##.adbox-outer\r\n";
	line += "##.adbox_300x600\r\n";
	line += "##.adbox_366x280\r\n";
	line += "##.adbox_468X60\r\n";
	line += "##.adbox_bottom\r\n";
	line += "##.adboxclass\r\n";
	line += "##.adbuttons\r\n";
	line += "##.adcode\r\n";
	line += "##.adcol1\r\n";
	line += "##.adcol2\r\n";
	line += "##.adcolumn\r\n";
	line += "##.adcolumn_wrapper\r\n";
	line += "~subito.it##.adcont\r\n";
	line += "##.adcopy\r\n";
	line += "~superbikeplanet.com##.addiv\r\n";
	line += "##.adfoot\r\n";
	line += "##.adfootbox\r\n";
	line += "~linux.com##.adframe\r\n";
	line += "##.adhead\r\n";
	line += "##.adheader\r\n";
	line += "##.adheader100\r\n";
	line += "##.adhere\r\n";
	line += "##.adhered\r\n";
	line += "##.adhi\r\n";
	line += "##.adhint\r\n";
	line += "~northjersey.com##.adholder\r\n";
	line += "##.adhoriz\r\n";
	line += "##.adi\r\n";
	line += "##.adiframe\r\n";
	line += "~backpage.com##.adinfo\r\n";
	line += "##.adinside\r\n";
	line += "##.adintro\r\n";
	line += "##.adjlink\r\n";
	line += "##.adkit\r\n";
	line += "##.adkit-advert\r\n";
	line += "##.adkit-lb-footer\r\n";
	line += "##.adlabel-horz\r\n";
	line += "##.adlabel-vert\r\n";
	line += "~gmhightechperformance.com,~hotrod.com,~miloyski.com,~superchevy.com##.adleft\r\n";
	line += "##.adleft1\r\n";
	line += "##.adline\r\n";
	line += "~superbikeplanet.com##.adlink\r\n";
	line += "##.adlinks\r\n";
	line += "~bipbip.co.il##.adlist\r\n";
	line += "##.adlnklst\r\n";
	line += "##.admarker\r\n";
	line += "##.admedrec\r\n";
	line += "##.admessage\r\n";
	line += "##.admodule\r\n";
	line += "##.admpu\r\n";
	line += "##.adnation-banner\r\n";
	line += "##.adnotice\r\n";
	line += "##.adops\r\n";
	line += "##.adp-AdPrefix\r\n";
	line += "##.adpadding\r\n";
	line += "##.adpane\r\n";
	line += "~bipbip.co.il,~quoka.de##.adpic\r\n";
	line += "##.adprice\r\n";
	line += "~tomwans.com##.adright\r\n";
	line += "##.adroot\r\n";
	line += "##.adrotate_widget\r\n";
	line += "##.adrow\r\n";
	line += "##.adrow-post\r\n";
	line += "##.adrule\r\n";
	line += "##.ads-125\r\n";
	line += "##.ads-728x90-wrap\r\n";
	line += "##.ads-banner\r\n";
	line += "##.ads-below-content\r\n";
	line += "##.ads-categories-bsa\r\n";
	line += "##.ads-favicon\r\n";
	line += "##.ads-links-general\r\n";
	line += "##.ads-mpu\r\n";
	line += "##.ads-profile\r\n";
	line += "##.ads-right\r\n";
	line += "##.ads-section\r\n";
	line += "##.ads-sidebar\r\n";
	line += "##.ads-sky\r\n";
	line += "##.ads-stripe\r\n";
	line += "##.ads-text\r\n";
	line += "##.ads-widget-partner-gallery\r\n";
	line += "##.ads2\r\n";
	line += "##.ads3\r\n";
	line += "##.ads300\r\n";
	line += "##.ads468\r\n";
	line += "##.ads728\r\n";
	line += "~amusingplanet.com,~bakeca.it,~chw.net,~cub.com,~joinmyband.co.uk,~lets-sell.info,~najauto.pl,~repubblica.it,~tonprenom.com##.ads:not(body)\r\n";
	line += "##.adsArea\r\n";
	line += "##.adsBelowHeadingNormal\r\n";
	line += "##.adsBlock\r\n";
	line += "##.adsBox\r\n";
	line += "##.adsCont\r\n";
	line += "##.adsDiv\r\n";
	line += "##.adsFull\r\n";
	line += "##.adsImages\r\n";
	line += "##.adsMPU\r\n";
	line += "##.adsRight\r\n";
	line += "##.adsTextHouse\r\n";
	line += "##.adsTop\r\n";
	line += "##.adsTower2\r\n";
	line += "##.adsTowerWrap\r\n";
	line += "##.adsWithUs\r\n";
	line += "##.ads_125_square\r\n";
	line += "##.ads_180\r\n";
	line += "##.ads_300\r\n";
	line += "##.ads_300x250\r\n";
	line += "##.ads_337x280\r\n";
	line += "##.ads_728x90\r\n";
	line += "##.ads_big\r\n";
	line += "##.ads_big-half\r\n";
	line += "##.ads_brace\r\n";
	line += "##.ads_catDiv\r\n";
	line += "##.ads_container\r\n";
	line += "##.ads_disc_anchor\r\n";
	line += "##.ads_disc_leader\r\n";
	line += "##.ads_disc_lwr_square\r\n";
	line += "##.ads_disc_skyscraper\r\n";
	line += "##.ads_disc_square\r\n";
	line += "##.ads_div\r\n";
	line += "##.ads_header\r\n";
	line += "##.ads_leaderboard\r\n";
	line += "##.ads_mpu\r\n";
	line += "##.ads_outer\r\n";
	line += "##.ads_rectangle\r\n";
	line += "##.ads_right\r\n";
	line += "##.ads_sc_bl_i\r\n";
	line += "##.ads_sc_tl_i\r\n";
	line += "##.ads_show_if\r\n";
	line += "##.ads_side\r\n";
	line += "##.ads_sidebar\r\n";
	line += "##.ads_singlepost\r\n";
	line += "##.ads_spacer\r\n";
	line += "##.ads_takeover\r\n";
	line += "##.ads_title\r\n";
	line += "##.ads_tr\r\n";
	line += "##.ads_widesky\r\n";
	line += "##.ads_wrapperads_top\r\n";
	line += "##.adsblockvert\r\n";
	line += "##.adsborder\r\n";
	line += "##.adsbottom\r\n";
	line += "##.adsbyyahoo\r\n";
	line += "##.adsc\r\n";
	line += "##.adscaleAdvert\r\n";
	line += "##.adsclick\r\n";
	line += "##.adscontainer\r\n";
	line += "##.adscreen\r\n";
	line += "##.adsection_a2\r\n";
	line += "##.adsection_c2\r\n";
	line += "~lalsace.fr,~lepays.fr,~tonprenom.com##.adsense\r\n";
	line += "##.adsense-ad\r\n";
	line += "##.adsense-category\r\n";
	line += "##.adsense-category-bottom\r\n";
	line += "##.adsense-heading\r\n";
	line += "##.adsense-post\r\n";
	line += "##.adsense-right\r\n";
	line += "##.adsense-title\r\n";
	line += "##.adsense3\r\n";
	line += "##.adsenseAds\r\n";
	line += "##.adsenseBlock\r\n";
	line += "##.adsenseContainer\r\n";
	line += "##.adsenseGreenBox\r\n";
	line += "##.adsense_bdc_v2\r\n";
	line += "##.adsensebig\r\n";
	line += "##.adsenseblock\r\n";
	line += "##.adsenseblock_bottom\r\n";
	line += "##.adsenseblock_top\r\n";
	line += "##.adsenselr\r\n";
	line += "##.adsensem_widget\r\n";
	line += "##.adsensesq\r\n";
	line += "##.adsenvelope\r\n";
	line += "##.adset\r\n";
	line += "##.adsforums\r\n";
	line += "##.adsghori\r\n";
	line += "##.adsgvert\r\n";
	line += "##.adside\r\n";
	line += "##.adsidebox\r\n";
	line += "##.adsider\r\n";
	line += "##.adsingle\r\n";
	line += "##.adsleft\r\n";
	line += "##.adslogan\r\n";
	line += "##.adsmalltext\r\n";
	line += "##.adsmessage\r\n";
	line += "##.adspace\r\n";
	line += "##.adspace-MR\r\n";
	line += "##.adspace180\r\n";
	line += "##.adspace_bottom\r\n";
	line += "##.adspace_buysell\r\n";
	line += "##.adspace_rotate\r\n";
	line += "##.adspace_skyscraper\r\n";
	line += "##.adspacer\r\n";
	line += "##.adspot\r\n";
	line += "##.adspot728x90\r\n";
	line += "##.adstextpad\r\n";
	line += "##.adstop\r\n";
	line += "##.adstrip\r\n";
	line += "~rinkworks.com##.adtable\r\n";
	line += "##.adtag\r\n";
	line += "##.adtech\r\n";
	line += "~anzwers.com.au,~bipbip.co.il,~ksl.com,~quoka.de,~u-file.net##.adtext\r\n";
	line += "##.adtext_gray\r\n";
	line += "##.adtext_horizontal\r\n";
	line += "##.adtext_onwhite\r\n";
	line += "##.adtext_vertical\r\n";
	line += "##.adtile\r\n";
	line += "##.adtips\r\n";
	line += "##.adtips1\r\n";
	line += "##.adtop\r\n";
	line += "##.adtravel\r\n";
	line += "##.adtxt\r\n";
	line += "##.adv-mpu\r\n";
	line += "##.adver\r\n";
	line += "##.adverTag\r\n";
	line += "##.adver_cont_below\r\n";
	line += "~beginyouridea.com,~irr.ru,~jobs.wa.gov.au,~manxtelecom.com,~storegate.co.uk,~storegate.com,~storegate.se,~swanseacity.net,~toonzaki.com,~travelblog.dailymail.co.uk,~tu-chemnitz.de,~wrexhamafc.co.uk,~yourvids.nl##.advert\r\n";
	line += "##.advert-article-bottom\r\n";
	line += "##.advert-bannerad\r\n";
	line += "##.advert-box\r\n";
	line += "##.advert-head\r\n";
	line += "~mobifrance.com##.advert-horizontal\r\n";
	line += "##.advert-iab-300-250\r\n";
	line += "##.advert-iab-468-60\r\n";
	line += "##.advert-mpu\r\n";
	line += "##.advert-skyscraper\r\n";
	line += "##.advert-text\r\n";
	line += "##.advert300\r\n";
	line += "##.advert4\r\n";
	line += "##.advert5\r\n";
	line += "##.advert8\r\n";
	line += "##.advertColumn\r\n";
	line += "##.advertCont\r\n";
	line += "##.advertContainer\r\n";
	line += "##.advertHeadline\r\n";
	line += "##.advertRight\r\n";
	line += "##.advertText\r\n";
	line += "##.advertTitleSky\r\n";
	line += "##.advert_468x60\r\n";
	line += "##.advert_box\r\n";
	line += "##.advert_cont\r\n";
	line += "##.advert_label\r\n";
	line += "##.advert_leaderboard\r\n";
	line += "~browsershots.org##.advert_list\r\n";
	line += "##.advert_note\r\n";
	line += "##.advert_top\r\n";
	line += "##.advertheader-red\r\n";
	line += "~tonprenom.com##.advertise\r\n";
	line += "##.advertise-here\r\n";
	line += "##.advertise-homestrip\r\n";
	line += "##.advertise-horz\r\n";
	line += "##.advertise-leaderboard\r\n";
	line += "##.advertise-vert\r\n";
	line += "##.advertiseContainer\r\n";
	line += "##.advertiseText\r\n";
	line += "##.advertise_ads\r\n";
	line += "##.advertise_here\r\n";
	line += "##.advertise_link\r\n";
	line += "##.advertise_link_sidebar\r\n";
	line += "~andkon.com,~wired.com##.advertisement\r\n";
	line += "##.advertisement-728x90\r\n";
	line += "##.advertisement-block\r\n";
	line += "##.advertisement-text\r\n";
	line += "##.advertisement-top\r\n";
	line += "##.advertisement468\r\n";
	line += "##.advertisementBox\r\n";
	line += "##.advertisementColumnGroup\r\n";
	line += "##.advertisementContainer\r\n";
	line += "##.advertisementHeader\r\n";
	line += "##.advertisementLabel\r\n";
	line += "##.advertisementPanel\r\n";
	line += "##.advertisement_btm\r\n";
	line += "##.advertisement_caption\r\n";
	line += "##.advertisement_g\r\n";
	line += "##.advertisement_header\r\n";
	line += "##.advertisement_horizontal\r\n";
	line += "##.advertisement_top\r\n";
	line += "~zlinked.com##.advertiser\r\n";
	line += "##.advertiser-links\r\n";
	line += "##.advertisespace_div\r\n";
	line += "~tonprenom.com,~trove.nla.gov.au##.advertising\r\n";
	line += "##.advertising-banner\r\n";
	line += "##.advertising-header\r\n";
	line += "##.advertising-local-links\r\n";
	line += "##.advertising2\r\n";
	line += "##.advertisingTable\r\n";
	line += "##.advertising_block\r\n";
	line += "##.advertising_images\r\n";
	line += "~macwelt.de##.advertisment\r\n";
	line += "##.advertisment_two\r\n";
	line += "##.advertize\r\n";
	line += "##.advertorial\r\n";
	line += "##.advertorial-2\r\n";
	line += "##.advertorial-promo-box\r\n";
	line += "##.adverts\r\n";
	line += "##.advt\r\n";
	line += "##.advt-banner-3\r\n";
	line += "##.advt-block\r\n";
	line += "##.advt300\r\n";
	line += "##.advt720\r\n";
	line += "##.adwordListings\r\n";
	line += "##.adwordsHeader\r\n";
	line += "##.adwrap\r\n";
	line += "~calgaryherald.com,~montrealgazette.com,~vancouversun.com,~windsorstar.com##.adwrapper\r\n";
	line += "##.adwrapper-lrec\r\n";
	line += "##.adwrapper948\r\n";
	line += "~virginmobile.fr##.affiliate\r\n";
	line += "##.affiliate-link\r\n";
	line += "##.affiliate-sidebar\r\n";
	line += "##.affiliateAdvertText\r\n";
	line += "##.affinityAdHeader\r\n";
	line += "##.after_ad\r\n";
	line += "##.agi-adsaleslinks\r\n";
	line += "##.alb-content-ad\r\n";
	line += "##.alt_ad\r\n";
	line += "##.anchorAd\r\n";
	line += "##.another_text_ad\r\n";
	line += "##.answer_ad_content\r\n";
	line += "##.aolSponsoredLinks\r\n";
	line += "##.aopsadvert\r\n";
	line += "##.apiAdMarkerAbove\r\n";
	line += "##.apiAds\r\n";
	line += "##.archive-ads\r\n";
	line += "##.art_ads\r\n";
	line += "##.article-ads\r\n";
	line += "##.articleAd\r\n";
	line += "##.articleAds\r\n";
	line += "##.articleAdsL\r\n";
	line += "##.articleEmbeddedAdBox\r\n";
	line += "##.article_ad\r\n";
	line += "##.article_adbox\r\n";
	line += "##.article_mpu_box\r\n";
	line += "##.articleads\r\n";
	line += "##.aseadn\r\n";
	line += "##.aux-ad-widget-2\r\n";
	line += "##.b-astro-sponsored-links_horizontal\r\n";
	line += "##.b-astro-sponsored-links_vertical\r\n";
	line += "##.banner-ad\r\n";
	line += "##.banner-ads\r\n";
	line += "##.banner-adverts\r\n";
	line += "##.banner300x100\r\n";
	line += "##.banner300x250\r\n";
	line += "##.banner468\r\n";
	line += "##.bannerAd\r\n";
	line += "##.bannerAdWrapper300x250\r\n";
	line += "##.bannerAdWrapper730x86\r\n";
	line += "##.bannerRightAd\r\n";
	line += "##.banner_300x250\r\n";
	line += "##.banner_728x90\r\n";
	line += "##.banner_ad\r\n";
	line += "##.banner_ad_footer\r\n";
	line += "##.banner_ad_leaderboard\r\n";
	line += "##.bannerad\r\n";
	line += "##.barkerAd\r\n";
	line += "##.base-ad-mpu\r\n";
	line += "##.base_ad\r\n";
	line += "##.bgnavad\r\n";
	line += "##.big-ads\r\n";
	line += "##.bigAd\r\n";
	line += "##.big_ad\r\n";
	line += "##.big_ads\r\n";
	line += "##.bigad\r\n";
	line += "##.bigad2\r\n";
	line += "##.bigbox_ad\r\n";
	line += "##.bigboxad\r\n";
	line += "##.billboard_ad\r\n";
	line += "##.blk_advert\r\n";
	line += "##.block-ad\r\n";
	line += "##.block-ad300\r\n";
	line += "##.block-admanager\r\n";
	line += "##.block-ads-bottom\r\n";
	line += "##.block-ads-top\r\n";
	line += "##.block-adsense\r\n";
	line += "##.block-openadstream\r\n";
	line += "##.block-openx\r\n";
	line += "##.block-thirdage-ads\r\n";
	line += "~kin0.org##.block_ad\r\n";
	line += "##.block_ad_sb_text\r\n";
	line += "##.block_ad_sponsored_links\r\n";
	line += "##.block_ad_sponsored_links-wrapper\r\n";
	line += "##.blocked-ads\r\n";
	line += "##.blog-ad-leader-inner\r\n";
	line += "##.blog-ads-container\r\n";
	line += "##.blogAd\r\n";
	line += "##.blogAdvertisement\r\n";
	line += "##.blogBigAd\r\n";
	line += "##.blog_ad\r\n";
	line += "##.blogads\r\n";
	line += "##.blox3featuredAd\r\n";
	line += "##.body_ad\r\n";
	line += "##.body_sponsoredresults_bottom\r\n";
	line += "##.body_sponsoredresults_middle\r\n";
	line += "##.body_sponsoredresults_top\r\n";
	line += "##.bookseller-header-advt\r\n";
	line += "##.bottomAd\r\n";
	line += "##.bottomAds\r\n";
	line += "##.bottom_ad\r\n";
	line += "~ixbtlabs.com##.bottom_ad_block\r\n";
	line += "##.bottom_sponsor\r\n";
	line += "##.bottomad\r\n";
	line += "##.bottomadvert\r\n";
	line += "##.bottomrightrailAd\r\n";
	line += "##.bottomvidad\r\n";
	line += "##.box-ad\r\n";
	line += "##.box-ads\r\n";
	line += "##.box-adsense\r\n";
	line += "##.boxAd\r\n";
	line += "##.box_ad\r\n";
	line += "##.box_ads\r\n";
	line += "##.box_advertising\r\n";
	line += "##.box_advertisment_62_border\r\n";
	line += "##.box_content_ad\r\n";
	line += "##.box_content_ads\r\n";
	line += "##.boxad\r\n";
	line += "##.boxyads\r\n";
	line += "##.bps-ad-wrapper\r\n";
	line += "##.bps-advertisement\r\n";
	line += "##.bps-advertisement-inline-ads\r\n";
	line += "##.br-ad\r\n";
	line += "##.bsa_ads\r\n";
	line += "##.btm_ad\r\n";
	line += "##.bullet-sponsored-links\r\n";
	line += "##.bullet-sponsored-links-gray\r\n";
	line += "##.burstContentAdIndex\r\n";
	line += "##.buttonAd\r\n";
	line += "##.buttonAds\r\n";
	line += "##.buttonadbox\r\n";
	line += "##.bx_ad\r\n";
	line += "##.bx_ad_right\r\n";
	line += "##.cA-adStrap\r\n";
	line += "##.cColumn-TextAdsBox\r\n";
	line += "##.care2_adspace\r\n";
	line += "##.catalog_ads\r\n";
	line += "##.cb-ad-container\r\n";
	line += "##.cb_footer_sponsor\r\n";
	line += "##.cb_navigation_ad\r\n";
	line += "##.cbstv_ad_label\r\n";
	line += "##.cbzadvert\r\n";
	line += "##.cbzadvert_block\r\n";
	line += "##.cdAdTitle\r\n";
	line += "##.cdmainlineSearchAdParent\r\n";
	line += "##.cdsidebarSearchAdParent\r\n";
	line += "##.centerAd\r\n";
	line += "##.center_ad\r\n";
	line += "##.centerad\r\n";
	line += "##.centered-ad\r\n";
	line += "##.cinemabotad\r\n";
	line += "##.clearerad\r\n";
	line += "##.cm_ads\r\n";
	line += "##.cms-Advert\r\n";
	line += "##.cnbc_badge_banner_ad_area\r\n";
	line += "##.cnn160AdFooter\r\n";
	line += "##.cnnAd\r\n";
	line += "##.cnnMosaic160Container\r\n";
	line += "##.cnnSearchSponsorBox\r\n";
	line += "##.cnnStoreAd\r\n";
	line += "##.cnnStoryElementBoxAd\r\n";
	line += "##.cnnWCAdBox\r\n";
	line += "##.cnnWireAdLtgBox\r\n";
	line += "##.cnn_728adbin\r\n";
	line += "##.cnn_adcntr300x100\r\n";
	line += "##.cnn_adcntr728x90\r\n";
	line += "##.cnn_adspc336cntr\r\n";
	line += "##.cnn_adtitle\r\n";
	line += "##.column2-ad\r\n";
	line += "##.com-ad-server\r\n";
	line += "##.comment-advertisement\r\n";
	line += "##.common_advertisement_title\r\n";
	line += "##.communityAd\r\n";
	line += "##.conTSponsored\r\n";
	line += "##.conductor_ad\r\n";
	line += "##.confirm_ad_left\r\n";
	line += "##.confirm_ad_right\r\n";
	line += "##.confirm_leader_ad\r\n";
	line += "##.consoleAd\r\n";
	line += "##.container-adwords\r\n";
	line += "##.containerSqAd\r\n";
	line += "##.container_serendipity_plugin_google_adsense\r\n";
	line += "##.content-ad\r\n";
	line += "~theology.edu##.contentAd\r\n";
	line += "##.contentAdFoot\r\n";
	line += "##.contentAdsWrapper\r\n";
	line += "##.content_ad\r\n";
	line += "##.content_ad_728\r\n";
	line += "##.content_adsq\r\n";
	line += "##.contentad\r\n";
	line += "##.contentad300x250\r\n";
	line += "##.contentad_right_col\r\n";
	line += "##.contentadcontainer\r\n";
	line += "##.contentadleft\r\n";
	line += "##.contenttextad\r\n";
	line += "##.contest_ad\r\n";
	line += "##.cp_ad\r\n";
	line += "##.cpmstarHeadline\r\n";
	line += "##.cpmstarText\r\n";
	line += "##.create_ad\r\n";
	line += "##.cs-mpu\r\n";
	line += "##.cscTextAd\r\n";
	line += "##.cse_ads\r\n";
	line += "##.cspAd\r\n";
	line += "##.ct_ad\r\n";
	line += "##.cube-ad\r\n";
	line += "##.cubeAd\r\n";
	line += "##.cube_ads\r\n";
	line += "##.currency_ad\r\n";
	line += "##.custom_ads\r\n";
	line += "##.darla_ad\r\n";
	line += "##.dartAdImage\r\n";
	line += "##.dart_ad\r\n";
	line += "##.dart_tag\r\n";
	line += "##.dartadvert\r\n";
	line += "##.dartiframe\r\n";
	line += "##.dc-ad\r\n";
	line += "##.dcAdvertHeader\r\n";
	line += "##.deckAd\r\n";
	line += "##.deckads\r\n";
	line += "##.detail-ads\r\n";
	line += "##.detailMpu\r\n";
	line += "##.detail_top_advert\r\n";
	line += "##.divAd\r\n";
	line += "##.divad1\r\n";
	line += "##.divad2\r\n";
	line += "##.divad3\r\n";
	line += "##.divads\r\n";
	line += "##.divider_ad\r\n";
	line += "##.dmco_advert_iabrighttitle\r\n";
	line += "##.download_ad\r\n";
	line += "##.downloadad\r\n";
	line += "##.dynamic-ads\r\n";
	line += "##.dynamic_ad\r\n";
	line += "##.e-ad\r\n";
	line += "##.ec-ads\r\n";
	line += "##.em-ad\r\n";
	line += "##.embed-ad\r\n";
	line += "##.entry_sidebar_ads\r\n";
	line += "##.entryad\r\n";
	line += "##.ez-clientAd\r\n";
	line += "##.f_Ads\r\n";
	line += "##.featuredAds\r\n";
	line += "##.featuredadvertising\r\n";
	line += "##.firstpost_advert_container\r\n";
	line += "##.flagads\r\n";
	line += "##.flash-advertisement\r\n";
	line += "##.flash_ad\r\n";
	line += "##.flash_advert\r\n";
	line += "##.flashad\r\n";
	line += "##.flexiad\r\n";
	line += "##.flipbook_v2_sponsor_ad\r\n";
	line += "##.floatad\r\n";
	line += "##.floated_right_ad\r\n";
	line += "##.footad\r\n";
	line += "##.footer-ad\r\n";
	line += "##.footerAd\r\n";
	line += "##.footerAdModule\r\n";
	line += "##.footerAdslot\r\n";
	line += "##.footerTextAd\r\n";
	line += "##.footer_ad\r\n";
	line += "##.footer_ads\r\n";
	line += "##.footer_block_ad\r\n";
	line += "##.footer_bottomad\r\n";
	line += "##.footer_line_ad\r\n";
	line += "##.footer_text_ad\r\n";
	line += "##.footerad\r\n";
	line += "##.forumtopad\r\n";
	line += "##.frn_adbox\r\n";
	line += "##.frn_cont_adbox\r\n";
	line += "##.ft-ad\r\n";
	line += "##.ftdAdBar\r\n";
	line += "##.ftdContentAd\r\n";
	line += "##.full_ad_box\r\n";
	line += "##.fullbannerad\r\n";
	line += "##.g3rtn-ad-site\r\n";
	line += "##.gAdvertising\r\n";
	line += "##.g_ggl_ad\r\n";
	line += "##.ga-textads-bottom\r\n";
	line += "##.ga-textads-top\r\n";
	line += "##.gaTeaserAdsBox\r\n";
	line += "##.gads\r\n";
	line += "##.gads_cb\r\n";
	line += "##.gads_container\r\n";
	line += "##.gamesPage_ad_content\r\n";
	line += "##.gglAds\r\n";
	line += "##.global_banner_ad\r\n";
	line += "##.googad\r\n";
	line += "##.googads\r\n";
	line += "##.google-ad\r\n";
	line += "##.google-ad-container\r\n";
	line += "##.google-ads\r\n";
	line += "##.google-ads-boxout\r\n";
	line += "##.google-ads-slim\r\n";
	line += "##.google-right-ad\r\n";
	line += "##.google-sponsored-ads\r\n";
	line += "##.google-sponsored-link\r\n";
	line += "##.google468_60\r\n";
	line += "##.googleAd\r\n";
	line += "##.googleAd-content\r\n";
	line += "##.googleAd-list\r\n";
	line += "##.googleAdBox\r\n";
	line += "##.googleAdSense\r\n";
	line += "##.googleAdSenseModule\r\n";
	line += "##.googleAd_body\r\n";
	line += "##.googleAds\r\n";
	line += "##.googleAds_article_page_above_comments\r\n";
	line += "##.googleAdsense\r\n";
	line += "##.googleContentAds\r\n";
	line += "##.googleProfileAd\r\n";
	line += "##.googleSearchAd_content\r\n";
	line += "##.googleSearchAd_sidebar\r\n";
	line += "##.google_ad\r\n";
	line += "##.google_add_container\r\n";
	line += "##.google_ads\r\n";
	line += "##.google_ads_bom_title\r\n";
	line += "##.google_ads_content\r\n";
	line += "##.googlead\r\n";
	line += "##.googleaddiv\r\n";
	line += "##.googleaddiv2\r\n";
	line += "##.googleads\r\n";
	line += "##.googleads_300x250\r\n";
	line += "##.googleads_title\r\n";
	line += "##.googley_ads\r\n";
	line += "##.gpAdBox\r\n";
	line += "##.gpAds\r\n";
	line += "##.gradientAd\r\n";
	line += "##.grey-ad-line\r\n";
	line += "##.group_ad\r\n";
	line += "##.gsfAd\r\n";
	line += "##.gt_ad\r\n";
	line += "##.gt_ad_300x250\r\n";
	line += "##.gt_ad_728x90\r\n";
	line += "##.gt_adlabel\r\n";
	line += "##.gutter-ad-left\r\n";
	line += "##.gutter-ad-right\r\n";
	line += "##.h-ad-728x90-bottom\r\n";
	line += "##.h_Ads\r\n";
	line += "##.h_ad\r\n";
	line += "##.half-ad\r\n";
	line += "##.half_ad_box\r\n";
	line += "##.hd_advert\r\n";
	line += "##.hdr-ads\r\n";
	line += "~assetbar.com,~burningangel.com##.header-ad\r\n";
	line += "##.header-advert\r\n";
	line += "~photobucket.com##.headerAd\r\n";
	line += "##.headerAds\r\n";
	line += "##.headerAdvert\r\n";
	line += "##.header_ad\r\n";
	line += "~associatedcontent.com##.header_ad_center\r\n";
	line += "##.header_ad_div\r\n";
	line += "##.header_advertisment\r\n";
	line += "##.headerad\r\n";
	line += "##.hi5-ad\r\n";
	line += "##.highlightsAd\r\n";
	line += "##.hm_advertisment\r\n";
	line += "##.home-ad-links\r\n";
	line += "##.homeAd\r\n";
	line += "##.homeAdBoxA\r\n";
	line += "##.homeAdBoxBetweenBlocks\r\n";
	line += "##.homeAdBoxInBignews\r\n";
	line += "##.homeAdSection\r\n";
	line += "##.homeMediumAdGroup\r\n";
	line += "##.home_ad_bottom\r\n";
	line += "##.home_advertisement\r\n";
	line += "##.home_mrec_ad\r\n";
	line += "##.homead\r\n";
	line += "##.homepage-ad\r\n";
	line += "##.homepage300ad\r\n";
	line += "##.homepageFlexAdOuter\r\n";
	line += "##.homepageMPU\r\n";
	line += "##.homepage_middle_right_ad\r\n";
	line += "##.hor_ad\r\n";
	line += "##.horiz_adspace\r\n";
	line += "##.horizontalAd\r\n";
	line += "~radaronline.com##.horizontal_ad\r\n";
	line += "##.horizontal_ads\r\n";
	line += "##.horizontaltextadbox\r\n";
	line += "##.horizsponsoredlinks\r\n";
	line += "##.hortad\r\n";
	line += "##.houseAdsStyle\r\n";
	line += "##.housead\r\n";
	line += "##.hp2-adtag\r\n";
	line += "##.hp_ad_cont\r\n";
	line += "##.hp_ad_text\r\n";
	line += "##.hp_t_ad\r\n";
	line += "##.hp_w_ad\r\n";
	line += "##.ic-ads\r\n";
	line += "##.ico-adv\r\n";
	line += "##.idMultiAd\r\n";
	line += "##.image-advertisement\r\n";
	line += "##.imageads\r\n";
	line += "##.imgad\r\n";
	line += "##.in-page-ad\r\n";
	line += "##.in-story-text-ad\r\n";
	line += "##.indie-sidead\r\n";
	line += "##.indy_googleads\r\n";
	line += "##.inline-ad\r\n";
	line += "##.inline-mpu-left\r\n";
	line += "##.inlineSideAd\r\n";
	line += "##.inline_ad\r\n";
	line += "##.inline_ad_title\r\n";
	line += "##.inlinead\r\n";
	line += "##.inlineadsense\r\n";
	line += "##.inlineadtitle\r\n";
	line += "##.inlist-ad\r\n";
	line += "##.inlistAd\r\n";
	line += "##.inner-advt-banner-3\r\n";
	line += "##.innerAds\r\n";
	line += "##.innerad\r\n";
	line += "##.inpostad\r\n";
	line += "##.insert_advertisement\r\n";
	line += "##.insertad\r\n";
	line += "##.insideStoryAd\r\n";
	line += "##.is24-adplace\r\n";
	line += "##.islandAd\r\n";
	line += "##.islandAdvert\r\n";
	line += "##.islandad\r\n";
	line += "##.jimdoAdDisclaimer\r\n";
	line += "##.jp-advertisment-promotional\r\n";
	line += "##.js-advert\r\n";
	line += "##.kw_advert\r\n";
	line += "##.kw_advert_pair\r\n";
	line += "##.l_ad_sub\r\n";
	line += "##.l_banner.ads_show_if\r\n";
	line += "##.labelads\r\n";
	line += "##.largeRectangleAd\r\n";
	line += "##.lastRowAd\r\n";
	line += "##.lcontentbox_ad\r\n";
	line += "##.leaderAdTop\r\n";
	line += "##.leaderAdvert\r\n";
	line += "##.leader_ad\r\n";
	line += "##.leaderboardAd\r\n";
	line += "##.leaderboardad\r\n";
	line += "##.leaderboardadtop\r\n";
	line += "##.left-ad\r\n";
	line += "##.leftAd\r\n";
	line += "##.leftAdColumn\r\n";
	line += "##.leftAds\r\n";
	line += "##.left_adlink\r\n";
	line += "##.left_ads\r\n";
	line += "##.leftad\r\n";
	line += "##.leftadtag\r\n";
	line += "##.leftbar_ad_160_600\r\n";
	line += "##.leftbarads\r\n";
	line += "##.leftnavad\r\n";
	line += "##.lgRecAd\r\n";
	line += "##.lg_ad\r\n";
	line += "##.ligatus\r\n";
	line += "##.linead\r\n";
	line += "##.link_adslider\r\n";
	line += "##.link_advertise\r\n";
	line += "##.live-search-list-ad-container\r\n";
	line += "##.ljad\r\n";
	line += "##.log_ads\r\n";
	line += "##.logoAds\r\n";
	line += "##.longAd\r\n";
	line += "##.lowerAds\r\n";
	line += "##.m-ad-tvguide-box\r\n";
	line += "##.m4-adsbygoogle\r\n";
	line += "##.m_banner_ads\r\n";
	line += "##.macAd\r\n";
	line += "##.macad\r\n";
	line += "##.main-ad\r\n";
	line += "##.main-tabs-ad-block\r\n";
	line += "##.main_ad\r\n";
	line += "##.main_adbox\r\n";
	line += "##.main_intro_ad\r\n";
	line += "##.map_media_banner_ad\r\n";
	line += "##.marginadsthin\r\n";
	line += "##.marketing-ad\r\n";
	line += "##.masthead_topad\r\n";
	line += "##.mdl-ad\r\n";
	line += "##.media-advert\r\n";
	line += "##.mediaAd\r\n";
	line += "##.mediaAdContainer\r\n";
	line += "##.medium-rectangle-ad\r\n";
	line += "##.mediumRectangleAdvert\r\n";
	line += "##.menuItemBannerAd\r\n";
	line += "##.messageBoardAd\r\n";
	line += "##.micro_ad\r\n";
	line += "##.mid_ad\r\n";
	line += "##.midad\r\n";
	line += "##.middleAds\r\n";
	line += "##.middleads\r\n";
	line += "##.min_navi_ad\r\n";
	line += "##.miniad\r\n";
	line += "##.mobile-sponsoring\r\n";
	line += "##.mod-ad-lrec\r\n";
	line += "##.mod-ad-n\r\n";
	line += "##.mod-adopenx\r\n";
	line += "##.mod_admodule\r\n";
	line += "~corrieredicomo.it##.module-ad\r\n";
	line += "##.module-ad-small\r\n";
	line += "##.module-ads\r\n";
	line += "##.moduleAdvertContent\r\n";
	line += "##.module_ad\r\n";
	line += "##.module_box_ad\r\n";
	line += "##.modulegad\r\n";
	line += "##.moduletable-advert\r\n";
	line += "##.moduletable-googleads\r\n";
	line += "##.moduletablesquaread\r\n";
	line += "~gamespot.com##.mpu\r\n";
	line += "##.mpu-ad\r\n";
	line += "##.mpu-advert\r\n";
	line += "##.mpu-footer\r\n";
	line += "##.mpu-fp\r\n";
	line += "##.mpu-title\r\n";
	line += "##.mpu-top-left\r\n";
	line += "##.mpu-top-left-banner\r\n";
	line += "##.mpu-top-right\r\n";
	line += "##.mpuAd\r\n";
	line += "##.mpuAdSlot\r\n";
	line += "##.mpuAdvert\r\n";
	line += "##.mpuArea\r\n";
	line += "##.mpuBox\r\n";
	line += "##.mpuContainer\r\n";
	line += "##.mpuHolder\r\n";
	line += "##.mpuTextAd\r\n";
	line += "##.mpu_ad\r\n";
	line += "##.mpu_advert\r\n";
	line += "##.mpu_gold\r\n";
	line += "##.mpu_holder\r\n";
	line += "##.mpu_platinum\r\n";
	line += "##.mpu_text_ad\r\n";
	line += "##.mpuad\r\n";
	line += "##.mpuholderportalpage\r\n";
	line += "##.mrec_advert\r\n";
	line += "##.ms-ads-link\r\n";
	line += "##.msfg-shopping-mpu\r\n";
	line += "##.mwaads\r\n";
	line += "##.nSponsoredLcContent\r\n";
	line += "##.nSponsoredLcTopic\r\n";
	line += "##.nadvt300\r\n";
	line += "##.narrow_ad_unit\r\n";
	line += "##.narrow_ads\r\n";
	line += "##.navAdsBanner\r\n";
	line += "##.navi_ad300\r\n";
	line += "##.naviad\r\n";
	line += "##.nba300Ad\r\n";
	line += "##.nbaT3Ad160\r\n";
	line += "##.nbaTVPodAd\r\n";
	line += "##.nbaTwo130Ads\r\n";
	line += "##.nbc_ad_carousel_wrp\r\n";
	line += "##.newTopAdContainer\r\n";
	line += "##.newad\r\n";
	line += "##.newsviewAdBoxInNews\r\n";
	line += "##.nf-adbox\r\n";
	line += "##.nn-mpu\r\n";
	line += "##.noAdForLead\r\n";
	line += "##.normalAds\r\n";
	line += "##.nrAds\r\n";
	line += "##.nsAdRow\r\n";
	line += "##.oas-ad\r\n";
	line += "##.oas-bottom-ads\r\n";
	line += "##.offer_sponsoredlinks\r\n";
	line += "##.oio-banner-zone\r\n";
	line += "##.oio-link-sidebar\r\n";
	line += "##.oio-zone-position\r\n";
	line += "##.on_single_ad_box\r\n";
	line += "##.onethirdadholder\r\n";
	line += "##.openads\r\n";
	line += "##.openadstext_after\r\n";
	line += "##.openx\r\n";
	line += "##.openx-ad\r\n";
	line += "##.osan-ads\r\n";
	line += "##.other_adv2\r\n";
	line += "##.ovAdPromo\r\n";
	line += "##.ovAdSky\r\n";
	line += "##.ovAdartikel\r\n";
	line += "##.ov_spns\r\n";
	line += "##.pageGoogleAd\r\n";
	line += "##.pageGoogleAdFlat\r\n";
	line += "##.pageLeaderAd\r\n";
	line += "##.page_content_right_ad\r\n";
	line += "##.pagead\r\n";
	line += "##.pagenavindexcontentad\r\n";
	line += "##.partnersTextLinks\r\n";
	line += "##.pencil_ad\r\n";
	line += "##.player_ad_box\r\n";
	line += "##.player_page_ad_box\r\n";
	line += "##.plista_inimg_box\r\n";
	line += "##.pnp_ad\r\n";
	line += "##.pod-ad-300\r\n";
	line += "##.podRelatedAdLinksWidget\r\n";
	line += "##.podSponsoredLink\r\n";
	line += "##.portalCenterContentAdBottom\r\n";
	line += "##.portalCenterContentAdMiddle\r\n";
	line += "##.portalCenterContentAdTop\r\n";
	line += "##.portalcontentad\r\n";
	line += "##.post-ad\r\n";
	line += "##.post_ad\r\n";
	line += "##.post_ads\r\n";
	line += "##.post_sponsor_unit\r\n";
	line += "##.postbit_adbit_register\r\n";
	line += "##.postbit_adcode\r\n";
	line += "##.postgroup-ads\r\n";
	line += "##.postgroup-ads-middle\r\n";
	line += "##.prebodyads\r\n";
	line += "##.premium_ad_container\r\n";
	line += "##.promoAd\r\n";
	line += "##.promoAds\r\n";
	line += "##.promo_ad\r\n";
	line += "##.publication-ad\r\n";
	line += "##.publicidad\r\n";
	line += "##.puff-advertorials\r\n";
	line += "##.qa_ad_left\r\n";
	line += "##.qm-ad-content\r\n";
	line += "##.qm-ad-content-news\r\n";
	line += "##.quigo-ad\r\n";
	line += "##.qzvAdDiv\r\n";
	line += "##.r_ad_box\r\n";
	line += "##.r_ads\r\n";
	line += "##.rad_container\r\n";
	line += "##.rect_ad_module\r\n";
	line += "##.rectad\r\n";
	line += "##.rectangleAd\r\n";
	line += "##.rectanglead\r\n";
	line += "##.redads_cont\r\n";
	line += "##.regular_728_ad\r\n";
	line += "##.regularad\r\n";
	line += "##.relatedAds\r\n";
	line += "##.related_post_google_ad\r\n";
	line += "##.remads\r\n";
	line += "##.resourceImagetAd\r\n";
	line += "##.result_ad\r\n";
	line += "##.results_sponsor\r\n";
	line += "##.results_sponsor_right\r\n";
	line += "##.reviewMidAdvertAlign\r\n";
	line += "##.rght300x250\r\n";
	line += "##.rhads\r\n";
	line += "##.rhs-ad\r\n";
	line += "##.rhs-ads-panel\r\n";
	line += "##.right-ad\r\n";
	line += "##.right-ad-holder\r\n";
	line += "##.right-ad2\r\n";
	line += "##.right-ads\r\n";
	line += "##.right-ads2\r\n";
	line += "##.rightAd\r\n";
	line += "##.rightColAd\r\n";
	line += "##.rightRailAd\r\n";
	line += "##.right_ad\r\n";
	line += "##.right_ad_text\r\n";
	line += "##.right_ad_top\r\n";
	line += "##.right_ads\r\n";
	line += "~dailymotion.com,~dailymotion.virgilio.it##.right_ads_column\r\n";
	line += "##.right_col_ad\r\n";
	line += "##.right_hand_advert_column\r\n";
	line += "##.rightad\r\n";
	line += "##.rightad_1\r\n";
	line += "##.rightad_2\r\n";
	line += "##.rightadbox1\r\n";
	line += "##.rightads\r\n";
	line += "##.rightadunit\r\n";
	line += "##.rightcol_boxad\r\n";
	line += "##.rightcoladvert\r\n";
	line += "##.rightcoltowerad\r\n";
	line += "##.rnav_ad\r\n";
	line += "##.rngtAd\r\n";
	line += "##.roundingrayboxads\r\n";
	line += "##.rt_ad1_300x90\r\n";
	line += "##.rt_ad_300x250\r\n";
	line += "##.rt_ad_call\r\n";
	line += "##.savvyad_unit\r\n";
	line += "##.sb-ad-sq-bg\r\n";
	line += "##.sbAd\r\n";
	line += "##.sbAdUnitContainer\r\n";
	line += "##.sb_adsN\r\n";
	line += "##.sb_adsNv2\r\n";
	line += "##.sb_adsW\r\n";
	line += "##.sb_adsWv2\r\n";
	line += "##.scanAd\r\n";
	line += "##.scc_advert\r\n";
	line += "##.sci-ad-main\r\n";
	line += "##.sci-ad-sub\r\n";
	line += "##.search-ad\r\n";
	line += "##.search-results-ad\r\n";
	line += "##.search-sponsor\r\n";
	line += "##.search-sponsored\r\n";
	line += "##.searchAd\r\n";
	line += "##.searchSponsoredResultsBox\r\n";
	line += "##.searchSponsoredResultsList\r\n";
	line += "##.search_column_results_sponsored\r\n";
	line += "##.search_results_sponsored_top\r\n";
	line += "##.section-ad2\r\n";
	line += "##.section-sponsor\r\n";
	line += "##.section_mpu_wrapper\r\n";
	line += "##.section_mpu_wrapper_wrapper\r\n";
	line += "##.selfServeAds\r\n";
	line += "##.serp_sponsored\r\n";
	line += "##.servsponserLinks\r\n";
	line += "##.shoppingGoogleAdSense\r\n";
	line += "##.sidbaread\r\n";
	line += "##.side-ad\r\n";
	line += "##.side-ads\r\n";
	line += "##.sideAd\r\n";
	line += "##.sideBoxAd\r\n";
	line += "##.side_ad\r\n";
	line += "##.side_ad2\r\n";
	line += "##.side_ad_1\r\n";
	line += "##.side_ad_2\r\n";
	line += "##.side_ad_3\r\n";
	line += "##.sidead\r\n";
	line += "##.sideads\r\n";
	line += "##.sideadsbox\r\n";
	line += "##.sideadvert\r\n";
	line += "##.sidebar-ad\r\n";
	line += "##.sidebar-ads\r\n";
	line += "##.sidebar-text-ad\r\n";
	line += "##.sidebarAd\r\n";
	line += "##.sidebarAdUnit\r\n";
	line += "##.sidebarAdvert\r\n";
	line += "##.sidebar_ad\r\n";
	line += "##.sidebar_ad_300_250\r\n";
	line += "##.sidebar_ads\r\n";
	line += "##.sidebar_ads_336\r\n";
	line += "##.sidebar_adsense\r\n";
	line += "##.sidebar_box_ad\r\n";
	line += "##.sidebarad\r\n";
	line += "##.sidebarad_bottom\r\n";
	line += "##.sidebaradbox\r\n";
	line += "##.sidebarboxad\r\n";
	line += "##.sideheadnarrowad\r\n";
	line += "##.sideheadsponsorsad\r\n";
	line += "##.singleAd\r\n";
	line += "##.singleAdsContainer\r\n";
	line += "##.singlead\r\n";
	line += "##.sitesponsor\r\n";
	line += "##.skinAd\r\n";
	line += "##.skin_ad_638\r\n";
	line += "##.sky-ad\r\n";
	line += "##.skyAd\r\n";
	line += "##.skyAdd\r\n";
	line += "##.sky_ad\r\n";
	line += "##.sky_scraper_ad\r\n";
	line += "##.skyad\r\n";
	line += "##.skyscraper-ad\r\n";
	line += "##.skyscraper_ad\r\n";
	line += "##.skyscraper_bannerAdHome\r\n";
	line += "##.slideshow-ad\r\n";
	line += "##.slpBigSlimAdUnit\r\n";
	line += "##.slpSquareAdUnit\r\n";
	line += "##.sm_ad\r\n";
	line += "##.smallSkyAd1\r\n";
	line += "##.smallSkyAd2\r\n";
	line += "##.small_ad\r\n";
	line += "##.small_ads\r\n";
	line += "##.smallad-left\r\n";
	line += "##.smallads\r\n";
	line += "##.smallsponsorad\r\n";
	line += "##.smart_ads_bom_title\r\n";
	line += "##.specialAd175x90\r\n";
	line += "##.speedyads\r\n";
	line += "##.sphereAdContainer\r\n";
	line += "##.spl-ads\r\n";
	line += "##.spl_ad\r\n";
	line += "##.spl_ad2\r\n";
	line += "##.spl_ad_plus\r\n";
	line += "##.splitAd\r\n";
	line += "##.sponlinkbox\r\n";
	line += "##.spons-link\r\n";
	line += "##.spons_links\r\n";
	line += "##.sponslink\r\n";
	line += "##.sponsor-ad\r\n";
	line += "##.sponsor-bottom\r\n";
	line += "##.sponsor-link\r\n";
	line += "##.sponsor-links\r\n";
	line += "##.sponsor-right\r\n";
	line += "##.sponsor-services\r\n";
	line += "##.sponsor-top\r\n";
	line += "##.sponsorArea\r\n";
	line += "##.sponsorBox\r\n";
	line += "##.sponsorPost\r\n";
	line += "##.sponsorPostWrap\r\n";
	line += "##.sponsorStrip\r\n";
	line += "##.sponsorTop\r\n";
	line += "##.sponsor_ad_area\r\n";
	line += "##.sponsor_footer\r\n";
	line += "##.sponsor_horizontal\r\n";
	line += "##.sponsor_line\r\n";
	line += "##.sponsor_links\r\n";
	line += "##.sponsor_logo\r\n";
	line += "##.sponsor_top\r\n";
	line += "##.sponsor_units\r\n";
	line += "##.sponsoradtitle\r\n";
	line += "##.sponsorbox\r\n";
	line += "~gamespot.com,~mint.com,~slidetoplay.com,~smh.com.au,~zattoo.com##.sponsored\r\n";
	line += "##.sponsored-ads\r\n";
	line += "##.sponsored-chunk\r\n";
	line += "##.sponsored-editorial\r\n";
	line += "##.sponsored-features\r\n";
	line += "##.sponsored-links\r\n";
	line += "##.sponsored-links-alt-b\r\n";
	line += "##.sponsored-links-holder\r\n";
	line += "##.sponsored-links-right\r\n";
	line += "##.sponsored-post\r\n";
	line += "##.sponsored-post_ad\r\n";
	line += "##.sponsored-results\r\n";
	line += "##.sponsored-right-border\r\n";
	line += "##.sponsored-text\r\n";
	line += "##.sponsoredInner\r\n";
	line += "##.sponsoredLinks\r\n";
	line += "##.sponsoredLinksHeader\r\n";
	line += "##.sponsoredProduct\r\n";
	line += "##.sponsoredSideInner\r\n";
	line += "##.sponsored_ads\r\n";
	line += "##.sponsored_box\r\n";
	line += "##.sponsored_box_search\r\n";
	line += "##.sponsored_by\r\n";
	line += "##.sponsored_links\r\n";
	line += "##.sponsored_links_title_container\r\n";
	line += "##.sponsored_links_title_container_top\r\n";
	line += "##.sponsored_links_top\r\n";
	line += "##.sponsored_results\r\n";
	line += "##.sponsored_well\r\n";
	line += "##.sponsoredibbox\r\n";
	line += "##.sponsoredlink\r\n";
	line += "##.sponsoredlinks\r\n";
	line += "##.sponsoredlinkscontainer\r\n";
	line += "##.sponsoredresults\r\n";
	line += "~excite.eu##.sponsoredtextlink_container\r\n";
	line += "##.sponsoredtextlink_container_ovt\r\n";
	line += "##.sponsorlink\r\n";
	line += "##.sponsorlink2\r\n";
	line += "##.sponsors\r\n";
	line += "##.sponsors-box\r\n";
	line += "##.sponsorshipbox\r\n";
	line += "##.spotlightAd\r\n";
	line += "##.squareAd\r\n";
	line += "##.square_ad\r\n";
	line += "##.squared_ad\r\n";
	line += "##.ss-ad-mpu\r\n";
	line += "##.staticAd\r\n";
	line += "##.stocks-ad-tag\r\n";
	line += "##.store-ads\r\n";
	line += "##.story_AD\r\n";
	line += "##.subad\r\n";
	line += "##.subcontent-ad\r\n";
	line += "##.super-ad\r\n";
	line += "##.supercommentad_left\r\n";
	line += "##.supercommentad_right\r\n";
	line += "##.supp-ads\r\n";
	line += "##.supportAdItem\r\n";
	line += "##.surveyad\r\n";
	line += "##.t10ad\r\n";
	line += "##.tab_ad\r\n";
	line += "##.tab_ad_area\r\n";
	line += "##.tablebordersponsor\r\n";
	line += "##.tadsanzeige\r\n";
	line += "##.tadsbanner\r\n";
	line += "##.tadselement\r\n";
	line += "##.tallad\r\n";
	line += "##.tblTopAds\r\n";
	line += "##.tbl_ad\r\n";
	line += "##.tbox_ad\r\n";
	line += "##.teaser-sponsor\r\n";
	line += "##.teaserAdContainer\r\n";
	line += "##.teaser_adtiles\r\n";
	line += "##.text-ad-links\r\n";
	line += "##.text-g-advertisement\r\n";
	line += "##.text-g-group-short-rec-ad\r\n";
	line += "##.text-g-net-grp-google-ads-article-page\r\n";
	line += "##.textAd\r\n";
	line += "##.textAdBox\r\n";
	line += "##.textAds\r\n";
	line += "##.text_ad\r\n";
	line += "##.text_ads\r\n";
	line += "##.textad\r\n";
	line += "##.textadContainer\r\n";
	line += "##.textad_headline\r\n";
	line += "##.textadbox\r\n";
	line += "~frogueros.com##.textads\r\n";
	line += "##.textadsfoot\r\n";
	line += "##.textlink-ads\r\n";
	line += "##.tf_page_ad_search\r\n";
	line += "##.thisIsAd\r\n";
	line += "##.thisIsAnAd\r\n";
	line += "##.ticket-ad\r\n";
	line += "##.tileAds\r\n";
	line += "##.tips_advertisement\r\n";
	line += "##.title-ad\r\n";
	line += "##.title_adbig\r\n";
	line += "##.tncms-region-ads\r\n";
	line += "##.toolad\r\n";
	line += "##.toolbar-ad\r\n";
	line += "##.top-ad\r\n";
	line += "##.top-ad-space\r\n";
	line += "##.top-ads\r\n";
	line += "##.top-menu-ads\r\n";
	line += "##.top-sponsors\r\n";
	line += "##.topAd\r\n";
	line += "##.topAdWrap\r\n";
	line += "~timescall.com##.topAds\r\n";
	line += "##.topAdvertisement\r\n";
	line += "##.topBannerAd\r\n";
	line += "##.topLeaderboardAd\r\n";
	line += "##.top_Ad\r\n";
	line += "##.top_ad\r\n";
	line += "##.top_ad_728\r\n";
	line += "##.top_ad_728_90\r\n";
	line += "##.top_ad_disclaimer\r\n";
	line += "##.top_ad_div\r\n";
	line += "##.top_ad_post\r\n";
	line += "##.top_ad_wrapper\r\n";
	line += "~trailvoy.com##.top_ads\r\n";
	line += "##.top_advert\r\n";
	line += "##.top_advertising_lb\r\n";
	line += "##.top_container_ad\r\n";
	line += "##.top_sponsor\r\n";
	line += "~pchome.com.tw##.topad\r\n";
	line += "##.topad-bar\r\n";
	line += "##.topadbox\r\n";
	line += "##.topads\r\n";
	line += "##.topadspot\r\n";
	line += "##.topadvertisementsegment\r\n";
	line += "##.topcontentadvertisement\r\n";
	line += "##.topic_inad\r\n";
	line += "##.topstoriesad\r\n";
	line += "##.toptenAdBoxA\r\n";
	line += "##.towerAd\r\n";
	line += "##.towerAdLeft\r\n";
	line += "##.towerAds\r\n";
	line += "##.tower_ad\r\n";
	line += "##.tower_ad_disclaimer\r\n";
	line += "##.towerad\r\n";
	line += "##.ts-ad_unit_bigbox\r\n";
	line += "##.ts-banner_ad\r\n";
	line += "##.ttlAdsensel\r\n";
	line += "##.tto-sponsored-element\r\n";
	line += "##.tvs-mpu\r\n";
	line += "##.twoColumnAd\r\n";
	line += "##.twoadcoll\r\n";
	line += "##.twoadcolr\r\n";
	line += "##.tx_smartadserver_pi1\r\n";
	line += "##.txt-ads\r\n";
	line += "##.txtAds\r\n";
	line += "##.txt_ads\r\n";
	line += "##.txtadvertise\r\n";
	line += "##.type_adscontainer\r\n";
	line += "##.type_miniad\r\n";
	line += "##.type_promoads\r\n";
	line += "##.ukAds\r\n";
	line += "##.undertimyads\r\n";
	line += "##.universalboxADVBOX01\r\n";
	line += "##.universalboxADVBOX03\r\n";
	line += "##.universalboxADVBOX04a\r\n";
	line += "##.usenext\r\n";
	line += "##.vertad\r\n";
	line += "##.videoAd\r\n";
	line += "##.videoBoxAd\r\n";
	line += "##.video_ad\r\n";
	line += "##.view-promo-mpu-right\r\n";
	line += "##.view_rig_ad\r\n";
	line += "##.virgin-mpu\r\n";
	line += "##.wa_adsbottom\r\n";
	line += "##.wide-ad\r\n";
	line += "##.wide-skyscraper-ad\r\n";
	line += "##.wideAdTable\r\n";
	line += "##.wide_ad\r\n";
	line += "##.wide_ad_unit_top\r\n";
	line += "##.wide_ads\r\n";
	line += "##.wide_google_ads\r\n";
	line += "##.widget-ad\r\n";
	line += "##.widget-ad300x250\r\n";
	line += "##.widget-entry-ads-160\r\n";
	line += "##.widgetYahooAds\r\n";
	line += "##.widget_ad\r\n";
	line += "##.widget_ad_rotator\r\n";
	line += "##.widget_island_ad\r\n";
	line += "##.widget_sdac_footer_ads_widget\r\n";
	line += "##.wikia-ad\r\n";
	line += "##.wikia_ad_placeholder\r\n";
	line += "##.withAds\r\n";
	line += "##.wnMultiAd\r\n";
	line += "##.wp125ad\r\n";
	line += "##.wp125ad_2\r\n";
	line += "##.wpn_ad_content\r\n";
	line += "##.wrap-ads\r\n";
	line += "##.wsSponsoredLinksRight\r\n";
	line += "##.wsTopSposoredLinks\r\n";
	line += "##.x03-adunit\r\n";
	line += "##.x04-adunit\r\n";
	line += "##.xads-blk2\r\n";
	line += "##.xads-ojedn\r\n";
	line += "##.y-ads\r\n";
	line += "##.y-ads-wide\r\n";
	line += "##.y7-advertisement\r\n";
	line += "##.yahoo-sponsored\r\n";
	line += "##.yahoo-sponsored-links\r\n";
	line += "##.yahooAds\r\n";
	line += "##.yahoo_ads\r\n";
	line += "##.yan-sponsored\r\n";
	line += "##.ygrp-ad\r\n";
	line += "##.yrail_ad_wrap\r\n";
	line += "##.yrail_ads\r\n";
	line += "##.ysmsponsor\r\n";
	line += "##.ysponsor\r\n";
	line += "##.yw-ad\r\n";
	line += "~marketgid.com,~mgid.com,~thechive.com##[id^=\"MarketGid\"]\r\n";
	line += "##a[href^=\"http://ad.doubleclick.net/\"]\r\n";
	line += "##a[href^=\"http://adserving.liveuniversenetwork.com/\"]\r\n";
	line += "##a[href^=\"http://galleries.pinballpublishernetwork.com/\"]\r\n";
	line += "##a[href^=\"http://galleries.securewebsiteaccess.com/\"]\r\n";
	line += "##a[href^=\"http://install.securewebsiteaccess.com/\"]\r\n";
	line += "##a[href^=\"http://latestdownloads.net/download.php?\"]\r\n";
	line += "##a[href^=\"http://secure.signup-page.com/\"]\r\n";
	line += "##a[href^=\"http://secure.signup-way.com/\"]\r\n";
	line += "##a[href^=\"http://www.FriendlyDuck.com/AF_\"]\r\n";
	line += "##a[href^=\"http://www.adbrite.com/mb/commerce/purchase_form.php?\"]\r\n";
	line += "##a[href^=\"http://www.friendlyduck.com/AF_\"]\r\n";
	line += "##a[href^=\"http://www.google.com/aclk?\"]\r\n";
	line += "##a[href^=\"http://www.liutilities.com/aff\"]\r\n";
	line += "##a[href^=\"http://www.liutilities.com/products/campaigns/adv/\"]\r\n";
	line += "##a[href^=\"http://www.my-dirty-hobby.com/?sub=\"]\r\n";
	line += "##a[href^=\"http://www.ringtonematcher.com/\"]\r\n";
	line += "!Google\r\n";
	line += "###mbEnd[cellspacing=\"0\"][cellpadding=\"0\"][style=\"padding: 0pt;\"]\r\n";
	line += "###mbEnd[cellspacing=\"0\"][style=\"padding: 0pt; white-space: nowrap;\"]\r\n";
	line += "##div#mclip_container:first-child:last-child\r\n";
	line += "##div#rhs_block[style=\"padding-top: 5px;\"]\r\n";
	line += "##div#rhs_block[style=\"padding-top:5px\"]\r\n";
	line += "##div#tads.c\r\n";
	line += "##table.ra[align=\"left\"][width=\"30%\"]\r\n";
	line += "##table.ra[align=\"right\"][width=\"30%\"]\r\n";
	line += "!-----------------Third-party advertisers-----------------!\r\n";
	line += "! *** easylist_adservers.txt ***\r\n";
	line += "||10pipsaffiliates.com^$third-party\r\n";
	line += "||1100i.com^$third-party\r\n";
	line += "||188server.com^$third-party\r\n";
	line += "||247realmedia.com^$third-party\r\n";
	line += "||2mdn.net^$third-party\r\n";
	line += "||360ads.com^$third-party\r\n";
	line += "||3rdads.com^$third-party\r\n";
	line += "||43plc.com^$third-party\r\n";
	line += "||600z.com^$third-party\r\n";
	line += "||777seo.com^$third-party\r\n";
	line += "||7search.com^$third-party\r\n";
	line += "||aa.voice2page.com^$third-party\r\n";
	line += "||accuserveadsystem.com^$third-party\r\n";
	line += "||acf-webmaster.net^$third-party\r\n";
	line += "||acronym.com^$third-party\r\n";
	line += "||ad-flow.com^$third-party\r\n";
	line += "||ad20.net^$third-party\r\n";
	line += "||ad2games.com^$third-party\r\n";
	line += "||ad4game.com^$third-party\r\n";
	line += "||adaction.se^$third-party\r\n";
	line += "||adaos-ads.net^$third-party\r\n";
	line += "||adbard.net^$third-party\r\n";
	line += "||adbasket.net^$third-party\r\n";
	line += "||adblade.com^$third-party\r\n";
	line += "||adbrite.com^$third-party\r\n";
	line += "||adbull.com^$third-party\r\n";
	line += "||adbureau.net^$third-party\r\n";
	line += "||adbutler.com^$third-party\r\n";
	line += "||adcde.com^$third-party\r\n";
	line += "||adcentriconline.com^$third-party\r\n";
	line += "||adchap.com^$third-party\r\n";
	line += "||adclickmedia.com^$third-party\r\n";
	line += "||adcolo.com^$third-party\r\n";
	line += "||adcru.com^$third-party\r\n";
	line += "||addynamo.com^$third-party\r\n";
	line += "||adecn.com^$third-party\r\n";
	line += "||adengage.com^$third-party\r\n";
	line += "||adf01.net^$third-party\r\n";
	line += "||adfactory88.com^$third-party\r\n";
	line += "||adfrontiers.com^$third-party\r\n";
	line += "||adfusion.com^$third-party\r\n";
	line += "||adgardener.com^$third-party\r\n";
	line += "||adgear.com^$third-party\r\n";
	line += "||adgent007.com^$third-party\r\n";
	line += "||adgine.net^$third-party\r\n";
	line += "||adgitize.com^$third-party\r\n";
	line += "||adgroups.com^$third-party\r\n";
	line += "||adhese.be^$third-party\r\n";
	line += "||adhese.net^$third-party\r\n";
	line += "||adhitzads.com^$third-party\r\n";
	line += "||adhostingsolutions.com^$third-party\r\n";
	line += "||adicate.com^$third-party\r\n";
	line += "||adimise.com^$third-party\r\n";
	line += "||adimpact.com^$third-party\r\n";
	line += "||adinterax.com^$third-party\r\n";
	line += "||adireland.com^$third-party\r\n";
	line += "||adisfy.com^$third-party\r\n";
	line += "||adisn.com^$third-party\r\n";
	line += "||adition.com^$third-party\r\n";
	line += "||adjal.com^$third-party\r\n";
	line += "||adjug.com^$third-party\r\n";
	line += "||adjuggler.com^$third-party\r\n";
	line += "||adjuggler.net^$third-party\r\n";
	line += "||adkonekt.com^$third-party\r\n";
	line += "||adlink.net^$third-party\r\n";
	line += "||adlisher.com^$third-party\r\n";
	line += "||admarketplace.net^$third-party\r\n";
	line += "||admaya.in^$third-party\r\n";
	line += "||admeld.com^$third-party\r\n";
	line += "||admeta.com^$third-party\r\n";
	line += "||admitad.com^$third-party\r\n";
	line += "||admpads.com^$third-party\r\n";
	line += "||adnet.biz^$third-party\r\n";
	line += "||adnet.com^$third-party\r\n";
	line += "||adnet.ru^$third-party\r\n";
	line += "||adocean.pl^$third-party\r\n";
	line += "||adoperator.com^$third-party\r\n";
	line += "||adoptim.com^$third-party\r\n";
	line += "||adotube.com^$third-party\r\n";
	line += "||adparlor.com^$third-party\r\n";
	line += "||adperium.com^$third-party\r\n";
	line += "||adpinion.com^$third-party\r\n";
	line += "||adpionier.de^$third-party\r\n";
	line += "||adpremo.com^$third-party\r\n";
	line += "||adprs.net^$third-party\r\n";
	line += "||adquest3d.com^$third-party\r\n";
	line += "||adreadytractions.com^$third-party\r\n";
	line += "||adrocket.com^$third-party\r\n";
	line += "||adroll.com^$third-party\r\n";
	line += "||ads-stats.com^$third-party\r\n";
	line += "||ads4cheap.com^$third-party\r\n";
	line += "||adscendmedia.com^$third-party\r\n";
	line += "||adsdk.com^$third-party\r\n";
	line += "||adsensecamp.com^$third-party\r\n";
	line += "||adservinginternational.com^$third-party\r\n";
	line += "||adsfactor.net^$third-party\r\n";
	line += "||adsfast.com^$third-party\r\n";
	line += "||adsforindians.com^$third-party\r\n";
	line += "||adsfuse.com^$third-party\r\n";
	line += "||adshopping.com^$third-party\r\n";
	line += "||adshuffle.com^$third-party\r\n";
	line += "||adsignals.com^$third-party\r\n";
	line += "||adsmarket.com^$third-party\r\n";
	line += "||adsmedia.cc^$third-party\r\n";
	line += "||adsonar.com^$third-party\r\n";
	line += "||adspeed.com^$third-party\r\n";
	line += "||adsrevenue.net^$third-party\r\n";
	line += "||adsupermarket.com^$third-party\r\n";
	line += "||adswizz.com^$third-party\r\n";
	line += "||adtaily.com^$third-party\r\n";
	line += "||adtaily.eu^$third-party\r\n";
	line += "||adtech.de^$third-party\r\n";
	line += "||adtechus.com^$third-party\r\n";
	line += "||adtoll.com^$third-party\r\n";
	line += "||adtology1.com^$third-party\r\n";
	line += "||adtology2.com^$third-party\r\n";
	line += "||adtology3.com^$third-party\r\n";
	line += "||adtoma.com^$third-party\r\n";
	line += "||adtotal.pl^$third-party\r\n";
	line += "||adtrgt.com^$third-party\r\n";
	line += "||adtrix.com^$third-party\r\n";
	line += "||adult-adv.com^$third-party\r\n";
	line += "||adultadworld.com^$third-party\r\n";
	line += "||adversalservers.com^$third-party\r\n";
	line += "||adverserve.net^$third-party\r\n";
	line += "||advertarium.com.ua^$third-party\r\n";
	line += "||adverticum.net^$third-party\r\n";
	line += "||advertise.com^$third-party\r\n";
	line += "||advertiseyourgame.com^$third-party\r\n";
	line += "||advertising-department.com^$third-party\r\n";
	line += "||advertising.com^$third-party\r\n";
	line += "||advertisingiq.com^$third-party\r\n";
	line += "||advertlets.com^$third-party\r\n";
	line += "||advertpay.net^$third-party\r\n";
	line += "||advertserve.com^$third-party\r\n";
	line += "||advertstatic.com^$third-party\r\n";
	line += "||advg.jp/$third-party\r\n";
	line += "||advgoogle.com^$third-party\r\n";
	line += "||adviva.net^$third-party\r\n";
	line += "||advmaker.ru^$third-party\r\n";
	line += "||advmd.com^$third-party\r\n";
	line += "||advpoints.com^$third-party\r\n";
	line += "||adworldmedia.com^$third-party\r\n";
	line += "||adxpower.com^$third-party\r\n";
	line += "||adyoz.com^$third-party\r\n";
	line += "||adzerk.net^$third-party\r\n";
	line += "||afcyhf.com^$third-party\r\n";
	line += "||affiliate.com^$third-party\r\n";
	line += "||affiliate.cx^$third-party\r\n";
	line += "||affiliatefuel.com^$third-party\r\n";
	line += "||affiliatefuture.com^$third-party\r\n";
	line += "||affiliatelounge.com^$third-party\r\n";
	line += "||affiliatemembership.com^$third-party\r\n";
	line += "||affiliatesensor.com^$third-party\r\n";
	line += "||affiliproducts.com^$third-party\r\n";
	line += "||affinity.com^$third-party\r\n";
	line += "||afterdownload.com^$third-party\r\n";
	line += "||afy11.net^$third-party\r\n";
	line += "||agentcenters.com^$third-party\r\n";
	line += "||aggregateknowledge.com^$third-party\r\n";
	line += "||aim4media.com^$third-party\r\n";
	line += "||aimatch.com^$third-party\r\n";
	line += "||ajansreklam.net^$third-party\r\n";
	line += "||alimama.cn^$third-party\r\n";
	line += "||alphagodaddy.com^$third-party\r\n";
	line += "||amgdgt.com^$third-party\r\n";
	line += "||ampxchange.com^$third-party\r\n";
	line += "||anrdoezrs.net^$third-party\r\n";
	line += "||apmebf.com^$third-party\r\n";
	line += "||arcade-advertisement.com^$third-party\r\n";
	line += "||arcadebannerexchange.net^$third-party\r\n";
	line += "||arcadebanners.com^$third-party\r\n";
	line += "||arcadebe.com^$third-party\r\n";
	line += "||arti-mediagroup.com^$third-party\r\n";
	line += "||as5000.com^$third-party\r\n";
	line += "||asklots.com^$third-party\r\n";
	line += "||assetize.com^$third-party\r\n";
	line += "||assoc-amazon.co.uk^$third-party\r\n";
	line += "||assoc-amazon.com^$third-party\r\n";
	line += "||atdmt.com^$third-party\r\n";
	line += "||atmalinks.com^$third-party\r\n";
	line += "||atwola.com^$third-party\r\n";
	line += "||audienceprofiler.com^$third-party\r\n";
	line += "||auditude.com^$third-party\r\n";
	line += "||auspipe.com^$third-party\r\n";
	line += "||automateyourlist.com^$third-party\r\n";
	line += "||avads.co.uk^$third-party\r\n";
	line += "||avantlink.com^$third-party\r\n";
	line += "||awaps.net^$third-party\r\n";
	line += "||awin1.com^$third-party\r\n";
	line += "||awltovhc.com^$third-party\r\n";
	line += "||axill.com^$third-party\r\n";
	line += "||azads.com^$third-party\r\n";
	line += "||azjmp.com^$third-party\r\n";
	line += "||azoogleads.com^$third-party\r\n";
	line += "||backbeatmedia.com^$third-party\r\n";
	line += "||banner-clix.com^$third-party\r\n";
	line += "||bannerbank.ru^$third-party\r\n";
	line += "||bannerblasters.com^$third-party\r\n";
	line += "||bannercde.com^$third-party\r\n";
	line += "||bannerconnect.com^$third-party\r\n";
	line += "||bannerconnect.net^$third-party\r\n";
	line += "||bannerflux.com^$third-party\r\n";
	line += "||bannerjammers.com^$third-party\r\n";
	line += "||bannerlot.com^$third-party\r\n";
	line += "||bannerrage.com^$third-party\r\n";
	line += "||bannersmania.com^$third-party\r\n";
	line += "||bannersnack.net^$third-party\r\n";
	line += "||bannertgt.com^$third-party\r\n";
	line += "||bbelements.com^$third-party\r\n";
	line += "||beaconads.com^$third-party\r\n";
	line += "||begun.ru^$third-party\r\n";
	line += "||belointeractive.com^$third-party\r\n";
	line += "||bestcasinopartner.com^$third-party\r\n";
	line += "||bestdeals.ws^$third-party\r\n";
	line += "||bestfindsite.com^$third-party\r\n";
	line += "||bestofferdirect.com^$third-party\r\n";
	line += "||bet365affiliates.com^$third-party\r\n";
	line += "||bfast.com^$third-party\r\n";
	line += "||bidvertiser.com^$third-party\r\n";
	line += "||biemedia.com^$third-party\r\n";
	line += "||bin-layer.de^$third-party\r\n";
	line += "||bin-layer.ru^$third-party\r\n";
	line += "||bingo4affiliates.com^$third-party\r\n";
	line += "||binlayer.de^$third-party\r\n";
	line += "||bittads.com^$third-party\r\n";
	line += "||blogads.com^$third-party\r\n";
	line += "||bluestreak.com^$third-party\r\n";
	line += "||bmanpn.com^$third-party\r\n";
	line += "||bnetworx.com^$third-party\r\n";
	line += "||bnr.sys.lv^$third-party\r\n";
	line += "||boo-box.com^$third-party\r\n";
	line += "||boylesportsreklame.com^$third-party\r\n";
	line += "||branchr.com^$third-party\r\n";
	line += "||bravenetmedianetwork.com^$third-party\r\n";
	line += "||bridgetrack.com^$third-party\r\n";
	line += "||btrll.com^$third-party\r\n";
	line += "||bu520.com^$third-party\r\n";
	line += "||buildtrafficx.com^$third-party\r\n";
	line += "||burstnet.com^$third-party\r\n";
	line += "||buysellads.com^$third-party\r\n";
	line += "||buzzparadise.com^$third-party\r\n";
	line += "||c-on-text.com^$third-party\r\n";
	line += "||c-planet.net^$third-party\r\n";
	line += "||c8.net.ua^$third-party\r\n";
	line += "||captainad.com^$third-party\r\n";
	line += "||casalemedia.com^$third-party\r\n";
	line += "||cash4members.com^$third-party\r\n";
	line += "||cbclickbank.com^$third-party\r\n";
	line += "||cc-dt.com^$third-party\r\n";
	line += "||cdna.tremormedia.com^$third-party\r\n";
	line += "||cgecwm.org^$third-party\r\n";
	line += "||checkm8.com^$third-party\r\n";
	line += "||checkmystats.com.au^$third-party\r\n";
	line += "||checkoutfree.com^$third-party\r\n";
	line += "||chipleader.com^$third-party\r\n";
	line += "||chitika.net^$third-party\r\n";
	line += "||cjt1.net^$third-party\r\n";
	line += "||clash-media.com^$third-party\r\n";
	line += "||claxonmedia.com^$third-party\r\n";
	line += "||click4free.info^$third-party\r\n";
	line += "||clickad.pl^$third-party\r\n";
	line += "||clickbooth.com^$third-party\r\n";
	line += "||clickexa.com^$third-party\r\n";
	line += "||clickexperts.net^$third-party\r\n";
	line += "||clickfuse.com^$third-party\r\n";
	line += "||clickintext.net^$third-party\r\n";
	line += "||clicksor.com^$third-party\r\n";
	line += "||clicksor.net^$third-party\r\n";
	line += "||clickthrucash.com^$third-party\r\n";
	line += "||clixgalore.com^$third-party\r\n";
	line += "||coadvertise.com^$third-party\r\n";
	line += "||cogsdigital.com^$third-party\r\n";
	line += "||collection-day.com^$third-party\r\n";
	line += "||collective-media.net^$third-party\r\n";
	line += "||come2play.net^$third-party\r\n";
	line += "||commission-junction.com^$third-party\r\n";
	line += "||commissionmonster.com^$third-party\r\n";
	line += "||comscore.com^$third-party\r\n";
	line += "||conduit-banners.com^$third-party\r\n";
	line += "||connectedads.net^$third-party\r\n";
	line += "||connextra.com^$third-party\r\n";
	line += "||contenture.com^$third-party\r\n";
	line += "||contexlink.se^$third-party\r\n";
	line += "||contextuads.com^$third-party\r\n";
	line += "||contextweb.com^$third-party\r\n";
	line += "||cpaclicks.com^$third-party\r\n";
	line += "||cpalead.com^$third-party\r\n";
	line += "||cpays.com^$third-party\r\n";
	line += "||cpmstar.com^$third-party\r\n";
	line += "||cpuim.com^$third-party\r\n";
	line += "||cpxinteractive.com^$third-party\r\n";
	line += "||crispads.com^$third-party\r\n";
	line += "||crowdgravity.com^$third-party\r\n";
	line += "||ctasnet.com^$third-party\r\n";
	line += "||ctm-media.com^$third-party\r\n";
	line += "||ctrhub.com^$third-party\r\n";
	line += "||cubics.com^$third-party\r\n";
	line += "||d.m3.net^$third-party\r\n";
	line += "||dashboardad.net^$third-party\r\n";
	line += "||dbbsrv.com^$third-party\r\n";
	line += "||decisionmark.com^$third-party\r\n";
	line += "||decisionnews.com^$third-party\r\n";
	line += "||decknetwork.net^$third-party\r\n";
	line += "||deepmetrix.com^$third-party\r\n";
	line += "||defaultimg.com^$third-party\r\n";
	line += "||deplayer.net^$third-party\r\n";
	line += "||destinationurl.com^$third-party\r\n";
	line += "||dexplatform.com^$third-party\r\n";
	line += "||dgmaustralia.com^$third-party\r\n";
	line += "||digitrevenue.com^$third-party\r\n";
	line += "||dinclinx.com^$third-party\r\n";
	line += "||directorym.com^$third-party\r\n";
	line += "||directtrack.com^$third-party\r\n";
	line += "||dl-rms.com^$third-party\r\n";
	line += "||domainsponsor.com^$third-party\r\n";
	line += "||dotomi.com^$third-party\r\n";
	line += "||doubleclick.net/ad/sevenload.*.smartclip/video;$object_subrequest\r\n";
	line += "||doubleclick.net/adx/*.collegehumor/$object_subrequest,third-party\r\n";
	line += "||doubleclick.net/pfadx/*.mtvi$object_subrequest,third-party\r\n";
	line += "||doubleclick.net/pfadx/*.sevenload.com_$object_subrequest\r\n";
	line += "||doubleclick.net/pfadx/*adcat=$object_subrequest,third-party\r\n";
	line += "||doubleclick.net^$object_subrequest,third-party,domain=addictinggames.com|atom.com|break.com|businessweek.com|cbs4denver.com|cnbc.com|darkhorizons.com|doubleviking.com|eonline.com|fandango.com|foxbusiness.com|foxnews.com|g4tv.com|joblo.com|mtv.co.uk|mtv.com|mtv.com.au|mtv.com.nz|mtvbase.com|mtvmusic.com|myfoxorlando.com|myfoxphoenix.com|newsweek.com|nick.com|nintendoeverything.com|pandora.com|play.it|ps3news.com|rte.ie|sbsun.com|sevenload.com|shockwave.com|southpark.nl|space.com|spike.com|thedailygreen.com|thedailyshow.com|thewire.com|ustream.tv|washingtonpost.com|wcbstv.com|wired.com|wkbw.com|wsj.com|wwe.com|youtube.com|zoomin.tv\r\n";
	line += "||doubleclick.net^$~object_subrequest,third-party\r\n";
	line += "||doubleclick.net^*;sz=$object_subrequest,third-party,domain=1up.com|breitbart.tv|digitaltrends.com|gamesradar.com|gametrailers.com|heavy.com|myfoxny.com|myspace.com|nbc.com|nfl.com|nhl.com|wptv.com\r\n";
	line += "||dpbolvw.net^$third-party\r\n";
	line += "||dt00.net^$domain=~marketgid.com|~mgid.com|~thechive.com\r\n";
	line += "||dt07.net^$domain=~marketgid.com|~mgid.com|~thechive.com\r\n";
	line += "||e-planning.net^$third-party\r\n";
	line += "||easyhits4u.com^$third-party\r\n";
	line += "||ebannertraffic.com^$third-party\r\n";
	line += "||ebayobjects.com.au^$third-party\r\n";
	line += "||ebayobjects.com^$third-party\r\n";
	line += "||edge-dl.andomedia.com^$third-party\r\n";
	line += "||egamingonline.com^$third-party\r\n";
	line += "||ekmas.com^$third-party\r\n";
	line += "||emediate.eu^$third-party\r\n";
	line += "||emediate.se^$third-party\r\n";
	line += "||engineseeker.com^$third-party\r\n";
	line += "||ero-advertising.com^$third-party\r\n";
	line += "||etology.com^$third-party\r\n";
	line += "||euroclick.com^$third-party\r\n";
	line += "||euros4click.de^$third-party\r\n";
	line += "||exelator.com^$third-party\r\n";
	line += "||exitexplosion.com^$third-party\r\n";
	line += "||exitjunction.com^$third-party\r\n";
	line += "||exponential.com^$third-party\r\n";
	line += "||eyereturn.com^$third-party\r\n";
	line += "||eyewonder.com^$third-party\r\n";
	line += "||fairadsnetwork.com^$third-party\r\n";
	line += "||fairfax.com.au^$~stylesheet,third-party\r\n";
	line += "||falkag.net^$third-party\r\n";
	line += "||fastclick.net^$third-party\r\n";
	line += "||fimserve.com^$third-party\r\n";
	line += "||findsthat.com^$third-party\r\n";
	line += "||firstadsolution.com^$third-party\r\n";
	line += "||firstlightera.com^$third-party\r\n";
	line += "||fixionmedia.com^$third-party\r\n";
	line += "||flashtalking.com^$third-party\r\n";
	line += "||fluxads.com^$third-party\r\n";
	line += "||fmpub.net^$third-party\r\n";
	line += "||footerslideupad.com^$third-party\r\n";
	line += "||forexyard.com^$third-party\r\n";
	line += "||forrestersurveys.com^$third-party\r\n";
	line += "||freebannerswap.co.uk^$third-party\r\n";
	line += "||freelancer.com^$third-party\r\n";
	line += "||friendlyduck.com^$third-party\r\n";
	line += "||ftjcfx.com^$third-party\r\n";
	line += "||funklicks.com^$third-party\r\n";
	line += "||fusionads.net^$third-party\r\n";
	line += "||fwmrm.net^$third-party\r\n";
	line += "||g.doubleclick.net^$third-party\r\n";
	line += "||gambling-affiliation.com^$third-party\r\n";
	line += "||game-advertising-online.com^$third-party\r\n";
	line += "||gameads.com^$third-party\r\n";
	line += "||gamecetera.com^$third-party\r\n";
	line += "||gamersbanner.com^$third-party\r\n";
	line += "||gannett.gcion.com^$third-party\r\n";
	line += "||gate-ru.com^$third-party\r\n";
	line += "||geek2us.net^$third-party\r\n";
	line += "||geo-idm.fr^$third-party\r\n";
	line += "||geopromos.com^$third-party\r\n";
	line += "||gestionpub.com^$third-party\r\n";
	line += "||ggncpm.com^$third-party\r\n";
	line += "||gimiclub.com^$third-party\r\n";
	line += "||gklmedia.com^$third-party\r\n";
	line += "||globaladsales.com^$third-party\r\n";
	line += "||globaladv.net^$third-party\r\n";
	line += "||gmads.net^$third-party\r\n";
	line += "||go2media.org^$third-party\r\n";
	line += "||googleadservices.com^$third-party\r\n";
	line += "||grabmyads.com^$third-party\r\n";
	line += "||gratisnetwork.com^$third-party\r\n";
	line += "||guardiandigitalcomparison.co.uk^$third-party\r\n";
	line += "||gumgum.com^$third-party\r\n";
	line += "||halogennetwork.com^$third-party\r\n";
	line += "||havamedia.net^$third-party\r\n";
	line += "||hb-247.com^$third-party\r\n";
	line += "||hit-now.com^$third-party\r\n";
	line += "||hits.sys.lv^$third-party\r\n";
	line += "||hopfeed.com^$third-party\r\n";
	line += "||hosticanaffiliate.com^$third-party\r\n";
	line += "||hot-hits.us^$third-party\r\n";
	line += "||hotptp.com^$third-party\r\n";
	line += "||httpool.com^$third-party\r\n";
	line += "||hypemakers.net^$third-party\r\n";
	line += "||hypervre.com^$third-party\r\n";
	line += "||ibatom.com^$third-party\r\n";
	line += "||icdirect.com^$third-party\r\n";
	line += "||imagesatlantic.com^$third-party\r\n";
	line += "||imedia.co.il^$third-party\r\n";
	line += "||imglt.com^$third-party\r\n";
	line += "||imho.ru/$third-party\r\n";
	line += "||imiclk.com^$third-party\r\n";
	line += "||impact-ad.jp^$third-party\r\n";
	line += "||impresionesweb.com^$third-party\r\n";
	line += "||indiabanner.com^$third-party\r\n";
	line += "||indiads.com^$third-party\r\n";
	line += "||indianbannerexchange.com^$third-party\r\n";
	line += "||indianlinkexchange.com^$third-party\r\n";
	line += "||industrybrains.com^$third-party\r\n";
	line += "||inetinteractive.com^$third-party\r\n";
	line += "||infinite-ads.com^$third-party\r\n";
	line += "||influads.com^$third-party\r\n";
	line += "||infolinks.com^$third-party\r\n";
	line += "||information-sale.com^$third-party\r\n";
	line += "||innity.com^$third-party\r\n";
	line += "||insightexpressai.com^$third-party\r\n";
	line += "||inskinad.com^$third-party\r\n";
	line += "||inskinmedia.com^$third-party\r\n";
	line += "||instantbannercreator.com^$third-party\r\n";
	line += "||intellibanners.com^$third-party\r\n";
	line += "||intellitxt.com^$third-party\r\n";
	line += "||interclick.com^$third-party\r\n";
	line += "||interpolls.com^$third-party\r\n";
	line += "||inuvo.com^$third-party\r\n";
	line += "||investingchannel.com^$third-party\r\n";
	line += "||ipromote.com^$third-party\r\n";
	line += "||jangonetwork.com^$third-party\r\n";
	line += "||jdoqocy.com^$third-party\r\n";
	line += "||jsfeedadsget.com^$third-party\r\n";
	line += "||jumboaffiliates.com^$third-party\r\n";
	line += "||justrelevant.com^$third-party\r\n";
	line += "||kalooga.com^$third-party\r\n";
	line += "||kanoodle.com^$third-party\r\n";
	line += "||kavanga.ru^$third-party\r\n";
	line += "||kehalim.com^$third-party\r\n";
	line += "||kerg.net^$third-party\r\n";
	line += "||ketoo.com^$third-party\r\n";
	line += "||kitnmedia.com^$third-party\r\n";
	line += "||klikvip.com^$third-party\r\n";
	line += "||klipmart.com^$third-party\r\n";
	line += "||kontera.com^$third-party\r\n";
	line += "||kqzyfj.com^$third-party\r\n";
	line += "||lakequincy.com^$third-party\r\n";
	line += "||lduhtrp.net^$third-party\r\n";
	line += "||leadacceptor.com^$third-party\r\n";
	line += "||liftdna.com^$third-party\r\n";
	line += "||ligatus.com^$third-party\r\n";
	line += "||lightningcast.net^$~object_subrequest,third-party\r\n";
	line += "||lingospot.com^$third-party\r\n";
	line += "||linkbucks.com^$third-party\r\n";
	line += "||linkbuddies.com^$third-party\r\n";
	line += "||linkexchange.com^$third-party\r\n";
	line += "||linkreferral.com^$third-party\r\n";
	line += "||linkshowoff.com^$third-party\r\n";
	line += "||linkstorm.net^$third-party\r\n";
	line += "||linksynergy.com^$third-party\r\n";
	line += "||linkworth.com^$third-party\r\n";
	line += "||linkz.net^$third-party\r\n";
	line += "||liverail.com^$third-party\r\n";
	line += "||liveuniversenetwork.com^$third-party\r\n";
	line += "||looksmart.com^$third-party\r\n";
	line += "||ltassrv.com.s3.amazonaws.com^$third-party\r\n";
	line += "||ltassrv.com^$third-party\r\n";
	line += "||lzjl.com^$third-party\r\n";
	line += "||madisonlogic.com^$third-party\r\n";
	line += "||markethealth.com^$third-party\r\n";
	line += "||marketingsolutions.yahoo.com^$third-party\r\n";
	line += "||marketnetwork.com^$third-party\r\n";
	line += "||maxserving.com^$third-party\r\n";
	line += "||mb01.com^$third-party\r\n";
	line += "||mbn.com.ua^$third-party\r\n";
	line += "||media6degrees.com^$third-party\r\n";
	line += "||mediag4.com^$third-party\r\n";
	line += "||mediagridwork.com^$third-party\r\n";
	line += "||medialand.ru^$third-party\r\n";
	line += "||medialation.net^$third-party\r\n";
	line += "||mediaonenetwork.net^$third-party\r\n";
	line += "||mediaplex.com^$third-party\r\n";
	line += "||mediatarget.com^$third-party\r\n";
	line += "||medleyads.com^$third-party\r\n";
	line += "||medrx.sensis.com.au^$third-party\r\n";
	line += "||meetic-partners.com^$third-party\r\n";
	line += "||megaclick.com^$third-party\r\n";
	line += "||mercuras.com^$third-party\r\n";
	line += "||metaffiliation.com^$third-party\r\n";
	line += "||mezimedia.com^$third-party\r\n";
	line += "||microsoftaffiliates.net^$third-party\r\n";
	line += "||milabra.com^$third-party\r\n";
	line += "||mirago.com^$third-party\r\n";
	line += "||miva.com^$third-party\r\n";
	line += "||mixpo.com^$third-party\r\n";
	line += "||mktseek.com^$third-party\r\n";
	line += "||money4ads.com^$third-party\r\n";
	line += "||mookie1.com^$third-party\r\n";
	line += "||mootermedia.com^$third-party\r\n";
	line += "||moregamers.com^$third-party\r\n";
	line += "||moreplayerz.com^$third-party\r\n";
	line += "||mpression.net^$third-party\r\n";
	line += "||msads.net^$third-party\r\n";
	line += "||nabbr.com^$third-party\r\n";
	line += "||nbjmp.com^$third-party\r\n";
	line += "||nbstatic.com^$third-party\r\n";
	line += "||neodatagroup.com^$third-party\r\n";
	line += "||neoffic.com^$third-party\r\n";
	line += "||net3media.com^$third-party\r\n";
	line += "||netavenir.com^$third-party\r\n";
	line += "||netseer.com^$third-party\r\n";
	line += "||networldmedia.net^$third-party\r\n";
	line += "||newsadstream.com^$third-party\r\n";
	line += "||newtention.net^$third-party\r\n";
	line += "||nexac.com^$third-party\r\n";
	line += "||nicheads.com^$third-party\r\n";
	line += "||nobleppc.com^$third-party\r\n";
	line += "||northmay.com^$third-party\r\n";
	line += "||nowlooking.net^$third-party\r\n";
	line += "||nvero.net^$third-party\r\n";
	line += "||nyadmcncserve-05y06a.com^$third-party\r\n";
	line += "||obeus.com^$third-party\r\n";
	line += "||obibanners.com^$third-party\r\n";
	line += "||objects.tremormedia.com^$~object_subrequest,third-party\r\n";
	line += "||objectservers.com^$third-party\r\n";
	line += "||oclus.com^$third-party\r\n";
	line += "||omg2.com^$third-party\r\n";
	line += "||omguk.com^$third-party\r\n";
	line += "||onads.com^$third-party\r\n";
	line += "||onenetworkdirect.net^$third-party\r\n";
	line += "||onlineadtracker.co.uk^$third-party\r\n";
	line += "||opensourceadvertisementnetwork.info^$third-party\r\n";
	line += "||openx.com^$third-party\r\n";
	line += "||openx.net^$third-party\r\n";
	line += "||openx.org^$third-party\r\n";
	line += "||opinionbar.com^$third-party\r\n";
	line += "||othersonline.com^$third-party\r\n";
	line += "||overture.com^$third-party\r\n";
	line += "||oxado.com^$third-party\r\n";
	line += "||p-advg.com^$third-party\r\n";
	line += "||pagead2.googlesyndication.com^$~object_subrequest,third-party\r\n";
	line += "||pakbanners.com^$third-party\r\n";
	line += "||paperg.com^$third-party\r\n";
	line += "||partner.video.syndication.msn.com^$~object_subrequest,third-party\r\n";
	line += "||partypartners.com^$third-party\r\n";
	line += "||payperpost.com^$third-party\r\n";
	line += "||pc-ads.com^$third-party\r\n";
	line += "||peer39.net^$third-party\r\n";
	line += "||pepperjamnetwork.com^$third-party\r\n";
	line += "||perfb.com^$third-party\r\n";
	line += "||performancingads.com^$third-party\r\n";
	line += "||pgmediaserve.com^$third-party\r\n";
	line += "||pgpartner.com^$third-party\r\n";
	line += "||pheedo.com^$third-party\r\n";
	line += "||picadmedia.com^$third-party\r\n";
	line += "||pinballpublishernetwork.com^$third-party\r\n";
	line += "||pixazza.com^$third-party\r\n";
	line += "||platinumadvertisement.com^$third-party\r\n";
	line += "||playertraffic.com^$third-party\r\n";
	line += "||pmsrvr.com^$third-party\r\n";
	line += "||pntra.com^$third-party\r\n";
	line += "||pntrac.com^$third-party\r\n";
	line += "||pntrs.com^$third-party\r\n";
	line += "||pointroll.com^$third-party\r\n";
	line += "||popads.net^$third-party\r\n";
	line += "||popadscdn.net^$third-party\r\n";
	line += "||ppclinking.com^$third-party\r\n";
	line += "||precisionclick.com^$third-party\r\n";
	line += "||predictad.com^$third-party\r\n";
	line += "||primaryads.com^$third-party\r\n";
	line += "||pro-advertising.com^$third-party\r\n";
	line += "||pro-market.net^$third-party\r\n";
	line += "||proadsdirect.com^$third-party\r\n";
	line += "||probannerswap.com^$third-party\r\n";
	line += "||prod.untd.com^$third-party\r\n";
	line += "||profitpeelers.com^$third-party\r\n";
	line += "||projectwonderful.com^$third-party\r\n";
	line += "||proximic.com^$third-party\r\n";
	line += "||psclicks.com^$third-party\r\n";
	line += "||ptp.lolco.net^$third-party\r\n";
	line += "||pubmatic.com^$third-party\r\n";
	line += "||pulse360.com^$third-party\r\n";
	line += "||qksrv.net^$third-party\r\n";
	line += "||qksz.net^$third-party\r\n";
	line += "||questionmarket.com^$third-party\r\n";
	line += "||questus.com^$third-party\r\n";
	line += "||quisma.com^$third-party\r\n";
	line += "||radiusmarketing.com^$third-party\r\n";
	line += "||rapt.com^$third-party\r\n";
	line += "||rbcdn.com^$third-party\r\n";
	line += "||realclick.co.kr^$third-party\r\n";
	line += "||realmedia.com^$third-party\r\n";
	line += "||reelcentric.com^$third-party\r\n";
	line += "||reklamz.com^$third-party\r\n";
	line += "||resultlinks.com^$third-party\r\n";
	line += "||revenuegiants.com^$third-party\r\n";
	line += "||revfusion.net^$third-party\r\n";
	line += "||revresda.com^$third-party\r\n";
	line += "||ricead.com^$third-party\r\n";
	line += "||ringtonematcher.com^$third-party\r\n";
	line += "||rmxads.com^$third-party\r\n";
	line += "||roirocket.com^$third-party\r\n";
	line += "||rotatingad.com^$third-party\r\n";
	line += "||rovion.com^$third-party\r\n";
	line += "||ru4.com/$third-party\r\n";
	line += "||rubiconproject.com^$third-party\r\n";
	line += "||rwpads.com^$third-party\r\n";
	line += "||sa.entireweb.com^$third-party\r\n";
	line += "||safelistextreme.com^$third-party\r\n";
	line += "||salvador24.com^$third-party\r\n";
	line += "||saple.net^$third-party\r\n";
	line += "||sbaffiliates.com^$third-party\r\n";
	line += "||scanscout.com^$third-party\r\n";
	line += "||search123.uk.com^$third-party\r\n";
	line += "||securewebsiteaccess.com^$third-party\r\n";
	line += "||sendptp.com^$third-party\r\n";
	line += "||servali.net^$third-party\r\n";
	line += "||sev4ifmxa.com^$third-party\r\n";
	line += "||sexmoney.com^$third-party\r\n";
	line += "||shareasale.com^$third-party\r\n";
	line += "||shareresults.com^$third-party\r\n";
	line += "||shinobi.jp^$third-party\r\n";
	line += "||simply.com^$third-party\r\n";
	line += "||siteencore.com^$third-party\r\n";
	line += "||skimlinks.com^$third-party\r\n";
	line += "||skimresources.com^$third-party\r\n";
	line += "||skoovyads.com^$third-party\r\n";
	line += "||smart.allocine.fr$third-party\r\n";
	line += "||smart2.allocine.fr^$third-party\r\n";
	line += "||smartadserver.com^$third-party\r\n";
	line += "||smarttargetting.co.uk^$third-party\r\n";
	line += "||smarttargetting.com^$third-party\r\n";
	line += "||smarttargetting.net^$third-party\r\n";
	line += "||smpgfx.com^$third-party\r\n";
	line += "||snap.com^$third-party\r\n";
	line += "||so-excited.com^$third-party\r\n";
	line += "||sochr.com^$third-party\r\n";
	line += "||sociallypublish.com^$third-party\r\n";
	line += "||socialmedia.com^$third-party\r\n";
	line += "||socialspark.com^$third-party\r\n";
	line += "||softonicads.com^$third-party\r\n";
	line += "||sonnerie.net^$third-party\r\n";
	line += "||sparkstudios.com^$third-party\r\n";
	line += "||specificclick.net^$third-party\r\n";
	line += "||specificmedia.com^$third-party\r\n";
	line += "||speedsuccess.net^$third-party\r\n";
	line += "||spinbox.freedom.com^$third-party\r\n";
	line += "||sponsorads.de^$third-party\r\n";
	line += "||sponsoredtweets.com^$third-party\r\n";
	line += "||sponsormob.com^$third-party\r\n";
	line += "||sponsorpalace.com^$third-party\r\n";
	line += "||sportsyndicator.com^$third-party\r\n";
	line += "||spotrails.com^$third-party\r\n";
	line += "||spottt.com^$third-party\r\n";
	line += "||spotxchange.com^$third-party,domain=~supernovatube.com\r\n";
	line += "||sproose.com^$third-party\r\n";
	line += "||srtk.net^$third-party\r\n";
	line += "||sta-ads.com^$third-party\r\n";
	line += "||starlayer.com^$third-party\r\n";
	line += "||statcamp.net^$third-party\r\n";
	line += "||stocker.bonnint.net^$third-party\r\n";
	line += "||struq.com^$third-party\r\n";
	line += "||sublimemedia.net^$third-party\r\n";
	line += "||supremeadsonline.com^$third-party\r\n";
	line += "||survey-poll.com^$third-party\r\n";
	line += "||tacoda.net^$third-party\r\n";
	line += "||tailsweep.com^$third-party\r\n";
	line += "||targetnet.com^$third-party\r\n";
	line += "||targetpoint.com^$third-party\r\n";
	line += "||targetspot.com^$third-party\r\n";
	line += "||teracent.net^$third-party\r\n";
	line += "||testnet.nl^$third-party\r\n";
	line += "||text-link-ads.com^$third-party\r\n";
	line += "||theloungenet.com^$third-party\r\n";
	line += "||thewebgemnetwork.com^$third-party\r\n";
	line += "||tidaltv.com^$third-party\r\n";
	line += "||tiser.com^$third-party\r\n";
	line += "||tkqlhce.com^$third-party\r\n";
	line += "||topauto10.com^$third-party\r\n";
	line += "||total-media.net^$third-party\r\n";
	line += "||tqlkg.com^$third-party\r\n";
	line += "||tradedoubler.com^$third-party\r\n";
	line += "||tradepub.com^$third-party\r\n";
	line += "||tradetracker.net^$third-party\r\n";
	line += "||trafficbarads.com^$third-party\r\n";
	line += "||trafficjunky.net^$third-party\r\n";
	line += "||trafficmasterz.net^$third-party\r\n";
	line += "||trafficrevenue.net^$third-party\r\n";
	line += "||trafficwave.net^$third-party\r\n";
	line += "||traveladvertising.com^$third-party\r\n";
	line += "||travelscream.com^$third-party\r\n";
	line += "||travidia.com^$third-party\r\n";
	line += "||triadmedianetwork.com^$third-party\r\n";
	line += "||tribalfusion.com^$third-party\r\n";
	line += "||trigami.com^$third-party\r\n";
	line += "||trker.com^$third-party\r\n";
	line += "||tvprocessing.com^$third-party\r\n";
	line += "||twinplan.com^$third-party\r\n";
	line += "||twittad.com^$third-party\r\n";
	line += "||tyroo.com^$third-party\r\n";
	line += "||udmserve.net^$third-party\r\n";
	line += "||ukbanners.com^$third-party\r\n";
	line += "||unanimis.co.uk^$third-party\r\n";
	line += "||unicast.com^$third-party\r\n";
	line += "||unrulymedia.com^$third-party\r\n";
	line += "||usbanners.com^$third-party\r\n";
	line += "||usemax.de^$third-party\r\n";
	line += "||usenetpassport.com^$third-party\r\n";
	line += "||usercash.com^$third-party\r\n";
	line += "||utarget.co.uk^$third-party\r\n";
	line += "||v.movad.de^*/ad.xml$third-party\r\n";
	line += "||validclick.com^$third-party\r\n";
	line += "||valuead.com^$third-party\r\n";
	line += "||valueclick.com^$third-party\r\n";
	line += "||valueclickmedia.com^$third-party\r\n";
	line += "||vcmedia.com^$third-party\r\n";
	line += "||velmedia.net^$third-party\r\n";
	line += "||versetime.com^$third-party\r\n";
	line += "||vianadserver.com^$third-party\r\n";
	line += "||vibrantmedia.com^$third-party\r\n";
	line += "||videoegg.com^$third-party\r\n";
	line += "||videostrip.com^$~object_subrequest,third-party\r\n";
	line += "||videostrip.com^*/admatcherclient.$object_subrequest,third-party\r\n";
	line += "||vidpay.com^$third-party\r\n";
	line += "||viglink.com^$third-party\r\n";
	line += "||vipquesting.com^$third-party\r\n";
	line += "||viraladnetwork.net^$third-party\r\n";
	line += "||visitdetails.com^$third-party\r\n";
	line += "||vitalads.net^$third-party\r\n";
	line += "||vpico.com^$third-party\r\n";
	line += "||vs20060817.com^$third-party\r\n";
	line += "||vsservers.net^$third-party\r\n";
	line += "||webads.co.nz^$third-party\r\n";
	line += "||webgains.com^$third-party\r\n";
	line += "||webmasterplan.com^$third-party\r\n";
	line += "||weborama.fr^$third-party\r\n";
	line += "||webtraffic.ttinet.com^$third-party\r\n";
	line += "||wgreatdream.com^$third-party\r\n";
	line += "||widgetbucks.com^$third-party\r\n";
	line += "||widgets.fccinteractive.com^$third-party\r\n";
	line += "||wootmedia.net^$third-party\r\n";
	line += "||worlddatinghere.com^$third-party\r\n";
	line += "||worthathousandwords.com^$third-party\r\n";
	line += "||wwbn.com^$third-party\r\n";
	line += "||wwwadcntr.com^$third-party\r\n";
	line += "||x4300tiz.com^$third-party\r\n";
	line += "||xcelltech.com^$third-party\r\n";
	line += "||xcelsiusadserver.com^$third-party\r\n";
	line += "||xchangebanners.com^$third-party\r\n";
	line += "||xgraph.net^$third-party\r\n";
	line += "||yceml.net^$third-party\r\n";
	line += "||yesnexus.com^$third-party\r\n";
	line += "||yieldbuild.com^$third-party\r\n";
	line += "||yieldmanager.com^$third-party\r\n";
	line += "||yieldmanager.net^$third-party\r\n";
	line += "||yldmgrimg.net^$third-party\r\n";
	line += "||yottacash.com^$third-party\r\n";
	line += "||yumenetworks.com^$third-party\r\n";
	line += "||zangocash.com^$third-party\r\n";
	line += "||zanox.com^$third-party\r\n";
	line += "||zeads.com^$third-party\r\n";
	line += "||zedo.com^$third-party\r\n";
	line += "||zoomdirect.com.au^$third-party\r\n";
	line += "||zxxds.net^$third-party\r\n";
	line += "!Mobile\r\n";
	line += "||admob.com^$third-party\r\n";
	line += "||adwhirl.com^$third-party\r\n";
	line += "||adzmob.com^$third-party\r\n";
	line += "||amobee.com^$third-party\r\n";
	line += "||mkhoj.com^$third-party\r\n";
	line += "||mojiva.com^$third-party\r\n";
	line += "||smaato.net^$third-party\r\n";
	line += "||waptrick.com^$third-party\r\n";
	line += "!-----------------Third-party adverts-----------------!\r\n";
	line += "! *** easylist_thirdparty.txt ***\r\n";
	line += "||208.43.84.120/trueswordsa3.gif$third-party\r\n";
	line += "||21nova.com/promodisplay?\r\n";
	line += "||770.com/banniere.php?\r\n";
	line += "||a.ucoz.net^\r\n";
	line += "||ablacrack.com/popup-pvd.js$third-party\r\n";
	line += "||adn.ebay.com^\r\n";
	line += "||ads.mp.mydas.mobi^\r\n";
	line += "||adserver-live.yoc.mobi^\r\n";
	line += "||adstil.indiatimes.com^\r\n";
	line += "||adultfriendfinder.com/banners/$third-party\r\n";
	line += "||adultfriendfinder.com/go/page/js_im_box?$third-party\r\n";
	line += "||advanced-intelligence.com/banner\r\n";
	line += "||affil.mupromo.com^\r\n";
	line += "||affiliate.astraweb.com^\r\n";
	line += "||affiliates.a2hosting.com^\r\n";
	line += "||affiliates.bravenet.com^\r\n";
	line += "||affiliates.generatorsoftware.com^\r\n";
	line += "||affiliates.hotelclub.com^\r\n";
	line += "||affiliates.jlist.com^\r\n";
	line += "||affiliates.supergreenhosting.com^\r\n";
	line += "||affiliation.fotovista.com^\r\n";
	line += "||affutdmedia.com^$third-party\r\n";
	line += "||allsend.com/public/assets/images/\r\n";
	line += "||allsolutionsnetwork.com/banners/\r\n";
	line += "||aolcdn.com/os/music/img/*-skin.jpg\r\n";
	line += "||apple.com/itunesaffiliates/\r\n";
	line += "||appwork.org/hoster/banner_$image\r\n";
	line += "||autoprivileges.net/news/\r\n";
	line += "||award.sitekeuring.net^\r\n";
	line += "||aweber.com/banners/\r\n";
	line += "||b.livesport.eu^\r\n";
	line += "||b.sell.com^$third-party\r\n";
	line += "||b92.putniktravel.com^\r\n";
	line += "||b92s.net/images/banners/\r\n";
	line += "||babylon.com/trans_box/*&affiliate=\r\n";
	line += "||banner.1and1.com^\r\n";
	line += "||banner.3ddownloads.com^\r\n";
	line += "||banner.telefragged.com^\r\n";
	line += "||banners.adultfriendfinder.com^$third-party\r\n";
	line += "||banners.cams.com^\r\n";
	line += "||banners.friendfinder.com^\r\n";
	line += "||banners.getiton.com^\r\n";
	line += "||banners.ixitools.com^\r\n";
	line += "||banners.penthouse.com^\r\n";
	line += "||banners.smarttweak.com^\r\n";
	line += "||banners.virtuagirlhd.com^\r\n";
	line += "||bc.coupons.com^$third-party\r\n";
	line += "||bet-at-home.com/oddbanner.aspx?\r\n";
	line += "||beta.down2crazy.com^$third-party\r\n";
	line += "||bigcdn.com^*/adttext.swf\r\n";
	line += "||bijk.com^*/banners/\r\n";
	line += "||bittorrent.am/serws.php?$third-party\r\n";
	line += "||blissful-sin.com/affiliates/\r\n";
	line += "||box.anchorfree.net^\r\n";
	line += "||bplaced.net/pub/\r\n";
	line += "||bravenet.com/cserv.php?\r\n";
	line += "||break.com^*/partnerpublish/\r\n";
	line += "||btguard.com/images/$third-party\r\n";
	line += "||bullguard.com^*/banners/\r\n";
	line += "||buy.com^*/affiliate/\r\n";
	line += "||buzznet.com^*/showpping-banner-$third-party\r\n";
	line += "||cas.clickability.com^\r\n";
	line += "||cash.neweramediaworks.com^\r\n";
	line += "||cashmakingpowersites.com^*/banners/\r\n";
	line += "||cashmyvideo.com/images/cashmyvideo_banner.gif\r\n";
	line += "||cazoz.com/banner.php\r\n";
	line += "||cbanners.virtuagirlhd.com^$third-party\r\n";
	line += "||cdn.sweeva.com/images/$third-party\r\n";
	line += "||challies.com^*/wtsbooks5.png$third-party\r\n";
	line += "||cimg.in/images/banners/\r\n";
	line += "||connect.summit.co.uk^\r\n";
	line += "||counter-strike.com/banners/\r\n";
	line += "||creatives.summitconnect.co.uk^\r\n";
	line += "||dapatwang.com/images/banner/\r\n";
	line += "||datakl.com/banner/\r\n";
	line += "||desi4m.com/desi4m.gif$third-party\r\n";
	line += "||dynw.com/banner\r\n";
	line += "||enticelabs.com/el/\r\n";
	line += "||entrecard.com/static/banners/\r\n";
	line += "||eplreplays.com/wl/\r\n";
	line += "||esport-betting.com^*/betbanner/\r\n";
	line += "||everestpoker.com^*/?adv=\r\n";
	line += "||facebook.com/whitepages/wpminiprofile.php?partner_id=$third-party\r\n";
	line += "||fantaz.com^*/banners/$third-party\r\n";
	line += "||fapturbo.com/testoid/\r\n";
	line += "||farmholidays.is/iframeallfarmsearch.aspx?$third-party\r\n";
	line += "||feedburner.com/~a/\r\n";
	line += "||filedownloader.net/design/$third-party\r\n";
	line += "||filesonic.com^*/banners/\r\n";
	line += "||flipchat.com/index.php?$third-party\r\n";
	line += "||forms.aweber.com/form/styled_popovers_and_lightboxes.js$third-party\r\n";
	line += "||fragfestservers.com/bannerb.gif\r\n";
	line += "||freakshare.net/banner/\r\n";
	line += "||free-football.tv/images/usd/\r\n";
	line += "||frogatto.com/images/$third-party\r\n";
	line += "||frontsight.com^*/banners/\r\n";
	line += "||fugger.netfirms.com/moa.swf$third-party\r\n";
	line += "||futuresite.register.com/us?$third-party\r\n";
	line += "||gamersaloon.com/images/banners/\r\n";
	line += "||gamestop.com^*/aflbanners/\r\n";
	line += "||gawkerassets.com/assets/marquee/$object,third-party\r\n";
	line += "||gfxa.sheetmusicplus.com^$third-party\r\n";
	line += "||ggmania.com^*.jpg$third-party\r\n";
	line += "||giganews.com/banners/$third-party\r\n";
	line += "||gogousenet.com^*/promo.cgi\r\n";
	line += "||googlesyndication.com^*/domainpark.cgi?\r\n";
	line += "||graboid.com/affiliates/\r\n";
	line += "||graduateinjapan.com/affiliates/\r\n";
	line += "||grammar.coursekey.com/inter/$third-party\r\n";
	line += "||gsniper.com/images/$third-party\r\n";
	line += "||hostingcatalog.com/banner.php?\r\n";
	line += "||idg.com.au/ggg/images/*_home.jpg$third-party\r\n";
	line += "||idownloadunlimited.com/aff-exchange/\r\n";
	line += "||ifilm.com/website/*_skin_$third-party\r\n";
	line += "||ign.com/js.ng/\r\n";
	line += "||image.com.com^*/skin2.jpg$third-party\r\n";
	line += "||img.mybet.com^$third-party\r\n";
	line += "||iol.co.za^*/sponsors/\r\n";
	line += "||iselectmedia.com^*/banners/\r\n";
	line += "||jimdo.com/s/img/aff/\r\n";
	line += "||jlist.com/feed.php?affid=$third-party\r\n";
	line += "||joylandcasino.com/promoredirect?$third-party\r\n";
	line += "||justcutegirls.com/banners/$third-party\r\n";
	line += "||kaango.com/fecustomwidgetdisplay?\r\n";
	line += "||kallout.com^*.php?id=\r\n";
	line += "||keyword-winner.com/demo/images/\r\n";
	line += "||krillion.com^*/productoffers.js\r\n";
	line += "||l.yimg.com^*&partner=*&url=\r\n";
	line += "||ladbrokes.com^*&aff_id=\r\n";
	line += "||lastlocation.com/images/banner\r\n";
	line += "||lego.com^*/affiliate/\r\n";
	line += "||letters.coursekey.com/lettertemplates_$third-party\r\n";
	line += "||liutilities.com/partners/affiliate/\r\n";
	line += "||livejasmin.com/?t_id=*&psid=$third-party\r\n";
	line += "||longtailvideo.com/ltas.swf$third-party\r\n";
	line += "||lowbird.com/random/$third-party\r\n";
	line += "||marketing.888.com^\r\n";
	line += "||marketsamurai.com/affiliate/\r\n";
	line += "||mastiway.com/webimages/$third-party\r\n";
	line += "||match.com^*/prm/$third-party\r\n";
	line += "||mazda.com.au/banners/\r\n";
	line += "||media-toolbar.com^$third-party\r\n";
	line += "||media.onlineteachers.co.in^$third-party\r\n";
	line += "||meta4-group.com^*/promotioncorner.js?\r\n";
	line += "||metaboli.fr^*/adgude_$third-party\r\n";
	line += "||mfeed.newzfind.com^$third-party\r\n";
	line += "||missnowmrs.com/images/banners/\r\n";
	line += "||mto.mediatakeout.com^$third-party\r\n";
	line += "||my-dirty-hobby.com/getmdhlink.$third-party\r\n";
	line += "||mydirtyhobby.com/?sub=$third-party\r\n";
	line += "||mydirtyhobby.com/banner/$third-party\r\n";
	line += "||mydirtyhobby.com/custom/$third-party\r\n";
	line += "||mydirtyhobby.com/getmdhlink.$third-party\r\n";
	line += "||mydirtyhobby.com/gpromo/$third-party\r\n";
	line += "||mydirtyhobby.com^*.php?*&goto=join$third-party\r\n";
	line += "||mydirtyhobby.com^*/gpromo/$third-party\r\n";
	line += "||myfreepaysite.info^*.gif$third-party\r\n";
	line += "||myfreeresources.com/getimg.php?$third-party\r\n";
	line += "||myhpf.co.uk/banners/\r\n";
	line += "||mytrafficstrategy.com/images/$third-party\r\n";
	line += "||myusenet.net/promo.cgi?\r\n";
	line += "||netload.in^*?refer_id=\r\n";
	line += "||nzpages.co.nz^*/banners/\r\n";
	line += "||nzphoenix.com/nzgamer/$third-party\r\n";
	line += "||onegameplace.com/iframe.php$third-party\r\n";
	line += "||oriongadgets.com^*/banners/\r\n";
	line += "||partner.bargaindomains.com^\r\n";
	line += "||partner.catchy.com^\r\n";
	line += "||partner.premiumdomains.com^\r\n";
	line += "||partners.agoda.com^\r\n";
	line += "||partners.dogtime.com/network/\r\n";
	line += "||partycasino.com^*?wm=$third-party\r\n";
	line += "||partypoker.com/hp_landingpages/$third-party\r\n";
	line += "||partypoker.com^*?wm=$third-party\r\n";
	line += "||pcash.imlive.com^$third-party\r\n";
	line += "||play-asia.com/paos-$third-party\r\n";
	line += "||pokerstars.com/euro_bnrs/\r\n";
	line += "||pop6.com/banners/\r\n";
	line += "||pornturbo.com/tmarket.php\r\n";
	line += "||ppc-coach.com/jamaffiliates/\r\n";
	line += "||pricegrabber.com/cb_table.php$third-party\r\n";
	line += "||pricegrabber.com/mlink.php?$third-party\r\n";
	line += "||promo.bauermedia.co.uk^\r\n";
	line += "||promos.fling.com^\r\n";
	line += "||promote.pair.com^\r\n";
	line += "||proxies2u.com/images/btn/$third-party\r\n";
	line += "||proxyroll.com/proxybanner.php\r\n";
	line += "||pub.betclick.com^\r\n";
	line += "||pubs.hiddennetwork.com^\r\n";
	line += "||qiksilver.net^*/banners/\r\n";
	line += "||radiocentre.ca/randomimages/$third-party\r\n";
	line += "||radioshack.com^*/promo/\r\n";
	line += "||rapidjazz.com/banner_rotation/\r\n";
	line += "||rcm*.amazon.$third-party\r\n";
	line += "||redvase.bravenet.com^$third-party\r\n";
	line += "||regnow.com/vendor/\r\n";
	line += "||robofish.com/cgi-bin/banner.cgi?\r\n";
	line += "||sayswap.com/banners/\r\n";
	line += "||searchportal.information.com/?$third-party\r\n";
	line += "||secondspin.com/twcontent/\r\n";
	line += "||sfimg.com/images/banners/\r\n";
	line += "||shaadi.com^*/get-banner.php?\r\n";
	line += "||shareflare.net/images/$third-party\r\n";
	line += "||shop-top1000.com/images/\r\n";
	line += "||shop4tech.com^*/banner/\r\n";
	line += "||shragle.com^*?ref=\r\n";
	line += "||singlemuslim.com/affiliates/\r\n";
	line += "||sitegrip.com^*/swagbucks-\r\n";
	line += "||skykingscasino.com/promoloaddisplay?\r\n";
	line += "||slickdeals.meritline.com^\r\n";
	line += "||smartclip.net/delivery/tag?\r\n";
	line += "||smilepk.com/bnrsbtns/\r\n";
	line += "||snapdeal.com^*.php$third-party\r\n";
	line += "||splashpagemaker.com/images/$third-party\r\n";
	line += "||stats.sitesuite.org^\r\n";
	line += "||stockroom.com/banners/\r\n";
	line += "||storage.to/affiliate/\r\n";
	line += "||sweed.to/affiliates/\r\n";
	line += "||sweeva.com/widget.php?w=$third-party\r\n";
	line += "||swiftco.net/banner/\r\n";
	line += "||theatm.info/images/$third-party\r\n";
	line += "||thebigchair.com.au^$subdocument,third-party\r\n";
	line += "||themes420.com/bnrsbtns/\r\n";
	line += "||themis-media.com^*/sponsorships/\r\n";
	line += "||ticketmaster.com/promotionalcontent/\r\n";
	line += "||tigerdirect.com^*/affiliate_\r\n";
	line += "||top5result.com/promo/\r\n";
	line += "||toptenreviews.com/widgets/af_widget.js$third-party\r\n";
	line += "||torrentfreebie.com/index.asp?pid=$third-party\r\n";
	line += "||tosol.co.uk/international.php?$third-party\r\n";
	line += "||toysrus.com/graphics/promo/\r\n";
	line += "||travelmail.traveltek.net^$third-party\r\n";
	line += "||turbotrafficsystem.com^*/banners/\r\n";
	line += "||twivert.com/external/banner234x60.\r\n";
	line += "||u-loader.com/image/hotspot_\r\n";
	line += "||unsereuni.at/resources/img/$third-party\r\n";
	line += "||valuate.com/banners/\r\n";
	line += "||veospot.com^*.html\r\n";
	line += "||videodetective.net/flash/players/plugins/iva_adaptvad.swf\r\n";
	line += "||videoplaza.com/creatives/\r\n";
	line += "||visit.homepagle.com^$third-party\r\n";
	line += "||visitorboost.com/images/$third-party\r\n";
	line += "||website.ws^*/banners/\r\n";
	line += "||williamhill.com/promoloaddisplay?\r\n";
	line += "||williamhillcasino.com/promoredirect?\r\n";
	line += "||wonderlabs.com/affiliate_pro/banners/\r\n";
	line += "||ws.amazon.*/widgets/q?$third-party\r\n";
	line += "||xgaming.com/rotate*.php?$third-party\r\n";
	line += "||xml.exactseek.com/cgi-bin/js-feed.cgi?$third-party\r\n";
	line += "!Preliminary third-party adult section\r\n";
	line += "||awempire.com/ban/$third-party\r\n";
	line += "||hotcaracum.com/banner/$third-party\r\n";
	line += "!Mobile\r\n";
	line += "||iadc.qwapi.com^\r\n";
	line += "!-----------------Specific advert blocking filters-----------------!\r\n";
	line += "! *** easylist_specific_block.txt ***\r\n";
	line += "||1057theoasis.com/addrotate_content.php?\r\n";
	line += "||1079thealternative.com/addrotate_content.php?\r\n";
	line += "||174.143.241.129^$domain=astalavista.com\r\n";
	line += "||1up.com^*/promos/\r\n";
	line += "||216.151.186.5^*/serve.php?$domain=sendspace.com\r\n";
	line += "||4chan.org/support/\r\n";
	line += "||5min.com^*/banners/\r\n";
	line += "||77.247.178.36/layer/$domain=movie2k.com\r\n";
	line += "||84.234.22.104/ads/$domain=tvcatchup.com\r\n";
	line += "||85.17.254.150^*.php?$domain=wiretarget.com\r\n";
	line += "||87.230.102.24/ads/\r\n";
	line += "||87.230.102.24/gads/\r\n";
	line += "||911tabs.com/img/takeover_app_\r\n";
	line += "||911tabs.com^*/ringtones_overlay.js\r\n";
	line += "||963kklz.com/addrotate_content.php?\r\n";
	line += "||9news.com/promo/\r\n";
	line += "||a.giantrealm.com^\r\n";
	line += "||a.thefreedictionary.com^\r\n";
	line += "||a7.org/info_en/\r\n";
	line += "||about.com/0g/$subdocument\r\n";
	line += "||abovetopsecret.com/300_\r\n";
	line += "||ac2.msn.com^\r\n";
	line += "||access.njherald.com^\r\n";
	line += "||activewin.com^*/blaze_static2.gif\r\n";
	line += "||adelaidecityfc.com.au/oak.swf\r\n";
	line += "||adpaths.com/_aspx/cpcinclude.aspx?\r\n";
	line += "||ads.readwriteweb.com^\r\n";
	line += "||adshare.freedocast.com^\r\n";
	line += "||adultswim.com^*/admanager.swf?\r\n";
	line += "||adv.letitbit.net^\r\n";
	line += "||advt.manoramaonline.com^\r\n";
	line += "||akipress.com/_ban/\r\n";
	line += "||akipress.org/ban/\r\n";
	line += "||akipress.org/bimages/\r\n";
	line += "||allmovieportal.com/dynbanner.php?\r\n";
	line += "||allthelyrics.com^*/popup.js\r\n";
	line += "||analytics.mmosite.com^\r\n";
	line += "||androidpit.com/app-seller/app-seller.swf?xmlpath=$object\r\n";
	line += "||anime-source.com/banzai/banner.$subdocument\r\n";
	line += "||animekuro.com/layout/google$subdocument\r\n";
	line += "||animenewsnetwork.com^*.aframe?\r\n";
	line += "||aniscartujo.com^*/layer.js\r\n";
	line += "||anonib.com/zimages/\r\n";
	line += "||anti-leech.com/al.php?\r\n";
	line += "||armorgames.com^*/banners/\r\n";
	line += "||armorgames.com^*/site-skins/\r\n";
	line += "||armorgames.com^*/siteskin.css\r\n";
	line += "||artima.com/zcr/\r\n";
	line += "||asianewsnet.net/banner/\r\n";
	line += "||astalavista.com/avtng/\r\n";
	line += "||athena-ads.wikia.com^\r\n";
	line += "||autosport.com/skinning/\r\n";
	line += "||avaxhome.ws/banners/\r\n";
	line += "||azlyrics.com^*_az.js\r\n";
	line += "||b92.net/images/banners/\r\n";
	line += "||banner.atomicgamer.com^\r\n";
	line += "||banner.itweb.co.za^\r\n";
	line += "||banners.expressindia.com^\r\n";
	line += "||banners.friday-ad.co.uk/hpbanneruploads/$image\r\n";
	line += "||banners.i-comers.com^\r\n";
	line += "||banners.itweb.co.za^\r\n";
	line += "||bbc.co.uk^*/bbccom.js?\r\n";
	line += "||bcdb.com^*/banners.pl?\r\n";
	line += "||beingpc.com^*/banners/\r\n";
	line += "||belfasttelegraph.co.uk/editorial/web/survey/recruit-div-img.js\r\n";
	line += "||bigpoint.com/xml/recommender.swf?\r\n";
	line += "||bigpond.com/home/skin_\r\n";
	line += "||bit-tech.net/images/backgrounds/skin/\r\n";
	line += "||bittorrent.am/banners/\r\n";
	line += "||blackberryforums.net/banners/\r\n";
	line += "||blinkx.com/adhocnetwork/\r\n";
	line += "||blinkx.com/f2/overlays/adhoc\r\n";
	line += "||blogspider.net/images/promo/\r\n";
	line += "||bloomberg.com^*/banner.js\r\n";
	line += "||bnrs.ilm.ee^\r\n";
	line += "||bollywoodbuzz.in^*/728x70.gif\r\n";
	line += "||bookingbuddy.com/js/bookingbuddy.strings.php?$domain=smartertravel.com\r\n";
	line += "||boyplz.com^*/layer.js\r\n";
	line += "||brothersoft.com/softsale/\r\n";
	line += "||brothersoft.com^*/float.js\r\n";
	line += "||browsershots.org/static/images/creative/\r\n";
	line += "||budapesttimes.hu/images/banners/\r\n";
	line += "||burnsoftware.info*/!\r\n";
	line += "||businesstimes.com.sg^*/ad\r\n";
	line += "||bwp.theinsider.com.com^\r\n";
	line += "||c-sharpcorner.com^*/banners/\r\n";
	line += "||c21media.net/uploads/flash/*.swf\r\n";
	line += "||cafimg.com/images/other/\r\n";
	line += "||candystand.com/banners/\r\n";
	line += "||carsguide.com.au^*/marketing/\r\n";
	line += "||cdmediaworld.com*/!\r\n";
	line += "||cdnlayer.com/howtogeek/geekers/up/netshel125x125.gif\r\n";
	line += "||celebjihad.com/widget/widget.js$domain=popbytes.com\r\n";
	line += "||centos.org/donors/\r\n";
	line += "||chapala.com/wwwboard/webboardtop.htm\r\n";
	line += "||china.com^*/googlehead.js\r\n";
	line += "||chinapost.com.tw/ad/\r\n";
	line += "||ciao.co.uk/load_file.php?\r\n";
	line += "||classicfeel.co.za^*/banners/\r\n";
	line += "||click.livedoor.com^\r\n";
	line += "||clk.about.com^\r\n";
	line += "||cms.myspacecdn.com^*/splash_assets/\r\n";
	line += "||codeasily.com^*/codeasily.js\r\n";
	line += "||codeproject.com^*/adm/\r\n";
	line += "||coderanch.com/shingles/\r\n";
	line += "||comm.kino.to^\r\n";
	line += "||comparestoreprices.co.uk/images/promotions/\r\n";
	line += "||complexmedianetwork.com^*/takeovers/\r\n";
	line += "||computerandvideogames.com^*/promos/\r\n";
	line += "||computerworld.com^*/jobroll/\r\n";
	line += "||consumerreports.org^*/sx.js\r\n";
	line += "||countrychannel.tv/telvos_banners/\r\n";
	line += "||covertarget.com^*_*.php\r\n";
	line += "||cpuid.com^*/cpuidbanner72x90_2.\r\n";
	line += "||crazymotion.net/video_*.php?key=\r\n";
	line += "||crushorflush.com/html/promoframe.html\r\n";
	line += "||d-addicts.com^*/banner/\r\n";
	line += "||da.feedsportal.com^\r\n";
	line += "||dads.new.digg.com^\r\n";
	line += "||dailydeals.sfgate.com/widget/\r\n";
	line += "||dailymail.co.uk^*/promoboxes/\r\n";
	line += "||dailymotion.com/images/ie.png\r\n";
	line += "||dailymotion.com/skin/data/default/partner/$~stylesheet\r\n";
	line += "||dailymotion.com^*masscast/\r\n";
	line += "||dailystar.com.lb/bannerin1.htm\r\n";
	line += "||dailystar.com.lb/bottombanner.htm\r\n";
	line += "||dailystar.com.lb/centerbanner.htm\r\n";
	line += "||dailystar.com.lb/googlearticle468.htm\r\n";
	line += "||dailystar.com.lb/leaderboard.htm\r\n";
	line += "||dailystar.com.lb/spcovbannerin.htm\r\n";
	line += "||dailytimes.com.pk/banners/\r\n";
	line += "||dailywritingtips.com^*/money-making.gif\r\n";
	line += "||davesite.com^*/aff/\r\n";
	line += "||dcad.watersoul.com^\r\n";
	line += "||demonoid.com/cached/ab_\r\n";
	line += "||demonoid.com/cached/bnr_\r\n";
	line += "||develop-online.net/static/banners/\r\n";
	line += "||dig.abclocal.go.com/preroll/\r\n";
	line += "||digdug.divxnetworks.com^\r\n";
	line += "||digitaljournal.com/promo/\r\n";
	line += "||digitallook.com^*/rbs-logo-ticker.gif\r\n";
	line += "||digitalreality.co.nz^*/360_hacks_banner.gif\r\n";
	line += "||divxme.com/images/play.png\r\n";
	line += "||divxstage.net/images/download.png\r\n";
	line += "||dl4all.com/data4.files/dpopupwindow.js\r\n";
	line += "||domaining.com/banners/\r\n";
	line += "||dontblockme.modaco.com^$~image\r\n";
	line += "||dvdvideosoft.com^*/banners/\r\n";
	line += "||earthlink.net^*/promos/\r\n";
	line += "||ebayrtm.com/rtm?\r\n";
	line += "||ebuddy.com/banners/\r\n";
	line += "||ebuddy.com/textlink.php?\r\n";
	line += "||ebuddy.com/web_banners/\r\n";
	line += "||ebuddy.com/web_banners_\r\n";
	line += "||ecommerce-journal.com/specdata.php?\r\n";
	line += "||ehow.com/images/brands/\r\n";
	line += "||ekrit.de/serious-gamer/1.swf\r\n";
	line += "||ekrit.de/serious-gamer/film1.swf\r\n";
	line += "||ekrit.de/serious-gamer/images/stories/city-quest.jpg\r\n";
	line += "||el33tonline.com^*/el33t_bg_\r\n";
	line += "||electricenergyonline.com^*/bannieres/\r\n";
	line += "||episodic.com^*/logos/player-\r\n";
	line += "||espn.vad.go.com^$domain=youtube.com\r\n";
	line += "||esus.com/images/regiochat_logo.png\r\n";
	line += "||eurogamer.net/quad.php\r\n";
	line += "||eva.ucas.com^\r\n";
	line += "||eweek.com/images/stories/marketing/\r\n";
	line += "||excite.com/gca_iframe.html?\r\n";
	line += "||expertreviews.co.uk/images/skins/\r\n";
	line += "||expreview.com/exp2/\r\n";
	line += "||fallout3nexus.com^*/300x600.php\r\n";
	line += "||feedsportal.com/creative/\r\n";
	line += "||ffiles.com/counters.js\r\n";
	line += "||fgfx.co.uk/banner.js?\r\n";
	line += "||filebase.to/gfx/*.jpg\r\n";
	line += "||filebase.to/xtend/\r\n";
	line += "||filebase.to^*/note.js\r\n";
	line += "||filefront.com/linkto/\r\n";
	line += "||filespazz.com/imx/template_r2_c3.jpg\r\n";
	line += "||filespazz.com^*/copyartwork_side_banner.gif\r\n";
	line += "||filetarget.com*/!\r\n";
	line += "||filetarget.com^*_*.php\r\n";
	line += "||findfiles.com/images/icatchallfree.png\r\n";
	line += "||findfiles.com/images/knife-dancing-1.gif\r\n";
	line += "||flixstertomatoes.com^*/jquery.js?\r\n";
	line += "||flixstertomatoes.com^*/jquery.rt_scrollmultimedia.js\r\n";
	line += "||flixstertomatoes.com^*/jquery.tooltip.min.js?\r\n";
	line += "||flv.sales.cbs.com^$object_subrequest,domain=cbsnews.com\r\n";
	line += "||flyordie.com/games/free/b/\r\n";
	line += "||fmr.co.za^*/banners/\r\n";
	line += "||fordforums.com.au/banner.swf\r\n";
	line += "||forumimg.ipmart.com/swf/ipmart_forum/banner\r\n";
	line += "||forumw.org/images/uploading.gif\r\n";
	line += "||foxbusiness.com/html/google_homepage_promo\r\n";
	line += "||foxnews1280.com^*/clientgraphics/\r\n";
	line += "||foxradio.com/common/dfpframe.\r\n";
	line += "||foxradio.com/media/module/billboards/\r\n";
	line += "||free-tv-video-online.info/300.html\r\n";
	line += "||free-tv-video-online.info/300s.html\r\n";
	line += "||freemediatv.com/images/inmemoryofmichael.jpg\r\n";
	line += "||freeworldgroup.com/banner\r\n";
	line += "||friday-ad.co.uk/banner.js?\r\n";
	line += "||fudzilla.com^*/banners/\r\n";
	line += "||gamecopyworld.com*/!\r\n";
	line += "||gamemakerblog.com/gma/gatob.php\r\n";
	line += "||gameplanet.co.nz^*-takeover.jpg\r\n";
	line += "||gametrailers.com^*/gt6_siteskin_$stylesheet\r\n";
	line += "||gbrej.com/c/\r\n";
	line += "||geocities.com/js_source/\r\n";
	line += "||geocities.yahoo.*/js/sq.\r\n";
	line += "||getprice.com.au/searchwidget.aspx?$subdocument\r\n";
	line += "||ghacks.net/skin-\r\n";
	line += "||glam.com^*/affiliate/\r\n";
	line += "||goauto.com.au/mellor/mellor.nsf/toy$subdocument\r\n";
	line += "||goodgearguide.com.au/files/skins/\r\n";
	line += "||gowilkes.com/cj/\r\n";
	line += "||gowilkes.com/other/\r\n";
	line += "||grapevine.is/media/flash/*.swf\r\n";
	line += "||guitaretab.com^*/ringtones_overlay.js\r\n";
	line += "||gumtree.com^*/dart_wrapper_\r\n";
	line += "||gwinnettdailypost.com/1.iframe.asp?\r\n";
	line += "||hdtvtest.co.uk^*/pricerunner.php\r\n";
	line += "||helsinkitimes.fi^*/banners/\r\n";
	line += "||holyfragger.com/images/skins/\r\n";
	line += "||horriblesubs.net/playasia*.gif\r\n";
	line += "||horriblesubs.org/playasia*.gif\r\n";
	line += "||hostsearch.com/creative/\r\n";
	line += "||hotfrog.com/adblock.ashx?\r\n";
	line += "||howtogeek.com/go/\r\n";
	line += "||hummy.org.uk^*/brotator/\r\n";
	line += "||i.com.com^*/vendor_bg_\r\n";
	line += "||i.i.com.com/cnwk.1d/*/tt_post_dl.jpg\r\n";
	line += "||i.neoseeker.com/d/$subdocument\r\n";
	line += "||i4u.com/_banner/\r\n";
	line += "||ibanners.empoweredcomms.com.au^\r\n";
	line += "||ibtimes.com/banner/\r\n";
	line += "||ibtimes.com^*/sponsor_\r\n";
	line += "||idg.com.au/images/*_promo$image\r\n";
	line += "||idg.com.au^*_skin.jpg\r\n";
	line += "||ifilm.com/website/*-skin-\r\n";
	line += "||iloveim.com/cadv4.jsp?\r\n";
	line += "||images-amazon.com^*/marqueepushdown/\r\n";
	line += "||imageshack.us/images/contests/*/lp-bg.jpg\r\n";
	line += "||imageshack.us/ym.php?\r\n";
	line += "||img*.i-comers.com^\r\n";
	line += "||impulsedriven.com/app_images/wallpaper/\r\n";
	line += "||independent.co.uk/multimedia/archive/$subdocument\r\n";
	line += "||informationmadness.com^*/banners/\r\n";
	line += "||informer.com/images/sponsored.gif\r\n";
	line += "||infoseek.co.jp/isweb/clip.html\r\n";
	line += "||injpn.net/images/banners/\r\n";
	line += "||insidehw.com/images/banners/\r\n";
	line += "||interfacelift.com/inc_new/$subdocument\r\n";
	line += "||internet.ziffdavis.com^\r\n";
	line += "||iptools.com/sky.php\r\n";
	line += "||isitnormal.com/img/iphone_hp_promo_wide.png\r\n";
	line += "||itpro.co.uk/images/skins/\r\n";
	line += "||itweb.co.za/banners/\r\n";
	line += "||itweb.co.za/logos/\r\n";
	line += "||iwebtool.com^*/bannerview.php\r\n";
	line += "||jame-world.com^*/adv/\r\n";
	line += "||japanvisitor.com/banners/\r\n";
	line += "||jdownloader.org/_media/screenshots/banner.png\r\n";
	line += "||jdownloader.org^*/smbanner.png\r\n";
	line += "||jewlicious.com/banners/\r\n";
	line += "||jewtube.com/banners/\r\n";
	line += "||johnbridge.com/vbulletin/banner_rotate.js\r\n";
	line += "||johnbridge.com/vbulletin/images/tyw/cdlogo-john-bridge.jpg\r\n";
	line += "||johnbridge.com/vbulletin/images/tyw/wedi-shower-systems-solutions.png\r\n";
	line += "||jollo.com/images/travel.gif\r\n";
	line += "||jpost.com/images/*/promos/\r\n";
	line += "||jpost.com/images/2009/newsite/\r\n";
	line += "||kcye.com/addrotate_content.php?\r\n";
	line += "||kdwn.com/addrotate_content.php?\r\n";
	line += "||kermit.macnn.com^\r\n";
	line += "||kestrel.ospreymedialp.com^\r\n";
	line += "||kewlshare.com/reward.html\r\n";
	line += "||kino.to/gr/blob/\r\n";
	line += "||kitz.co.uk/files/jump2/\r\n";
	line += "||kjul1047.com^*/clientgraphics/\r\n";
	line += "||klav1230am.com^*/banners/\r\n";
	line += "||knowfree.net^*/ezm125x125.gif\r\n";
	line += "||krapps.com^*-banner-\r\n";
	line += "||krebsonsecurity.com^*banner.swf?\r\n";
	line += "||kstp.com^*/flexhousepromotions/\r\n";
	line += "||kxlh.com/images/banner/\r\n";
	line += "||kyivpost.com^*/adv_\r\n";
	line += "||kyivpost.com^*/banner/\r\n";
	line += "||labtimes.org/banner/\r\n";
	line += "||lastminute.com^*/universal.html?\r\n";
	line += "||latex-community.org/images/banners/\r\n";
	line += "||lightningcast.net^*/getplaylist?$third-party,domain=reuters.com\r\n";
	line += "||linksafe.info^*/mirror.png\r\n";
	line += "||linuxtopia.org/includes/$subdocument\r\n";
	line += "||livestream.com^*/overlay/\r\n";
	line += "||loaded.it/images/ban*.swf\r\n";
	line += "||loaded.it^*/geld-internet-verdienen.jpg\r\n";
	line += "||loaded.it^*/iframe_vid.\r\n";
	line += "||loaded.it^*/my_banner\r\n";
	line += "||londonstockexchange.com/prices-and-news/*/fx.gif\r\n";
	line += "||looky.hyves.org^\r\n";
	line += "||loveolgy.com/banners/\r\n";
	line += "||lowbird.com/lbpu.php\r\n";
	line += "||lowellsun.com/litebanner/\r\n";
	line += "||lowyat.net/catfish/\r\n";
	line += "||lowyat.net^*/images/header/\r\n";
	line += "||lyricsfreak.com^*/overlay.js\r\n";
	line += "||macmillandictionary.com/info/frame.html?zone=\r\n";
	line += "||macobserver.com/js/givetotmo.js\r\n";
	line += "||macobserver.com^*/deal_brothers/\r\n";
	line += "||macworld.co.uk^*/textdeals/\r\n";
	line += "||madskristensen.net/discount2.js\r\n";
	line += "||mail.google.com/mail/*&view=ad\r\n";
	line += "||majorgeeks.com/aff/\r\n";
	line += "||majorgeeks.com/images/mb-hb-2.jpg\r\n";
	line += "||majorgeeks.com/images/mg120.jpg\r\n";
	line += "||majorgeeks.com^*/banners/\r\n";
	line += "||mangafox.com/media/game321/\r\n";
	line += "||mangaupdates.com/affiliates/\r\n";
	line += "||mani-admin-plugin.com^*/banners/\r\n";
	line += "||mccont.com/sda/\r\n";
	line += "||mccont.com/takeover/\r\n";
	line += "||mcstatic.com^*/billboard_\r\n";
	line += "||medhelp.org/hserver/\r\n";
	line += "||media.abc.go.com^*/callouts/\r\n";
	line += "||media.mtvnservices.com/player/scripts/mtvn_player_control.js$domain=spike.com\r\n";
	line += "||mediafire.com^*/linkto/default-$subdocument\r\n";
	line += "||mediafire.com^*/remove_ads.gif\r\n";
	line += "||mediamgr.ugo.com^\r\n";
	line += "||meetic.com/js/*/site_under_\r\n";
	line += "||megaupload.com/mc.php?\r\n";
	line += "||megavideo.com/goviral.php\r\n";
	line += "||megavideo.com/unruley.php\r\n";
	line += "||merriam-webster.com^*/accipiter.js\r\n";
	line += "||mgnetwork.com/dealtaker/\r\n";
	line += "||mirror.co.uk^*/gutters/\r\n";
	line += "||mirror.co.uk^*/m4_gutters/\r\n";
	line += "||mirror.co.uk^*/m4_partners/\r\n";
	line += "||mirror.co.uk^*/people_promotions/\r\n";
	line += "||mmorpg.com/images/skins/\r\n";
	line += "||mochiads.com/srv/\r\n";
	line += "||movshare.net^*/remove_ads.jpg\r\n";
	line += "||movstreaming.com/images/edhim.jpg\r\n";
	line += "||mp3mediaworld.com*/!\r\n";
	line += "||mp3raid.com/imesh.gif\r\n";
	line += "||msn.com/?adunitid\r\n";
	line += "||musicremedy.com/banner/\r\n";
	line += "||musictarget.com*/!\r\n";
	line += "||myspace.com^*.adtooltip&\r\n";
	line += "||mystream.to^*/button_close.png\r\n";
	line += "||myway.com/gca_iframe.html\r\n";
	line += "||nationalturk.com^*/banner\r\n";
	line += "||naukimg.com/banner/\r\n";
	line += "||nba.com^*/amex_logo\r\n";
	line += "||nba.com^*/steinersports_\r\n";
	line += "||nearlygood.com^*/_aff.php?\r\n";
	line += "||neoseeker.com/a_pane.php\r\n";
	line += "||nerej.com/c/\r\n";
	line += "||newport-county.co.uk/images/general_images/blue_square_update_01.gif\r\n";
	line += "||newport-county.co.uk/images/home_page_images/234x60.gif\r\n";
	line += "||newport-county.co.uk/images/home_page_images/premier_sport_anmin.gif\r\n";
	line += "||news-leader.com^*/banner.js\r\n";
	line += "||news.com.au/news/vodafone/$object\r\n";
	line += "||news.com.au^*-promo$image\r\n";
	line += "||news.com.au^*/promos/\r\n";
	line += "||nirsoft.net/banners/\r\n";
	line += "||ntdtv.com^*/adv/\r\n";
	line += "||ny1.com^*/servecontent.aspx?iframe=\r\n";
	line += "||nyaatorrents.org/images/nw.\r\n";
	line += "||nyaatorrents.org/images/skyscraper.\r\n";
	line += "||nymag.com^*/metrony_\r\n";
	line += "||nyrej.com/c/\r\n";
	line += "||objects.tremormedia.com/embed/swf/bcacudeomodule.swf$domain=radaronline.com\r\n";
	line += "||ocforums.com/adj/\r\n";
	line += "||oldgames.sk/images/topbar/\r\n";
	line += "||osdir.com/ml/$subdocument\r\n";
	line += "||oyetimes.com/join/advertisers.html\r\n";
	line += "||payplay.fm^*/mastercs.js\r\n";
	line += "||pbsrc.com/sponsor/\r\n";
	line += "||pbsrc.com^*/sponsor/\r\n";
	line += "||pcauthority.com.au^*/skins/\r\n";
	line += "||pcpro.co.uk/images/*_siteskin\r\n";
	line += "||pcpro.co.uk/images/skins/\r\n";
	line += "||pcpro.co.uk^*/pcprositeskin\r\n";
	line += "||pcpro.co.uk^*skin_wide.\r\n";
	line += "||pcworld.idg.com.au/files/skins/\r\n";
	line += "||pettube.com/images/*-partner.\r\n";
	line += "||phobos.apple.com^$object_subrequest,domain=dailymotion.com\r\n";
	line += "||photoshopguides.com/banners/\r\n";
	line += "||photosupload.net/photosupload.js\r\n";
	line += "||pinknews.co.uk/newweb/\r\n";
	line += "||pitchero.com^*/toolstation.gif\r\n";
	line += "||popbytes.com/img/*-ad.jpg\r\n";
	line += "||popbytes.com/img/becomeasponsor.gif\r\n";
	line += "||popbytes.com/img/no-phone-zone.gif\r\n";
	line += "||popbytes.com/img/sunset-idle-1.gif\r\n";
	line += "||popbytes.com/img/thinkups-230x115.gif\r\n";
	line += "||popbytes.com/img/visitmysponsors.gif\r\n";
	line += "||prisonplanet.com^*advert.\r\n";
	line += "||prisonplanet.com^*banner\r\n";
	line += "||prisonplanet.com^*sky.\r\n";
	line += "||project-for-sell.com/_google.php\r\n";
	line += "||promo.fileforum.com^\r\n";
	line += "||proxy.org/af.html\r\n";
	line += "||proxy.org/ah.html\r\n";
	line += "||ps3news.com/banner/\r\n";
	line += "||ps3news.com^*.swf\r\n";
	line += "||ps3news.com^*/200x90.jpg\r\n";
	line += "||ps3news.com^*/200x90_\r\n";
	line += "||ps3news.com^*/200x90f.jpg\r\n";
	line += "||ps3news.com^*/global_background_ps3break.jpg\r\n";
	line += "||psx-scene.com^*/cyb_banners/\r\n";
	line += "||psx-scene.com^*/sponsors/\r\n";
	line += "||qrz.com/pix/*.gif\r\n";
	line += "||querverweis.net/pu.js\r\n";
	line += "||quickload.to^*/layer.divx.js\r\n";
	line += "||quickload.to^*/note.divx.js\r\n";
	line += "||quickload.to^*/note.js\r\n";
	line += "||rad.msn.com^\r\n";
	line += "||radiovaticana.org^*/alitalia\r\n";
	line += "||readwriteweb.com^*/clouddownloadvmwarepromo.png\r\n";
	line += "||readwriteweb.com^*/rwcloudlearnmorebutton.png\r\n";
	line += "||rejournal.com/images/banners/\r\n";
	line += "||rejournal.com/users/blinks/\r\n";
	line += "||rejournal.com^*/images/homepage/\r\n";
	line += "||retrevo.com^*/pcwframe.jsp?\r\n";
	line += "||rfu.com/js/jquery.jcarousel.js\r\n";
	line += "||richmedia.yimg.com^\r\n";
	line += "||riderfans.com/other/\r\n";
	line += "||sameip.org/images/froghost.gif\r\n";
	line += "||satelliteguys.us/burst_header_iframe.\r\n";
	line += "||satelliteguys.us/burstbox_iframe.\r\n";
	line += "||satelliteguys.us/burstsky_iframe.\r\n";
	line += "||scenereleases.info/wp-content/*.swf\r\n";
	line += "||sciencedaily.com^*/google-story2-rb.js\r\n";
	line += "||seatguru.com/deals?\r\n";
	line += "||seeingwithsound.com/noad.gif\r\n";
	line += "||sendspace.com/defaults/framer.html?z=\r\n";
	line += "||sendspace.com^*?zone=\r\n";
	line += "||sensongs.com/nfls/\r\n";
	line += "||serialzz.us/ad.js\r\n";
	line += "||sharebeast.com^*/remove_ads.gif\r\n";
	line += "||sharetera.com/images/icon_download.png\r\n";
	line += "||sharetera.com/promo.php?\r\n";
	line += "||shop.com/cc.class/dfp?\r\n";
	line += "||shopping.stylelist.com/widget?\r\n";
	line += "||shoppingpartners2.futurenet.com^\r\n";
	line += "||shops.tgdaily.com^*&widget=\r\n";
	line += "||shortcuts.search.yahoo.com^*&callback=yahoo.shortcuts.utils.setdittoadcontents&\r\n";
	line += "||showstreet.com/banner.\r\n";
	line += "||sify.com^*/gads_\r\n";
	line += "||sk-gaming.com/image/acersocialw.gif\r\n";
	line += "||sk-gaming.com/image/pts/\r\n";
	line += "||sk-gaming.com/www/skdelivery/\r\n";
	line += "||slyck.com/pics/*304x83_\r\n";
	line += "||smh.com.au/images/promo/\r\n";
	line += "||snopes.com^*/casalebox.asp\r\n";
	line += "||snopes.com^*/tribalbox.asp\r\n";
	line += "||soccerlens.com/files1/\r\n";
	line += "||soccerway.com/banners/\r\n";
	line += "||soccerway.com/buttons/120x90_\r\n";
	line += "||soccerway.com/media/img/mybet_banner.gif\r\n";
	line += "||softarchive.net/js/getbanner.php?\r\n";
	line += "||softcab.com/google.php?\r\n";
	line += "||softonic.com/specials_leaderboard/\r\n";
	line += "||soundtracklyrics.net^*_az.js\r\n";
	line += "||space.com/promo/\r\n";
	line += "||sternfannetwork.com/forum/images/banners/\r\n";
	line += "||steroid.com/banner/\r\n";
	line += "||steroid.com/dsoct09.swf\r\n";
	line += "||stlyrics.com^*_az.js\r\n";
	line += "||stuff.tv/client/skinning/\r\n";
	line += "||suntimes.com^*/banners/\r\n";
	line += "||supernovatube.com/spark.html\r\n";
	line += "||sydneyolympicfc.com/admin/media_manager/media/mm_magic_display/$image\r\n";
	line += "||techpowerup.com/images/bnnrs/\r\n";
	line += "||techradar.com^*/img/*_takeover_\r\n";
	line += "||techsupportforum.com^*/banners/\r\n";
	line += "||techtree.com^*/jquery.catfish.js\r\n";
	line += "||teesoft.info/images/uniblue.png\r\n";
	line += "||telegraphindia.com^*/banners/\r\n";
	line += "||telegraphindia.com^*/hoabanner.\r\n";
	line += "||tentonhammer.com^*/takeovers/\r\n";
	line += "||theaquarian.com^*/banners/\r\n";
	line += "||thecorrsmisc.com/10feet_banner.gif\r\n";
	line += "||thecorrsmisc.com/brokenthread.jpg\r\n";
	line += "||thecorrsmisc.com/msb_banner.jpg\r\n";
	line += "||thehighstreetweb.com^*/banners/\r\n";
	line += "||theispguide.com/premiumisp.html\r\n";
	line += "||theispguide.com/topbanner.asp?\r\n";
	line += "||themis-media.com/media/global/images/cskins/\r\n";
	line += "||themis.yahoo.com^\r\n";
	line += "||thepiratebay.org/img/bar.gif\r\n";
	line += "||thewb.com/thewb/swf/tmz-adblock/\r\n";
	line += "||tigerdroppings.com^*&adcode=\r\n";
	line += "||times-herald.com/pubfiles/\r\n";
	line += "||titanbet.com/promoloaddisplay?\r\n";
	line += "||tomsguide.com/*/cdntests_cedexis.php\r\n";
	line += "||torrentfreak.com^*/wyzo.gif\r\n";
	line += "||trackitdown.net/skins/*_campaign/\r\n";
	line += "||tripadvisor.co.uk/adp/\r\n";
	line += "||tripadvisor.com/adp/\r\n";
	line += "||tripadvisor.com^*/skyscraper.jpg\r\n";
	line += "||turbobit.net/js/popunder2.js\r\n";
	line += "||tweaktown.com/cms/includes/i*.php\r\n";
	line += "||typicallyspanish.com/banners/\r\n";
	line += "||ua.badongo.com^\r\n";
	line += "||ubuntugeek.com/images/dnsstock.png\r\n";
	line += "||uimserv.net^\r\n";
	line += "||ultimate-guitar.com^*/takeover/\r\n";
	line += "||uncoached.com/smallpics/ashley\r\n";
	line += "||unicast.ign.com^\r\n";
	line += "||unicast.msn.com^\r\n";
	line += "||universalhub.com/bban/\r\n";
	line += "||videodownloadtoolbar.com/fancybox/\r\n";
	line += "||videogamer.com^*/css/skins/$stylesheet\r\n";
	line += "||videoplaza.com/resources/preroll_interactive/\r\n";
	line += "||videos.mediaite.com/decor/live/white_alpha_60.\r\n";
	line += "||videosift.com/bnr.php?\r\n";
	line += "||videoweed.com^*/remove_ads.png\r\n";
	line += "||videoweed.com^*/stream_movies_hd_button.png\r\n";
	line += "||viewdocsonline.com/images/banners/\r\n";
	line += "||vortez.co.uk^*120x600.swf\r\n";
	line += "||vortez.co.uk^*skyscraper.jpg\r\n";
	line += "||w3schools.com/banners/\r\n";
	line += "||wareseeker.com/banners/\r\n";
	line += "||webhostingtalk.com/js/hail.js\r\n";
	line += "||webnewswire.com/images/banner\r\n";
	line += "||weddingtv.com/src/usefulstuff/*banner\r\n";
	line += "||werlv.com^*banner\r\n";
	line += "||weselectmodels.com^*/new_banner.jpg\r\n";
	line += "||whispersinthecorridors.com/banner2009/\r\n";
	line += "||wikia.com/__varnish_\r\n";
	line += "||windowsitpro.com^*/doubleclick/\r\n";
	line += "||windowsitpro.com^*/googleafc\r\n";
	line += "||windowsitpro.com^*/roadblock.js\r\n";
	line += "||wnst.net/img/coupon/\r\n";
	line += "||wolf-howl.com/wp-content/banners/\r\n";
	line += "||wollongongfc.com.au/images/banners/\r\n";
	line += "||worthdownloading.com/images/mirrors/preview_logo.gif\r\n";
	line += "||wowwiki.com/__varnish_\r\n";
	line += "||www2.sys-con.com^*.cfm\r\n";
	line += "||xbitlabs.com^*/xbanner.php?\r\n";
	line += "||xbox-scene.com/crave/logo_on_white_s160.jpg\r\n";
	line += "||xoops-theme.com/images/banners/\r\n";
	line += "||yahoo.*/serv?s=\r\n";
	line += "||yahoo.com/darla/\r\n";
	line += "||yahoo.com/ysmload.html?\r\n";
	line += "||yahoo.com^*/eyc-themis?\r\n";
	line += "||yfrog.com/images/contests/*/lp-bg.jpg\r\n";
	line += "||yfrog.com/ym.php?\r\n";
	line += "||yimg.com/a/1-$~stylesheet\r\n";
	line += "||yimg.com^*/fairfax/$image\r\n";
	line += "||yimg.com^*/flash/promotions/\r\n";
	line += "||yourmovies.com.au^*/side_panels_\r\n";
	line += "||yourtomtom.com^*/back_adblock.gif\r\n";
	line += "||ytimg.com^*/new_watch_background$domain=youtube.com\r\n";
	line += "||ytimg.com^*_banner$domain=youtube.com\r\n";
	line += "||zam.com/i/promos/*-skin.\r\n";
	line += "||zambiz.co.zm/banners/\r\n";
	line += "||zophar.net/files/tf_\r\n";
	line += "||zurrieqfc.com/images/banners/\r\n";
	line += "!Anti-Adblock\r\n";
	line += "||illimitux.net/js/abp.js\r\n";
	line += "||indieclicktv.com/player/swf/*/icmmva%5eplugin.swf$object_subrequest\r\n";
	line += "!-----------------Specific element hiding rules-----------------!\r\n";
	line += "! *** easylist_specific_hide.txt ***\r\n";
	line += "10minutemail.com###shoutouts\r\n";
	line += "123people.co.uk###l_banner\r\n";
	line += "1911encyclopedia.org##.google_block_style\r\n";
	line += "2gb-hosting.com##div.info[align=\"center\"]\r\n";
	line += "4chan.org###ad\r\n";
	line += "4megaupload.com##table[width=\"100%\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][bgcolor=\"#d8d8d0\"]\r\n";
	line += "4shared.com##.signupbanner\r\n";
	line += "4shared.com##center > img[width=\"13\"][height=\"84\"][style=\"cursor: pointer;\"]\r\n";
	line += "4shared.com##img[alt=\"Remove Ads\"]\r\n";
	line += "6lyrics.com##.ad\r\n";
	line += "7tutorials.com##.block-openx\r\n";
	line += "9news.com##.poster\r\n";
	line += "9to5mac.com###block-dealmac-0\r\n";
	line += "9to5mac.com###page-top\r\n";
	line += "a10.com###gameunderbanner\r\n";
	line += "abc2news.com##.ad\r\n";
	line += "abclocal.go.com###bannerTop\r\n";
	line += "abclocal.go.com##.linksWeLike\r\n";
	line += "abcnews.go.com##.ad\r\n";
	line += "abndigital.com###banner468\r\n";
	line += "abndigital.com###leaderboard_728x90\r\n";
	line += "about.com###adB\r\n";
	line += "about.com##.gB\r\n";
	line += "accuweather.com###googleContainer\r\n";
	line += "achieve360points.com###a1\r\n";
	line += "achieve360points.com###a3\r\n";
	line += "actiontrip.com###banner300\r\n";
	line += "adelaidecityfc.com.au##td[width=\"130\"][valign=\"top\"][align=\"right\"]:last-child > table[width=\"125\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][align=\"right\"]:first-child:last-child\r\n";
	line += "adelaideunited.com.au##.promotion_wrapper\r\n";
	line += "adultswim.com###ad\r\n";
	line += "advocate.com###BottomBanners\r\n";
	line += "advocate.com###TopBanners\r\n";
	line += "afl.com.au##div[style=\"width: 300px; height: 250px;\"]\r\n";
	line += "afro.com###leaderboard\r\n";
	line += "afterdawn.com###dlSoftwareDesc300x250\r\n";
	line += "afterdawn.com##.uniblue\r\n";
	line += "airspacemag.com###top-banners\r\n";
	line += "ajaxian.com###taeheader\r\n";
	line += "akeelwap.net##a[href^=\"http://c.admob.com/\"]\r\n";
	line += "akeelwap.net##a[href^=\"http://click.buzzcity.net/click.php?\"]\r\n";
	line += "akihabaranews.com###bbTop\r\n";
	line += "akihabaranews.com###recSidebar\r\n";
	line += "alarabiya.net###side_banner\r\n";
	line += "allakhazam.com###bannerMain\r\n";
	line += "allakhazam.com###towerRt\r\n";
	line += "allbusiness.com##.search_results\r\n";
	line += "allexperts.com###sl\r\n";
	line += "allmovieportal.com##table[width=\"100%\"][height=\"90\"]\r\n";
	line += "allmusicals.com##img[width=\"190\"]\r\n";
	line += "allshopsuk.co.uk##table[border=\"0\"][align=\"center\"][width=\"100%\"]\r\n";
	line += "allthelyrics.com##div[style=\"padding: 0px 0px 15px;\"]\r\n";
	line += "altavista.com###spons\r\n";
	line += "altavista.com##a[href*=\".overture.com/d/sr/\"]\r\n";
	line += "alternet.org###premium\r\n";
	line += "alternet.org###premium2_container\r\n";
	line += "alternet.org##.premium-container\r\n";
	line += "amazon.co.uk##.bm\r\n";
	line += "amazon.co.uk##.tigerbox\r\n";
	line += "amazon.co.uk##iframe[title=\"Ad\"]\r\n";
	line += "amazon.com##.pa_containerWrapper\r\n";
	line += "amazon.com##iframe[title=\"Ad\"]\r\n";
	line += "america.fm###banbo\r\n";
	line += "ampercent.com###centersidebar\r\n";
	line += "ampercent.com##.titlebelow\r\n";
	line += "anandtech.com##.ad\r\n";
	line += "ancestry.com##.uprPromo\r\n";
	line += "androidpit.com##.boxLightLeft[style=\"width: 620px; text-align: center; font-size: 95%;\"]\r\n";
	line += "anime-planet.com##.medrec\r\n";
	line += "animea.net##.ad\r\n";
	line += "animenewsnetwork.com###page-header-banner\r\n";
	line += "animepaper.net###ifiblockthisthenicheatap\r\n";
	line += "animetake.com##.top-banner\r\n";
	line += "anonymouse.org###mouselayer\r\n";
	line += "ansearch.com.au##.sponsor\r\n";
	line += "answerology.com##.leaderboard\r\n";
	line += "answers.com###radLinks\r\n";
	line += "aol.ca###rA\r\n";
	line += "aol.co.uk###banner\r\n";
	line += "aol.co.uk###rA\r\n";
	line += "aol.co.uk##.sad_cont\r\n";
	line += "aol.co.uk##.sidebarBanner\r\n";
	line += "aol.com###rA\r\n";
	line += "aol.com##.g_slm\r\n";
	line += "aol.com##.gsl\r\n";
	line += "aol.com##.sad_cont\r\n";
	line += "aolnews.com##.fauxArticleIMU\r\n";
	line += "ap.org##td[width=\"160\"]\r\n";
	line += "app.com,argusleader.com,battlecreekenquirer.com,baxterbulletin.com,bucyrustelegraphforum.com,burlingtonfreepress.com,centralohio.com,chillicothegazette.com,cincinnati.com,citizen-times.com,clarionledger.com,coloradoan.com,coshoctontribune.com,courier-journal.com,courierpostonline.com,dailyrecord.com,dailyworld.com,delawareonline.com,delmarvanow.com,democratandchronicle.com,desmoinesregister.com,dnj.com,fdlreporter.com,freep.com,greatfallstribune.com,greenbaypressgazette.com,greenvilleonline.com,guampdn.com,hattiesburgamerican.com,hometownlife.com,honoluluadvertiser.com,htrnews.com,indystar.com,jacksonsun.com,jconline.com,lancastereaglegazette.com,lansingstatejournal.com,livingstondaily.com,lohud.com,mansfieldnewsjournal.com,marionstar.com,marshfieldnewsherald.com,montgomeryadvertiser.com,mycentraljersey.com,mydesert.com,newarkadvocate.com,news-leader.com,news-press.com,newsleader.com,pal-item.com,pnj.com,portclintonnewsherald.com,postcrescent.com,poughkeepsiejournal.com,press-citizen.com,pressconnects.com,rgj.com,sctimes.com,sheboyganpress.com,shreveporttimes.com,stargazette.com,statesmanjournal.com,stevenspointjournal.com,tallahassee.com,tennessean.com,theadvertiser.com,thecalifornian.com,thedailyjournal.com,theithacajournal.com,theleafchronicle.com,thenews-messenger.com,thenewsstar.com,thenorthwestern.com,thespectrum.com,thestarpress.com,thetimesherald.com,thetowntalk.com,visaliatimesdelta.com,wausaudailyherald.com,wisconsinrapidstribune.com,zanesvilletimesrecorder.com##.articleflex-container\r\n";
	line += "app.com,argusleader.com,battlecreekenquirer.com,baxterbulletin.com,bucyrustelegraphforum.com,burlingtonfreepress.com,centralohio.com,chillicothegazette.com,cincinnati.com,citizen-times.com,clarionledger.com,coloradoan.com,coshoctontribune.com,courier-journal.com,courierpostonline.com,dailyrecord.com,dailyworld.com,delawareonline.com,delmarvanow.com,democratandchronicle.com,desmoinesregister.com,dnj.com,fdlreporter.com,freep.com,greatfallstribune.com,greenbaypressgazette.com,greenvilleonline.com,guampdn.com,hattiesburgamerican.com,hometownlife.com,honoluluadvertiser.com,htrnews.com,indystar.com,jacksonsun.com,jconline.com,lancastereaglegazette.com,lansingstatejournal.com,livingstondaily.com,lohud.com,mansfieldnewsjournal.com,marionstar.com,marshfieldnewsherald.com,montgomeryadvertiser.com,mycentraljersey.com,mydesert.com,newarkadvocate.com,news-leader.com,news-press.com,newsleader.com,pal-item.com,pnj.com,portclintonnewsherald.com,postcrescent.com,poughkeepsiejournal.com,press-citizen.com,pressconnects.com,rgj.com,sctimes.com,sheboyganpress.com,shreveporttimes.com,stargazette.com,statesmanjournal.com,stevenspointjournal.com,tallahassee.com,tennessean.com,theadvertiser.com,thecalifornian.com,thedailyjournal.com,theithacajournal.com,theleafchronicle.com,thenews-messenger.com,thenewsstar.com,thenorthwestern.com,thespectrum.com,thestarpress.com,thetimesherald.com,thetowntalk.com,visaliatimesdelta.com,wausaudailyherald.com,wisconsinrapidstribune.com,zanesvilletimesrecorder.com##.leaderboard-container\r\n";
	line += "app.com,argusleader.com,battlecreekenquirer.com,baxterbulletin.com,bucyrustelegraphforum.com,burlingtonfreepress.com,centralohio.com,chillicothegazette.com,cincinnati.com,citizen-times.com,clarionledger.com,coloradoan.com,coshoctontribune.com,courier-journal.com,courierpostonline.com,dailyrecord.com,dailyworld.com,delawareonline.com,delmarvanow.com,democratandchronicle.com,desmoinesregister.com,dnj.com,fdlreporter.com,freep.com,greatfallstribune.com,greenbaypressgazette.com,greenvilleonline.com,guampdn.com,hattiesburgamerican.com,hometownlife.com,honoluluadvertiser.com,htrnews.com,indystar.com,jacksonsun.com,jconline.com,lancastereaglegazette.com,lansingstatejournal.com,livingstondaily.com,lohud.com,mansfieldnewsjournal.com,marionstar.com,marshfieldnewsherald.com,montgomeryadvertiser.com,mycentraljersey.com,mydesert.com,newarkadvocate.com,news-leader.com,news-press.com,newsleader.com,pal-item.com,pnj.com,portclintonnewsherald.com,postcrescent.com,poughkeepsiejournal.com,press-citizen.com,pressconnects.com,rgj.com,sctimes.com,sheboyganpress.com,shreveporttimes.com,stargazette.com,statesmanjournal.com,stevenspointjournal.com,tallahassee.com,tennessean.com,theadvertiser.com,thecalifornian.com,thedailyjournal.com,theithacajournal.com,theleafchronicle.com,thenews-messenger.com,thenewsstar.com,thenorthwestern.com,thespectrum.com,thestarpress.com,thetimesherald.com,thetowntalk.com,visaliatimesdelta.com,wausaudailyherald.com,wisconsinrapidstribune.com,zanesvilletimesrecorder.com##.leaderboard-container-top\r\n";
	line += "appleinsider.com###aadbox\r\n";
	line += "appleinsider.com###ldbd\r\n";
	line += "appleinsider.com##.bottombox\r\n";
	line += "appleinsider.com##.leaderboard\r\n";
	line += "appleinsider.com##.main_box4\r\n";
	line += "appleinsider.com##div[style=\"border: 1px solid rgb(221, 221, 221); width: 498px; height: 250px; font-size: 14px;\"]\r\n";
	line += "appleinsider.com##div[style=\"padding: 10px 0pt; width: auto; height: 60px; margin: 0pt 0pt 0pt 348px;\"]\r\n";
	line += "appleinsider.com##div[style=\"padding: 10px 0pt; width: auto; height: 60px; margin: 0pt 0pt 0pt 348px;\"]\r\n";
	line += "appleinsider.com##img[width=\"300\"][height=\"250\"]\r\n";
	line += "appleinsider.com##td[width=\"150\"][valign=\"top\"]\r\n";
	line += "appleinsider.com##td[width=\"180\"][valign=\"top\"]\r\n";
	line += "aquariumfish.net##table[width=\"440\"][height=\"330\"]\r\n";
	line += "aquariumfish.net##td[align=\"center\"][width=\"100%\"][height=\"100\"]\r\n";
	line += "arabianbusiness.com###banner-container\r\n";
	line += "arabiclookup.com##td[style=\"width: 156px; border-style: solid; text-align: center;\"]\r\n";
	line += "arabiclookup.com##td[style=\"width: 157px; border-style: solid; text-align: left;\"]\r\n";
	line += "armorgames.com###leaderboard\r\n";
	line += "arnnet.com.au###marketplace\r\n";
	line += "arnnet.com.au##.careerone_search\r\n";
	line += "arsenal.com###banner\r\n";
	line += "arstechnica.com###daehtsam-da\r\n";
	line += "artima.com###floatingbox\r\n";
	line += "artima.com###topbanner\r\n";
	line += "arto.com###BannerInfobox\r\n";
	line += "asia.cnet.com###sp-box\r\n";
	line += "asia.cnet.com##.splink\r\n";
	line += "ask.com###rbox\r\n";
	line += "ask.com##.spl_unshd\r\n";
	line += "associatedcontent.com##div[style=\"width: 300px; height: 250px; position: relative;\"]\r\n";
	line += "associatedcontent.com##div[style=\"width: 300px; height: 250px;\"]\r\n";
	line += "asylum.co.uk##.sidebarBanner\r\n";
	line += "asylum.com##.sidebarBanner\r\n";
	line += "asylum.com##.topBanner\r\n";
	line += "atom.com###iframe_container300x250\r\n";
	line += "atomicgamer.com###bannerFeatures\r\n";
	line += "au.movies.yahoo.com##table.y7mv-wraptable[width=\"750\"][height=\"112\"]\r\n";
	line += "au.yahoo.com###y708-windowshade\r\n";
	line += "audioreview.com##.MiddleTableRightColumn\r\n";
	line += "audioreview.com##script + table[width=\"539\"]\r\n";
	line += "audioreview.com##table[width=\"300\"][style=\"border: 1px solid rgb(65, 103, 122); margin-left: 10px;\"]\r\n";
	line += "autoblog.com##.leader\r\n";
	line += "autoblog.com##.medrect\r\n";
	line += "autoblog.com##.topleader\r\n";
	line += "autobloggreen.com##.medrect\r\n";
	line += "autonews.com##div[style=\"width: 300px; height: 128px; margin-bottom: 5px; border-top: 2px solid rgb(236, 236, 236); border-bottom: 2px solid rgb(236, 236, 236); padding-top: 3px; font-family: arial,helvetica; font-size: 10px; text-align: center;\"]\r\n";
	line += "autonewseurope.com###header_bottom\r\n";
	line += "autonewseurope.com##div[style=\"width: 300px; height: 128px; margin-bottom: 5px; border-top: 2px solid rgb(236, 236, 236); border-bottom: 2px solid rgb(236, 236, 236); padding-top: 3px; font-family: arial,helvetica; font-size: 10px; text-align: center;\"]\r\n";
	line += "autosport.com##.content[width] td[height=\"17\"][bgcolor=\"#dcdcdc\"]\r\n";
	line += "autosport.com##td[align=\"center\"][valign=\"top\"][height=\"266\"][bgcolor=\"#dcdcdc\"]\r\n";
	line += "autotrader.co.uk###placeholderTopLeaderboard\r\n";
	line += "avaxsearch.com###bottom_block\r\n";
	line += "avaxsearch.com###top_block\r\n";
	line += "avsforum.com##td[width=\"125\"][valign=\"top\"][style=\"padding-left: 15px;\"]\r\n";
	line += "avsforum.com##td[width=\"193\"][valign=\"top\"]\r\n";
	line += "avsforum.com##td[width=\"300\"][valign=\"top\"][rowspan=\"3\"]\r\n";
	line += "awfulplasticsurgery.com##a[href=\"http://www.blogads.com\"]\r\n";
	line += "awfulplasticsurgery.com##a[href^=\"http://www.freeipadoffer.com/default.aspx?r=\"]\r\n";
	line += "azarask.in##.ad\r\n";
	line += "azstarnet.com##.bannerinstory\r\n";
	line += "babble.com.au###leaderboard-bottom\r\n";
	line += "babble.com.au###leaderboard-top\r\n";
	line += "babble.com.au###medium-rectangle\r\n";
	line += "babelfish.yahoo.com##.ovt\r\n";
	line += "babynamegenie.com##.promo\r\n";
	line += "bangkokpost.com##.boomboxSize1\r\n";
	line += "bangkokpost.com##.buzzBoombox\r\n";
	line += "basketball.com##td[width=\"530\"] + td[width=\"120\"]\r\n";
	line += "battellemedia.com##.sidebar\r\n";
	line += "bbc.co.uk##.bbccom_display_none\r\n";
	line += "bdnews24.com###bannerdiv2\r\n";
	line += "bdnews24.com##.add\r\n";
	line += "bebo.com##.spon-mod\r\n";
	line += "bebo.com##table[style=\"background-color: rgb(247, 246, 246);\"]\r\n";
	line += "belfasttelegraph.co.uk###yahooLinks\r\n";
	line += "belfasttelegraph.co.uk##.googleThird\r\n";
	line += "belfasttelegraph.co.uk##table[width=\"300\"][height=\"250\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"]\r\n";
	line += "bellinghamherald.com###mastBanner\r\n";
	line += "bestbuy.com###dart-container-728x90\r\n";
	line += "betterpropaganda.com##div[style=\"width: 848px; height: 91px; margin: 0pt; position: relative;\"]\r\n";
	line += "bigblueball.com###text-384255551\r\n";
	line += "bigdownload.com##.quigo\r\n";
	line += "bigpond.com###header_banner\r\n";
	line += "bikeradar.com###shopping_partner_box_fat\r\n";
	line += "bingo-hunter.com##img[width=\"250\"][height=\"250\"]\r\n";
	line += "birminghampost.net##.promotop\r\n";
	line += "bit-tech.net##div[style=\"width: 728px; height: 90px;\"]\r\n";
	line += "biz.yahoo.com##table[bgcolor=\"white\"][width=\"100%\"]\r\n";
	line += "bizrate.com###banner_top\r\n";
	line += "bizrate.com###rectangular\r\n";
	line += "blackberryforums.com##td[align=\"left\"][width=\"160\"][valign=\"top\"]\r\n";
	line += "blackberryforums.com.au##td[valign=\"top\"][style=\"width: 175px;\"]\r\n";
	line += "blackmesasource.com##.ad\r\n";
	line += "block.opendns.com###acbox\r\n";
	line += "bloggingbuyouts.com##.topleader\r\n";
	line += "bloggingstocks.com##.topleader\r\n";
	line += "blogoscoped.com##.adBlock\r\n";
	line += "blogoscoped.com##.adBlockBottom\r\n";
	line += "blogoscoped.com##.adBlockBottomBreak\r\n";
	line += "blogtv.com##div[style=\"width: 752px; height: 115px; padding-top: 5px; overflow: hidden;\"]\r\n";
	line += "blogtv.com##div[style=\"width: 752px; top: 5px; height: 100px; text-align: center; padding-top: 5px;\"]\r\n";
	line += "blogtv.com##div[style=\"width: 990px; height: 115px; padding-top: 5px; overflow: hidden;\"]\r\n";
	line += "blogtv.com##div[style=\"width: 990px; top: 5px; height: 100px; text-align: center; padding-top: 5px;\"]\r\n";
	line += "bloomberg.com##.leaderboard\r\n";
	line += "blurtit.com##.adblock\r\n";
	line += "boingboing.net###cheetos_collapsed\r\n";
	line += "boingboing.net##.ad\r\n";
	line += "boingboing.net##div[style=\"height: 630px; width: 300px;\"]\r\n";
	line += "bollywoodbuzz.in##div[style=\"height: 250px; width: 300px;\"]\r\n";
	line += "bollywoodbuzz.in##div[style=\"height: 90px; width: 728px;\"]\r\n";
	line += "books.google.ca###rhswrapper font[size=\"-1\"]\r\n";
	line += "books.google.co.nz###rhswrapper font[size=\"-1\"]\r\n";
	line += "books.google.co.uk###rhswrapper font[size=\"-1\"]\r\n";
	line += "books.google.co.za###rhswrapper font[size=\"-1\"]\r\n";
	line += "books.google.com###rhswrapper font[size=\"-1\"]\r\n";
	line += "books.google.com.au###rhswrapper font[size=\"-1\"]\r\n";
	line += "booookmark.com###sitematches\r\n";
	line += "boston.com###externalBanner\r\n";
	line += "bostonherald.com##div[style=\"position: relative; margin-bottom: 16px; background-color: rgb(233, 233, 233); border-left: 16px solid rgb(23, 23, 23); padding: 20px 12px 20px 20px; clear: both;\"]\r\n";
	line += "brandsoftheworld.com###leaderboardTop\r\n";
	line += "break.com##.ad\r\n";
	line += "break.com##.breaking_news\r\n";
	line += "breakingviews.com###floatit\r\n";
	line += "breitbart.com##.sidebar\r\n";
	line += "briefmobile.com##td[style=\"height: 90px; width: 960px; background-color: rgb(255, 255, 255); padding: 10px; vertical-align: middle;\"]\r\n";
	line += "brighouseecho.co.uk###banner01\r\n";
	line += "brisbaneroar.com.au##.promotion_wrapper\r\n";
	line += "brisbanetimes.com.au##.ad\r\n";
	line += "broadbandreports.com##td[width=\"125\"][style=\"border-right: 1px solid rgb(204, 204, 204);\"]\r\n";
	line += "broadcastnewsroom.com###shopperartbox\r\n";
	line += "broadcastnewsroom.com##.bfua\r\n";
	line += "broadcastnewsroom.com##.bottombanner\r\n";
	line += "brothersoft.com##.sponsor\r\n";
	line += "btjunkie.org###main > div[height=\"10\"]:first-child + table[width=\"100%\"]\r\n";
	line += "btjunkie.org###main > div[height=\"10\"]:first-child + table[width=\"100%\"] + .tab_results\r\n";
	line += "btjunkie.org##th[align=\"left\"][height=\"100%\"]\r\n";
	line += "buenosairesherald.com###publiTopHeader\r\n";
	line += "buffalonews.com###bot-main\r\n";
	line += "buffalonews.com##.leaderboard_top\r\n";
	line += "builderau.com.au###leaderboard\r\n";
	line += "bunalti.com##img[width=\"728\"][height=\"90\"]\r\n";
	line += "business.com###railFls\r\n";
	line += "business.com###sponsoredwellcontainerbottom\r\n";
	line += "business.com###sponsoredwellcontainertop\r\n";
	line += "business.com##.wellFls\r\n";
	line += "businessdailyafrica.com##.c15r\r\n";
	line += "businessdictionary.com###topBnr\r\n";
	line += "businessinsider.com###FM1\r\n";
	line += "businessinsurance.com##.StoryInsert\r\n";
	line += "businesstimes.com.sg##td[bgcolor=\"#333333\"]\r\n";
	line += "businessweek.com###bwMall\r\n";
	line += "businessweek.com##.ad\r\n";
	line += "buxtonadvertiser.co.uk###banner01\r\n";
	line += "buzzfocus.com###eyebrowtop\r\n";
	line += "buzznet.com###topSection\r\n";
	line += "c21media.net##table[border=\"0\"][width=\"130\"]\r\n";
	line += "caller.com###content_match_wrapper\r\n";
	line += "caller.com##.bigbox_wrapper\r\n";
	line += "campustechnology.com###leaderboard\r\n";
	line += "candystand.com##.cs_square_banner\r\n";
	line += "candystand.com##.cs_wide_banner\r\n";
	line += "canmag.com##td[align=\"center\"][height=\"278\"]\r\n";
	line += "canoe.ca###commerce\r\n";
	line += "canoe.ca###subbanner\r\n";
	line += "cantbeunseen.com###top-leaderboard\r\n";
	line += "cantbeunseen.com##.leaderboard\r\n";
	line += "caranddriver.com##.shopping-tools\r\n";
	line += "carrentals.co.uk##div[style=\"float: right; width: 220px; height: 220px;\"]\r\n";
	line += "carsguide.com.au##.CG_loancalculator\r\n";
	line += "cataloguecity.co.uk##.bordered\r\n";
	line += "catholicnewsagency.com##div[style=\"background-color: rgb(247, 247, 247); width: 256px; height: 250px;\"]\r\n";
	line += "caymannewsservice.com###content-top\r\n";
	line += "caymannewsservice.com##[style=\"width: 175px; height: 200px;\"]\r\n";
	line += "caymannewsservice.com##[style=\"width: 450px; height: 100px;\"]\r\n";
	line += "caymannewsservice.com##[style=\"width: 550px; height: 100px;\"]\r\n";
	line += "cboe.com###simplemodal-overlay\r\n";
	line += "cbs5.com##.cbstv_partners_wrap\r\n";
	line += "cbsnews.com##.searchSponsoredResultsList\r\n";
	line += "cbssports.com###leaderboardRow\r\n";
	line += "cbssports.com##table[cellpadding=\"0\"][width=\"310\"]\r\n";
	line += "ccfcforum.com##.tablepad\r\n";
	line += "ccfcforum.com##img[alt=\"ISS\"]\r\n";
	line += "ccmariners.com.au##.promotion_wrapper\r\n";
	line += "celebnipslipblog.com###HeaderBanner\r\n";
	line += "celebuzz.com###bmSuperheader\r\n";
	line += "cell.com###main_banner\r\n";
	line += "cgenie.com###ja-banner\r\n";
	line += "cgenie.com##.cgenie_banner4\r\n";
	line += "chacha.com##.show-me-the-money\r\n";
	line += "chairmanlol.com###top-leaderboard\r\n";
	line += "chairmanlol.com##.leaderboard\r\n";
	line += "chami.com##.c\r\n";
	line += "channel3000.com###leaderboard-sticky\r\n";
	line += "checkoutmyink.com###centerbanner\r\n";
	line += "checkoutmyink.com###mid\r\n";
	line += "checkthisvid.com##a[href^=\"http://links.verotel.com/\"]\r\n";
	line += "chicagobreakingbusiness.com##.ad\r\n";
	line += "chicagotribune.com###story-body-parent + .rail\r\n";
	line += "chinadaily.com.cn##table[width=\"130\"][height=\"130\"]\r\n";
	line += "chinapost.com.tw###winner\r\n";
	line += "chinasmack.com##.ad\r\n";
	line += "chinatechnews.com###banner1\r\n";
	line += "christianitytoday.com##.bgbanner\r\n";
	line += "christianitytoday.com##.bgshop\r\n";
	line += "chrome-hacks.net##div[style=\"width: 600px; height: 250px;\"]\r\n";
	line += "chronicle.northcoastnow.com##table[width=\"100%\"][height=\"90\"][bgcolor=\"#236aa7\"]\r\n";
	line += "cio.com.au##.careerone_search\r\n";
	line += "cio.com.au##.careerone_tj_box\r\n";
	line += "citynews.ca###SuperBannerContainer\r\n";
	line += "citynews.ca##.Box.BigBox\r\n";
	line += "citypaper.com###topLeaderboard\r\n";
	line += "citypaper.com##div[style=\"display: block; width: 980px; height: 120px;\"]\r\n";
	line += "classifieds.aol.co.uk###dmn_results\r\n";
	line += "classifieds.aol.co.uk###dmn_results1\r\n";
	line += "clubwebsite.co.uk##td[width=\"158\"][valign=\"top\"] > start_lspl_exclude > end_lspl_exclude > .boxpadbot[width=\"100%\"][cellspacing=\"0\"][cellpadding=\"6\"][border=\"0\"][style=\"background-color: rgb(0, 51, 0);\"]:last-child\r\n";
	line += "cnbc.com##.fL[style=\"width: 185px; height: 40px; margin: 10px 0pt 0pt 25px; float: none;\"]\r\n";
	line += "cnbc.com##.fL[style=\"width: 365px; margin-bottom: 20px; margin-top: 0px; padding-top: 0px; padding-left: 25px; padding-bottom: 100px; border-top: 1px solid rgb(204, 204, 204); border-left: 1px solid rgb(204, 204, 204);\"]\r\n";
	line += "cnbc.com##.fL[style=\"width: 960px; height: 90px; margin: 0pt 0pt 5px;\"]\r\n";
	line += "cnet.com##.ad\r\n";
	line += "cnet.com##.bidwar\r\n";
	line += "cnet.com.au##.ad\r\n";
	line += "cnet.com.au##.explain-promo\r\n";
	line += "cnmnewsnetwork.com###rightcol\r\n";
	line += "cnn.com##.cnnSearchSponsorBox\r\n";
	line += "cnsnews.com###ctl00_leaderboard\r\n";
	line += "cocoia.com###ad\r\n";
	line += "codeasily.com##.money\r\n";
	line += "codinghorror.com##.welovecodinghorror\r\n";
	line += "coffeegeek.com##img[width=\"200\"][height=\"250\"]\r\n";
	line += "coffeegeek.com##img[width=\"200\"][height=\"90\"]\r\n";
	line += "coffeegeek.com##td[align=\"center\"][width=\"100%\"][valign=\"middle\"]\r\n";
	line += "coldfusion.sys-con.com###header-title\r\n";
	line += "collegehumor.com##.partner_links\r\n";
	line += "columbiatribune.com##.ad\r\n";
	line += "columbiatribune.com##.skyscraper\r\n";
	line += "com.au##table.fdMember\r\n";
	line += "comcast.net##.ad\r\n";
	line += "comedy.com###hat\r\n";
	line += "comicartfans.com###contentcolumn:last-child > center > table[cellspacing=\"0\"][cellpadding=\"1\"][border=\"0\"]:first-child\r\n";
	line += "comicgenesis.com###ks_da\r\n";
	line += "comicsalliance.com##.sidebarBanner\r\n";
	line += "comicsalliance.com##.topBanner\r\n";
	line += "comingsoon.net###col2TopPub\r\n";
	line += "comingsoon.net###upperPub\r\n";
	line += "complex.com##.ad\r\n";
	line += "complex.com##div[style=\"float: left; position: relative; margin: 5px auto; width: 960px; height: 90px; border: 0px solid rgb(0, 0, 0); text-align: center;\"]\r\n";
	line += "computeractive.co.uk##.leaderboard\r\n";
	line += "computerandvideogames.com###skyslot\r\n";
	line += "computerweekly.com##.sponsors\r\n";
	line += "computerworld.com##table[align=\"center\"][width=\"336\"][valign=\"top\"]\r\n";
	line += "computerworld.com##table[width=\"342\"][height=\"290\"][bgcolor=\"#bbbbbb\"]\r\n";
	line += "computerworlduk.com###bottomBanner\r\n";
	line += "computerworlduk.com###topBanner\r\n";
	line += "computing.co.uk##.leaderboard\r\n";
	line += "computing.net###top_banner\r\n";
	line += "computingondemand.com###sub-nav\r\n";
	line += "cookingforengineers.com##div[style=\"border: 0px solid rgb(255, 255, 160); width: 160px; height: 600px;\"]\r\n";
	line += "cookingforengineers.com##div[style=\"border: 0px solid rgb(255, 255, 160); width: 728px; height: 90px; margin: 0pt auto;\"]\r\n";
	line += "cookingforengineers.com##div[style=\"height: 60px; width: 120px; margin: 0pt 20px 5px;\"]\r\n";
	line += "coolest-gadgets.com##.banner1\r\n";
	line += "coolest-gadgets.com##.contentbox\r\n";
	line += "coolest-gadgets.com##.contentboxred\r\n";
	line += "core77.com###rsDesignDir\r\n";
	line += "countryliving.com###sub_promo\r\n";
	line += "cpu-world.com##table[width=\"760\"][style=\"border: 1px solid rgb(64, 64, 64);\"]\r\n";
	line += "cracked.com##.Ad\r\n";
	line += "crackserver.com##input[onclick^=\"window.open('http://www.friendlyduck.com/AF_\"]\r\n";
	line += "crazymotion.net###fadeinbox\r\n";
	line += "crazymotion.net##[style=\"margin: 10px auto 0pt; width: 875px;\"]\r\n";
	line += "cricbuzz.com###chrome_home_banner\r\n";
	line += "cricbuzz.com###tata_phton_home_banner\r\n";
	line += "cricinfo.com##.ciHomeSponcerLink\r\n";
	line += "cricinfo.com##.hpSpncrHead\r\n";
	line += "cricinfo.com##.seatwaveM\r\n";
	line += "cricinfo.com##.seriesSpncr\r\n";
	line += "cricvid.info###bannerfloat2\r\n";
	line += "crikey.com.au###top\r\n";
	line += "crikey.com.au##.crikey_widget_small_island\r\n";
	line += "crmbuyer.com##.content-block-slinks\r\n";
	line += "crmbuyer.com##.content-tab-slinks\r\n";
	line += "crn.com###channelwebmarketplacewrapper\r\n";
	line += "crooksandliars.com###block-clam-1\r\n";
	line += "crooksandliars.com###block-clam-3\r\n";
	line += "crooksandliars.com###block-clam-7\r\n";
	line += "crunchgear.com##.ad\r\n";
	line += "crunchyroll.com##.anime-mrec\r\n";
	line += "crunchyroll.com##a[href^=\"http://clk.atdmt.com/\"]\r\n";
	line += "cubeecraft.com###leaderboard\r\n";
	line += "cultofmac.com###skyBlock\r\n";
	line += "cyberciti.biz###leaderboard\r\n";
	line += "cynagames.com##li[style=\"width: 25%; margin: 0pt; clear: none; padding: 0pt; float: left; display: block;\"]\r\n";
	line += "dailyblogtips.com##img[border=\"0\"]\r\n";
	line += "dailyblogtips.com##img[style=\"margin-right: 16px;\"]\r\n";
	line += "dailyblogtips.com##img[width=\"125\"][height=\"125\"]\r\n";
	line += "dailyfinance.com##div[style=\"background: url(\\\"http://o.aolcdn.com/art/ch_pf/advertisement-text\\\") no-repeat scroll 295px 90px rgb(240, 240, 240); padding-top: 20px; margin: 0pt 0pt 10px; height: 84px;\"]\r\n";
	line += "dailyfreegames.com###banner_886x40\r\n";
	line += "dailyfreegames.com###topratedgames\r\n";
	line += "dailyhaha.com###sponheader\r\n";
	line += "dailymail.co.uk##.classified-list\r\n";
	line += "dailymotion.com##.dmpi_masscast\r\n";
	line += "dailymotion.com##.dmpi_subheader\r\n";
	line += "dailymotion.com##.ie_download\r\n";
	line += "dailymotion.com##.masscast_box_Middle\r\n";
	line += "dailystar.co.uk###hugebanner\r\n";
	line += "dailystar.co.uk##.greyPanelOuter\r\n";
	line += "dailystar.co.uk##.greyPanelOuterSmall\r\n";
	line += "dailystar.co.uk##div[style=\"width: 165px; text-align: center; border: 1px solid rgb(184, 184, 184);\"]\r\n";
	line += "dailystar.co.uk##div[style=\"width: 300px; height: 250px; background: url(\\\"http://cdn.images.dailystar-uk.co.uk/img/adverts/mpufail.gif\\\") repeat scroll 0% 0% transparent;\"]\r\n";
	line += "dancingwhilewhite.com##.add\r\n";
	line += "daniweb.com###textsponsor\r\n";
	line += "daparto.de###leaderboard\r\n";
	line += "daringfireball.net###SidebarTheDeck\r\n";
	line += "darkhorizons.com###content-island\r\n";
	line += "dataopedia.com##.container_banner\r\n";
	line += "deadspin.com###skyscraper\r\n";
	line += "dealbrothers.com##.specials\r\n";
	line += "dealmac.com###banner-bottom\r\n";
	line += "dealnews.com##.banner\r\n";
	line += "deargirlsaboveme.com##.ad\r\n";
	line += "deditv.com##.overlayVid\r\n";
	line += "deletedspam.blogspot.com##.LinkList\r\n";
	line += "deletedspam.blogspot.com##img[width=\"125\"][height=\"125\"]\r\n";
	line += "delicious.com###spns\r\n";
	line += "deliciousdays.com###adlove\r\n";
	line += "deliciousdays.com###book\r\n";
	line += "deliciousdays.com###recipeshelf\r\n";
	line += "demogeek.com##div[style=\"height: 250px; width: 250px; margin: 10px;\"]\r\n";
	line += "demogeek.com##div[style=\"height: 280px; width: 336px; margin: 10px;\"]\r\n";
	line += "demonoid.com##.pad9px_right\r\n";
	line += "denofgeek.com##.skyright\r\n";
	line += "depositfiles.com###adv_banner_sidebar\r\n";
	line += "derbyshiretimes.co.uk###banner01\r\n";
	line += "derbyshiretimes.co.uk##.roundedboxesgoogle\r\n";
	line += "deseretnews.com##.continue\r\n";
	line += "deskbeauty.com##tr > td[width=\"100%\"][height=\"95\"][align=\"center\"]\r\n";
	line += "destructoid.com##div[style=\"overflow: hidden; width: 300px; height: 250px;\"]\r\n";
	line += "develop-online.net##.newsinsert\r\n";
	line += "deviantart.com###overhead-you-know-what\r\n";
	line += "deviantart.com##.ad-blocking-makes-fella-confused\r\n";
	line += "deviantart.com##.hidoframe\r\n";
	line += "deviantart.com##.sleekadbubble\r\n";
	line += "deviantart.com##.subbyCloseX\r\n";
	line += "deviantart.com##a[href^=\"http://advertising.deviantart.com/\"]\r\n";
	line += "deviantart.com##div[gmi-name=\"ad_zone\"]\r\n";
	line += "deviantart.com##div[style=\"float: right; position: relative; width: 410px; text-align: left;\"]\r\n";
	line += "devx.com##.expwhitebox > table[cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][align=\"center\"][style=\"margin-left: 0pt; margin-bottom: 0pt;\"]:last-child\r\n";
	line += "devx.com##.expwhitebox[style=\"border: 0px none;\"] > table[cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][align=\"right\"]:first-child\r\n";
	line += "devx.com##div[align=\"center\"][style=\"margin-top: 0px; margin-bottom: 0px; width: 100%;\"]\r\n";
	line += "devx.com##div[style=\"margin: 20px auto auto;\"] > div[align=\"center\"][style=\"margin-top: 0px; margin-bottom: 0px; width: 100%; padding: 10px;\"]\r\n";
	line += "devx.com##div[style=\"margin: 20px auto auto;\"] > table[align=\"center\"][style=\"border: 2px solid rgb(255, 102, 0); padding-right: 2px; width: 444px; background-color: rgb(255, 255, 255); text-align: left;\"]\r\n";
	line += "devx.com##table[width=\"164\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][style=\"margin-bottom: 5px;\"]:first-child:last-child\r\n";
	line += "dgvid.com##.overlayVid\r\n";
	line += "dickens-literature.com##td[width=\"7%\"][valign=\"top\"]\r\n";
	line += "dictionary.co.uk##table[border=\"0\"][width=\"570\"]\r\n";
	line += "didyouwatchporn.com###bottomBanner\r\n";
	line += "didyouwatchporn.com###recos_box\r\n";
	line += "digitalspy.co.uk##.marketing_puff\r\n";
	line += "dir.yahoo.com##td[width=\"215\"]\r\n";
	line += "discountvouchers.co.uk##a[rel=\"nofollow\"]\r\n";
	line += "discovery.com##.rectangle\r\n";
	line += "dishusa.net##div[style=\"border: 1px dotted rgb(190, 190, 190); background-color: rgb(255, 255, 224); padding: 5px;\"]\r\n";
	line += "dishusa.net##table[style=\"border: 3px outset rgb(87, 173, 198); font-size: 16px; background-color: rgb(253, 252, 240); margin-bottom: 10px;\"]\r\n";
	line += "disney.go.com###banner\r\n";
	line += "disney.go.com###superBanner\r\n";
	line += "disney.go.com##div[style=\"position: relative; float: right; clear: right; width: 300px; height: 260px; top: 5px; margin: 10px 0px 5px 5px;\"]\r\n";
	line += "divxden.com###divxshowboxt > a[target=\"_blank\"] > img[width=\"158\"]\r\n";
	line += "divxden.com##.ad\r\n";
	line += "divxden.com##.header_greenbar\r\n";
	line += "divxme.com##a[href^=\"http://www.jdoqocy.com/\"]\r\n";
	line += "divxstage.net##.ad\r\n";
	line += "diyfail.com###top-leaderboard\r\n";
	line += "diyfail.com##.leaderboard\r\n";
	line += "dkszone.net##.liutilities-top\r\n";
	line += "dl4all.com###deluxePopupWindow-container\r\n";
	line += "dogpile.com##.paidSearchResult\r\n";
	line += "dosgamesarchive.com###banner\r\n";
	line += "dosgamesarchive.com##.rectangle\r\n";
	line += "dosgamesarchive.com##.skyscraper\r\n";
	line += "dotsauce.com###leadsponsor\r\n";
	line += "dotsauce.com##.onetwentyfive\r\n";
	line += "dotsauce.com##img[width=\"260\"][height=\"260\"]\r\n";
	line += "downforeveryoneorjustme.com###container > .domain + p + br + center:last-child\r\n";
	line += "downloadhelper.net##.banner-468x60\r\n";
	line += "downloadsquad.com###newjobs-module\r\n";
	line += "downloadsquad.com###topleader-wrap\r\n";
	line += "downloadsquad.com##.medrect\r\n";
	line += "dragcave.net###prefooter\r\n";
	line += "drive.com.au##.cA-twinPromo\r\n";
	line += "drugs.com###topbannerWrap\r\n";
	line += "dt-updates.com##.adv_items\r\n";
	line += "dt-updates.com##div[style=\"margin: 20px auto 0pt; text-align: left;\"]\r\n";
	line += "dubbed-scene.com##.adblock\r\n";
	line += "dubcnn.com##img[border=\"0\"][width=\"200\"]\r\n";
	line += "dumbassdaily.com##a[href$=\".clickbank.net\"]\r\n";
	line += "dumbassdaily.com##a[href^=\"http://www.badjocks.com\"]\r\n";
	line += "dumblittleman.com##.ad\r\n";
	line += "dumblittleman.com##.cats_box2\r\n";
	line += "dv.com##table[width=\"665\"]\r\n";
	line += "eartheasy.com##td[width=\"200\"][height=\"4451\"][bgcolor=\"#e1e3de\"][rowspan=\"19\"]\r\n";
	line += "earthweb.com##.footerbanner\r\n";
	line += "easybib.com##.banner\r\n";
	line += "ebaumsworld.com###eacs-sidebar\r\n";
	line += "ebuddy.com###Rectangle\r\n";
	line += "ebuddy.com###banner_rectangle\r\n";
	line += "eclipse.org##.ad\r\n";
	line += "ecommerce-journal.com###runnews\r\n";
	line += "ecommercetimes.com##.slink-text\r\n";
	line += "ecommercetimes.com##.slink-title\r\n";
	line += "economist.com###classified_wrapper\r\n";
	line += "economist.com###footer-classifieds\r\n";
	line += "economist.com###leaderboard\r\n";
	line += "economist.com###top_banner\r\n";
	line += "ecr.co.za###thebug\r\n";
	line += "ecr.co.za##.block_120x600\r\n";
	line += "ectnews.com###welcome-box\r\n";
	line += "ectnews.com##.content-block-slinks\r\n";
	line += "ectnews.com##.content-tab-slinks\r\n";
	line += "edge-online.com###above-header-region\r\n";
	line += "edn.com###headerwildcard\r\n";
	line += "edn.com##.sponsorcontent\r\n";
	line += "edn.com##div[style=\"font-family: verdana,sans-serif; font-style: normal; font-variant: normal; font-weight: normal; font-size: 10px; line-height: normal; font-size-adjust: none; font-stretch: normal; -x-system-font: none; color: rgb(51, 51, 51); background-color: rgb(160, 186, 200); padding-bottom: 7px;\"]\r\n";
	line += "eeeuser.com###header\r\n";
	line += "efinancialnews.com##.promo-leaderboard\r\n";
	line += "egreetings.com##td[style=\"background-color: rgb(255, 255, 255); vertical-align: top;\"]\r\n";
	line += "ehow.com###jsWhoCanHelp\r\n";
	line += "ehow.com##.takeoverBanner\r\n";
	line += "el33tonline.com###AdA\r\n";
	line += "el33tonline.com###AdB\r\n";
	line += "el33tonline.com###AdC\r\n";
	line += "el33tonline.com###AdD\r\n";
	line += "el33tonline.com###AdE\r\n";
	line += "el33tonline.com###AdF\r\n";
	line += "el33tonline.com###AdG\r\n";
	line += "el33tonline.com###AdH\r\n";
	line += "el33tonline.com###AdI\r\n";
	line += "electricenergyonline.com###top_pub\r\n";
	line += "electricenergyonline.com###tower_pub\r\n";
	line += "electricenergyonline.com##.sponsor\r\n";
	line += "electronista.com###footerleft\r\n";
	line += "electronista.com###footerright\r\n";
	line += "electronista.com###leaderboard\r\n";
	line += "electronista.com###supportbod\r\n";
	line += "elizium.nu##center > ul[style=\"padding: 0pt; width: 100%; margin: 0pt; list-style: none outside none;\"]\r\n";
	line += "elle.com###ad-block-bottom\r\n";
	line += "emaillargefile.com##a[href^=\"http://www.mb01.com/lnk.asp?\"]\r\n";
	line += "emedtv.com###leaderboard\r\n";
	line += "empireonline.com##table[align=\"center\"][width=\"950\"][height=\"130\"]\r\n";
	line += "empireonline.com##td.smallgrey[width=\"300\"][height=\"250\"]\r\n";
	line += "engadget.com##.medrect\r\n";
	line += "engadget.com##.siteswelike\r\n";
	line += "england.fm###banbo\r\n";
	line += "englishforum.ch##td[style=\"width: 160px; padding-left: 15px;\"]\r\n";
	line += "englishforum.ch##td[width=\"176\"][style=\"padding-left: 15px;\"]\r\n";
	line += "environmentalgraffiti.com##div[style=\"width: 300px; height: 250px; overflow: hidden;\"]\r\n";
	line += "eonline.com###franchise\r\n";
	line += "eonline.com###module_sky_scraper\r\n";
	line += "epicurious.com###sweepstakes\r\n";
	line += "epinions.com##td[width=\"180\"][valign=\"top\"]\r\n";
	line += "eq2flames.com##td[width=\"120\"][style=\"padding-left: 5px; white-space: normal;\"]\r\n";
	line += "erictric.com###head-banner468\r\n";
	line += "esecurityplanet.com###gemhover\r\n";
	line += "esecurityplanet.com###partners\r\n";
	line += "esecurityplanet.com##.vspace\r\n";
	line += "esl.eu##.bannerContainer\r\n";
	line += "esoft.web.id###content-top\r\n";
	line += "espn.go.com##.mast-container\r\n";
	line += "espn.go.com##.spons_optIn\r\n";
	line += "esus.com##a[href=\"http://www.regiochat.be\"]\r\n";
	line += "etonline.com##.superbanner\r\n";
	line += "eurogamer.net###skyscraper\r\n";
	line += "eurogamer.net###tabbaz\r\n";
	line += "euronews.net###OAS1\r\n";
	line += "euronews.net###OAS2\r\n";
	line += "euronews.net##.col-pub-skyscraper\r\n";
	line += "euronews.net##.google-banner\r\n";
	line += "everyjoe.com##.ad\r\n";
	line += "eweek.com###Table_01\r\n";
	line += "eweek.com###hp_special_reports\r\n";
	line += "eweek.com###syndication\r\n";
	line += "eweek.com##.hp_link_online_classifieds\r\n";
	line += "eweek.com##.omniture_module_tracker\r\n";
	line += "eweek.com##table[width=\"500\"][style=\"border: 1px solid rgb(204, 204, 204); margin: 5px 10px 5px 20px;\"]\r\n";
	line += "exactseek.com###featured\r\n";
	line += "exactseek.com##.recommended\r\n";
	line += "examiner.co.uk##.promotop\r\n";
	line += "examiner.com##.headerbg\r\n";
	line += "excelforum.com##.contentLeft\r\n";
	line += "excelforum.com##div[style=\"width: 300px; height: 250px; float: left; display: inline; margin-left: 5%;\"]\r\n";
	line += "excelforum.com##div[style=\"width: 300px; height: 250px; float: right; display: inline; margin-left: 5%;\"]\r\n";
	line += "excite.com##.mexContentBdr\r\n";
	line += "expertreviews.co.uk###skin\r\n";
	line += "expertreviews.co.uk###skyScrapper\r\n";
	line += "expertreviews.co.uk##.leaderBoard\r\n";
	line += "expertreviews.co.uk##.leaderLeft\r\n";
	line += "expertreviews.co.uk##.leaderRight\r\n";
	line += "experts-exchange.com###compCorpAcc\r\n";
	line += "experts-exchange.com###compSky\r\n";
	line += "experts-exchange.com##.ontopBanner\r\n";
	line += "explainthisimage.com###top-leaderboard\r\n";
	line += "explainthisimage.com##.leaderboard\r\n";
	line += "explosm.net##td[height=\"90\"][bgcolor=\"#000000\"][style=\"border: 3px solid rgb(55, 62, 70);\"]\r\n";
	line += "extremeoverclocking.com##td[height=\"104\"][colspan=\"2\"]\r\n";
	line += "facebook.com###home_sponsor_nile\r\n";
	line += "facebook.com##.ego_spo\r\n";
	line += "facebook.com##.fbEmu\r\n";
	line += "factoidz.com##div[style=\"float: left; margin: 0pt 30px 20px 0pt; width: 336px; height: 280px;\"]\r\n";
	line += "fairyshare.com##.google_top\r\n";
	line += "famousbloggers.net###hot_offer\r\n";
	line += "famousbloggers.net##.stop_sign\r\n";
	line += "fanhouse.com##.ad\r\n";
	line += "fanpix.net###leaderboard\r\n";
	line += "fanpop.com###rgad\r\n";
	line += "fanpop.com##div[style=\"width: 300px; height: 250px; background-color: rgb(0, 0, 0); color: rgb(153, 153, 153);\"]\r\n";
	line += "fark.com###rightSideRightMenubar\r\n";
	line += "fasterlouder.com.au##.ad\r\n";
	line += "faststats.cricbuzz.com##td[style=\"width: 300px; height: 250px;\"]\r\n";
	line += "fatwallet.com###promoBand\r\n";
	line += "favicon.co.uk##img[width=\"190\"][height=\"380\"]\r\n";
	line += "faxo.com###fa_l\r\n";
	line += "fayobserver.com###bottom-leaderboard\r\n";
	line += "feedburner.com##a[href^=\"http://ads.pheedo.com/\"]\r\n";
	line += "feedicons.com###footerboard\r\n";
	line += "feministing.com###bannerBottom\r\n";
	line += "feministing.com###bannerLeft\r\n";
	line += "feministing.com###bannerTop\r\n";
	line += "ffiles.com###right_col\r\n";
	line += "file-extensions.org###uniBan\r\n";
	line += "filedropper.com###sidebar\r\n";
	line += "filefactory.com###aContainer\r\n";
	line += "filefront.com##div[style=\"width: 300px; min-height: 250px;\"]\r\n";
	line += "filehippo.com##.ad\r\n";
	line += "filestube.com##.nova\r\n";
	line += "filetrip.net###products\r\n";
	line += "finance.yahoo.com###yfi_pf_ysm\r\n";
	line += "finance.yahoo.com###yfi_ysm\r\n";
	line += "finance.yahoo.com##.ad\r\n";
	line += "finance.yahoo.com##.ysm\r\n";
	line += "financialpost.com###npLeaderboardRow\r\n";
	line += "findfiles.com##.tvisible[width=\"468\"][cellspacing=\"0\"][cellpadding=\"0\"][bgcolor=\"#ffffff\"][align=\"center\"]\r\n";
	line += "firewallguide.com##td[width=\"300\"][height=\"250\"]\r\n";
	line += "flashgot.net##.tla\r\n";
	line += "flashscore.com##div[style=\"height: 240px ! important;\"]\r\n";
	line += "flashscore.com##div[style=\"height: 90px ! important;\"]\r\n";
	line += "flixster.com##div[style=\"position: relative; height: 270px;\"]\r\n";
	line += "flvz.com###additional_plugins_bar\r\n";
	line += "flvz.com##a[href^=\"http://www.flvpro.com/movies/?aff=\"]\r\n";
	line += "fontstock.net##.mediaBox\r\n";
	line += "foodnetwork.ca##.bannerContainer\r\n";
	line += "foodnetwork.ca##.bboxContainer\r\n";
	line += "foodnetwork.co.uk###pre-header-banner\r\n";
	line += "foodnetwork.co.uk##.right_header_row\r\n";
	line += "foodnetwork.com##.mrec\r\n";
	line += "fool.com###promoAndLeaderboard\r\n";
	line += "footylatest.com###leaderboardspace\r\n";
	line += "forbes.com##.fifthN\r\n";
	line += "forbes.com##.top_banner\r\n";
	line += "forbes.com##div[style=\"width: 125px; height: 125px; padding: 20px 20px 20px 25px; float: left;\"]\r\n";
	line += "forbes.com##div[style=\"width: 125px; height: 125px; padding: 20px 25px 20px 20px; float: right;\"]\r\n";
	line += "forrst.com##.ad\r\n";
	line += "forum.notebookreview.com##td[width=\"400\"][height=\"280\"]\r\n";
	line += "forum.rpg.net##img[border=\"0\"][style=\"outline: medium none;\"]\r\n";
	line += "forums.battle.net##td[align=\"center\"][width=\"130\"]\r\n";
	line += "forums.scifi.com###flippingBanner\r\n";
	line += "forums.vr-zone.com##.perm_announcement\r\n";
	line += "forums.worldofwarcraft.com##td[align=\"center\"][width=\"130\"]\r\n";
	line += "forums.worldofwarcraft.com##td[width=\"130px\"][valign=\"top\"][align=\"center\"]:last-child\r\n";
	line += "forums.wow-europe.com##td[align=\"center\"][width=\"130\"]\r\n";
	line += "forums.wow-europe.com##td[width=\"130px\"][valign=\"top\"][align=\"center\"]:last-child\r\n";
	line += "forumserver.twoplustwo.com##td[width=\"120\"][valign=\"top\"][style=\"padding-left: 10px;\"]\r\n";
	line += "fox.com##.ad\r\n";
	line += "foxbusiness.com###cb_medrect1_div\r\n";
	line += "foxnews.com###console300x100\r\n";
	line += "foxnews.com###marketplace\r\n";
	line += "foxnews.com##.ad\r\n";
	line += "foxnews.com##.quigo\r\n";
	line += "fpsbanana.com##a[href^=\"http://www.xfactorservers.com/clients/link.php?id=\"]\r\n";
	line += "free-tv-video-online.info##a[style=\"margin-left: auto; font-size: 14px; padding: 3px; margin-right: auto; width: 640px; display: block; text-decoration: none;\"]\r\n";
	line += "freecodesource.com###banner\r\n";
	line += "freedict.com##.partners\r\n";
	line += "freeiconsdownload.com###LeftBanner\r\n";
	line += "freestreamtube.com###ad\r\n";
	line += "freshmeat.net##.banner-imu\r\n";
	line += "freshmeat.net##.banner1\r\n";
	line += "friday-ad.co.uk##.PlateFood\r\n";
	line += "ft.com###leaderboard\r\n";
	line += "ft.com##.marketing\r\n";
	line += "ftv.com###hdr_c\r\n";
	line += "ftv.com###hdr_r\r\n";
	line += "fudzilla.com###showcase\r\n";
	line += "fudzilla.com##.artbannersxtd\r\n";
	line += "funnyexam.com###top-leaderboard\r\n";
	line += "funnyexam.com##.leaderboard\r\n";
	line += "funnytipjars.com###top-leaderboard\r\n";
	line += "funnytipjars.com##.leaderboard\r\n";
	line += "fxnetworks.com###adBlock\r\n";
	line += "gadgetzone.com.au###Leaderboard-placeholder\r\n";
	line += "gadgetzone.com.au##td[style=\"width: 300px;\"]\r\n";
	line += "gadling.com##.medrect\r\n";
	line += "gadling.com##.topleader\r\n";
	line += "gamebanshee.com##.banner\r\n";
	line += "gamegrep.com##.leaderboard_unit\r\n";
	line += "gamepro.com.au##.rhs_300x250\r\n";
	line += "gamerevolution.com##td[height=\"100\"][style=\"padding-left: 5px; padding-top: 5px; padding-right: 5px;\"]\r\n";
	line += "gamernode.com##.ad\r\n";
	line += "gamerstemple.com###banner\r\n";
	line += "gamerstemple.com###tower1\r\n";
	line += "gamerstemple.com###tower2\r\n";
	line += "gamersyde.com##.placeholder-top\r\n";
	line += "games.com##.ad\r\n";
	line += "gamesindustry.biz###leader\r\n";
	line += "gamesindustry.biz###leader-container\r\n";
	line += "gamesradar.com##.tablets\r\n";
	line += "gawker.com###skySpacer\r\n";
	line += "gawker.com###skyscraper\r\n";
	line += "gbrej.com###bottom_banner\r\n";
	line += "gearlive.com###google\r\n";
	line += "gearlive.com##.wellvert\r\n";
	line += "geek.com##.leaderboard\r\n";
	line += "geek.com##.picksBox\r\n";
	line += "geek.com##a[href^=\"http://www.geek.com/partners?\"]\r\n";
	line += "geek.com##td[width=\"170\"]\r\n";
	line += "geekologie.com###leaderboard\r\n";
	line += "generation-nt.com##.innerpub125\r\n";
	line += "generation-nt.com##.innerpub250\r\n";
	line += "generation-nt.com##.pub125\r\n";
	line += "generation-nt.com##.pub2\r\n";
	line += "generation-nt.com##.pub3\r\n";
	line += "genesisowners.com##.tborder[width=\"160\"]\r\n";
	line += "genuineforextrading.com###clickbank\r\n";
	line += "get-ip.de##div[style=\"display: block; background: none repeat scroll 0% 0% rgb(238, 238, 238); width: 300px; height: 250px; margin-top: 10px;\"]\r\n";
	line += "get-ip.de##div[style=\"display: block; background: none repeat scroll 0% 0% rgb(238, 238, 238); width: 300px; height: 250px;\"]\r\n";
	line += "getfoxyproxy.org###ad\r\n";
	line += "gethuman.com##td[style=\"width: 200px;\"]\r\n";
	line += "getprice.com.au##li[style=\"clear: both; padding-left: 0pt; padding-bottom: 0pt; width: 580px;\"]\r\n";
	line += "ghacks.net##.gutterlink\r\n";
	line += "gigabyteupload.com##input[onclick^=\"window.location.href='http://www.affbuzzads.com/affiliate/\"]\r\n";
	line += "gigasize.com##.topbanner\r\n";
	line += "gigwise.com###skyscraper\r\n";
	line += "giveawayoftheday.com##.before_hot_tags\r\n";
	line += "givesmehope.com###droitetop\r\n";
	line += "gizmocrunch.com##div[style=\"background-color: rgb(235, 242, 247); width: 560px;\"]\r\n";
	line += "gizmodo.com###marquee-frame\r\n";
	line += "gizmodo.com###skySpacer\r\n";
	line += "gizmodo.com###skyscraper\r\n";
	line += "gizmodo.com###spacer160\r\n";
	line += "gmanews.tv##div[style=\"width: 250px; height: 280px; border-top: 1px solid rgb(204, 204, 204);\"]\r\n";
	line += "goal.com###marketplaceModule\r\n";
	line += "goal.com##.betting200x120\r\n";
	line += "goal.com##.betting364x80\r\n";
	line += "goauto.com.au###leftnavcontainer + table[width=\"130\"]\r\n";
	line += "gocurrency.com###gosense\r\n";
	line += "goldcoastunited.com.au##.promotion_wrapper\r\n";
	line += "golivewire.com##div[style=\"height: 292px; margin-left: 10px; background-image: url(http://img.golivewire.com/stickynote-gray.gif); background-repeat: no-repeat; background-position: 0px 3px; padding-left: 26px; padding-top: 26px;\"]\r\n";
	line += "good.is##.ad\r\n";
	line += "goodhopefm.co.za##.mrec\r\n";
	line += "goodhousekeeping.com###hpV2_728x90\r\n";
	line += "google.co.uk##.ts[style=\"margin: 0pt 0pt 12px; height: 92px;\"]\r\n";
	line += "google.com##.ts[style=\"margin: 0pt 0pt 12px; height: 92px;\"]\r\n";
	line += "googletutor.com##div[style=\"width: 125px; text-align: center;\"]\r\n";
	line += "googlewatch.eweek.com###topBanner\r\n";
	line += "governmentvideo.com##table[width=\"665\"]\r\n";
	line += "gpsreview.net###lead\r\n";
	line += "grapevine.is##.ad\r\n";
	line += "grapevine.is##div[style=\"padding: 12px 0pt; text-align: center;\"]\r\n";
	line += "grindtv.com###LREC\r\n";
	line += "grindtv.com###SKY\r\n";
	line += "grindtv.com###hLREC\r\n";
	line += "growingbusiness.co.uk##.siteLeaderBoard\r\n";
	line += "gtplanet.net###a2\r\n";
	line += "gtplanet.net###a3\r\n";
	line += "guardian.co.uk###commercial-partners\r\n";
	line += "guardian.co.uk##.kelkoo\r\n";
	line += "guitaretab.com##.ring_link\r\n";
	line += "guru3d.com##a[href^=\"http://driveragent.com/?ref=\"]\r\n";
	line += "guruji.com###SideBar\r\n";
	line += "guruji.com##div[style=\"border: 1px solid rgb(250, 239, 209); margin: 0px 4px; padding: 4px; background-color: rgb(255, 248, 221);\"]\r\n";
	line += "h-online.com##.bcadv\r\n";
	line += "haaretz.com##.affiliates\r\n";
	line += "haaretz.com##.buttonBanners\r\n";
	line += "halifaxcourier.co.uk###banner01\r\n";
	line += "hardocp.com##.ad\r\n";
	line += "harpers.org##.topbanner\r\n";
	line += "hdtvtest.co.uk##.deal\r\n";
	line += "healthboards.com##td[\\!valign=\"top\"]\r\n";
	line += "healthboards.com##td[align=\"left\"][width=\"300\"]:first-child\r\n";
	line += "hebdenbridgetimes.co.uk###banner01\r\n";
	line += "helenair.com##table.bordered[align=\"center\"][width=\"728\"]\r\n";
	line += "hellmode.com###header\r\n";
	line += "hellomagazine.com###publi\r\n";
	line += "help.com###bwp\r\n";
	line += "helpwithwindows.com###ad\r\n";
	line += "helpwithwindows.com###desc\r\n";
	line += "heraldscotland.com###leaderboard\r\n";
	line += "heraldsun.com.au##.multi-promo\r\n";
	line += "hi5.com###hi5-common-header-banner\r\n";
	line += "hi5.com##.hi5-common-header-banner-ad\r\n";
	line += "highdefdigest.com##table[width=\"300\"][cellspacing=\"0\"][cellpadding=\"0\"]\r\n";
	line += "hilarious-pictures.com###block-block-1\r\n";
	line += "hilarious-pictures.com###block-block-12\r\n";
	line += "hilarious-pictures.com###block-block-8\r\n";
	line += "hilarious-pictures.com##.horizontal\r\n";
	line += "hindustantimes.com##.story_lft_wid\r\n";
	line += "hitfix.com##.googlewide\r\n";
	line += "hollywoodreporter.com##.ad\r\n";
	line += "holyfragger.com##.ad\r\n";
	line += "hotfrog.com##.search-middle-adblock\r\n";
	line += "hotfroguk.co.uk##.search-middle-adblock\r\n";
	line += "hotjobs.yahoo.com###sponsorResults\r\n";
	line += "hotlinkfiles.com###leaderboard\r\n";
	line += "hotshare.net##.publi_videos1\r\n";
	line += "howstuffworks.com###MedRectHome\r\n";
	line += "howstuffworks.com###SponLogo\r\n";
	line += "howstuffworks.com##.adv\r\n";
	line += "howstuffworks.com##.ch\r\n";
	line += "howstuffworks.com##.search-span\r\n";
	line += "howstuffworks.com##td[width=\"980\"][height=\"90\"]\r\n";
	line += "howtoforge.com##div[style=\"margin-top: 10px; font-size: 11px;\"]\r\n";
	line += "howtogeek.com##body > div[style=\"height: 90px;\"]:first-child\r\n";
	line += "howtogeek.com##div[style=\"padding-top: 20px; margin-top: 10px; margin-bottom: 10px; min-height: 115px; text-align: center; width: 750px; margin-left: 113px;\"]\r\n";
	line += "howtogeek.com##div[style=\"padding-top: 20px; margin-top: 210px; margin-bottom: 10px; min-height: 115px; text-align: center; width: 750px; margin-left: -15px;\"]\r\n";
	line += "hplusmagazine.com###bottom\r\n";
	line += "huffingtonpost.com##.contin_below\r\n";
	line += "hvac-talk.com##td[align=\"center\"][valign=\"top\"][style=\"padding-left: 10px;\"]\r\n";
	line += "i-comers.com###headerfix\r\n";
	line += "i-programmer.info###iProgrammerAmazoncolum\r\n";
	line += "i-programmer.info##.bannergroup\r\n";
	line += "i4u.com###topBanner > div#right\r\n";
	line += "iafrica.com###c_row1_bannerHolder\r\n";
	line += "iafrica.com##.article_Banner\r\n";
	line += "iamdisappoint.com###top-leaderboard\r\n";
	line += "iamdisappoint.com##.leaderboard\r\n";
	line += "iberia.com##.bannerGiraffe\r\n";
	line += "ibtimes.com###DHTMLSuite_modalBox_contentDiv\r\n";
	line += "ibtimes.com##.modalDialog_contentDiv_shadow\r\n";
	line += "ibtimes.com##.modalDialog_transparentDivs\r\n";
	line += "icenews.is###topbanner\r\n";
	line += "idg.com.au###skin_bump\r\n";
	line += "idg.com.au###top_adblock_fix\r\n";
	line += "ign.com###boards_medrec_relative\r\n";
	line += "illimitux.net###screens\r\n";
	line += "illimitux.net##.pub_bot\r\n";
	line += "illimitux.net##.pub_top\r\n";
	line += "iloubnan.info###bann\r\n";
	line += "iloveim.com###closeAdsDiv\r\n";
	line += "imagebanana.com##.ad\r\n";
	line += "images.search.yahoo.com###r-n\r\n";
	line += "imageshack.us###add_frame\r\n";
	line += "imdb.com###top_rhs_1_wrapper\r\n";
	line += "imdb.com###top_rhs_wrapper\r\n";
	line += "imtranslator.net##td[align=\"right\"][valign=\"bottom\"][height=\"96\"]\r\n";
	line += "inbox.com###r\r\n";
	line += "inbox.com##.slinks\r\n";
	line += "indeed.co.uk##.sjl\r\n";
	line += "independent.co.uk###article > .box\r\n";
	line += "independent.co.uk###bottom_link\r\n";
	line += "independent.co.uk###yahooLinks\r\n";
	line += "independent.co.uk##.commercialpromo\r\n";
	line += "independent.co.uk##.googleCols\r\n";
	line += "independent.co.uk##.homepagePartnerList\r\n";
	line += "independent.co.uk##.spotlight\r\n";
	line += "indianexpress.com###shopping_deals\r\n";
	line += "indiatimes.com###jobsbox\r\n";
	line += "indiatimes.com##.hover2bg\r\n";
	line += "indiatimes.com##.tpgrynw > .topbrnw:first-child + div\r\n";
	line += "indiatimes.com##td[valign=\"top\"][height=\"110\"][align=\"center\"]\r\n";
	line += "indiewire.com###promo_book\r\n";
	line += "indyposted.com##.ad\r\n";
	line += "indystar.com##.ad\r\n";
	line += "info.co.uk##.p\r\n";
	line += "info.co.uk##.spon\r\n";
	line += "infoplease.com###gob\r\n";
	line += "infoplease.com###ssky\r\n";
	line += "infoplease.com##.tutIP-infoarea\r\n";
	line += "informationmadness.com###ja-topsl\r\n";
	line += "informationweek.com###buylink\r\n";
	line += "informationweek.com##.ad\r\n";
	line += "informit.com###leaderboard\r\n";
	line += "infoworld.com##.recRes_head\r\n";
	line += "inquirer.net###bottom_container\r\n";
	line += "inquirer.net###leaderboard_frame\r\n";
	line += "inquirer.net###marketplace_vertical_container\r\n";
	line += "inquirer.net##.bgadgray10px\r\n";
	line += "inquirer.net##.fontgraysmall\r\n";
	line += "inquirer.net##.padtopbot5\r\n";
	line += "inquirer.net##table[width=\"780\"][height=\"90\"]\r\n";
	line += "inquisitr.com###topx2\r\n";
	line += "insanelymac.com##.ad\r\n";
	line += "instapaper.com###deckpromo\r\n";
	line += "intelligencer.ca###banZone\r\n";
	line += "intelligencer.ca##.bnr\r\n";
	line += "interfacelift.com##.ad\r\n";
	line += "internet.com###contentmarketplace\r\n";
	line += "internet.com###mbEnd\r\n";
	line += "internet.com##.ch\r\n";
	line += "internetevolution.com##div[style=\"border: 2px solid rgb(230, 230, 230); margin-top: 30px;\"]\r\n";
	line += "investopedia.com###leader\r\n";
	line += "investopedia.com##.mainbodyleftcolumntrade\r\n";
	line += "investopedia.com##div[style=\"float: left; width: 250px; height: 250px; margin-right: 5px;\"]\r\n";
	line += "io9.com###marquee\r\n";
	line += "io9.com###marquee-frame\r\n";
	line += "io9.com###skyscraper\r\n";
	line += "io9.com##.highlite\r\n";
	line += "iol.co.za###sp_links\r\n";
	line += "iol.co.za###weatherbox-bottom\r\n";
	line += "iol.co.za##.lead_sp_links\r\n";
	line += "iol.co.za##table[width=\"120\"]\r\n";
	line += "iomtoday.co.im###banner01\r\n";
	line += "ipmart-forum.com###table1\r\n";
	line += "irishtimes.com###banner-area\r\n";
	line += "israelnationalnews.com##.leftColumn\r\n";
	line += "itnews.com.au##div[style=\"width: 300px; height: 250px;\"]\r\n";
	line += "itnews.com.au##div[style=\"width: 728px; height: 90px; margin-left: auto; margin-right: auto; padding-bottom: 20px;\"]\r\n";
	line += "itnewsonline.com##table[width=\"300\"][height=\"250\"]\r\n";
	line += "itnewsonline.com##td[width=\"120\"]\r\n";
	line += "itp.net##.top_bit\r\n";
	line += "itpro.co.uk###skyScraper\r\n";
	line += "itproportal.com###hp-accordion\r\n";
	line += "itproportal.com##.se_left\r\n";
	line += "itproportal.com##.se_right\r\n";
	line += "itproportal.com##.teaser\r\n";
	line += "itreviews.co.uk###bmmBox\r\n";
	line += "itweb.co.za###cosponsor\r\n";
	line += "itweb.co.za###cosponsor-logo\r\n";
	line += "itweb.co.za###cosponsorTab\r\n";
	line += "itweb.co.za###highlight-on\r\n";
	line += "itweb.co.za###sponsor\r\n";
	line += "itweb.co.za###sponsor-logo\r\n";
	line += "itweb.co.za###top-banner\r\n";
	line += "itweb.co.za##.hidden\r\n";
	line += "itweb.co.za##div[style=\"width: 300px; height: 266px; overflow: hidden; margin: 0pt;\"]\r\n";
	line += "itworld.com###more_resources\r\n";
	line += "itworld.com###partner_strip\r\n";
	line += "iwebtool.com##table[cellspacing=\"0\"][cellpadding=\"0\"][border=\"1\"]\r\n";
	line += "ixquick.com##td[bgcolor=\"#f7f9ff\"]\r\n";
	line += "jacarandafm.com###thebug\r\n";
	line += "jacarandafm.com##.block_120x600\r\n";
	line += "jakeludington.com###ablock\r\n";
	line += "jalopnik.com###skyscraper\r\n";
	line += "jame-world.com###adv_top\r\n";
	line += "jame-world.com##.adv_right\r\n";
	line += "jamendo.com###ad\r\n";
	line += "jamendo.com##.col_extra\r\n";
	line += "japanator.com##.gutters\r\n";
	line += "japanator.com##div[style=\"background-color: rgb(176, 176, 176); width: 320px; height: 260px; padding: 20px 10px 10px;\"]\r\n";
	line += "japanisweird.com###top-leaderboard\r\n";
	line += "japanisweird.com##.leaderboard\r\n";
	line += "japannewsreview.com##div[style=\"width: 955px; height: 90px; margin-bottom: 10px;\"]\r\n";
	line += "japantimes.co.jp###FooterAdBlock\r\n";
	line += "japantimes.co.jp###HeaderAdsBlockFront\r\n";
	line += "japantimes.co.jp##.RealEstateAdBlock\r\n";
	line += "japantimes.co.jp##.SmallBanner\r\n";
	line += "japantimes.co.jp##.UniversitySearchAdBlock\r\n";
	line += "japantimes.co.jp##table[height=\"250\"][width=\"250\"]\r\n";
	line += "japanvisitor.com###sponsor\r\n";
	line += "jarrowandhebburngazette.com###banner01\r\n";
	line += "javalobby.org###topLeaderboard\r\n";
	line += "jayisgames.com##.bfg-feature\r\n";
	line += "jdownloader.org##a[href^=\"http://fileserve.com/signup.php?reff=\"]\r\n";
	line += "jeuxvideo-flash.com###pub_header\r\n";
	line += "jewtube.com###adv\r\n";
	line += "jewtube.com##div[style=\"display: block; width: 468px; height: 60px; padding: 5px; border: 1px solid rgb(221, 221, 221); text-align: left;\"]\r\n";
	line += "jezebel.com###skyscraper\r\n";
	line += "johnbridge.com###header_right_cell\r\n";
	line += "johnbridge.com##td[valign=\"top\"] > table.tborder[width=\"140\"][cellspacing=\"1\"][cellpadding=\"6\"][border=\"0\"]\r\n";
	line += "joomla.org##div[style=\"margin: 0pt auto; width: 728px; height: 100px;\"]\r\n";
	line += "joox.net###body-sidebar\r\n";
	line += "joox.net##img[alt=\"Download FLV Direct\"]\r\n";
	line += "joystickdivision.com###Page_Header\r\n";
	line += "joystiq.com###medrect\r\n";
	line += "joystiq.com###medrectrb\r\n";
	line += "joystiq.com###topleader-wrap\r\n";
	line += "joystiq.com##.medrect\r\n";
	line += "jpost.com###topBanner\r\n";
	line += "jpost.com##.jp-grid-oppsidepane\r\n";
	line += "jpost.com##.padtopblubar\r\n";
	line += "jpost.com##[id=\"ads.gbox.1\"]\r\n";
	line += "jumptags.com##div[style=\"background: none repeat scroll 0% 0% rgb(255, 255, 255); padding: 5px; border-bottom: 1px solid rgb(170, 170, 170); height: 95px;\"]\r\n";
	line += "justhungry.com##a[href^=\"http://affiliates.jlist.com/\"]\r\n";
	line += "justin.tv###iphone_banner\r\n";
	line += "kaldata.net###header2\r\n";
	line += "katu.com###mrktplace_tabbed\r\n";
	line += "katu.com##.callout\r\n";
	line += "katz.cd###spon\r\n";
	line += "katzforums.com###aff\r\n";
	line += "kayak.co.in##[height=\"330\"][width=\"270\"][bgcolor=\"#fff8dd\"]\r\n";
	line += "kayak.co.uk##[height=\"330\"][width=\"270\"][bgcolor=\"#fff8dd\"]\r\n";
	line += "kayak.com##[height=\"330\"][width=\"270\"][bgcolor=\"#fff8dd\"]\r\n";
	line += "keepvid.com##.sponsors\r\n";
	line += "keepvid.com##.sponsors-s\r\n";
	line += "kewlshare.com###rollAdRKLA\r\n";
	line += "kibagames.com##.adv_default_box_container\r\n";
	line += "kibagames.com##.category_adv_container\r\n";
	line += "kibagames.com##.dc_color_lightgreen.dc_bg_for_adv\r\n";
	line += "kibagames.com##.search_adv_container\r\n";
	line += "kibagames.com##.start_overview_adv_container\r\n";
	line += "kibagames.com##div[style=\"border: 0px solid rgb(0, 0, 0); width: 160px; height: 600px;\"]\r\n";
	line += "kibagames.com##div[style=\"margin-bottom: 10px; border: 1px solid rgb(0, 0, 0); height: 90px;\"]\r\n";
	line += "kids-in-mind.com##td[valign=\"top\"][style=\"padding-left: 5px; padding-right: 5px;\"]\r\n";
	line += "kidsinmind.com##td[valign=\"top\"][style=\"padding-left: 5px; padding-right: 5px;\"]\r\n";
	line += "kidzworld.com##div[style=\"width: 160px; height: 617px; margin: auto;\"]\r\n";
	line += "kidzworld.com##div[style=\"width: 300px; height: 117px; margin: auto;\"]\r\n";
	line += "kidzworld.com##div[style=\"width: 300px; height: 267px; margin: auto;\"]\r\n";
	line += "king-mag.com###banner_468\r\n";
	line += "king-mag.com###leaderboard\r\n";
	line += "king-mag.com###mediumrec\r\n";
	line += "king-mag.com###skyscraper\r\n";
	line += "kino.to###LeftFull\r\n";
	line += "kino.to###RightFull\r\n";
	line += "kino.to##.Special\r\n";
	line += "kioskea.net###topContent\r\n";
	line += "kissfm961.com##div[style=\"padding-top: 10px; padding-left: 10px; height: 250px;\"]\r\n";
	line += "kizna-blog.com##a[href$=\".clickbank.net\"]\r\n";
	line += "knowfree.net###mta_bar\r\n";
	line += "knowfree.net##.web_link\r\n";
	line += "knowfree.net##a[href^=\"http://kvors.com/click/\"]\r\n";
	line += "knowyourmeme.com##.a160x600\r\n";
	line += "knowyourmeme.com##.a250x250\r\n";
	line += "knowyourmeme.com##.a728x90\r\n";
	line += "knoxnews.com###leaderboard\r\n";
	line += "knoxnews.com##.big_box\r\n";
	line += "kohit.net##.banner_468\r\n";
	line += "kohit.net##.top_banner\r\n";
	line += "kohit.net##div[style=\"width: 300px; height: 250px; background-color: rgb(0, 0, 0);\"]\r\n";
	line += "kotaku.com###skySpacer\r\n";
	line += "kotaku.com###skyscraper\r\n";
	line += "kovideo.net##.h-728\r\n";
	line += "kovideo.net##.right-def-160\r\n";
	line += "kovideo.net##.search-728\r\n";
	line += "krapps.com###header\r\n";
	line += "krapps.com##a[href^=\"index.php?adclick=\"]\r\n";
	line += "krebsonsecurity.com###sidebar-b\r\n";
	line += "krebsonsecurity.com###sidebar-box\r\n";
	line += "krillion.com###sponCol\r\n";
	line += "ksl.com##div[style=\"float: left; width: 300px; margin: 5px 0px 13px;\"]\r\n";
	line += "ksl.com##table[style=\"width: 635px; padding: 0pt; margin: 0pt; background-color: rgb(230, 239, 255);\"]\r\n";
	line += "kstp.com###siteHeaderLeaderboard\r\n";
	line += "ktvu.com###leaderboard-sticky\r\n";
	line += "kuklaskorner.com###ultimate\r\n";
	line += "kval.com###mrktplace_tabbed\r\n";
	line += "kval.com##.callout\r\n";
	line += "langmaker.com##table[width=\"120\"]\r\n";
	line += "lastminute.com###sponsoredFeature\r\n";
	line += "latimes.com###article-promo\r\n";
	line += "law.com###leaderboard\r\n";
	line += "layoutstreet.com##.ad\r\n";
	line += "lbc.co.uk###topbanner\r\n";
	line += "learninginfo.org##table[align=\"left\"][width=\"346\"]\r\n";
	line += "lemondrop.com##.sidebarBanner\r\n";
	line += "lemondrop.com##.topBanner\r\n";
	line += "licensing.biz##.newsinsert\r\n";
	line += "life.com##.ad\r\n";
	line += "lifehack.org##.offer\r\n";
	line += "lifehacker.com###skySpacer\r\n";
	line += "lifehacker.com###skyscraper\r\n";
	line += "lifespy.com##.SRR\r\n";
	line += "lightreading.com##div[align=\"center\"][style=\"height: 114px;\"]\r\n";
	line += "linux-mag.com##.sponsor-widget\r\n";
	line += "linuxforums.org###rightColumn\r\n";
	line += "linuxforums.org##div[style=\"margin: 2px; float: right; width: 300px; height: 250px;\"]\r\n";
	line += "linuxinsider.com###welcome-box\r\n";
	line += "linuxinsider.com##.content-block-slinks\r\n";
	line += "linuxinsider.com##.content-tab-slinks\r\n";
	line += "linuxquestions.org##div[style=\"margin: -3px -3px 5px 5px; float: right;\"]\r\n";
	line += "linuxtopia.org###bookcover_sky\r\n";
	line += "lionsdenu.com###banner300-top-right\r\n";
	line += "lionsdenu.com###sidebar-bottom-left\r\n";
	line += "lionsdenu.com###sidebar-bottom-right\r\n";
	line += "live365.com##.ad\r\n";
	line += "livejournal.com##.ljad\r\n";
	line += "liverpooldailypost.co.uk##.promotop\r\n";
	line += "livestream.com##.ad\r\n";
	line += "livevss.net###ad\r\n";
	line += "livevss.tv###floatLayer1\r\n";
	line += "living.aol.co.uk##.wide.horizontal_promo_HPHT\r\n";
	line += "lmgtfy.com###sponsor\r\n";
	line += "lmgtfy.com###sponsor_wrapper\r\n";
	line += "load.to##.download_right\r\n";
	line += "loaded.it###apDiv1\r\n";
	line += "loaded.it###bottomcorner\r\n";
	line += "loaded.it###ppad1\r\n";
	line += "loaded.it##img[style=\"border: 0px none; width: 750px;\"]\r\n";
	line += "local.co.uk###borderTab\r\n";
	line += "local.yahoo.com##.yls-rs-paid\r\n";
	line += "londonstockexchange.com##.banner\r\n";
	line += "londonstockexchange.com##.bannerTop\r\n";
	line += "loombo.com##.ad\r\n";
	line += "lotro-lore.com###banner\r\n";
	line += "lowbird.com##.teaser\r\n";
	line += "lowyat.net##img[border=\"1\"]\r\n";
	line += "lowyat.net##tr[style=\"cursor: pointer;\"]\r\n";
	line += "luxist.com###topleader-wrap\r\n";
	line += "luxist.com##.medrect\r\n";
	line += "lyrics007.com##td[bgcolor=\"#ffcc00\"][width=\"770\"][height=\"110\"]\r\n";
	line += "lyricsfreak.com###ticketcity\r\n";
	line += "lyricsfreak.com##.ad\r\n";
	line += "lyricsfreak.com##.ringtone\r\n";
	line += "lyricsmode.com##div[style=\"text-align: center; margin-top: 15px; height: 90px;\"]\r\n";
	line += "lyricwiki.org###p-navigation + .portlet\r\n";
	line += "lyrster.com##.el_results\r\n";
	line += "m-w.com###google_creative_3\r\n";
	line += "m-w.com###skyscraper_creative_2\r\n";
	line += "maannews.net##td[style=\"border: 1px solid rgb(204, 204, 204); width: 250px; height: 120px;\"]\r\n";
	line += "maannews.net##td[style=\"border: 1px solid rgb(204, 204, 204); width: 640px; height: 80px;\"]\r\n";
	line += "macdailynews.com###right\r\n";
	line += "macintouch.com###yellows\r\n";
	line += "macintouch.com##img[width=\"125\"][height=\"125\"]\r\n";
	line += "macintouch.com##img[width=\"468\"]\r\n";
	line += "macleans.ca###leaderboard\r\n";
	line += "maclife.com###top-banner\r\n";
	line += "macnewsworld.com##.content-block-slinks\r\n";
	line += "macnewsworld.com##.content-tab-slinks\r\n";
	line += "macnn.com###leaderboard\r\n";
	line += "macnn.com###supportbod\r\n";
	line += "macobserver.com##.dealsontheweb\r\n";
	line += "macobserver.com##.specials\r\n";
	line += "macosxhints.com##div[style=\"border-bottom: 2px solid rgb(123, 123, 123); padding-bottom: 8px; margin-bottom: 5px;\"]\r\n";
	line += "macrumors.com###googleblock300\r\n";
	line += "macrumors.com###mr_banner_topad\r\n";
	line += "macstories.net###ad\r\n";
	line += "macsurfer.com##.text_top_box\r\n";
	line += "macsurfer.com##table[width=\"300\"][height=\"250\"]\r\n";
	line += "macthemes2.net###imagelinks\r\n";
	line += "macupdate.com###promoSidebar\r\n";
	line += "macupdate.com##div[style=\"width: 728px; height: 90px; margin: 0px auto; display: block;\"]\r\n";
	line += "macworld.co.uk###footer\r\n";
	line += "macworld.co.uk###topBannerSpot\r\n";
	line += "macworld.com###shopping\r\n";
	line += "madeformums.com###contentbanner\r\n";
	line += "magic.co.uk###headerRowOne\r\n";
	line += "magme.com###top_banner\r\n";
	line += "mail.google.com###\\:lq\r\n";
	line += "mail.live.com###SkyscraperContent\r\n";
	line += "mail.yahoo.com###MNW\r\n";
	line += "mail.yahoo.com###MON > div[style=\"color: rgb(0, 0, 0); font-size: 10px; font-family: Verdana,arial,sans-serif; text-align: center;\"]\r\n";
	line += "mail.yahoo.com###SKY\r\n";
	line += "mail.yahoo.com###northbanner\r\n";
	line += "mail.yahoo.com###nwPane\r\n";
	line += "mail.yahoo.com###slot_LREC\r\n";
	line += "majorgeeks.com##.Outlines\r\n";
	line += "majorgeeks.com##a[href^=\"http://www.pctools.com/registry-mechanic/?ref=\"]\r\n";
	line += "majorgeeks.com##a[href^=\"https://secure.avangate.com/affiliate.php\"]\r\n";
	line += "majorgeeks.com##a[target=\"1\"]\r\n";
	line += "majorgeeks.com##a[target=\"top\"]\r\n";
	line += "majorgeeks.com##table[align=\"right\"][width=\"336\"][style=\"padding-left: 5px;\"]\r\n";
	line += "maketecheasier.com##a[href=\"http://maketecheasier.com/advertise\"]\r\n";
	line += "makeuseof.com##a[href=\"http://www.makeuseof.com/advertise/\"]\r\n";
	line += "makeuseof.com##div[style=\"margin-bottom: 15px; margin-top: 15px; padding: 5px; border: 1px solid rgb(198, 215, 225); background-color: rgb(216, 234, 242);\"]\r\n";
	line += "makezine.com##.ad\r\n";
	line += "malaysiastory.com##.box2\r\n";
	line += "maltonmercury.co.uk###banner01\r\n";
	line += "mangafox.com##a[href^=\"http://fs.game321.com/\"]\r\n";
	line += "map24.com###cont_m24up\r\n";
	line += "mapquest.com###offers\r\n";
	line += "marketingmag.ca###leaderboard_container\r\n";
	line += "marketingpilgrim.com###ad\r\n";
	line += "mashable.com##.header-banner\r\n";
	line += "massively.com###topleader-wrap\r\n";
	line += "mcvuk.com##.newsinsert\r\n";
	line += "mediabistro.com##.right-column-boxes-content-partners\r\n";
	line += "mediacoderhq.com##.gg1\r\n";
	line += "mediafire.com###catfish_div\r\n";
	line += "mediafire.com##.download_banner_container\r\n";
	line += "mediafire.com##.ninesixty_container:last-child td[align=\"right\"][valign=\"top\"]:first-child\r\n";
	line += "mediafiresearch.net##a[href^=\"http://mediafiresearch.net/adv1.php\"]\r\n";
	line += "mediaite.com###magnify_widget_rect_handle\r\n";
	line += "mediaite.com###supertop\r\n";
	line += "medicineandtechnology.com##div[style=\"width: 728px; height: 90px; position: relative; margin: 0pt; padding: 0pt; text-align: left;\"]\r\n";
	line += "megafileupload.com##.banner300\r\n";
	line += "megafileupload.com##.big_banner\r\n";
	line += "megauploadsearch.net##a[href^=\"http://megauploadsearch.net/adv.php\"]\r\n";
	line += "megavideo.com##div[style=\"position: relative; width: 355px; height: 299px; margin-top: 2px;\"]\r\n";
	line += "megavideo.com##div[style=\"position: relative; width: 359px; height: 420px; margin-left: -3px; margin-top: 1px;\"]\r\n";
	line += "melbourneheartfc.com.au##.promotion_wrapper\r\n";
	line += "melbournevictory.com.au##.promotion_wrapper\r\n";
	line += "mercurynews.com###mn_SP_Links\r\n";
	line += "merriam-webster.com###Dictionary-MW_DICT_728_BOT\r\n";
	line += "merriam-webster.com###google_creative_3\r\n";
	line += "metacafe.com###Billboard\r\n";
	line += "metadivx.com##.ad\r\n";
	line += "metblogs.com###a_medrect\r\n";
	line += "metblogs.com###a_widesky\r\n";
	line += "metro.co.uk##.google-sky\r\n";
	line += "metro.co.uk##.sky\r\n";
	line += "metro.us##div[style=\"width: 300px; height: 250px; float: right;\"]\r\n";
	line += "metrolyrics.com###cee_box\r\n";
	line += "metrolyrics.com###cee_overlay\r\n";
	line += "metrolyrics.com###ipod\r\n";
	line += "metromix.com###leaderboard\r\n";
	line += "mg.co.za###masthead > table[style=\"padding-right: 5px;\"]:first-child\r\n";
	line += "mg.co.za###miway-creative\r\n";
	line += "mg.co.za##.articlecontinues\r\n";
	line += "mg.co.za##div[style=\"width: 300px; height: 250px;\"]\r\n";
	line += "miamiherald.com###leaderboard\r\n";
	line += "microsoft-watch.com###topBannerContainer\r\n";
	line += "miloyski.com##a.button[target=\"_blank\"]\r\n";
	line += "mindspark.com##.desc\r\n";
	line += "miniclip.com##.block_300x250\r\n";
	line += "miniclip.com##.letterbox\r\n";
	line += "minnpost.com##.topleader\r\n";
	line += "missoulian.com###yahoo-contentmatch\r\n";
	line += "mlfat4arab.com##img[width=\"234\"][height=\"60\"]\r\n";
	line += "mmosite.com##.c_gg\r\n";
	line += "mmosite.com##.mmo_gg\r\n";
	line += "mmosite.com##.mmo_gg2\r\n";
	line += "mmosite.com##.mmo_textsponsor\r\n";
	line += "mobile-ent.biz##.newsinsert\r\n";
	line += "mobilecrunch.com##.ad\r\n";
	line += "mobilemoviezone.com##a[href^=\"http://adsalvo.com/\"]\r\n";
	line += "mobilemoviezone.com##a[href^=\"http://clk.mobgold.com/\"]\r\n";
	line += "mobilust.net##a[href^=\"http://nicevid.net/?af=\"]\r\n";
	line += "modernhealthcare.com##.mh_topshade_b\r\n";
	line += "mofunzone.com###ldrbrd_td\r\n";
	line += "money.co.uk###topBar\r\n";
	line += "morefailat11.com###top-leaderboard\r\n";
	line += "morefailat11.com##.leaderboard\r\n";
	line += "morningstar.com##.LeaderWrap\r\n";
	line += "morningstar.com##.aadsection_b1\r\n";
	line += "morningstar.com##.aadsection_b2\r\n";
	line += "mortgageguide101.com###ppc\r\n";
	line += "mosnews.com##.right_pop\r\n";
	line += "motherboard.tv##.banner\r\n";
	line += "motherboard.tv##.moreFromVice\r\n";
	line += "motherjones.com##.post-continues\r\n";
	line += "motherproof.com###leader\r\n";
	line += "motionempire.com##div[style=\"width: 728px; margin-top: 3px; margin-bottom: 3px; height: 90px; overflow: hidden; margin-left: 113px;\"]\r\n";
	line += "motorcycle-usa.com##.bannergoogle\r\n";
	line += "movie2k.com###ball\r\n";
	line += "movie2k.com##a[href^=\"http://www.affbuzzads.com/affiliate/\"]\r\n";
	line += "movie2k.com##a[style=\"color: rgb(255, 0, 0); font-size: 14px;\"]\r\n";
	line += "moviecritic.com.au###glinks\r\n";
	line += "moviefone.com###WIAModule\r\n";
	line += "moviefone.com##.ent_promo_sidetexttitle\r\n";
	line += "movies.yahoo.com###banner\r\n";
	line += "movies.yahoo.com##.lrec\r\n";
	line += "moviesfoundonline.com###banner\r\n";
	line += "moviesmobile.net##a[href*=\".amobee.com\"]\r\n";
	line += "moviesmobile.net##a[href*=\".mojiva.com\"]\r\n";
	line += "moviesplanet.com##.Banner468X60\r\n";
	line += "moviesplanet.com##.gb\r\n";
	line += "movshare.net##.ad\r\n";
	line += "movstore.com##.overlayVid\r\n";
	line += "mp3-shared.net##a[href^=\"http://click.yottacash.com?PID=\"]\r\n";
	line += "mp3lyrics.org###bota\r\n";
	line += "mp3raid.com##td[align=\"left\"]\r\n";
	line += "mpfour.net##.overlayVid\r\n";
	line += "msn.com###Sales1\r\n";
	line += "msn.com###Sales2\r\n";
	line += "msn.com###Sales3\r\n";
	line += "msn.com###Sales4\r\n";
	line += "msn.com###ad\r\n";
	line += "msn.com##.abs\r\n";
	line += "msn.com##.ad\r\n";
	line += "msnbc.msn.com###Dcolumn\r\n";
	line += "msnbc.msn.com###marketplace\r\n";
	line += "msnbc.msn.com##.w460\r\n";
	line += "mstar.com##.MPFBannerWrapper\r\n";
	line += "mtv.co.uk###mtv-shop\r\n";
	line += "mtv.com###gft-sponsors\r\n";
	line += "multiupload.com##div[style=\"position: relative; width: 701px; height: 281px; background-image: url(\\\"img/ad_bgr.gif\\\");\"]\r\n";
	line += "mumbaimirror.com##.bottombanner\r\n";
	line += "mumbaimirror.com##.topbanner\r\n";
	line += "music.yahoo.com###YMusicRegion_T3_R2C2_R1\r\n";
	line += "music.yahoo.com###lrec\r\n";
	line += "music.yahoo.com###lrecTop\r\n";
	line += "musicradar.com##.shopping_partners\r\n";
	line += "musicsonglyrics.com###adv_bg\r\n";
	line += "musicsonglyrics.com##td[width=\"300\"][valign=\"top\"]\r\n";
	line += "muskogeephoenix.com##div[style=\"height: 240px; width: 350px; background-color: rgb(238, 238, 238);\"]\r\n";
	line += "my360.com.au##div[style=\"height: 250px;\"]\r\n";
	line += "myfoxny.com##.marketplace\r\n";
	line += "myfoxphoenix.com###leaderboard\r\n";
	line += "myfoxphoenix.com##.marketplace\r\n";
	line += "myfoxphoenix.com##.module.horizontal\r\n";
	line += "myfoxphoenix.com##.vert.expanded\r\n";
	line += "mygaming.co.za##.banner_300\r\n";
	line += "mygaming.co.za##.banner_468\r\n";
	line += "mylifeisaverage.com##.ad\r\n";
	line += "myoutsourcedbrain.com###HTML2\r\n";
	line += "myretrotv.com##img[width=\"875\"][height=\"110\"]\r\n";
	line += "mysearch.com##a.desc > div\r\n";
	line += "myspace.com###marketing\r\n";
	line += "myspace.com###medRec\r\n";
	line += "myspace.com###music_googlelinks\r\n";
	line += "myspace.com###music_medrec\r\n";
	line += "myspace.com###tkn_medrec\r\n";
	line += "myspace.com##.SitesMedRecModule\r\n";
	line += "mystream.to###adv\r\n";
	line += "mystream.to###sysbar\r\n";
	line += "mystream.to##a[href^=\"out/\"]\r\n";
	line += "myway.com##.desc\r\n";
	line += "mywebsearch.com##.desc\r\n";
	line += "narutofan.com###right-spon\r\n";
	line += "nasdaq.com##div[style=\"vertical-align: middle; width: 336px; height: 284px;\"]\r\n";
	line += "nation.co.ke##.c15r\r\n";
	line += "nationalgeographic.com###headerboard\r\n";
	line += "nationalpost.com##.ad\r\n";
	line += "naukri.com##.collMTp\r\n";
	line += "nbc.com###nbc-300\r\n";
	line += "nbcbayarea.com##.ad\r\n";
	line += "nbcbayarea.com##.promo\r\n";
	line += "nbcconnecticut.com###marketingPromo\r\n";
	line += "nbcsandiego.com###partnerBar\r\n";
	line += "nbcsandiego.com##.ad\r\n";
	line += "nbcsports.com###top_90h\r\n";
	line += "ncrypt.in##a[title=\"HIGHSPEED Download\"]\r\n";
	line += "ndtv.com##div[style=\"position: relative; height: 260px; width: 300px;\"]\r\n";
	line += "nearlygood.com###abf\r\n";
	line += "necn.com###main_117\r\n";
	line += "necn.com###main_121\r\n";
	line += "necn.com###main_175\r\n";
	line += "necn.com###right_generic_117\r\n";
	line += "necn.com###right_generic_121\r\n";
	line += "necn.com###right_generic_175\r\n";
	line += "neopets.com###ban_bottom\r\n";
	line += "neopets.com##a[style=\"display: block; margin-left: auto; margin-right: auto; width: 996px; height: 94px;\"]\r\n";
	line += "neowin.net###special-steve\r\n";
	line += "neowin.net##.unspecific\r\n";
	line += "neowin.net##div[style=\"background: url(\\\"/images/atlas/aww2.png\\\") no-repeat scroll center center transparent ! important; height: 250px; width: 300px;\"]\r\n";
	line += "neowin.net##div[style=\"background:url(/images/atlas/aww2.png) no-repeat center center !important;height:250px;width:300px\"]\r\n";
	line += "nerej.com###bottom_banner\r\n";
	line += "nerve.com###topBanner\r\n";
	line += "netchunks.com###af_adblock\r\n";
	line += "netchunks.com###m_top_adblock\r\n";
	line += "netchunks.com###sponsorsM\r\n";
	line += "netmag.co.uk##div[style=\"margin: 0px auto; padding-right: 0px; float: left; padding-bottom: 0px; width: 320px; padding-top: 0px; height: 290px; background-color: rgb(255, 255, 255);\"]\r\n";
	line += "networkworld.com###lb_container_top\r\n";
	line += "networkworld.com###promoslot\r\n";
	line += "networkworld.com##.sponsor\r\n";
	line += "nevadaappeal.com##.youradhere\r\n";
	line += "newcastlejets.com.au##.promotion_wrapper\r\n";
	line += "newgrounds.com##.wide_storepromo\r\n";
	line += "newgrounds.com##.wide_storepromobot\r\n";
	line += "news.aol.co.uk###tdiv60\r\n";
	line += "news.aol.co.uk###tdiv71\r\n";
	line += "news.aol.co.uk###tdiv74\r\n";
	line += "news.cnet.com###bottom-leader\r\n";
	line += "news.com.au##.ad\r\n";
	line += "news.com.au##.sponsors\r\n";
	line += "news.yahoo.com###ymh-invitational-recs\r\n";
	line += "newsarama.com##.marketplace\r\n";
	line += "newsday.co.zw##.articlecontinues\r\n";
	line += "newsday.co.zw##div[style=\"width: 300px; height: 250px;\"]\r\n";
	line += "newsfactor.com##td[style=\"border-left: 1px solid rgb(192, 192, 192); padding-top: 3px; padding-bottom: 3px;\"]\r\n";
	line += "newsmax.com###noprint1\r\n";
	line += "newsmax.com##.sponsors_spacer\r\n";
	line += "newsnet5.com##.ad\r\n";
	line += "newsnet5.com##.marketplace\r\n";
	line += "newsniche.com##a[style=\"font-size: 12px; color: rgb(255, 166, 23);\"]\r\n";
	line += "newsonjapan.com###squarebanner300x250\r\n";
	line += "newsroomamerica.com###promotional\r\n";
	line += "newstatesman.com###footerbanner\r\n";
	line += "newsweek.com##.sponsor\r\n";
	line += "newsweek.com##.sponsorship\r\n";
	line += "nicknz.co.nz##.lrec\r\n";
	line += "nicknz.co.nz##.top-banner\r\n";
	line += "ninemsn.com.au###ad\r\n";
	line += "ninemsn.com.au###bannerTop\r\n";
	line += "ninemsn.seek.com.au###msnhd_div3\r\n";
	line += "nintendolife.com##.the300x250\r\n";
	line += "nitrome.com###banner_box\r\n";
	line += "nitrome.com###banner_description\r\n";
	line += "nitrome.com###banner_shadow\r\n";
	line += "nitrome.com###skyscraper_box\r\n";
	line += "nitrome.com###skyscraper_description\r\n";
	line += "nitrome.com###skyscraper_shadow\r\n";
	line += "nmap.org##img[height=\"90\"][width=\"120\"]\r\n";
	line += "nme.com###editorial_sky\r\n";
	line += "nme.com###skyscraper\r\n";
	line += "northjersey.com##.detail_boxwrap\r\n";
	line += "northjersey.com##.detail_pane_text\r\n";
	line += "notdoppler.com##table[width=\"312\"][height=\"252\"]\r\n";
	line += "notdoppler.com##td[background=\"/img/topad_1a.gif\"]\r\n";
	line += "notdoppler.com##td[background=\"/img/topad_1b.gif\"]\r\n";
	line += "notdoppler.com##td[background=\"/img/topad_1c.gif\"]\r\n";
	line += "notdoppler.com##td[background=\"/img/topad_2a.gif\"]\r\n";
	line += "notdoppler.com##td[height=\"100\"][rowspan=\"3\"]\r\n";
	line += "notdoppler.com##td[style=\"background-image: url(\\\"img/main_topshadow-light.gif\\\"); background-repeat: repeat-x; background-color: rgb(243, 243, 243);\"]\r\n";
	line += "notdoppler.com##td[width=\"728\"][height=\"90\"]\r\n";
	line += "notebooks.com##.efbleft\r\n";
	line += "noupe.com##.ad\r\n";
	line += "novamov.com##.ad\r\n";
	line += "novamov.com##.top_banner\r\n";
	line += "nqfury.com.au##.promotion_wrapper\r\n";
	line += "nullscript.info##div[style=\"border: 2px solid red; margin: 10px; padding: 10px; text-align: left; height: 80px; background-color: rgb(255, 247, 182);\"]\r\n";
	line += "nwanime.com###iwarn\r\n";
	line += "nwsource.com###skyscraperwide\r\n";
	line += "nwsource.com##.adblock\r\n";
	line += "nwsource.com##.googlemiddle\r\n";
	line += "ny1.com##.bannerSidebar\r\n";
	line += "ny1.com##.bannerTop\r\n";
	line += "nyaatorrents.org##a[href^=\"http://www.nyaatorrents.org/a?\"]\r\n";
	line += "nydailynews.com###nydn-topbar\r\n";
	line += "nydailynews.com##.z_sponsor\r\n";
	line += "nymag.com###partner-feeds\r\n";
	line += "nymag.com##.google-bottom\r\n";
	line += "nypost.com##.ad\r\n";
	line += "nyrej.com###bottom_banner\r\n";
	line += "nytimes.com##.ad\r\n";
	line += "nzgamer.com###premierholder\r\n";
	line += "nzgamer.com##.article_banner_holder\r\n";
	line += "nzherald.co.nz##.marketPlace\r\n";
	line += "o2cinemas.com##.links\r\n";
	line += "objectiface.com###top-leaderboard\r\n";
	line += "objectiface.com##.leaderboard\r\n";
	line += "ocregister.com###bannertop2\r\n";
	line += "ocworkbench.com##.shopwidget1\r\n";
	line += "offshore-mag.com##.sponsoredBy\r\n";
	line += "offshore-mag.com##.webcast-promo-box-sponsorname\r\n";
	line += "oldgames.sk###r_TopBar\r\n";
	line += "omg-facts.com###droitetop\r\n";
	line += "omg-facts.com##table[border=\"0\"][width=\"330px\"][height=\"270px\"]\r\n";
	line += "omg.yahoo.com###omg-lrec\r\n";
	line += "omgili.com###ad\r\n";
	line += "oneindia.in##.deal_lists\r\n";
	line += "oneindia.in##.fotfont\r\n";
	line += "oneindia.in##td[width=\"300\"][height=\"250\"]\r\n";
	line += "onjava.com###leaderboard\r\n";
	line += "online-literature.com##.leader-wrap-bottom\r\n";
	line += "online-literature.com##.leader-wrap-middle\r\n";
	line += "online-literature.com##.leader-wrap-top\r\n";
	line += "onlineathens.com##.story-insert\r\n";
	line += "onlineathens.com##.yahoo_hoz\r\n";
	line += "opendiary.com##div[style=\"width: 300px; height: 250px; border: 1px solid black; margin: 0px; padding: 0px;\"]\r\n";
	line += "opendiary.com##div[style=\"width: 728px; height: 90px; margin: 0px auto; padding: 0px;\"]\r\n";
	line += "opendrivers.com###google336x280\r\n";
	line += "orange.co.uk###home_leaderboard\r\n";
	line += "orange.co.uk###home_mpu\r\n";
	line += "orange.co.uk###home_partnerlinks\r\n";
	line += "orange.co.uk###home_shoppinglinks\r\n";
	line += "orange.co.uk##.spon_sored\r\n";
	line += "oreillynet.com###leaderboard\r\n";
	line += "osnews.com##.ad\r\n";
	line += "ourfamilygenes.ca##div[style=\"width: 100%; display: block; margin-bottom: 10px; height: 90px;\"]\r\n";
	line += "ovguide.com##.banner-rectangleMedium\r\n";
	line += "p2pnet.net###sidebar > ul:first-child + table[width=\"19%\"]\r\n";
	line += "p2pnet.net###sidebar2\r\n";
	line += "p2pnet.net##td[align=\"center\"][width=\"100%\"] > a[style=\"border: 0px none ; margin: 0px;\"][target=\"_blank\"] > img\r\n";
	line += "pagead2.googlesyndication.com##html\r\n";
	line += "passedoutphotos.com###top-leaderboard\r\n";
	line += "passedoutphotos.com##.leaderboard\r\n";
	line += "pbs.org###corp-sponsor-sec\r\n";
	line += "pbs.org###masthead1\r\n";
	line += "pbs.org###masthead2\r\n";
	line += "pbs.org##.newshour-support-wrap\r\n";
	line += "pc-freak.net##div[style=\"position: absolute; left: 740px; top: 240px; width: 0px;\"]\r\n";
	line += "pcadvisor.co.uk###broadbandchoices_frm\r\n";
	line += "pcadvisor.co.uk###mastHeadTopLeft\r\n";
	line += "pcauthority.com.au##.featured-retailers\r\n";
	line += "pcgamer.com##.ad\r\n";
	line += "pcmag.com###special_offers_trio\r\n";
	line += "pcmag.com##.content-links\r\n";
	line += "pcmag.com##.partners\r\n";
	line += "pcmag.com##.sp-links\r\n";
	line += "pcmag.com##.special-offers\r\n";
	line += "pcmag.com##.spotlight\r\n";
	line += "pcpro.co.uk###skin\r\n";
	line += "pcpro.co.uk###skyScrapper\r\n";
	line += "pcpro.co.uk##.leaderBoard\r\n";
	line += "pcr-online.biz##.newsinsert\r\n";
	line += "pcstats.com##table[cellpadding=\"2\"][align=\"right\"][width=\"300\"][style=\"border: 1px solid ;\"]\r\n";
	line += "pctipsbox.com###daikos-text-4\r\n";
	line += "pcworld.co.nz###sponsor_div\r\n";
	line += "pcworld.com###bizPromo\r\n";
	line += "pcworld.com###ciscoOOSBlog\r\n";
	line += "pcworld.com###industryWebcasts\r\n";
	line += "pcworld.com###resourceCenters\r\n";
	line += "pcworld.com###resourceLinks\r\n";
	line += "pcworld.com###specialOffers\r\n";
	line += "pcworld.com##.msReminderBadgeBanner\r\n";
	line += "pcworld.com##.skyscraper\r\n";
	line += "pdfmyurl.com##.banner\r\n";
	line += "pdfzone.com##.Skyscraper_BG\r\n";
	line += "pdfzone.com##.sponsors_container\r\n";
	line += "pdfzone.com##div[style=\"float: left; width: 336px; margin-right: 16px; margin-bottom: 5px;\"]\r\n";
	line += "pedulum.com###header_top\r\n";
	line += "penny-arcade.com###funding-h\r\n";
	line += "people.com##.quigo\r\n";
	line += "perfectlytimedphotos.com###top-leaderboard\r\n";
	line += "perfectlytimedphotos.com##.leaderboard\r\n";
	line += "perl.com###leaderboard\r\n";
	line += "perthglory.com.au##.promotion_wrapper\r\n";
	line += "pettube.com###ca\r\n";
	line += "phazeddl.com##a[href^=\"http://www.mydownloader.net/pr/\"]\r\n";
	line += "phazeddl.com##table#searchResult:first-child\r\n";
	line += "phazemp3.com##a[href^=\"http://www.mydownloader.net/pr/\"]\r\n";
	line += "phazemp3.com##table#searchResult:first-child\r\n";
	line += "phonescoop.com###botlink\r\n";
	line += "phonescoop.com###promob\r\n";
	line += "phoronix.com###welcome_screen\r\n";
	line += "photobucket.com##.bannerContainer\r\n";
	line += "phpbb.com##a[rel=\"external affiliate\"]\r\n";
	line += "phpbb.com##a[rel=\"external sponsor\"]\r\n";
	line += "phpbbhacks.com##div[style=\"height: 90px;\"]\r\n";
	line += "picapp.com##.ipad_300_250\r\n";
	line += "picapp.com##.ipad_728_90\r\n";
	line += "ping.eu##td[height=\"9\"][bgcolor=\"white\"][style=\"padding: 10px 25px 0px;\"]\r\n";
	line += "pingtest.net##.ad\r\n";
	line += "pinknews.co.uk##a[href^=\"http://www.pinknews.co.uk/clicks/\"]\r\n";
	line += "pitchero.com###clubSponsor\r\n";
	line += "planetxbox360.com###rightCol3gameHome\r\n";
	line += "planetxbox360.com##div[style=\"margin: 0px 0pt; padding: 2px; width: 300px; height: 250px;\"]\r\n";
	line += "planetxbox360.com##td#rightCol1[align=\"right\"][valign=\"top\"]\r\n";
	line += "planetxbox360.com##td[align=\"center\"][height=\"100\"][bgcolor=\"#3f3f3f\"]\r\n";
	line += "play.tm###lbc\r\n";
	line += "play.tm###sky\r\n";
	line += "playkidsgames.com##table[bgcolor=\"#333333\"][width=\"320\"][height=\"219\"]\r\n";
	line += "playkidsgames.com##table[width=\"100%\"][height=\"105\"]\r\n";
	line += "plusnetwork.com##.more_links\r\n";
	line += "plussports.com##.midBanner\r\n";
	line += "pmptoday.com##div[style=\"background-color: rgb(255, 255, 255); border: 1px solid rgb(51, 0, 0); font-family: Verdana,Arial,Sans-serif; font-size: 10px; padding: 0px; line-height: 11px; color: rgb(0, 0, 0); width: 728px; height: 90px;\"]\r\n";
	line += "politico.com##.in-story-banner\r\n";
	line += "politics.co.uk###top-banner\r\n";
	line += "politifact.com##.pfad\r\n";
	line += "ponged.com##.adv\r\n";
	line += "popbytes.com##div[align=\"left\"][style=\"padding-top: 0px; padding-bottom: 4px; width: 230px; background: none repeat scroll 0% 0% rgb(255, 255, 255);\"]\r\n";
	line += "popbytes.com##div[align=\"left\"][style=\"width: 230px; background: none repeat scroll 0% 0% rgb(255, 255, 255);\"]\r\n";
	line += "popbytes.com##table[cellspacing=\"1\"][cellpadding=\"0\"][border=\"0\"][bgcolor=\"#b9e70c\"]\r\n";
	line += "popbytes.com##table[width=\"229\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][bgcolor=\"#000000\"]\r\n";
	line += "popbytes.com##table[width=\"230\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][bgcolor=\"#000000\"]\r\n";
	line += "popbytes.com##table[width=\"230\"][cellspacing=\"0\"][cellpadding=\"0\"][border=\"0\"][bgcolor=\"#ffffff\"]\r\n";
	line += "popbytes.com##table[width=\"230\"][cellspacing=\"0\"][cellpadding=\"4\"][border=\"0\"][bgcolor=\"#ffffff\"]\r\n";
	line += "popbytes.com##table[width=\"230\"][cellspacing=\"5\"][cellpadding=\"3\"][style=\"overflow: hidden; border: 0px solid rgb(204, 204, 204); background-color: rgb(44, 161, 200);\"]\r\n";
	line += "popeater.com##.sidebarBanner\r\n";
	line += "popularmechanics.com###circ300x100\r\n";
	line += "popularmechanics.com###circ300x200\r\n";
	line += "popularmechanics.com###circ620x100\r\n";
	line += "post-trib.com###zip2save_link_widget\r\n";
	line += "press-citizen.com##.rightrail-promo\r\n";
	line += "pressf1.co.nz###sponsor_div\r\n";
	line += "pri.org###amazonBox180\r\n";
	line += "pricegrabber.co.uk###spl\r\n";
	line += "pricegrabber.com##.topBanner\r\n";
	line += "pricespy.co.nz##.ad\r\n";
	line += "prisonplanet.com###bottombanners\r\n";
	line += "prisonplanet.com###efoods\r\n";
	line += "proaudioreview.com##table[width=\"665\"]\r\n";
	line += "productreview.com.au##td[width=\"160\"][valign=\"top\"]\r\n";
	line += "projectw.org##a[href^=\"http://uploading.com/partners/\"]\r\n";
	line += "ps3news.com###bglink\r\n";
	line += "ps3news.com###sidebar > div > div > table[cellspacing=\"5px\"]:first-child\r\n";
	line += "psu.com###ad\r\n";
	line += "psx-scene.com##tr[valign=\"top\"]:first-child:last-child > td[width=\"125\"][valign=\"top\"][style=\"padding-left: 5px;\"]:last-child\r\n";
	line += "ptinews.com##.fullstoryadd\r\n";
	line += "ptinews.com##.fullstorydivright\r\n";
	line += "publicradio.org###amzContainer\r\n";
	line += "punjabimob.org##a[href*=\".smaato.net\"]\r\n";
	line += "pureoverclock.com###adblock1\r\n";
	line += "pureoverclock.com###adblock2\r\n";
	line += "pureoverclock.com###mainbanner\r\n";
	line += "qj.net###shoppingapi\r\n";
	line += "qj.net##.square\r\n";
	line += "quackit.com###rightColumn\r\n";
	line += "quackit.com##div[style=\"margin: auto; width: 180px; height: 250px; text-align: center; background: url(\\\"/pix/ads/ad_zappyhost_search_box_180x250.gif\\\") no-repeat scroll left top rgb(255, 255, 255);\"]\r\n";
	line += "quickload.to##a[href^=\"http://www.quickload.to/click.php?id=\"]\r\n";
	line += "quizlet.com##.googlewrap\r\n";
	line += "radaronline.com###videoExternalBanner\r\n";
	line += "radaronline.com###videoSkyscraper\r\n";
	line += "rapidlibrary.com##table[cellspacing=\"1\"][cellpadding=\"3\"][border=\"0\"][width=\"98%\"]\r\n";
	line += "ratemyprofessors.com##.rmp_leaderboard\r\n";
	line += "rawstory.com##td[width=\"101\"][align=\"center\"][style][margin=\"0\"]\r\n";
	line += "readmetro.com##.header\r\n";
	line += "readwriteweb.com###ad_block\r\n";
	line += "readwriteweb.com###fm_conversationalist_zone\r\n";
	line += "readwriteweb.com###rwcloud_promo\r\n";
	line += "readwriteweb.com###rwwpartners\r\n";
	line += "readwriteweb.com###vmware-trial\r\n";
	line += "realitytvobsession.com###glinks\r\n";
	line += "realworldtech.com##.leaderboard_wrapper\r\n";
	line += "receeve.it##.carousel\r\n";
	line += "redbookmag.com###special_offer_300x100\r\n";
	line += "reddit.com##.promotedlink\r\n";
	line += "rediff.com###world_right1\r\n";
	line += "rediff.com###world_top\r\n";
	line += "redmondmag.com##.ad\r\n";
	line += "reference.com###Resource_Center\r\n";
	line += "reference.com###abvFold\r\n";
	line += "reference.com###bannerTop\r\n";
	line += "reference.com###bnrTop\r\n";
	line += "reference.com###centerbanner_game\r\n";
	line += "reference.com###rc\r\n";
	line += "reference.com##.spl_unshd\r\n";
	line += "reference.com##.spl_unshd_NC\r\n";
	line += "rejournal.com##img[style=\"border-width: 0px;\"]\r\n";
	line += "rejournal.com##img[width=\"200\"][height=\"100\"]\r\n";
	line += "reminderfox.mozdev.org###promotion3\r\n";
	line += "restaurants.com##.latad\r\n";
	line += "reverso.net##.columnBanner2\r\n";
	line += "rhylfc.co.uk##.bannergroup\r\n";
	line += "rinkworks.com##table[style=\"float: right; border: 1px solid red; width: 250px; padding: 10px; margin: 10px;\"]\r\n";
	line += "rivals.com###thecontainer\r\n";
	line += "roadfly.com###leaderboardHead\r\n";
	line += "roadfly.com##.adv\r\n";
	line += "roadrunner.com##.leaderboard\r\n";
	line += "robotswithfeelings.com##div[style=\"height: 250px; width: 300px; background-color: rgb(0, 0, 0);\"]\r\n";
	line += "robotswithfeelings.com##div[style=\"height: 90px; width: 728px; margin-left: auto; margin-right: auto; background-color: rgb(0, 0, 0);\"]\r\n";
	line += "robtex.com##div[style=\"width: 728px; height: 90px; margin-left: auto; margin-right: auto;\"]\r\n";
	line += "rockpapershotgun.com##.marketing\r\n";
	line += "rollcall.com##.ad\r\n";
	line += "rollingstone.com##.ad\r\n";
	line += "rotoruadailypost.co.nz##.marketPlace\r\n";
	line += "rottentomatoes.com###afc_sidebar\r\n";
	line += "roughlydrafted.com###banner\r\n";
	line += "roulettereactions.com###top-leaderboard\r\n";
	line += "roulettereactions.com##.leaderboard\r\n";
	line += "royalgazette.com##div[style=\"height: 60px; width: 468px;\"]\r\n";
	line += "rr.com##.leaderboard\r\n";
	line += "rr.com##.leaderboardTop\r\n";
	line += "rs-catalog.com##div[onmouseout=\"this.style.backgroundColor='#fff7b6'\"]\r\n";
	line += "rte.ie###island300x250-inside\r\n";
	line += "rte.ie###story_island\r\n";
	line += "rte.ie###tilesHolder\r\n";
	line += "rte.ie##div[style=\"background-color: rgb(239, 238, 234); text-align: center; width: 728px; height: 92px; padding-top: 2px;\"]\r\n";
	line += "rubbernews.com##td[width=\"250\"]\r\n";
	line += "runescape.com###tb\r\n";
	line += "rushlimbaugh.com###top_leaderboard\r\n";
	line += "rwonline.com##table[width=\"665\"]\r\n";
	line += "sacbee.com###leaderboard\r\n";
	line += "satelliteguys.us##div[style=\"width: 300px; float: right; height: 250px; margin-left: 10px; margin-right: 10px; margin-bottom: 10px;\"]\r\n";
	line += "satelliteguys.us##td[width=\"160\"][valign=\"top\"][align=\"left\"]\r\n";
	line += "schlockmercenary.com##td[colspan=\"3\"]\r\n";
	line += "sci-tech-today.com##td[style=\"border-left: 1px dashed rgb(192, 192, 192); padding: 5px;\"]\r\n";
	line += "sci-tech-today.com##td[style=\"border-left: 1px solid rgb(192, 192, 192); padding-top: 3px; padding-bottom: 3px;\"]\r\n";
	line += "scienceblogs.com###leaderboard\r\n";
	line += "scienceblogs.com##.skyscraper\r\n";
	line += "sciencedaily.com##.rectangle\r\n";
	line += "sciencedaily.com##.skyscraper\r\n";
	line += "sciencedirect.com###leaderboard\r\n";
	line += "scientificamerican.com##a[href^=\"/ad-sections/\"]\r\n";
	line += "scientificamerican.com##div[style=\"height: 275px; margin: 0pt;\"]\r\n";
	line += "scoop.co.nz###top-banner\r\n";
	line += "scoop.co.nz###top-banner-base\r\n";
	line += "scoop.co.nz###topHeader\r\n";
	line += "scotsman.com###banner01\r\n";
	line += "search.aol.ca##.SLL\r\n";
	line += "search.aol.ca##.WOL\r\n";
	line += "search.aol.co.uk##.PMB\r\n";
	line += "search.aol.co.uk##.SLL\r\n";
	line += "search.aol.co.uk##.WOL\r\n";
	line += "search.aol.com##.PMB\r\n";
	line += "search.aol.com##.SLL\r\n";
	line += "search.aol.com##.WOL\r\n";
	line += "search.aol.in##.SLL\r\n";
	line += "search.cnbc.com###ms_aur\r\n";
	line += "search.com##.citeurl\r\n";
	line += "search.com##.dtext\r\n";
	line += "search.com##a[href^=\"http://shareware.search.com/click?\"]\r\n";
	line += "search.excite.co.uk###results11_container\r\n";
	line += "search.excite.co.uk##td[width=\"170\"][valign=\"top\"]\r\n";
	line += "search.icq.com##.more_sp\r\n";
	line += "search.icq.com##.more_sp_end\r\n";
	line += "search.icq.com##.res_sp\r\n";
	line += "search.netscape.com##.SLL\r\n";
	line += "search.netscape.com##.SWOL\r\n";
	line += "search.virginmedia.com##.s-links\r\n";
	line += "search.winamp.com##.SLL\r\n";
	line += "search.winamp.com##.SWOL\r\n";
	line += "search.winamp.com##.WOL\r\n";
	line += "search.yahoo.com###east\r\n";
	line += "search.yahoo.com###sec-col\r\n";
	line += "search.yahoo.com##.bbox\r\n";
	line += "search.yahoo.com##.overture\r\n";
	line += "searchalot.com##td[onmouseout=\"cs()\"]\r\n";
	line += "searchenginejournal.com##.even\r\n";
	line += "searchenginejournal.com##.odd\r\n";
	line += "searchenginesuggestions.com###top-leaderboard\r\n";
	line += "searchenginesuggestions.com##.leaderboard\r\n";
	line += "seattlepi.com##.wingadblock\r\n";
	line += "secretmaryo.org##div[style=\"width: 728px; height: 90px; margin-left: 6px;\"]\r\n";
	line += "securityfocus.com##td[width=\"160\"][bgcolor=\"#eaeaea\"]\r\n";
	line += "securityweek.com###banner\r\n";
	line += "seekfind.org##table[width=\"150\"]\r\n";
	line += "sensis.com.au##.pfpRightParent\r\n";
	line += "sensis.com.au##.pfplist\r\n";
	line += "serialnumber.in##div[style^=\"display: block; position: absolute;\"]\r\n";
	line += "serialnumber.in##div[style^=\"display: block; text-align: center; line-height: normal; visibility: visible; position: absolute;\"]\r\n";
	line += "sevenload.com###superbaannerContainer\r\n";
	line += "sevenload.com###yahoo-container\r\n";
	line += "sfgate.com##.kaango\r\n";
	line += "sfgate.com##.z-sponsored-block\r\n";
	line += "sfx.co.uk###banner\r\n";
	line += "shine.yahoo.com###ylf-ysm-side\r\n";
	line += "shinyshiny.tv##.leaderboard\r\n";
	line += "shitbrix.com###top-leaderboard\r\n";
	line += "shitbrix.com##.leaderboard\r\n";
	line += "shopping.com###featListingSection\r\n";
	line += "shopping.findtarget.com##div[style=\"background: none repeat scroll 0% 0% rgb(255, 255, 255); padding: 0pt 0.4em 0.1em 0pt; margin: 0.3em 0pt;\"]\r\n";
	line += "shopping.net##table[border=\"1\"][width=\"580\"]\r\n";
	line += "shopping.yahoo.com##.shmod-ysm\r\n";
	line += "sify.com##div[style=\"width: 250px; height: 250px;\"]\r\n";
	line += "siliconchip.com.au##td[align=\"RIGHT\"][width=\"50%\"][valign=\"BOTTOM\"]\r\n";
	line += "siliconvalley.com##.blogBox\r\n";
	line += "siliconvalley.com##.lnbbgcolor\r\n";
	line += "silverlight.net##.banner_header\r\n";
	line += "simplyassist.co.uk##.std_BottomLine\r\n";
	line += "simplyhired.com##.featured\r\n";
	line += "siteadvisor.com##.midPageSmallOuterDiv\r\n";
	line += "sitepoint.com##.industrybrains\r\n";
	line += "siteseer.ca###banZone\r\n";
	line += "sixbillionsecrets.com###droitetop\r\n";
	line += "sk-gaming.com###pts\r\n";
	line += "sk-gaming.com###ptsf\r\n";
	line += "skins.be##.shortBioShadowB.w240\r\n";
	line += "skyrock.com###pub_up\r\n";
	line += "slashfood.com##.quigo\r\n";
	line += "slate.com##.bizbox_promo\r\n";
	line += "slideshare.net##.medRecBottom2\r\n";
	line += "sloughobserver.co.uk###buttons-mpu-box\r\n";
	line += "slyck.com##div[style=\"width: 295px; border: 1px solid rgb(221, 221, 221); text-align: center; background: none repeat scroll 0% 0% rgb(255, 255, 255); padding: 5px; font: 12px verdana;\"]\r\n";
	line += "smarter.com##.favboxmiddlesearch\r\n";
	line += "smarter.com##.favwrapper\r\n";
	line += "smash247.com###RT1\r\n";
	line += "smashingmagazine.com###commentsponsortarget\r\n";
	line += "smashingmagazine.com###mediumrectangletarget\r\n";
	line += "smashingmagazine.com###sidebaradtarget\r\n";
	line += "smashingmagazine.com###sponsorlisttarget\r\n";
	line += "smashingmagazine.com##.ed\r\n";
	line += "smh.com.au##.ad\r\n";
	line += "snapfiles.com###bannerbar\r\n";
	line += "snapfiles.com###borderbar\r\n";
	line += "snapfiles.com###prodmsg\r\n";
	line += "snow.co.nz###content-footer-wrap\r\n";
	line += "snow.co.nz###header-banner\r\n";
	line += "snowtv.co.nz###header-banner\r\n";
	line += "soccerphile.com###midbanners\r\n";
	line += "soccerphile.com###topbanners\r\n";
	line += "socialmarker.com###ad\r\n";
	line += "soft32.com##a[href=\"http://p.ly/regbooster\"]\r\n";
	line += "softonic.com##.topbanner\r\n";
	line += "softonic.com##.topbanner_program\r\n";
	line += "softpedia.com##.logotable[align=\"right\"] > a[target=\"_blank\"]\r\n";
	line += "softpedia.com##.pagehead_op2\r\n";
	line += "softpedia.com##img[width=\"600\"][height=\"90\"]\r\n";
	line += "softpedia.com##td[align=\"right\"][style=\"padding-bottom: 5px; padding-left: 22px; padding-right: 17px;\"]\r\n";
	line += "solarmovie.com###l_35061\r\n";
	line += "someecards.com###shop\r\n";
	line += "someecards.com###some-ads\r\n";
	line += "someecards.com###some-more-ads\r\n";
	line += "someecards.com###store\r\n";
	line += "somethingawful.com##.oma_pal\r\n";
	line += "songlyrics.com##.content-bottom-banner\r\n";
	line += "songs.pk##img[width=\"120\"][height=\"60\"]\r\n";
	line += "songs.pk##table[width=\"149\"][height=\"478\"]\r\n";
	line += "songs.pk##td[width=\"100%\"][height=\"20\"]\r\n";
	line += "space.com###expandedBanner\r\n";
	line += "space.com##table[width=\"321\"][height=\"285\"][bgcolor=\"#000000\"]\r\n";
	line += "space.com##td[colspan=\"2\"]:first-child > table[width=\"968\"]:first-child\r\n";
	line += "sparesomelol.com###top-leaderboard\r\n";
	line += "sparesomelol.com##.leaderboard\r\n";
	line += "spectator.org##.ad\r\n";
	line += "spectrum.ieee.org###whtpprs\r\n";
	line += "speedtest.net##.ad\r\n";
	line += "spikedhumor.com###ctl00_CraveBanners\r\n";
	line += "spikedhumor.com##.ad\r\n";
	line += "spoiledphotos.com###top-leaderboard\r\n";
	line += "spoiledphotos.com##.leaderboard\r\n";
	line += "spokesman.com##.ad\r\n";
	line += "squidoo.com###header_banner\r\n";
	line += "stagevu.com##.ad\r\n";
	line += "start64.com##td[height=\"92\"][colspan=\"2\"]\r\n";
	line += "startpage.com###inlinetable\r\n";
	line += "startribune.com###bottomLeaderboard\r\n";
	line += "startribune.com###topLeaderboard\r\n";
	line += "staticice.com.au##table[rules=\"none\"][style=\"border: 1px solid rgb(135, 185, 245);\"]\r\n";
	line += "staticice.com.au##td[align=\"center\"][valign=\"middle\"][height=\"80\"]\r\n";
	line += "sternfannetwork.com##[align=\"center\"] > .tborder[width=\"728\"][cellspacing=\"1\"][cellpadding=\"0\"][border=\"0\"][align=\"center\"]\r\n";
	line += "stickam.com###f_BottomBanner\r\n";
	line += "stickam.com###h_TopBanner\r\n";
	line += "stopdroplol.com###top-leaderboard\r\n";
	line += "stopdroplol.com##.leaderboard\r\n";
	line += "storagereview.com##td[width=\"410\"]:first-child + td[align=\"right\"]\r\n";
	line += "stormfront.org##img[border=\"0\"][rel=\"nofollow\"]\r\n";
	line += "stormfront.org##table[width=\"863\"]\r\n";
	line += "streamingmedia.com##.sponlinkbox\r\n";
	line += "stripes.com##.ad\r\n";
	line += "stumblehere.com##td[width=\"270\"][height=\"110\"]\r\n";
	line += "stv.tv###collapsedBanner\r\n";
	line += "stv.tv###expandedBanner\r\n";
	line += "stv.tv###google\r\n";
	line += "stylelist.com###cod-promo\r\n";
	line += "stylelist.com##.fromsponsor\r\n";
	line += "stylelist.com##.partnerPromo\r\n";
	line += "stylelist.com##div[style=\"position: relative; border: 1px solid rgb(191, 191, 191); background: none repeat scroll 0% 0% white; width: 424px; display: block;\"]\r\n";
	line += "sunderlandecho.com###banner01\r\n";
	line += "sunshinecoastdaily.com.au###localOffers\r\n";
	line += "supernovatube.com##a[href^=\"http://preview.licenseacquisition.org/\"]\r\n";
	line += "superpages.com##.sponsreulst\r\n";
	line += "swamppolitics.com###leaderboard\r\n";
	line += "switched.com###topleader-wrap\r\n";
	line += "switched.com##.medrect\r\n";
	line += "swns.com##.story_mpu\r\n";
	line += "sydneyfc.com##.promotion_wrapper\r\n";
	line += "sydneyolympicfc.com###horiz_image_rotation\r\n";
	line += "sys-con.com###elementDiv\r\n";
	line += "sys-con.com##td[width=\"180\"][valign=\"top\"][rowspan=\"3\"]\r\n";
	line += "talkingpointsmemo.com##.seventwentyeight\r\n";
	line += "talkxbox.com###features-sub\r\n";
	line += "tarot.com###leaderboardOuter\r\n";
	line += "tattoofailure.com###top-leaderboard\r\n";
	line += "tattoofailure.com##.leaderboard\r\n";
	line += "tcmagazine.com###bannerfulltext\r\n";
	line += "tcmagazine.com###topbanner\r\n";
	line += "tcmagazine.info###bannerfulltext\r\n";
	line += "tcmagazine.info###topbanner\r\n";
	line += "tcpalm.com##.bigbox_wrapper\r\n";
	line += "teamliquid.net##div[style=\"width: 472px; height: 64px; overflow: hidden; padding: 0px; margin: 0px;\"]\r\n";
	line += "tech-recipes.com###first-300-ad\r\n";
	line += "tech-recipes.com###leaderboard\r\n";
	line += "techcrunch.com###post_unit_medrec\r\n";
	line += "techcrunch.com##.ad\r\n";
	line += "techcrunchit.com##.ad\r\n";
	line += "techdigest.tv##.leaderboard\r\n";
	line += "techdirt.com##.ad\r\n";
	line += "techguy.org##div[style=\"height: 100px; width: 100%; text-align: center;\"]\r\n";
	line += "techhamlet.com###text-32\r\n";
	line += "technewsworld.com##.content-block-slinks\r\n";
	line += "technewsworld.com##.content-tab-slinks\r\n";
	line += "technologyreview.com##div[style=\"padding-bottom: 8px;\"]\r\n";
	line += "technologyreview.com##div[style=\"text-align: center; background: url(\\\"/images/divider_horiz.gif\\\") repeat-x scroll left bottom transparent; padding: 10px;\"]\r\n";
	line += "technologyreview.com##p[style=\"clear: both; text-align: center; background: url(\\\"/images/divider_horiz.gif\\\") repeat-x scroll left bottom transparent; font-size: 11px; padding: 0pt; margin: 0pt;\"]\r\n";
	line += "technorati.com###ad\r\n";
	line += "technorati.com##.ad\r\n";
	line += "techrepublic.com.com###medusa\r\n";
	line += "techrepublic.com.com###ppeHotspot\r\n";
	line += "techrepublic.com.com###spotlight\r\n";
	line += "techrepublic.com.com###wpPromo\r\n";
	line += "techrepublic.com.com##.essentialTopics\r\n";
	line += "techrepublic.com.com##.hotspot\r\n";
	line += "techwatch.co.uk##table[width=\"250\"][height=\"300\"]\r\n";
	line += "techweb.com###h_banner\r\n";
	line += "tectonic.co.za##.tdad125\r\n";
	line += "teenhut.net##td[align=\"left\"][width=\"160\"][valign=\"top\"]\r\n";
	line += "teesoft.info###footer-800\r\n";
	line += "teesoft.info###uniblue\r\n";
	line += "telecompaper.com##.side_banner\r\n";
	line += "telegramcommunications.com###leftBanner\r\n";
	line += "telegramcommunications.com###rightBanner\r\n";
	line += "telegraph.co.uk###gafsslot1\r\n";
	line += "telegraph.co.uk###gafsslot2\r\n";
	line += "telegraph.co.uk##.comPuff\r\n";
	line += "telegraph.co.uk##a[href^=\"http://www.telegraph.co.uk/sponsored/\"]\r\n";
	line += "telegraphindia.com##.Caption\r\n";
	line += "televisionbroadcast.com##table[width=\"665\"]\r\n";
	line += "tesco.com###dartLeftSkipper\r\n";
	line += "tesco.com###dartRightSkipper\r\n";
	line += "tesco.com##.dart\r\n";
	line += "tf2maps.net##a[href=\"http://forums.tf2maps.net/payments.php\"]\r\n";
	line += "tf2maps.net##form[name=\"search\"] + div + fieldset\r\n";
	line += "tf2maps.net##form[name=\"search\"] + div + fieldset + br + br + fieldset\r\n";
	line += "tfportal.net###snt_wrapper\r\n";
	line += "tgdaily.com###right-banner\r\n";
	line += "thatvideogameblog.com##table[width=\"310\"][height=\"260\"]\r\n";
	line += "thatvideosite.com##div[style=\"padding-bottom: 15px; height: 250px;\"]\r\n";
	line += "the217.com###textpromo\r\n";
	line += "theaa.com###unanimis1\r\n";
	line += "theage.com.au##.ad\r\n";
	line += "thebizzare.com##.adblock\r\n";
	line += "thecelebritycafe.com##table[width=\"135\"][height=\"240\"]\r\n";
	line += "thecourier.co.uk###sidebarMiddleCol\r\n";
	line += "theeagle.com##.SectionRightRail300x600Box\r\n";
	line += "theeastafrican.co.ke##.c15r\r\n";
	line += "thefashionspot.com###roadblock\r\n";
	line += "thefreedictionary.com###Ov\r\n";
	line += "thefreedictionary.com##.Ov\r\n";
	line += "thefrisky.com##.partner-link-boxes-container\r\n";
	line += "thegameslist.com##.leader\r\n";
	line += "thegauntlet.ca##div[style=\"width: 170px; height: 620px; background: url(\\\"/advertisers/your-ad-here-160x600.gif\\\") repeat scroll 0% 0% rgb(204, 204, 204); vertical-align: top; text-align: center;\"]\r\n";
	line += "thegauntlet.ca##div[style=\"width: 190px; height: 110px; background: url(\\\"/advertisers/your-ad-here-180x90.gif\\\") repeat scroll 0% 0% rgb(204, 204, 204); vertical-align: top; text-align: center;\"]\r\n";
	line += "thegauntlet.ca##div[style=\"width: 190px; height: 170px; background: url(\\\"/advertisers/your-ad-here-180x150.gif\\\") repeat scroll 0% 0% rgb(204, 204, 204); vertical-align: top; text-align: center;\"]\r\n";
	line += "thegauntlet.ca##div[style=\"width: 738px; height: 110px; background: url(\\\"/advertisers/your-ad-here-728x90.gif\\\") repeat scroll 0% 0% rgb(204, 204, 204); vertical-align: top; text-align: center;\"]\r\n";
	line += "theglobeandmail.com##.ad\r\n";
	line += "thegrumpiest.com##td[align=\"left\"][width=\"135px\"]\r\n";
	line += "thegrumpiest.com##td[align=\"left\"][width=\"135px\"] + td#table1\r\n";
	line += "thehill.com###topbanner\r\n";
	line += "thehill.com##.banner\r\n";
	line += "thehill.com##.lbanner\r\n";
	line += "thehill.com##.vbanner\r\n";
	line += "thelocalweb.net##.verdana9green\r\n";
	line += "themaineedge.com##td[height=\"80\"][style=\"background-color: rgb(0, 0, 0);\"]\r\n";
	line += "themaineedge.com##td[width=\"180\"][style=\"background-color: rgb(51, 95, 155); text-align: center;\"]\r\n";
	line += "themoscowtimes.com##.adv_block\r\n";
	line += "themoscowtimes.com##.top_banner\r\n";
	line += "thenation.com##.ad\r\n";
	line += "thenation.com##.modalContainer\r\n";
	line += "thenation.com##.modalOverlay\r\n";
	line += "thenextweb.com##.promo\r\n";
	line += "thenextweb.com##.promotion_frame\r\n";
	line += "theonion.com##.ad\r\n";
	line += "thepittsburghchannel.com##.MS\r\n";
	line += "thepspblog.com###featured\r\n";
	line += "thepspblog.com###mta_bar\r\n";
	line += "theregister.co.uk###jobs-promo\r\n";
	line += "theregister.co.uk###msdn-promo\r\n";
	line += "theregister.co.uk##.papers-promo\r\n";
	line += "theregister.co.uk##.wptl\r\n";
	line += "thesaurus.com###abvFold\r\n";
	line += "thesaurus.com##.spl_unshd\r\n";
	line += "theserverside.com###leaderboard\r\n";
	line += "thesixthaxis.com##.map-header-mainblock\r\n";
	line += "thesixthaxis.com##.map-main-right-takeover\r\n";
	line += "thesixtyone.com##div[style=\"width: 968px; text-align: center; margin-top: 12px; clear: both; float: left;\"]\r\n";
	line += "thesmokinggun.com###skyscraper\r\n";
	line += "thestandard.com###leaderboard_banner\r\n";
	line += "thestates.fm###banbo\r\n";
	line += "thestreet.com###brokerage\r\n";
	line += "thestreet.com###textLinks\r\n";
	line += "thestreet.com###textLinksContainer\r\n";
	line += "thesun.co.uk###takeoverleft\r\n";
	line += "thesun.co.uk###takeoverright\r\n";
	line += "thesun.co.uk##.float-right.padding-left-10.width-300.padding-bottom-10.padding-top-10\r\n";
	line += "thesun.co.uk##.srch_cont\r\n";
	line += "thesuperficial.com###leaderboard\r\n";
	line += "thetandd.com##.yahoo_content_match\r\n";
	line += "thevarguy.com###middlebannerwrapper\r\n";
	line += "thevarguy.com##.squarebanner160x160\r\n";
	line += "thinkpads.com###sponsorbar\r\n";
	line += "thisisbath.co.uk###mast-head\r\n";
	line += "thisisbristol.co.uk###mast-head\r\n";
	line += "thisisleicestershire.co.uk###mast-head\r\n";
	line += "thisisleicestershire.co.uk##.banner-extButton\r\n";
	line += "thisismoney.co.uk###Sky\r\n";
	line += "thisisplymouth.co.uk##.leaderboard\r\n";
	line += "threatpost.com###partners\r\n";
	line += "tidbits.com###top_banner\r\n";
	line += "tigerdirect.ca##div[style=\"width: 936px; clear: both; margin-top: 2px; height: 90px;\"]\r\n";
	line += "tigerdroppings.com##td[height=\"95\"][bgcolor=\"#dedede\"]\r\n";
	line += "time.com##.sep\r\n";
	line += "timeanddate.com##fieldset[style=\"float: right; width: 180px;\"]\r\n";
	line += "timeout.com##.MD_textLinks01\r\n";
	line += "timeoutdubai.com###tleaderb\r\n";
	line += "timesdispatch.com###dealoftheday\r\n";
	line += "timesnewsline.com##div[style=\"border: 1px solid rgb(227, 227, 227); background: none repeat scroll 0% 0% rgb(255, 248, 221); padding: 5px; width: 95%;\"]\r\n";
	line += "timesnewsline.com##table[width=\"300\"][height=\"250\"][align=\"left\"]\r\n";
	line += "timesofindia.indiatimes.com##div[style=\"float: left; padding-left: 5px;\"]\r\n";
	line += "timesofindia.indiatimes.com##div[style=\"height: 100px;\"]\r\n";
	line += "timesonline.co.uk##.bg-f0eff5.padding-left-right-9.padding-top-6.link-width.word-wrap\r\n";
	line += "timesonline.co.uk##.bg-f0eff5.padding-left-right-9.padding-top-6.padding-bottom-7.word-wrap\r\n";
	line += "timesonline.co.uk##.classifieds-long-container\r\n";
	line += "tinypic.com##.ad\r\n";
	line += "tinypic.com##.medrec\r\n";
	line += "tips.net###googlebig\r\n";
	line += "titantv.com##.leaderboard\r\n";
	line += "tmz.com###leaderboard\r\n";
	line += "tmz.com###skyscraper\r\n";
	line += "tnt.tv###right300x250\r\n";
	line += "todaystmj4.com###leaderboard1\r\n";
	line += "todaytechnews.com##.advText\r\n";
	line += "tomsgames.com###pub_header\r\n";
	line += "tomsgames.it###pub_header\r\n";
	line += "tomsguide.com##.sideOffers\r\n";
	line += "tomwans.com##a.big_button[target=\"_blank\"]\r\n";
	line += "toofab.com###leaderboard\r\n";
	line += "top4download.com##div[style=\"float: left; width: 620px; height: 250px; clear: both;\"]\r\n";
	line += "top4download.com##div[style=\"width: 450px; height: 205px; clear: both;\"]\r\n";
	line += "topgear.com###skyscraper\r\n";
	line += "topix.com###freecredit\r\n";
	line += "topix.com###krillion_container\r\n";
	line += "topsocial.info##a[href^=\"http://click.search123.uk.com/\"]\r\n";
	line += "toptechnews.com##.regtext[style=\"border: 1px solid rgb(192, 192, 192); padding: 5px;\"]\r\n";
	line += "toptechnews.com##table[width=\"370\"][cellpadding=\"10\"][style=\"border: 1px solid rgb(204, 204, 204); border-collapse: collapse;\"]\r\n";
	line += "toptechnews.com##table[width=\"990\"][cellpadding=\"5\"]\r\n";
	line += "toptenreviews.com##.google_add_container\r\n";
	line += "toptut.com##.af-form\r\n";
	line += "torontosun.com###buttonRow\r\n";
	line += "torrent-finder.com##.cont_lb\r\n";
	line += "torrents.to##.da-top\r\n";
	line += "torrentz.com##div[style=\"width: 1000px; margin: 0pt auto;\"]\r\n";
	line += "totalfark.com###rightSideRightMenubar\r\n";
	line += "totalfilm.com###mpu_container\r\n";
	line += "totalfilm.com###skyscraper_container\r\n";
	line += "tothepc.com##.sidebsa\r\n";
	line += "toynews-online.biz##.newsinsert\r\n";
	line += "travel.yahoo.com##.spon\r\n";
	line += "travel.yahoo.com##.tgl-block\r\n";
	line += "treatmentforbruises.net##.fltlft\r\n";
	line += "treatmentforbruises.net##.fltrt\r\n";
	line += "treehugger.com##.google-indiv-box2\r\n";
	line += "treehugger.com##.leaderboard\r\n";
	line += "tripadvisor.ca##.commerce\r\n";
	line += "tripadvisor.co.uk##.commerce\r\n";
	line += "tripadvisor.com##.commerce\r\n";
	line += "tripadvisor.ie##.commerce\r\n";
	line += "tripadvisor.in##.commerce\r\n";
	line += "trovit.co.uk##.wrapper_trovit_ppc\r\n";
	line += "trucknetuk.com###page-body > div[style=\"margin: 0pt auto; text-align: center;\"]\r\n";
	line += "trucknetuk.com##table[width=\"100%\"][bgcolor=\"#cecbce\"] > tbody > tr > #sidebarright[valign=\"top\"]:last-child\r\n";
	line += "trucknetuk.com##table[width=\"620\"][cellspacing=\"3\"][bgcolor=\"#ffffff\"][align=\"center\"][style=\"border: thin solid black;\"]\r\n";
	line += "trueslant.com##.bot_banner\r\n";
	line += "trustedreviews.com###bottom-sky\r\n";
	line += "trustedreviews.com###top-sky\r\n";
	line += "trutv.com##.banner\r\n";
	line += "tsviewer.com###layer\r\n";
	line += "tuaw.com##.medrect\r\n";
	line += "tuaw.com##.topleader\r\n";
	line += "tucows.com##.w952.h85\r\n";
	line += "tucsoncitizen.com##.bigbox_container\r\n";
	line += "tucsoncitizen.com##.leaderboard_container_top\r\n";
	line += "tucsoncitizen.com##.skyscraper_container\r\n";
	line += "tutsplus.com###AdobeBanner\r\n";
	line += "tutsplus.com##.leader_board\r\n";
	line += "tutzone.net###bigBox\r\n";
	line += "tv.yahoo.com##.spons\r\n";
	line += "tvgolo.com##.inner2\r\n";
	line += "tvgolo.com##.title-box4\r\n";
	line += "tvgolo.com##.title-box5\r\n";
	line += "tvguide.co.uk##table[width=\"160\"][height=\"620\"]\r\n";
	line += "tvsquad.com###tvsquad_topBanner\r\n";
	line += "tvsquad.com##.banner\r\n";
	line += "tvtechnology.com##table[width=\"665\"]\r\n";
	line += "twcenter.net##div[style=\"width: 728px; height: 90px; margin: 1em auto 0pt;\"]\r\n";
	line += "twilightwap.com##.ahblock2\r\n";
	line += "twitter.com##.promoted-account\r\n";
	line += "twitter.com##.promoted-trend\r\n";
	line += "twitter.com##.promoted-tweet\r\n";
	line += "twitter.com##li[data*=\"advertiser_id\"]\r\n";
	line += "u-file.net##.spottt_tb\r\n";
	line += "ucas.com##a[href^=\"http://eva.ucas.com/s/redirect.php?ad=\"]\r\n";
	line += "ucoz.com##[id^=\"adBar\"]\r\n";
	line += "ugotfile.com##a[href=\"https://www.astrill.com/\"]\r\n";
	line += "ugotfile.com##a[href^=\"http://ugotfile.com/affiliate?\"]\r\n";
	line += "ukclimbing.com##img[width=\"250\"][height=\"350\"]\r\n";
	line += "ultimate-guitar.com##.pca\r\n";
	line += "ultimate-guitar.com##.pca2\r\n";
	line += "ultimate-guitar.com##td[align=\"center\"][width=\"160\"]\r\n";
	line += "ultimate-guitar.com##td[style=\"height: 110px; vertical-align: middle; text-align: center;\"]\r\n";
	line += "ultimate-guitar.com##td[width=\"100%\"][valign=\"middle\"][height=\"110\"]\r\n";
	line += "ultimate-rihanna.com###ad\r\n";
	line += "uncoached.com###sidebar300X250\r\n";
	line += "united-ddl.com##table[width=\"435\"][bgcolor=\"#575e57\"]\r\n";
	line += "unknown-horizons.org###akct\r\n";
	line += "unrealitymag.com###header\r\n";
	line += "unrealitymag.com###sidebar300X250\r\n";
	line += "uploaded.to##div[style=\"background-repeat: no-repeat; width: 728px; height: 90px; margin-left: 0px;\"]\r\n";
	line += "uploading.com##div[style=\"background: rgb(246, 246, 246) none repeat scroll 0% 0%; width: 35%; -moz-background-clip: border; -moz-background-origin: padding; -moz-background-inline-policy: continuous; height: 254px;\"]\r\n";
	line += "uploading.com##div[style=\"margin: -2px auto 19px; display: block; position: relative;\"]\r\n";
	line += "uploadville.com##a[href^=\"http://www.flvpro.com/movies/?aff=\"]\r\n";
	line += "uploadville.com##a[href^=\"http://www.gygan.com/affiliate/\"]\r\n";
	line += "urbandictionary.com###dfp_define_rectangle\r\n";
	line += "urbandictionary.com###dfp_homepage_medium_rectangle\r\n";
	line += "urbandictionary.com###dfp_skyscraper\r\n";
	line += "urbandictionary.com###rollup\r\n";
	line += "urbandictionary.com##.zazzle_links\r\n";
	line += "url.org###resspons1\r\n";
	line += "url.org###resspons2\r\n";
	line += "urlesque.com##.sidebarBanner\r\n";
	line += "urlesque.com##.topBanner\r\n";
	line += "usatoday.com###expandedBanner\r\n";
	line += "usatoday.com###footerSponsorOne\r\n";
	line += "usatoday.com###footerSponsorTwo\r\n";
	line += "usatoday.com##.ad\r\n";
	line += "usautoparts.net##td[height=\"111\"][align=\"center\"][valign=\"top\"]\r\n";
	line += "userscripts.org##.sponsor\r\n";
	line += "userstyles.org##.ad\r\n";
	line += "usnews.com##.ad\r\n";
	line += "usniff.com###bottom\r\n";
	line += "usniff.com##.top-usniff-torrents\r\n";
	line += "v3.co.uk###superSky\r\n";
	line += "v3.co.uk##.ad\r\n";
	line += "v3.co.uk##.hpu\r\n";
	line += "v3.co.uk##.leaderboard\r\n";
	line += "v8x.com.au##td[align=\"RIGHT\"][width=\"50%\"][valign=\"BOTTOM\"]\r\n";
	line += "variety.com###googlesearch\r\n";
	line += "variety.com###w300x250\r\n";
	line += "variety.com##.sponsor\r\n";
	line += "veehd.com##.isad\r\n";
	line += "venturebeat.com###leader\r\n";
	line += "venturebeat.com##div[style=\"height: 300px; text-align: center;\"]\r\n";
	line += "verizon.net##.sponsor\r\n";
	line += "vg247.com###leader\r\n";
	line += "vg247.com###rightbar > #halfpage\r\n";
	line += "vidbox.net##.overlayVid\r\n";
	line += "vidbux.com##a[href=\"http://www.vidbux.com/ccount/click.php?id=4\"]\r\n";
	line += "video.foxnews.com###cb_medrect1_div\r\n";
	line += "video2mp3.net###ad\r\n";
	line += "videogamer.com##.skinClick\r\n";
	line += "videogamer.com##.widesky\r\n";
	line += "videography.com##table[width=\"665\"]\r\n";
	line += "videohelp.com###leaderboard\r\n";
	line += "videohelp.com##.stylenormal[width=\"24%\"][valign=\"top\"][align=\"left\"]\r\n";
	line += "videohelp.com##td[valign=\"top\"][height=\"200\"][style=\"background-color: rgb(255, 255, 255);\"]\r\n";
	line += "videojug.com##.forceMPUSize\r\n";
	line += "videoweed.com##.ad\r\n";
	line += "videoweed.com##div[style=\"width: 460px; height: 60px; border: 1px solid rgb(204, 204, 204); margin: 0px auto 10px;\"]\r\n";
	line += "videoweed.com##div[style^=\"width: 160px; height: 600px; border: 1px solid rgb(204, 204, 204); float:\"]\r\n";
	line += "vidreel.com##.overlayVid\r\n";
	line += "vidxden.com###divxshowboxt > a[target=\"_blank\"] > img[width=\"158\"]\r\n";
	line += "vidxden.com##.ad\r\n";
	line += "vidxden.com##.header_greenbar\r\n";
	line += "vimeo.com##.ad\r\n";
	line += "vioku.com##.ad\r\n";
	line += "virginmedia.com##.s-links\r\n";
	line += "virtualnights.com###head-banner\r\n";
	line += "virus.gr###block-block-19\r\n";
	line += "viz.com##div[style^=\"position: absolute; width: 742px; height: 90px;\"]\r\n";
	line += "vladtv.com###banner-bottom\r\n";
	line += "w2c.in##[href^=\"http://c.admob.com/\"]\r\n";
	line += "w3schools.com##a[rel=\"nofollow\"]\r\n";
	line += "w3schools.com##div[style=\"width: 890px; height: 94px; position: relative; margin: 0px; padding: 0px; overflow: hidden;\"]\r\n";
	line += "walesonline.co.uk##.promobottom\r\n";
	line += "walesonline.co.uk##.promotop\r\n";
	line += "walletpop.com###attic\r\n";
	line += "walletpop.com##.medrect\r\n";
	line += "walletpop.com##.sponsWidget\r\n";
	line += "walyou.com##.ad\r\n";
	line += "warez-files.com##.premium_results\r\n";
	line += "warezchick.com##div.top > p:last-child\r\n";
	line += "warezchick.com##img[border=\"0\"]\r\n";
	line += "wareznova.com##img[width=\"298\"][height=\"53\"]\r\n";
	line += "wareznova.com##img[width=\"468\"]\r\n";
	line += "wareznova.com##input[value=\"Download from DLP\"]\r\n";
	line += "wareznova.com##input[value=\"Start Premium Downloader\"]\r\n";
	line += "washingtonexaminer.com###header_top\r\n";
	line += "washingtonpost.com###textlinkWrapper\r\n";
	line += "washingtonscene.thehill.com##.top\r\n";
	line += "wasterecyclingnews.com##.bigbanner\r\n";
	line += "watoday.com.au##.ad\r\n";
	line += "wattpad.com##div[style=\"width: 100%; height: 90px; text-align: center;\"]\r\n";
	line += "weather.ninemsn.com.au###msnhd_div3\r\n";
	line += "weatherbug.com##.wXcds1\r\n";
	line += "weatherbug.com##.wXcds2\r\n";
	line += "webdesignerwall.com##.ad\r\n";
	line += "webdesignstuff.com###headbanner\r\n";
	line += "webopedia.com##.bstext\r\n";
	line += "webpronews.com##.articleleftcol\r\n";
	line += "webresourcesdepot.com##.Banners\r\n";
	line += "webresourcesdepot.com##img[width=\"452px\"][height=\"60px\"]\r\n";
	line += "webworldindex.com##table[bgcolor=\"#ceddf0\"]\r\n";
	line += "weddingmuseum.com##a[href^=\"http://click.linksynergy.com/\"]\r\n";
	line += "weeklyworldnews.com##.top-banner\r\n";
	line += "wefindads.co.uk##div.posts-holder[style=\"margin-top: 10px;\"]\r\n";
	line += "wefollow.com##.ad\r\n";
	line += "wenn.com###topbanner\r\n";
	line += "weselectmodels.com##div[style=\"width: 728px; height: 90px; background-color: black; text-align: center;\"]\r\n";
	line += "westlothianhp.co.uk###banner01\r\n";
	line += "westsussextoday.co.uk###banner01\r\n";
	line += "wftv.com###leaderboard-sticky\r\n";
	line += "whatismyip.com##.gotomypc\r\n";
	line += "whatismyip.com##span[style=\"margin: 2px; float: left; width: 301px; height: 251px;\"]\r\n";
	line += "wheels.ca##div[style=\"color: rgb(153, 153, 153); font-size: 9px; clear: both; border-top: 1px solid rgb(238, 238, 238); padding-top: 15px;\"]\r\n";
	line += "wheels.ca##div[style=\"float: left; width: 237px; height: 90px; margin-right: 5px;\"]\r\n";
	line += "wheels.ca##div[style=\"float: left; width: 728px; height: 90px; z-index: 200000;\"]\r\n";
	line += "whistlestopper.com##td[align=\"left\"][width=\"160\"][valign=\"top\"]\r\n";
	line += "widescreengamingforum.com###banner-content\r\n";
	line += "wikia.com###HOME_LEFT_SKYSCRAPER_1\r\n";
	line += "wikia.com###HOME_TOP_LEADERBOARD\r\n";
	line += "wikia.com###LEFT_SKYSCRAPER_1\r\n";
	line += "wikia.com###LEFT_SKYSCRAPER_2\r\n";
	line += "wikia.com###TOP_LEADERBOARD\r\n";
	line += "winamp.com###subheader\r\n";
	line += "windows7download.com##div[style=\"width: 336px; height: 280px;\"]\r\n";
	line += "windows7download.com##div[style=\"width: 680px; height: 280px; clear: both;\"]\r\n";
	line += "windowsbbs.com##span[style=\"margin: 2px; float: left; width: 337px; height: 281px;\"]\r\n";
	line += "windowsitpro.com###dnn_pentonRoadblock_pnlRoadblock\r\n";
	line += "windowsxlive.net##div[style=\"width: 160px; height: 600px; margin-left: 12px; margin-top: 16px;\"]\r\n";
	line += "windowsxlive.net##div[style=\"width: 336px; height: 380px; float: right; margin: 8px;\"]\r\n";
	line += "winsupersite.com###footerLinks > table[width=\"100%\"]:first-child\r\n";
	line += "winsupersite.com##td[style=\"border-top: 1px none rgb(224, 224, 224); color: rgb(0, 0, 0); font-weight: normal; font-style: normal; font-family: sans-serif; font-size: 8pt; padding-right: 3px; padding-bottom: 3px; padding-top: 3px; text-align: left;\"]\r\n";
	line += "wired.co.uk##.banner-top\r\n";
	line += "wired.co.uk##.banner1\r\n";
	line += "wired.com###featured\r\n";
	line += "wirelessforums.org##td[width=\"160\"][valign=\"top\"]\r\n";
	line += "wisegeek.com##[action=\"/the-best-schools-for-you.htm\"]\r\n";
	line += "wishtv.com###leaderboard\r\n";
	line += "wlfi.com###leaderboard\r\n";
	line += "wordreference.com##.bannertop\r\n";
	line += "workforce.com##td[width=\"970\"][height=\"110\"]\r\n";
	line += "worksopguardian.co.uk###banner01\r\n";
	line += "worldmag.com##div[style=\"padding: 8px 0px; text-align: center;\"]\r\n";
	line += "worldmag.com##div[style=\"text-align: center; padding: 8px 0px; clear: both;\"]\r\n";
	line += "worthdownloading.com##tr:first-child:last-child > td:first-child:last-child > .small_titleGrey[align=\"center\"]:first-child\r\n";
	line += "worthingherald.co.uk###banner01\r\n";
	line += "worthplaying.com##.ad\r\n";
	line += "wow.com###topleader-wrap\r\n";
	line += "wow.com##.medrect\r\n";
	line += "wowwiki.com###HOME_LEFT_SKYSCRAPER_1\r\n";
	line += "wowwiki.com###HOME_TOP_LEADERBOARD\r\n";
	line += "wowwiki.com###LEFT_SKYSCRAPER_1\r\n";
	line += "wowwiki.com###TOP_LEADERBOARD\r\n";
	line += "wpbt2.org##.home_banners\r\n";
	line += "wphostingdiscount.com##.ad\r\n";
	line += "wptv.com##.module.horizontal\r\n";
	line += "wsj.com##.spn_links_box\r\n";
	line += "wwl.com###BannerXGroup\r\n";
	line += "wwtdd.com###showpping\r\n";
	line += "wwtdd.com##.post_insert\r\n";
	line += "wwtdd.com##.s728x90\r\n";
	line += "www.google.co.in##table[cellpadding=\"0\"][width=\"100%\"][style^=\"border: 1px solid\"]\r\n";
	line += "www.google.com##table[cellpadding=\"0\"][width=\"100%\"][style^=\"border: 1px solid\"]\r\n";
	line += "wypr.org###leaderboard\r\n";
	line += "xbox360rally.com###topbanner\r\n";
	line += "xe.com###HomePage_Slot1\r\n";
	line += "xe.com###HomePage_Slot2\r\n";
	line += "xe.com###HomePage_Slot3\r\n";
	line += "xe.com###UCCInputPage_Slot1\r\n";
	line += "xe.com###UCCInputPage_Slot2\r\n";
	line += "xe.com###UCCInputPage_Slot3\r\n";
	line += "xe.com###leaderB\r\n";
	line += "xe.com##.wa_leaderboard\r\n";
	line += "xfm.co.uk###commercial\r\n";
	line += "xml.com###leaderboard\r\n";
	line += "xml.com##.recommended_div2\r\n";
	line += "xml.com##.secondary[width=\"153\"][bgcolor=\"#efefef\"]\r\n";
	line += "xtremesystems.org##embed[width=\"728\"]\r\n";
	line += "xtremesystems.org##img[width=\"728\"]\r\n";
	line += "xtshare.com##.overlayVid\r\n";
	line += "xxlmag.com###medium-rec\r\n";
	line += "yahoo.com###ad\r\n";
	line += "yahoo.com###marketplace\r\n";
	line += "yahoo.com###mw-ysm-cm\r\n";
	line += "yahoo.com###y_provider_promo\r\n";
	line += "yahoo.com###ygmapromo\r\n";
	line += "yahoo.com###ylf-ysm\r\n";
	line += "yahoo.com###yn-gmy-promo-answers\r\n";
	line += "yahoo.com###yn-gmy-promo-groups\r\n";
	line += "yahoo.com##.fpad\r\n";
	line += "yahoo.com##.marketplace\r\n";
	line += "yahoo.com##.y708-commpartners\r\n";
	line += "yahoo.com##.yschspns\r\n";
	line += "yatsoba.com##.sponsors\r\n";
	line += "yauba.com###sidebar > .block_result:first-child\r\n";
	line += "yauba.com##.resultscontent:first-child\r\n";
	line += "yesasia.com##.advHr\r\n";
	line += "yfrog.com##.promo-area\r\n";
	line += "yodawgpics.com###top-leaderboard\r\n";
	line += "yodawgpics.com##.leaderboard\r\n";
	line += "yoimaletyoufinish.com###top-leaderboard\r\n";
	line += "yoimaletyoufinish.com##.leaderboard\r\n";
	line += "yorkshireeveningpost.co.uk###banner01\r\n";
	line += "yorkshirepost.co.uk###banner01\r\n";
	line += "yourmindblown.com##div[style=\"float: right; width: 300px; height: 600px; padding: 10px 0px;\"]\r\n";
	line += "yourmindblown.com##div[style=\"width: 300px; min-height: 250px; padding: 10px 0px; background: none repeat scroll 0% 0% rgb(255, 255, 255);\"]\r\n";
	line += "yourtomtom.com##.bot\r\n";
	line += "yourtomtom.com##div[style=\"height: 600px; padding: 6px 0pt; border: 1px solid rgb(180, 195, 154); background: none repeat scroll 0% 0% rgb(249, 252, 241); margin: 0pt;\"]\r\n";
	line += "youtube.com###feedmodule-PRO\r\n";
	line += "youtube.com###homepage-chrome-side-promo\r\n";
	line += "youtube.com###search-pva\r\n";
	line += "youtube.com###watch-branded-actions\r\n";
	line += "youtube.com###watch-buy-urls\r\n";
	line += "youtube.com##.promoted-videos\r\n";
	line += "youtube.com##.watch-extra-info-column\r\n";
	line += "youtube.com##.watch-extra-info-right\r\n";
	line += "ytmnd.com###please_dont_block_me\r\n";
	line += "ytmnd.com##td[colspan=\"5\"]\r\n";
	line += "zalaa.com##.left_iframe\r\n";
	line += "zalaa.com##.overlayVid\r\n";
	line += "zalaa.com##a[href^=\"http://www.graboid.com/affiliates/\"]\r\n";
	line += "zambiz.co.zm##td[width=\"130\"][height=\"667\"]\r\n";
	line += "zambiz.co.zm##td[width=\"158\"][height=\"667\"]\r\n";
	line += "zath.co.uk##.ad\r\n";
	line += "zdnet.co.uk##.sponsor\r\n";
	line += "zdnet.com###pplayLinks\r\n";
	line += "zdnet.com##.dirListSuperSpons\r\n";
	line += "zdnet.com##.hotspot\r\n";
	line += "zdnet.com##.promoBox\r\n";
	line += "zedomax.com##.entry > div[style=\"width: 100%; height: 280px;\"]\r\n";
	line += "zedomax.com##.entry > div[style=\"width: 336px; height: 280px;\"]\r\n";
	line += "zeenews.com##.ban-720-container\r\n";
	line += "zippyshare.com##.center_reklamy\r\n";
	line += "zomganime.com##a[href=\"http://fs.game321.com/?utm_source=zomganime&utm_medium=skin_banner&utm_term=free&utm_campaign=fs_zomg_skin\"]\r\n";
	line += "zomganime.com##div[style=\"background-color: rgb(153, 153, 153); width: 300px; height: 250px; overflow: hidden; margin: 0pt auto;\"]\r\n";
	line += "zomganime.com##div[style=\"background-color: rgb(239, 239, 239); width: 728px; height: 90px; overflow: hidden;\"]\r\n";
	line += "zomganime.com##marquee[width=\"160\"]\r\n";
	line += "zone.msn.com##.SuperBannerTVMain\r\n";
	line += "zonelyrics.net###panelRng\r\n";
	line += "zoozle.org###search_right\r\n";
	line += "zoozle.org###search_topline\r\n";
	line += "zoozle.org##a[onclick^=\"downloadFile('download_big', null,\"]\r\n";
	line += "zoozle.org##a[onclick^=\"downloadFile('download_related', null,\"]\r\n";
	line += "zuploads.com###buttoncontainer\r\n";
	line += "zuploads.com##.hispeed\r\n";
	line += "zuploads.net###buttoncontainer\r\n";
	line += "zuula.com##.sponsor\r\n";
	line += "zxxo.net##a[href^=\"http://www.linkbucks.com/referral/\"]\r\n";
	line += "!-----------------Whitelists-----------------!\r\n";
	line += "! *** easylist_whitelist.txt ***\r\n";
	line += "@@&adname=$script,domain=sankakucomplex.com\r\n";
	line += "@@||2mdn.net/*/dartshell*.swf\r\n";
	line += "@@||2mdn.net/*_ecw_$image,domain=wwe.com\r\n";
	line += "@@||2mdn.net/crossdomain.xml$object_subrequest\r\n";
	line += "@@||2mdn.net/instream/ads_sdk_config.xml$object_subrequest,domain=youtube.com\r\n";
	line += "@@||2mdn.net/instream/adsapi_$object_subrequest,domain=youtube.com\r\n";
	line += "@@||2mdn.net/viewad/817-grey.gif$object_subrequest,domain=imdb.com\r\n";
	line += "@@||a.ads2.msads.net^*.swf$domain=msnbc.msn.com\r\n";
	line += "@@||a.giantrealm.com/assets/vau/grplayer*.swf\r\n";
	line += "@@||abc.vad.go.com/dynamicvideoad?$object_subrequest\r\n";
	line += "@@||ad.103092804.com/st?ad_type=$subdocument,domain=wizard.mediacoderhq.com\r\n";
	line += "@@||ad.doubleclick.net/adx/nbcu.nbc/rewind$object_subrequest\r\n";
	line += "@@||ad.doubleclick.net/adx/vid.age/$object_subrequest\r\n";
	line += "@@||ad.doubleclick.net/pfadx/nbcu.nbc/rewind$object_subrequest\r\n";
	line += "@@||ad.zanox.com/ppc/$subdocument,domain=wisedock.at|wisedock.co.uk|wisedock.com|wisedock.de|wisedock.eu\r\n";
	line += "@@||ad3.liverail.com^$object_subrequest,domain=breitbart.tv|seesaw.com\r\n";
	line += "@@||adhostingsolutions.com/crossdomain.xml$object_subrequest,domain=novafm.com.au\r\n";
	line += "@@||adjuggler.com^$script,domain=videodetective.com\r\n";
	line += "@@||adm.fwmrm.net^*/admanager.swf?\r\n";
	line += "@@||admin.brightcove.com/viewer/*/advertisingmodule.swf$domain=guardian.co.uk|slate.com\r\n";
	line += "@@||adnet.twitvid.com/crossdomain.xml$object_subrequest\r\n";
	line += "@@||ads.adap.tv/control?$object_subrequest\r\n";
	line += "@@||ads.adap.tv/crossdomain.xml$object_subrequest\r\n";
	line += "@@||ads.adap.tv/redir/client/adplayer.swf$domain=xxlmag.com\r\n";
	line += "@@||ads.adultswim.com/js.ng/site=toonswim&toonswim_pos=600x400_ctr&toonswim_rollup=games$script\r\n";
	line += "@@||ads.belointeractive.com/realmedia/ads/adstream_mjx.ads/www.kgw.com/video/$script\r\n";
	line += "@@||ads.cnn.com/js.ng/*&cnn_intl_subsection=download$script\r\n";
	line += "@@||ads.cricbuzz.com/adserver/units/microsites/faststats.leaderboard.customcode.php$subdocument\r\n";
	line += "@@||ads.forbes.com/realmedia/ads/*@videopreroll$script\r\n";
	line += "@@||ads.fox.com/fox/black_2sec_600.flv\r\n";
	line += "@@||ads.foxnews.com/api/*-slideshow-data.js?\r\n";
	line += "@@||ads.foxnews.com/js/ad.js\r\n";
	line += "@@||ads.foxnews.com/js/omtr_code.js\r\n";
	line += "@@||ads.hulu.com^*.flv\r\n";
	line += "@@||ads.hulu.com^*.swf\r\n";
	line += "@@||ads.jetpackdigital.com.s3.amazonaws.com^$image,domain=vibe.com\r\n";
	line += "@@||ads.jetpackdigital.com/jquery.tools.min.js?$domain=vibe.com\r\n";
	line += "@@||ads.jetpackdigital.com^*/_uploads/$image,domain=vibe.com\r\n";
	line += "@@||ads.monster.com/html.ng/$background,image,subdocument,domain=monster.com\r\n";
	line += "@@||ads.morningstar.com/realmedia/ads/adstream_lx.ads/www.morningstar.com/video/$object_subrequest\r\n";
	line += "@@||ads.revsci.net/adserver/ako?$script,domain=foxbusiness.com|foxnews.com\r\n";
	line += "@@||ads.trutv.com/crossdomain.xml$object_subrequest\r\n";
	line += "@@||ads.trutv.com/html.ng/tile=*&site=trutv&tru_tv_pos=preroll&$object_subrequest\r\n";
	line += "@@||ads.yimg.com/ev/eu/any/$object\r\n";
	line += "@@||ads.yimg.com/ev/eu/any/vint/videointerstitial*.js\r\n";
	line += "@@||ads.yimg.com^*/any/yahoologo$image\r\n";
	line += "@@||ads.yimg.com^*/search/b/syc_logo_2.gif\r\n";
	line += "@@||ads.yimg.com^*videoadmodule*.swf\r\n";
	line += "@@||ads1.msn.com/ads/pronws/$image,domain=live.com\r\n";
	line += "@@||ads1.msn.com/library/dap.js$domain=msnbc.msn.com|wowarmory.com\r\n";
	line += "@@||adserver.bigwigmedia.com/ingamead3.swf\r\n";
	line += "@@||adserver.tvcatchup.com/crossdomain.xml$object_subrequest\r\n";
	line += "@@||adserver.tvcatchup.com/|$object_subrequest\r\n";
	line += "@@||adserver.yahoo.com/a?*&l=head&$script,domain=yahoo.com\r\n";
	line += "@@||adserver.yahoo.com/a?*=headr$script,domain=mail.yahoo.com\r\n";
	line += "@@||adtech.de/crossdomain.xml$object_subrequest,domain=deluxetelevision.com|gigwise.com|nelonen.fi|tv2.dk\r\n";
	line += "@@||app.promo.tubemogul.com/feed/placement.html?id=$script,domain=comedy.com\r\n";
	line += "@@||apple.com^*/video-ad.html\r\n";
	line += "@@||applevideo.edgesuite.net/admedia/*.flv\r\n";
	line += "@@||ar.atwola.com/file/adswrapper.js$script,domain=gasprices.mapquest.com\r\n";
	line += "@@||as.webmd.com/html.ng/transactionid=$object_subrequest\r\n";
	line += "@@||as.webmd.com/html.ng/transactionid=*&frame=$subdocument\r\n";
	line += "@@||assets.idiomag.com/flash/adverts/yume_$object_subrequest\r\n";
	line += "@@||att.com/images/*/admanager/\r\n";
	line += "@@||auctiva.com/listings/checkcustomitemspecifics.aspx?*&adtype=$script\r\n";
	line += "@@||autotrader.co.nz/data/adverts/$image\r\n";
	line += "@@||avclub.com/ads/av-video-ad/$xmlhttprequest\r\n";
	line += "@@||bing.com/images/async?q=$xmlhttprequest\r\n";
	line += "@@||bing.net/images/thumbnail.aspx?q=$image\r\n";
	line += "@@||bitgravity.com/revision3/swf/player/admanager.swf?$object_subrequest,domain=area5.tv\r\n";
	line += "@@||break.com/ads/preroll/$object_subrequest,domain=videosift.com\r\n";
	line += "@@||brothersoft.com/gads/coop_show_download.php?soft_id=$script\r\n";
	line += "@@||burotime.*/xml_*/reklam.xml$object_subrequest\r\n";
	line += "@@||campusfood.com/css/ad.css?\r\n";
	line += "@@||candystand.com/assets/images/ads/$image\r\n";
	line += "@@||cbs.com/sitecommon/includes/cacheable/combine.php?*/adfunctions.\r\n";
	line += "@@||cdn.last.fm/adserver/video/adroll/*/adroll.swf$domain=last.fm\r\n";
	line += "@@||cdn.springboard.gorillanation.com/storage/lightbox_code/static/companion_ads.js$domain=comingsoon.net|gamerevolution.com\r\n";
	line += "@@||channel4.com/media/scripts/oasconfig/siteads.js\r\n";
	line += "@@||chibis.adotube.com/appruntime/player/$object,object_subrequest\r\n";
	line += "@@||chloe.videogamer.com/data/*/videos/adverts/$object_subrequest\r\n";
	line += "@@||cisco.com/html.ng/site=cdc&concept=products$script\r\n";
	line += "@@||clustrmaps.com/images/clustrmaps-back-soon.jpg$third-party\r\n";
	line += "@@||cms.myspacecdn.com/cms/js/ad_wrapper*.js\r\n";
	line += "@@||cnet.com/ads/common/adclient/*.swf\r\n";
	line += "@@||creative.ak.fbcdn.net/ads3/creative/$image,domain=facebook.com\r\n";
	line += "@@||cubeecraft.com/openx/www/\r\n";
	line += "@@||dart.clearchannel.com/html.ng/$object_subrequest,domain=kissfm961.com|radio1045.com\r\n";
	line += "@@||deviantart.com/global/difi/?*&ad_frame=$subdocument\r\n";
	line += "@@||direct.fairfax.com.au/hserver/*/site=vid.*/adtype=embedded/$script\r\n";
	line += "@@||discovery.com/components/consolidate-static/?files=*/adsense-\r\n";
	line += "@@||disneyphotopass.com/adimages/\r\n";
	line += "@@||doubleclick.net/ad/*smartclip$script,domain=last.fm\r\n";
	line += "@@||doubleclick.net/adi/amzn.*;ri=digital-music-track;$subdocument\r\n";
	line += "@@||doubleclick.net/adi/dhd/homepage;sz=728x90;*;pos=top;$subdocument,domain=deadline.com\r\n";
	line += "@@||doubleclick.net/adj/*smartclip$script,domain=last.fm\r\n";
	line += "@@||doubleclick.net/adj/imdb2.consumer.video/*;sz=320x240,$script\r\n";
	line += "@@||doubleclick.net/adj/nbcu.nbc/videoplayer-$script\r\n";
	line += "@@||doubleclick.net/adj/pong.all/*;dcopt=ist;$script\r\n";
	line += "@@||doubleclick.net/pfadx/channel.video.crn/;*;cue=pre;$object_subrequest\r\n";
	line += "@@||doubleclick.net/pfadx/slate.v.video/*;cue=pre;$object_subrequest\r\n";
	line += "@@||doubleclick.net/pfadx/umg.*;sz=10x$script\r\n";
	line += "@@||doubleclick.net^*/adj/wwe.shows/ecw_ecwreplay;*;sz=624x325;$script\r\n";
	line += "@@||doubleclick.net^*/listen/*;sz=$script,domain=last.fm\r\n";
	line += "@@||doubleclick.net^*/ndm.tcm/video;$script,domain=player.video.news.com.au\r\n";
	line += "@@||doubleclick.net^*/videoplayer*=worldnow$subdocument,domain=ktiv.com|wflx.com\r\n";
	line += "@@||dstw.adgear.com/crossdomain.xml$domain=hot899.com|nj1015.com|streamtheworld.com\r\n";
	line += "@@||dstw.adgear.com/impressions/int/as=*.json?ag_r=$object_subrequest,domain=hot899.com|nj1015.com|streamtheworld.com\r\n";
	line += "@@||dyncdn.buzznet.com/catfiles/?f=dojo/*.googleadservices.$script\r\n";
	line += "@@||ebayrtm.com/rtm?rtmcmd&a=json&cb=parent.$script\r\n";
	line += "@@||edgar.pro-g.co.uk/data/*/videos/adverts/$object_subrequest\r\n";
	line += "@@||edmontonjournal.com/js/adsync/adsynclibrary.js\r\n";
	line += "@@||emediate.eu/crossdomain.xml$object_subrequest,domain=tv3play.se\r\n";
	line += "@@||emediate.eu/eas?cu_key=*;ty=playlist;$object_subrequest,domain=tv3play.se\r\n";
	line += "@@||emediate.se/eas_tag.1.0.js$domain=tv3play.se\r\n";
	line += "@@||espn.go.com^*/espn360/banner?$subdocument\r\n";
	line += "@@||eyewonder.com^$object,script,domain=last.fm\r\n";
	line += "@@||eyewonder.com^*/video/$object_subrequest,domain=last.fm\r\n";
	line += "@@||fdimages.fairfax.com.au^*/ffxutils.js$domain=thevine.com.au\r\n";
	line += "@@||feeds.videogamer.com^*/videoad.xml?$object_subrequest\r\n";
	line += "@@||fifa.com/flash/videoplayer/libs/advert_$object_subrequest\r\n";
	line += "@@||fwmrm.net/ad/p/1?$object_subrequest\r\n";
	line += "@@||fwmrm.net/crossdomain.xml$object_subrequest\r\n";
	line += "@@||gannett.gcion.com/addyn/3.0/*/adtech;alias=pluck_signin$script\r\n";
	line += "@@||garrysmod.org/ads/$background,image,script,stylesheet\r\n";
	line += "@@||go.com/dynamicvideoad?$object_subrequest,domain=disney.go.com\r\n";
	line += "@@||google.*/complete/search?$script\r\n";
	line += "@@||google.com/uds/?file=ads&$script,domain=guardian.co.uk\r\n";
	line += "@@||google.com/uds/api/ads/$script,domain=guardian.co.uk\r\n";
	line += "@@||gpacanada.com/img/sponsors/\r\n";
	line += "@@||gr.burstnet.com/crossdomain.xml$object_subrequest,domain=filefront.com\r\n";
	line += "@@||gstatic.com/images?q=$image\r\n";
	line += "@@||guim.co.uk^*/styles/wide/google-ads.css\r\n";
	line += "@@||gws.ign.com/ws/search?*&google_adpage=$script\r\n";
	line += "@@||hp.com/ad-landing/\r\n";
	line += "@@||huffingtonpost.com/images/v/etp_advert.png\r\n";
	line += "@@||i.cdn.turner.com^*/adserviceadapter.swf\r\n";
	line += "@@||i.real.com/ads/*.swf?clicktag=$domain=rollingstone.com\r\n";
	line += "@@||identity-us.com/ads/ads.html\r\n";
	line += "@@||ign.com/js.ng/size=headermainad&site=teamxbox$script,domain=teamxbox.com\r\n";
	line += "@@||ikea.com/ms/img/ads/\r\n";
	line += "@@||img.thedailywtf.com/images/ads/\r\n";
	line += "@@||img.weather.weatherbug.com^*/stickers/$background,image,stylesheet\r\n";
	line += "@@||imgag.com/product/full/el/adaptvadplayer.swf$domain=egreetings.com\r\n";
	line += "@@||imwx.com/js/adstwo/adcontroller.js$domain=weather.com\r\n";
	line += "@@||itv.com^*.adserver.js\r\n";
	line += "@@||itweb.co.za/banners/en-cdt*.gif\r\n";
	line += "@@||jdn.monster.com/render/adservercontinuation.aspx?$subdocument,domain=monster.com\r\n";
	line += "@@||jobs.wa.gov.au/images/advertimages/\r\n";
	line += "@@||js.revsci.net/gateway/gw.js?$domain=foxbusiness.com|foxnews.com\r\n";
	line += "@@||ksl.com/resources/classifieds/graphics/ad_\r\n";
	line += "@@||last.fm/ads.php?zone=*listen$subdocument\r\n";
	line += "@@||lightningcast.net/servlets/getplaylist?*&responsetype=asx&$object\r\n";
	line += "@@||live365.com/mini/blank300x250.html\r\n";
	line += "@@||live365.com/scripts/liveads.js\r\n";
	line += "@@||liverail.com/crossdomain.xml$object_subrequest\r\n";
	line += "@@||liverail.com/swf/*/plugins/flowplayer/\r\n";
	line += "@@||ltassrv.com/crossdomain.xml$object_subrequest,domain=animecrazy.net|gamepro.com\r\n";
	line += "@@||ltassrv.com/yume.swf$domain=animecrazy.net|gamepro.com\r\n";
	line += "@@||ltassrv.com/yume/yume_$object_subrequest,domain=animecrazy.net|gamepro.com\r\n";
	line += "@@||mads.cbs.com/mac-ad?$object_subrequest\r\n";
	line += "@@||mads.com.com/ads/common/faith/*.xml$object_subrequest\r\n";
	line += "@@||marines.com/videos/commercials/$object_subrequest\r\n";
	line += "@@||maxmind.com/app/geoip.js$domain=incgamers.com\r\n";
	line += "@@||media.abc.com/streaming/ads/preroll_$object_subrequest,domain=abc.go.com\r\n";
	line += "@@||media.monster.com/ads/$background,image,domain=monster.com\r\n";
	line += "@@||media.newjobs.com/ads/$background,image,object,domain=monster.com\r\n";
	line += "@@||media.salemwebnetwork.com/js/admanager/swfobject.js$domain=christianity.com\r\n";
	line += "@@||media.scanscout.com/ads/ss_ads3.swf$domain=failblog.org|icanhascheezburger.com\r\n";
	line += "@@||media.washingtonpost.com/wp-srv/ad/ad_v2.js\r\n";
	line += "@@||media.washingtonpost.com/wp-srv/ad/tiffany_manager.js\r\n";
	line += "@@||medrx.sensis.com.au/images/sensis/afl/util.js$domain=afl.com.au\r\n";
	line += "@@||meduniwien.ac.at/homepage/uploads/tx_macinabanners/$image\r\n";
	line += "@@||mercurial.selenic.com/images/sponsors/\r\n";
	line += "@@||mircscripts.org/advertisements.js\r\n";
	line += "@@||mlb.mlb.com/scripts/dc_ads.js\r\n";
	line += "@@||monster.com/services/bannerad.asmx/getadsrc$xmlhttprequest,domain=monster.com\r\n";
	line += "@@||mozilla.com/img/tignish/plugincheck/*/728_90/loading.png$domain=mozilla.com\r\n";
	line += "@@||msads.net/*.swf|$domain=msnbc.msn.com\r\n";
	line += "@@||msads.net/crossdomain.xml$object_subrequest,domain=msnbc.msn.com\r\n";
	line += "@@||msads.net^*.flv|$domain=msnbc.msn.com\r\n";
	line += "@@||mscommodin.webege.com/images/inicio/sponsors/$image\r\n";
	line += "@@||mxtabs.net/ads/interstitial$subdocument\r\n";
	line += "@@||newgrounds.com/ads/ad_medals.gif\r\n";
	line += "@@||newsarama.com/common/js/advertisements.js\r\n";
	line += "@@||newsweek.com/ads/adscripts/prod/*_$script\r\n";
	line += "@@||nick.com/js/ads.jsp\r\n";
	line += "@@||o.aolcdn.com/ads/adswrapper.js$domain=photos.tmz.com\r\n";
	line += "@@||oas.absoluteradio.co.uk/realmedia/ads/$object_subrequest\r\n";
	line += "@@||oas.bigflix.com/realmedia/ads/$object_subrequest\r\n";
	line += "@@||oas.five.tv/realmedia/ads/adstream_sx.ads/demand.five.tv/$object_subrequest\r\n";
	line += "@@||oascentral.feedroom.com/realmedia/ads/adstream_sx.ads/$script,domain=businessweek.com|economist.com|feedroom.com|stanford.edu\r\n";
	line += "@@||oascentral.surfline.com/realmedia/ads/adstream_sx.ads/www.surfline.com/articles$object_subrequest\r\n";
	line += "@@||objects.tremormedia.com/embed/js/$domain=bostonherald.com|deluxetelevision.com\r\n";
	line += "@@||objects.tremormedia.com/embed/swf/acudeoplayer.swf$domain=bostonherald.com|deluxetelevision.com\r\n";
	line += "@@||objects.tremormedia.com/embed/swf/admanager*.swf\r\n";
	line += "@@||objects.tremormedia.com/embed/xml/*.xml?r=$object_subrequest,domain=mydamnchannel.com\r\n";
	line += "@@||omgili.com/ads.search?\r\n";
	line += "@@||omnikool.discovery.com/realmedia/ads/adstream_mjx.ads/dsc.discovery.com/$script\r\n";
	line += "@@||onionstatic.com^*/videoads.js\r\n";
	line += "@@||pagead2.googlesyndication.com/pagead/ads?client=$subdocument,domain=artificialvision.com|metamodal.com|seeingwithsound.com\r\n";
	line += "@@||pagead2.googlesyndication.com/pagead/expansion_embed.js$domain=artificialvision.com|gameserver.n4cer.de|gpxplus.net|metamodal.com|myspace.com|seeingwithsound.com\r\n";
	line += "@@||pagead2.googlesyndication.com/pagead/scache/show_invideo_ads.js$domain=sciencedaily.com\r\n";
	line += "@@||pagead2.googlesyndication.com/pagead/show_ads.js$domain=articlewagon.com|artificialvision.com|gameserver.n4cer.de|gpxplus.net|metamodal.com|myspace.com|omegadrivers.net|seeingwithsound.com|spreadlink.us|warp2search.net\r\n";
	line += "@@||pagead2.googlesyndication.com/pagead/static?format=in_video_ads&$elemhide,subdocument\r\n";
	line += "@@||partner.googleadservices.com/gampad/google_ads.js$domain=avclub.com\r\n";
	line += "@@||partner.googleadservices.com/gampad/google_service.js$domain=avclub.com\r\n";
	line += "@@||partners.thefilter.com/crossdomain.xml$object_subrequest,domain=dailymotion.com|dailymotion.virgilio.it\r\n";
	line += "@@||partners.thefilter.com/dailymotionservice/$image,object_subrequest,script,domain=dailymotion.com|dailymotion.virgilio.it\r\n";
	line += "@@||pix04.revsci.net^*/pcx.js?$script,domain=foxbusiness.com|foxnews.com\r\n";
	line += "@@||pressdisplay.com/advertising/showimage.aspx?\r\n";
	line += "@@||promo2.tubemogul.com/adtags/slim_no_iframe.js$domain=comedy.com\r\n";
	line += "@@||promo2.tubemogul.com/flash/youtube.swf$domain=comedy.com\r\n";
	line += "@@||promo2.tubemogul.com/lib/tubemoguldisplaylib.js$domain=comedy.com\r\n";
	line += "@@||quit.org.au/images/images/ad/\r\n";
	line += "@@||redir.adap.tv/redir/client/adplayer.swf$domain=cracked.com|egreetings.com|ehow.com|imgag.com|videosift.com|xxlmag.com\r\n";
	line += "@@||redir.adap.tv/redir/client/static/as3adplayer.swf$domain=king5.com|newsinc.com|stickam.com|videosift.com|wkbw.com\r\n";
	line += "@@||redir.adap.tv/redir/javascript/adaptvadplayer.js$object_subrequest,domain=imgag.com\r\n";
	line += "@@||rosauers.com/locations/ads.html\r\n";
	line += "@@||rotate.infowars.com/www/delivery/fl.js\r\n";
	line += "@@||rotate.infowars.com/www/delivery/spcjs.php\r\n";
	line += "@@||sam.itv.com/xtserver/acc_random=*.video.preroll/seg=$object_subrequest\r\n";
	line += "@@||sankakucomplex.com^$script\r\n";
	line += "@@||sankakustatic.com^$script\r\n";
	line += "@@||scorecardresearch.com/beacon.js$domain=deviantart.com\r\n";
	line += "@@||search.excite.co.uk/minify.php?files*/css/feed/adsearch.css\r\n";
	line += "@@||seesaw.com/cp/c4/realmedia/ads/adstream_sx.ads/$xmlhttprequest\r\n";
	line += "@@||serve.vdopia.com/adserver/ad*.php$object_subrequest,script,xmlhttprequest\r\n";
	line += "@@||server.cpmstar.com/adviewas3.swf?contentspotid=$object_subrequest,domain=armorgames.com|freewebarcade.com|gamesforwork.com\r\n";
	line += "@@||server.cpmstar.com/view.aspx?poolid=$domain=newgrounds.com\r\n";
	line += "@@||sfx-images.mozilla.org^$image,domain=spreadfirefox.com\r\n";
	line += "@@||smartadserver.com/call/pubj/*/affiliate_id$script,domain=deezer.com\r\n";
	line += "@@||smartadserver.com/def/def/showdef.asp$domain=deezer.com\r\n";
	line += "@@||smartclip.net/delivery/tag?sid=$script,domain=last.fm\r\n";
	line += "@@||sonicstate.com/video/hd/hdconfig-geo.cfm?*/www/delivery/$object_subrequest\r\n";
	line += "@@||southparkstudios.com/layout/common/js/reporting/mtvi_ads_reporting.js\r\n";
	line += "@@||southparkstudios.com/layout/common/js/reporting/mtvi_ads_reporting_config.js\r\n";
	line += "@@||spotrails.com/crossdomain.xml$object_subrequest\r\n";
	line += "@@||spotrails.com^*/flowplayeradplayerplugin.swf\r\n";
	line += "@@||static.2mdn.net^*.xml$object_subrequest,domain=photoradar.com|youtube.com\r\n";
	line += "@@||static.ak.fbcdn.net^*/ads/$script\r\n";
	line += "@@||static.linkbucks.com^$script,stylesheet,domain=zxxo.net\r\n";
	line += "@@||static.scanscout.com/ads/are3.swf$domain=failblog.org|icanhascheezburger.com\r\n";
	line += "@@||superfundo.org/advertisement.js\r\n";
	line += "@@||telegraphcouk.skimlinks.com/api/telegraph.skimlinks.js\r\n";
	line += "@@||thefrisky.com/js/adspaces.min.js\r\n";
	line += "@@||thenewsroom.com^*/advertisement.xml$object_subrequest\r\n";
	line += "@@||theonion.com/ads/video-ad/$object_subrequest,xmlhttprequest\r\n";
	line += "@@||theonion.com^*/videoads.js\r\n";
	line += "@@||thestreet.com/js/ads/adplacer.js\r\n";
	line += "@@||timeinc.net/people/static/i/advertising/getpeopleeverywhere-*$background,domain=people.com|peoplestylewatch.com\r\n";
	line += "@@||timeinc.net^*/tii_ads.js$domain=ew.com\r\n";
	line += "@@||trutv.com/includes/banners/de/video/*.ad|$object_subrequest\r\n";
	line += "@@||turner.com^*/advertisement/cnnmoney_sponsors.gif$domain=money.cnn.com\r\n";
	line += "@@||tvgorge.com^*/adplayer.swf\r\n";
	line += "@@||tvnz.co.nz/stylesheets/tvnz/lib/js/advertisement.js\r\n";
	line += "@@||twitvid.com/mediaplayer_*.swf?\r\n";
	line += "@@||ultrabrown.com/images/adheader.jpg\r\n";
	line += "@@||upload.wikimedia.org/wikipedia/\r\n";
	line += "@@||utarget.co.uk/crossdomain.xml$object_subrequest,domain=tvcatchup.com\r\n";
	line += "@@||vancouversun.com/js/adsync/adsynclibrary.js\r\n";
	line += "@@||video-cdn.abcnews.com/ad_$object_subrequest\r\n";
	line += "@@||videoads.washingtonpost.com^$object_subrequest,domain=slatev.com\r\n";
	line += "@@||vidtech.cbsinteractive.com/plugins/*_adplugin.swf\r\n";
	line += "@@||vortex.accuweather.com/adc2004/pub/ads/js/ads-2006_vod.js\r\n";
	line += "@@||vox-static.liverail.com/swf/*/admanager.swf\r\n";
	line += "@@||vtstage.cbsinteractive.com/plugins/*_adplugin.swf\r\n";
	line += "@@||we7.com/api/streaming/advert-info?*&playsource=$object_subrequest\r\n";
	line += "@@||weather.com/common/a2/oasadframe.html?position=pagespon\r\n";
	line += "@@||weather.com/common/a2/oasadframe.html?position=pointspon\r\n";
	line += "@@||widget.slide.com^*/ads/*/preroll.swf\r\n";
	line += "@@||wikimedia.org^$elemhide\r\n";
	line += "@@||wikipedia.org^$elemhide\r\n";
	line += "@@||wrapper.teamxbox.com/a?size=headermainad&altlocdir=teamxbox$script\r\n";
	line += "@@||www.google.*/search?$subdocument\r\n";
	line += "@@||yallwire.com/pl_ads.php?$object_subrequest\r\n";
	line += "@@||yimg.com^*&yat/js/ads_\r\n";
	line += "@@||yimg.com^*/java/promotions/js/ad_eo_1.1.js\r\n";
	line += "@@||zedo.com/*.swf$domain=rajshri.com\r\n";
	line += "@@||zedo.com/*.xml$object_subrequest,domain=rajshri.com\r\n";
	line += "@@||zedo.com//$object_subrequest,domain=rajshri.com\r\n";
	line += "!Anti-Adblock\r\n";
	line += "@@/_468.gif$domain=seeingwithsound.com\r\n";
	line += "@@/_728.gif$domain=seeingwithsound.com\r\n";
	line += "@@/_728_90.$image,domain=seeingwithsound.com\r\n";
	line += "@@/_728x90.$image,domain=seeingwithsound.com\r\n";
	line += "@@_728by90.$image,domain=seeingwithsound.com\r\n";
	line += "@@||195.241.77.82^$image,domain=seeingwithsound.com\r\n";
	line += "@@||212.115.192.168^$image,domain=seeingwithsound.com\r\n";
	line += "@@||216.97.231.225^$domain=seeingwithsound.com\r\n";
	line += "@@||84.243.214.232^$image,domain=seeingwithsound.com\r\n";
	line += "@@||ads.clicksor.com/showad.php?*&adtype=7&$script,domain=rapid8.com\r\n";
	line += "@@||ads.gtainside.com/openx/ad.js\r\n";
	line += "@@||akihabaranews.com/images/ad/\r\n";
	line += "@@||artificialvision.com^$elemhide,image,script\r\n";
	line += "@@||arto.com/includes/js/adtech.de/script.axd/adframe.js?\r\n";
	line += "@@||avforums.com/forums/adframe.js\r\n";
	line += "@@||cinshare.com/js/embed.js?*=http://adserving.cpxinteractive.com/?\r\n";
	line += "@@||content.ytmnd.com/assets/js/a/adx.js\r\n";
	line += "@@||dailykos.com/ads/adblocker.blogads.css\r\n";
	line += "@@||dropbox.com^$image,script,domain=seeingwithsound.com\r\n";
	line += "@@||eq2flames.com/adframe.js\r\n";
	line += "@@||funkyfun.altervista.org/adsense.js$domain=livevss.net\r\n";
	line += "@@||gdataonline.com/exp/textad.js\r\n";
	line += "@@||googlepages.com^$image,script,domain=artificialvision.com|metamodal.com|seeingwithsound.com\r\n";
	line += "@@||gpxplus.net^$elemhide\r\n";
	line += "@@||hackers.co.id/adframe/adframe.js\r\n";
	line += "@@||home.tiscali.nl^$domain=seeingwithsound.com\r\n";
	line += "@@||livevss.net/adsense.js\r\n";
	line += "@@||lunarpages.com^$image,script,domain=artificialvision.com|metamodal.com|seeingwithsound.com\r\n";
	line += "@@||macobserver.com/js/adlink.js\r\n";
	line += "@@||metamodal.com^$elemhide,image,script\r\n";
	line += "@@||multi-load.com/peel.js$domain=multi-load.com\r\n";
	line += "@@||multiup.org/advertisement.js\r\n";
	line += "@@||ninjaraider.com/ads/$script\r\n";
	line += "@@||ninjaraider.com/adsense/$script\r\n";
	line += "@@||novamov.com/ads.js?*&ad_url=/adbanner\r\n";
	line += "@@||nwanime.com^$script\r\n";
	line += "@@||onlinevideoconverter.com/scripts/advertisement.js\r\n";
	line += "@@||pagead2.googlesyndication.com/pagead/render_ads.js$domain=seeingwithsound.com\r\n";
	line += "@@||photobucket.com^$image,domain=seeingwithsound.com\r\n";
	line += "@@||playtv.fr/img/design/adbg.jpg\r\n";
	line += "@@||pub.clicksor.net/newserving/js/show_ad.js$domain=rapid8.com\r\n";
	line += "@@||ratebeer.com/javascript/advertisement.js\r\n";
	line += "@@||seeingwithsound.cn^$image,script,domain=artificialvision.com|metamodal.com|seeingwithsound.com\r\n";
	line += "@@||seeingwithsound.com^$elemhide,image,script\r\n";
	line += "@@||serw.clicksor.com/newserving/getkey.php?$script,domain=rapid8.com\r\n";
	line += "@@||sharejunky.com/adserver/$script\r\n";
	line += "@@||sites.google.com/site/$image,script,domain=artificialvision.com|metamodal.com|seeingwithsound.com\r\n";
	line += "@@||sportsm8.com/adsense.js\r\n";
	line += "@@||spreadlink.us/advertisement.js\r\n";
	line += "@@||succesfactoren.nl^$image,domain=seeingwithsound.com\r\n";
	line += "@@||teknogods.com/advertisement.js\r\n";
	line += "@@||theteacherscorner.net/adiframe/$script\r\n";
	line += "@@||tpmrpg.net/adframe.js\r\n";
	line += "@@||video2mp3.net/img/ad*.js\r\n";
	line += "@@||visualprosthesis.com^$image,script,domain=artificialvision.com|metamodal.com|seeingwithsound.com\r\n";
	line += "@@||zshare.net/ads.js?*&ad_url=/adbanner\r\n";
	line += "!Non-English\r\n";
	line += "@@||24ur.com/adserver/adall.php?*&video_on_page=1\r\n";
	line += "@@||ads.globo.com/crossdomain.xml$object_subrequest\r\n";
	line += "@@||ads.globo.com/realmedia/ads/adstream_jx.ads/$object_subrequest,domain=globo.com\r\n";
	line += "@@||adser.localport.it/banman.asp?zoneid=71$subdocument\r\n";
	line += "@@||adswizz.com/www/delivery/$object_subrequest,domain=alloclips.com|video.belga.be\r\n";
	line += "@@||adtech.de/?adrawdata/3.0/*;|$object_subrequest,domain=nelonen.fi|tv2.dk\r\n";
	line += "@@||adtech.panthercustomer.com^*.flv$domain=tv3.ie\r\n";
	line += "@@||afterdark-nfs.com/ad/$background,image,script,stylesheet\r\n";
	line += "@@||aka-cdn-ns.adtech.de^*.flv$domain=tv3.ie\r\n";
	line += "@@||alimama.cn/taobaocdn/css/s8.css$domain=taobao.com\r\n";
	line += "@@||amarillas.cl/advertise.do?$xmlhttprequest\r\n";
	line += "@@||amarillas.cl/js/advertise/$script\r\n";
	line += "@@||autoscout24.*/all.js.aspx?m=css&*=/stylesheets/adbanner.css\r\n";
	line += "@@||banneradmin.rai.it/js.ng/sezione_rai=barramenu$script\r\n";
	line += "@@||bnrs.ilm.ee/www/delivery/fl.js\r\n";
	line += "@@||cpalead.com/mygateway.php?pub=$script,domain=serialnumber.in|spotifyripping.com|stumblehere.com|videodownloadx.com|yourpcmovies.net\r\n";
	line += "@@||e-planning.net/eb/*?*fvp=2&$object_subrequest,domain=clarin.com|emol.com\r\n";
	line += "@@||ebayrtm.com/rtm?$script,domain=annonces.ebay.fr|ebay.it\r\n";
	line += "@@||fotojorgen.no/images/*/webadverts/\r\n";
	line += "@@||hry.cz/ad/adcode.js\r\n";
	line += "@@||img.deniksport.cz/css/reklama.css?\r\n";
	line += "@@||mail.bg/mail/index/getads/$xmlhttprequest\r\n";
	line += "@@||nextmedia.com/admedia/$object_subrequest\r\n";
	line += "@@||ninjaraider.com^*/adsense.js\r\n";
	line += "@@||openx.motomedia.nl/live/www/delivery/$script\r\n";
	line += "@@||openx.zomoto.nl/live/www/delivery/fl.js\r\n";
	line += "@@||openx.zomoto.nl/live/www/delivery/spcjs.php?id=\r\n";
	line += "@@||pagead2.googlesyndication.com/pagead/abglogo/abg-da-100c-000000.png$domain=janno.dk|nielco.dk\r\n";
	line += "@@||ping.indieclicktv.com/www/delivery/ajs.php?zoneid=$object_subrequest,domain=penny-arcade.com\r\n";
	line += "@@||ring.bg/adserver/adall.php?*&video_on_page=1\r\n";
	line += "@@||static.mobile.eu^*/resources/images/ads/superteaser_$image,domain=automobile.fr|automobile.it|mobile.eu|mobile.ro\r\n";
	line += "@@||style.seznam.cz/ad/im.js\r\n";
	line += "@@||uol.com.br/html.ng/*&affiliate=$object_subrequest\r\n";
	line += "@@||video1.milanofinanza.it/movie/movie/adserver_$object,object_subrequest\r\n";
	line += "@@||virpl.ru^*_advert.php$xmlhttprequest,domain=virpl.ru\r\n";


            // Write file
            DWORD dwBytesWritten = 0;
            if (::WriteFile(hFile, line.GetBuffer(), line.GetLength(), &dwBytesWritten, NULL) && dwBytesWritten == line.GetLength())
            {
		        // Set correct version
                CPluginSettings* settings = CPluginSettings::GetInstance();

                settings->AddFilterUrl(CString(FILTERS_PROTOCOL) + CString(FILTERS_HOST) + "/easylist.txt", 1);
                settings->AddFilterFileName(CString(FILTERS_PROTOCOL) + CString(FILTERS_HOST) + "/easylist.txt", "filter1.txt");
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
