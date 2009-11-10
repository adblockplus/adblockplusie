#ifndef _ADPLUGIN_FILTER_H_
#define _ADPLUGIN_FILTER_H_


#include "AdPluginTypedef.h"


// ============================================================================
// CFilterElementHideAttrSelector
// ============================================================================

class CFilterElementHideAttrSelector
{

public:

    bool m_isStarting;
    bool m_isEnding;
    bool m_isAnywhere;
    bool m_isExact;

    bool m_isStyle;
    bool m_isId;
    bool m_isClass;

    CComBSTR m_bstrAttr;
    CStringA m_value;
    
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

    CStringA m_filterText;
    CStringA m_filterFile;

    // For domain specific filters only
    CStringA m_tagId;
    CStringA m_tagClassName;

    std::set<CStringA> m_domainsNot;

    std::vector<CFilterElementHideAttrSelector> m_attributeSelectors;

    CFilterElementHide(const CStringA& filterText="", const CStringA& filterFile="");
    CFilterElementHide(const CFilterElementHide& filter);
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
    } filterType;

    int m_contentType;
    enum EFilterType m_filterType;
    std::vector<CStringA> m_stringElements;
    bool m_isMatchCase;
    bool m_isFirstParty;
    bool m_isThirdParty;
    bool m_isFromStart;
    bool m_isFromStartDomain;
    bool m_isFromEnd;
    int m_hitCount;
    CStringA m_filterText;
    CStringA m_filterFile;
    std::set<CStringA> m_domains;
    std::set<CStringA> m_domainsNot;

    CFilter(const CFilter&);
    CFilter();
};

// ============================================================================
// CAdPluginFilter
// ============================================================================

class CAdPluginFilter
{

private:

	CStringA m_dataPath;

    std::map<CStringA, int> m_contentMap;
    std::map<int, CStringA> m_contentMapText;

    static CComAutoCriticalSection s_criticalSectionFilterMap;

	typedef std::map<DWORD, CFilter> TFilterMap;
	typedef std::vector<CFilter> TFilterMapDefault;

    // Tag* -> Filter
    typedef std::multimap<CStringA,CFilterElementHide> TFilterElementHideDomain;

    // (Tag,Name) -> Filter
	typedef std::map<std::pair<CStringA,CStringA>, CFilterElementHide> TFilterElementHideTagsNamed;

    // Tag -> Filter
	typedef std::map<CStringA, CFilterElementHide> TFilterElementHideTags;

    // Domain -> Domain list
    typedef std::map<CStringA, TFilterElementHideDomain> TFilterElementHideDomains;

    TFilterElementHideTagsNamed m_elementHideTagsId;
    TFilterElementHideTagsNamed m_elementHideTagsClass;
    TFilterElementHideTags m_elementHideTags;

    TFilterElementHideDomains m_elementHideDomains;

	TFilterMap m_filterMap[2][2];
	TFilterMapDefault m_filterMapDefault[2];

	void ParseFilters(const TFilterFileList& urlList);

    int FindMatch(const CStringA& src, CStringA filterPart, int startPos=0) const;
    bool IsSpecialChar(char testChar) const;
    bool IsSubdomain(const CStringA& subdomain, const CStringA& domain) const;

public:

	CAdPluginFilter(const TFilterFileList& urlList, const CStringA& dataPath);
	CAdPluginFilter(const CStringA& dataPath = "");

    bool ReadFilter(const CStringA& filename, const CStringA& downloadPath="");

	void AddFilter(CStringA filter, CStringA filterFile, int filterType);
	bool AddFilterElementHide(CStringA filter, CStringA filterFile);

    bool IsElementHidden(const CStringA& tag, IHTMLElement* pEl, const CStringA& domain, const CStringA& indent) const;

	const CFilter* MatchFilter(int filterType, const CStringA& src, int contentType, const CStringA& domain) const;
	bool IsMatchFilter(const CFilter& filter, CStringA src, const CStringA& srcDomain, const CStringA& domain) const;

	bool IsMatchFilterElementHide(const CFilterElementHide& filter, IHTMLElement* pEl, const CStringA& domain) const;

#if (defined PRODUCT_ADBLOCKER)
    bool static DownloadFilterFile(const CStringA& url, const CStringA& filename);
    void static CreateFilters();
    bool IsAlive() const;
#endif

	bool ShouldBlock(CStringA src, int contentType, const CStringA& domain, bool addDebug=false) const;
	bool ShouldWhiteList(CStringA url) const;
};


#endif // _ADPLUGIN_FILTER_H_
