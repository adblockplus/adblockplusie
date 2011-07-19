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
	//We don't accept too short filters. Those are suspicious.
	if (filterString.GetLength() < 5)
		return;

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
