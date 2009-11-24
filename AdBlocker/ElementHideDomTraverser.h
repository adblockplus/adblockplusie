#ifndef _ELEMENT_HIDE_DOM_TRAVERSER_H_
#define _ELEMENT_HIDE_DOM_TRAVERSER_H_


#include "PluginDomTraverser.h"

class CElementHideDomTraverserCache : public CPluginDomTraverserCache
{
public:

    bool m_isHidden;

	CElementHideDomTraverserCache() : CPluginDomTraverserCache(), m_isHidden(false) {}

	void Init() { CPluginDomTraverserCache::Init(); m_isHidden = false; }
};


class CElementHideDomTraverser : public CPluginDomTraverser<CElementHideDomTraverserCache>
{

public:

	CElementHideDomTraverser();

	void TraverseSubdocument(IWebBrowser2* pBrowser, const CString& domain, const CString& documentName);

protected:

	bool OnIFrame(IHTMLElement* pEl, const CString& url, CString& indent);
    bool OnElement(IHTMLElement* pEl, const CString& tag, CElementHideDomTraverserCache* cache, bool isDebug, CString& indent);

	bool IsEnabled();

	void HideElement(IHTMLElement* pEl, const CString& type, const CString& url, bool isDebug, CString& indent);

};


#endif // _ELEMENT_HIDE_DOM_TRAVERSER_H_
