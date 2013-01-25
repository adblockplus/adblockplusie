#ifndef _PLUGIN_FILTER_H_
#define _PLUGIN_FILTER_H_


#include "PluginTypedef.h"


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

    CString m_filterText;
    CString m_filterFile;

    // For domain specific filters only
    CString m_tagId;
    CString m_tagClassName;

    std::set<CString> m_domainsNot;

    std::vector<CFilterElementHideAttrSelector> m_attributeSelectors;

    CFilterElementHide(const CString& filterText="", const CString& filterFile="");
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
		filterTypeUnknown = 3
    } filterType;

    int m_contentType;
    enum EFilterType m_filterType;
    std::vector<CString> m_stringElements;
    bool m_isMatchCase;
    bool m_isFirstParty;
    bool m_isThirdParty;
    bool m_isFromStart;
    bool m_isFromStartDomain;
    bool m_isFromEnd;
    int m_hitCount;
    CString m_filterText;
    CString m_filterFile;
    std::set<CString> m_domains;
    std::set<CString> m_domainsNot;

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

    // Tag* -> Filter
    typedef std::multimap<CString,CFilterElementHide> TFilterElementHideDomain;

    // (Tag,Name) -> Filter
	typedef std::map<std::pair<CString,CString>, CFilterElementHide> TFilterElementHideTagsNamed;

    // Tag -> Filter
	typedef std::map<CString, CFilterElementHide> TFilterElementHideTags;

    // Domain -> Domain list
    typedef std::map<CString, TFilterElementHideDomain> TFilterElementHideDomains;

    TFilterElementHideTagsNamed m_elementHideTagsId;
    TFilterElementHideTagsNamed m_elementHideTagsClass;
    TFilterElementHideTags m_elementHideTags;

    TFilterElementHideDomains m_elementHideDomains;

	TFilterMap m_filterMap[2][2];
	TFilterMapDefault m_filterMapDefault[2];

	void ParseFilters(const TFilterFileList& urlList);

    int FindMatch(const CString& src, CString filterPart, int startPos=0) const;
    bool IsSpecialChar(TCHAR testChar) const;
    bool IsSubdomain(const CString& subdomain, const CString& domain) const;

public:

	CPluginFilter(const TFilterFileList& urlList, const CString& dataPath);
	CPluginFilter(const CString& dataPath = "");

    bool ReadFilter(const CString& filename, const CString& downloadPath="");

	void AddFilter(CString filter, CString filterFile, int filterType);
	bool AddFilterElementHide(CString filter, CString filterFile);

    bool IsElementHidden(const CString& tag, IHTMLElement* pEl, const CString& domain, const CString& indent) const;

	const CFilter* MatchFilter(int filterType, const CString& src, int contentType, const CString& domain) const;
	bool IsMatchFilter(const CFilter& filter, CString src, const CString& srcDomain, const CString& domain) const;

	bool IsMatchFilterElementHide(const CFilterElementHide& filter, IHTMLElement* pEl, const CString& domain) const;

#if (defined PRODUCT_ADBLOCKPLUS)
    bool static DownloadFilterFile(const CString& url, const CString& filename);
    void static CreateFilters();
    bool IsAlive() const;
#endif

	bool ShouldBlock(CString src, int contentType, const CString& domain, bool addDebug=false) const;
	bool ShouldWhiteList(CString url) const;
};


#endif // _PLUGIN_FILTER_H_
