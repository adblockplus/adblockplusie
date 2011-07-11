#ifndef _PLUGIN_DOM_TRAVERSER_BASE_H_
#define _PLUGIN_DOM_TRAVERSER_BASE_H_


#include "PluginTypedef.h"
#include "PluginTab.h"


class CPluginDomTraverserCacheBase
{
public:

    long m_elements;

    CPluginDomTraverserCacheBase() : m_elements(0) {};
	void Init() { m_elements=0; }
};

template <class T>
class CPluginDomTraverserBase
{

public:

	CPluginDomTraverserBase(CPluginTab* tab);
	~CPluginDomTraverserBase();

	void TraverseHeader(bool isHeaderTraversed);

	void TraverseDocument(IWebBrowser2* pBrowser, const CString& domain, const CString& documentName);
	void TraverseSubdocument(IWebBrowser2* pBrowser, const CString& domain, const CString& documentName);

    virtual void ClearCache();

	void ShowNotification(CPluginTab* tab);

protected:

	virtual bool OnIFrame(IHTMLElement* pEl, const CString& url, CString& indent) { return true; }
	virtual bool OnElement(IHTMLElement* pEl, const CString& tag, T* cache, bool isDebug, CString& indent) { return true; }

	virtual bool IsEnabled();

protected:

	void TraverseDocument(IWebBrowser2* pBrowser, bool isMainDoc, CString indent);
    void TraverseChild(IHTMLElement* pEl, IWebBrowser2* pBrowser, CString& indent, bool isCached=true);

	CComAutoCriticalSection m_criticalSection;

	CString m_domain;
	CString m_documentName;

	bool m_isHeaderTraversed;

	// Caching	
	long m_cacheDomElementCount;

    int m_cacheIndexLast;
    int m_cacheElementsMax;
    std::set<CString> m_cacheDocumentHasFrames;
    std::set<CString> m_cacheDocumentHasIframes;

    T* m_cacheElements;

	CPluginTab* m_tab;
	CComPtr<IWebBrowser2> m_pBrowser;
};

template <class T>
CPluginDomTraverserBase<T>::CPluginDomTraverserBase(CPluginTab* tab) : 
	m_tab(tab), m_isHeaderTraversed(false), m_cacheDomElementCount(0), m_cacheIndexLast(0), m_cacheElementsMax(5000)
{
	m_cacheElements = new T[m_cacheElementsMax];
}


template <class T>
CPluginDomTraverserBase<T>::~CPluginDomTraverserBase()
{
    delete [] m_cacheElements;
}

template <class T>
void CPluginDomTraverserBase<T>::TraverseHeader(bool isHeaderTraversed)
{
	m_isHeaderTraversed = isHeaderTraversed;
}

template <class T>
void CPluginDomTraverserBase<T>::TraverseDocument(IWebBrowser2* pBrowser, const CString& domain, const CString& documentName)
{
	m_domain = domain;

	TraverseDocument(pBrowser, true, "");
}


template <class T>
void CPluginDomTraverserBase<T>::TraverseSubdocument(IWebBrowser2* pBrowser, const CString& domain, const CString& documentName)
{
	m_domain = domain;

	TraverseDocument(pBrowser, false, "");
}


template <class T>
bool CPluginDomTraverserBase<T>::IsEnabled()
{
	return CPluginSettings::GetInstance()->IsPluginEnabled();
}


template <class T>
void CPluginDomTraverserBase<T>::TraverseDocument(IWebBrowser2* pBrowser, bool isMainDoc, CString indent)
{
	if (!IsEnabled()) return;

	VARIANT_BOOL isBusy;    
    if (SUCCEEDED(pBrowser->get_Busy(&isBusy)))
    {
        if (isBusy)
        {
            return;
        }
    }

	// Get document
	CComPtr<IDispatch> pDocDispatch;
	HRESULT hr = pBrowser->get_Document(&pDocDispatch);
	if (FAILED(hr) || !pDocDispatch)
	{
	    return;
    }
    
	CComQIPtr<IHTMLDocument3> pDoc = pDocDispatch;
	if (!pDoc)
	{
	    return;
    }

	CComPtr<IHTMLElement> pBody;
	if (FAILED(pDoc->get_documentElement(&pBody)) || !pBody)
	{
	    return;
    }

	CComPtr<IHTMLElement> pBodyEl;

	if (m_isHeaderTraversed)
	{
		 pBodyEl = pBody;
	}
	else
	{
		CComPtr<IHTMLElementCollection> pBodyCollection;
		if (FAILED(pDoc->getElementsByTagName(L"body", &pBodyCollection)) || !pBodyCollection)
		{
			return;
		}

		CComVariant vIndex(0);        
		CComPtr<IDispatch> pBodyDispatch;
		if (FAILED(pBodyCollection->item(vIndex, vIndex, &pBodyDispatch)) || !pBodyDispatch)
		{
			return;
		}

		if (FAILED(pBodyDispatch->QueryInterface(IID_IHTMLElement, (LPVOID*)&pBodyEl)) || !pBodyEl)
		{
			return;
		}
	}

    // Clear cache (if eg. refreshing) ???
    if (isMainDoc)
    {
        CComVariant vCacheIndex;
        
        if (FAILED(pBodyEl->getAttribute(L"sab", 0, &vCacheIndex)) || vCacheIndex.vt == VT_NULL)
        {
            ClearCache();
        }
    }

    // Hide elements in body part
    TraverseChild(pBodyEl, pBrowser, indent);

    // Check frames and iframes
    bool hasFrames = false;
    bool hasIframes = false;
    
    m_criticalSection.Lock();
    {
        hasFrames = m_cacheDocumentHasFrames.find(m_documentName) != m_cacheDocumentHasFrames.end();
        hasIframes = m_cacheDocumentHasIframes.find(m_documentName) != m_cacheDocumentHasIframes.end();
    }
    m_criticalSection.Unlock();

    // Frames
    if (hasFrames)
    {
        // eg. http://gamecopyworld.com/
	    long frameCount = 0;
	    CComPtr<IHTMLElementCollection> pFrameCollection;
	    if (SUCCEEDED(pDoc->getElementsByTagName(L"frame", &pFrameCollection)) && pFrameCollection)
	    {
		    pFrameCollection->get_length(&frameCount);
	    }

	    // Iterate through all frames
	    for (long i = 0; i < frameCount; i++)
	    {
		    CComVariant vIndex(i);
		    CComVariant vRetIndex;
		    CComPtr<IDispatch> pFrameDispatch;

		    if (SUCCEEDED(pFrameCollection->item(vIndex, vRetIndex, &pFrameDispatch)) && pFrameDispatch)
		    {
                CComQIPtr<IWebBrowser2> pFrameBrowser = pFrameDispatch;
                if (pFrameBrowser)
                {
                    CComBSTR bstrSrc;
                    CString src;

                    if (SUCCEEDED(pFrameBrowser->get_LocationURL(&bstrSrc)))
                    {
                        src = bstrSrc;
				        CPluginClient::UnescapeUrl(src);
                    }

                    if (!src.IsEmpty())
                    {
                        TraverseDocument(pFrameBrowser, false, indent);
                    }
                }
	        }
	    }
    }

    // Iframes
    if (hasIframes)
    {
	    long frameCount = 0;
	    CComPtr<IHTMLElementCollection> pFrameCollection;
	    if (SUCCEEDED(pDoc->getElementsByTagName(L"iframe", &pFrameCollection)) && pFrameCollection)
	    {
		    pFrameCollection->get_length(&frameCount);
	    }

	    // Iterate through all iframes
	    for (long i = 0; i < frameCount; i++)
	    {
		    CComVariant vIndex(i);
		    CComVariant vRetIndex;
		    CComPtr<IDispatch> pFrameDispatch;

		    if (SUCCEEDED(pFrameCollection->item(vIndex, vRetIndex, &pFrameDispatch)) && pFrameDispatch)
		    {
                CComQIPtr<IHTMLElement> pFrameEl = pFrameDispatch;
                if (pFrameEl)
                {
                    CComVariant vAttr;

                    if (SUCCEEDED(pFrameEl->getAttribute(L"src", 0, &vAttr)) && vAttr.vt == VT_BSTR && ::SysStringLen(vAttr.bstrVal) > 0)
                    {
		                CString src = vAttr.bstrVal;

		                // Some times, domain is missing. Should this be added on image src's as well?''
            		    
		                // eg. gadgetzone.com.au
		                if (src.Left(2) == L"//")
		                {
			                src = L"http:" + src;
		                }
		                // eg. http://w3schools.com/html/html_examples.asp
		                else if (src.Left(4) != L"http" && src.Left(6) != L"res://")
		                {
			                src = L"http://" + m_domain + src;
		                }
		                
		                CPluginClient::UnescapeUrl(src);

						// Check if Iframe should be traversed
                        if (OnIFrame(pFrameEl, src, indent))
	                    {
                            CComQIPtr<IWebBrowser2> pFrameBrowser = pFrameDispatch;
                            if (pFrameBrowser)
		                    {
                                TraverseDocument(pFrameBrowser, false, indent);
                            }
                        }	                        
					}
		        }
	        }
	    }
    }
}


template <class T>
void CPluginDomTraverserBase<T>::TraverseChild(IHTMLElement* pEl, IWebBrowser2* pBrowser, CString& indent, bool isCached)
{
    int  cacheIndex = -1;
    long cacheAllElementsCount = -1;

    m_criticalSection.Lock();
    {
        CComVariant vCacheIndex;
        if (isCached && SUCCEEDED(pEl->getAttribute(L"sab", 0, &vCacheIndex)) && vCacheIndex.vt == VT_I4)
        {
            cacheIndex = vCacheIndex.intVal;
            
            cacheAllElementsCount = m_cacheElements[cacheIndex].m_elements;
        }
        else
        {
            isCached = false;

            cacheIndex = m_cacheIndexLast++;

            // Resize cache???            
            if (cacheIndex >= m_cacheElementsMax)
            {
                T* oldCacheElements = m_cacheElements;
                
                m_cacheElements = new T[2*m_cacheElementsMax];

                memcpy(m_cacheElements, oldCacheElements, m_cacheElementsMax*sizeof(T));

                m_cacheElementsMax *= 2;

                delete [] oldCacheElements;                
            }
            
			m_cacheElements[cacheIndex].Init();

            vCacheIndex.vt = VT_I4;
            vCacheIndex.intVal = cacheIndex;
     
            pEl->setAttribute(L"sab", vCacheIndex);
        }
    }
    m_criticalSection.Unlock();

    // Get number of elements in the scope of pEl
    long allElementsCount = 0;
    
    CComPtr<IDispatch> pAllCollectionDisp;

    if (SUCCEEDED(pEl->get_all(&pAllCollectionDisp)) && pAllCollectionDisp)
    {
        CComPtr<IHTMLElementCollection> pAllCollection;

        if (SUCCEEDED(pAllCollectionDisp->QueryInterface(IID_IHTMLElementCollection, (LPVOID*)&pAllCollection)) && pAllCollection)
        {
            // If number of elements = cached number, return
            if (SUCCEEDED(pAllCollection->get_length(&allElementsCount)) && allElementsCount == cacheAllElementsCount)
            {
                return;
            }
        }
    }

    // Update cache
    m_criticalSection.Lock();
    {
        m_cacheElements[cacheIndex].m_elements = allElementsCount;
    }
    m_criticalSection.Unlock();

    // Get tag
    CComBSTR bstrTag;
    if (FAILED(pEl->get_tagName(&bstrTag)) || !bstrTag)
    {
        return;
    }

    CString tag = bstrTag;
    tag.MakeLower();

    // Custom OnElement
	if (!OnElement(pEl, tag, &m_cacheElements[cacheIndex], false, indent))
	{
		return;
	}

	// Update frame/iframe cache
	if (tag == "iframe")
    {
        m_criticalSection.Lock();
        {
            m_cacheDocumentHasIframes.insert(m_documentName);
        }
        m_criticalSection.Unlock();
	}
	else if (tag == "frame")
	{
	    m_criticalSection.Lock();
	    {
            m_cacheDocumentHasFrames.insert(m_documentName);
        }
	    m_criticalSection.Unlock();
	}

    // Iterate through children of this element
    if (allElementsCount > 0)
    {
        long childElementsCount = 0;

        CComPtr<IDispatch> pChildCollectionDisp;
        if (SUCCEEDED(pEl->get_children(&pChildCollectionDisp)) && pChildCollectionDisp)
        {
            CComPtr<IHTMLElementCollection> pChildCollection;
            if (SUCCEEDED(pChildCollectionDisp->QueryInterface(IID_IHTMLElementCollection, (LPVOID*)&pChildCollection)) && pChildCollection)
            {
                pChildCollection->get_length(&childElementsCount);

	            CComVariant vIndex(0);

	            // Iterate through all children
	            for (long i = 0; i < childElementsCount; i++)
	            {
                    CComPtr<IDispatch> pChildElDispatch;
	                CComVariant vRetIndex;

	                vIndex.intVal = i;

		            if (SUCCEEDED(pChildCollection->item(vIndex, vRetIndex, &pChildElDispatch)) && pChildElDispatch)
		            {
                        CComPtr<IHTMLElement> pChildEl;
			            if (SUCCEEDED(pChildElDispatch->QueryInterface(IID_IHTMLElement, (LPVOID*)&pChildEl)) && pChildEl)
			            {
                            TraverseChild(pChildEl, pBrowser, indent + "  ", isCached);
			            }
		            }
	            }
            }
        }
    }
}


template <class T>
void CPluginDomTraverserBase<T>::ClearCache()
{
    m_criticalSection.Lock();
    {
        m_cacheIndexLast = 0;
        m_cacheDocumentHasFrames.clear();
        m_cacheDocumentHasIframes.clear();
    }
    m_criticalSection.Unlock();
}
template <class T>
void CPluginDomTraverserBase<T>::ShowNotification(CPluginTab* tab)
{
	if (tab->m_plugin->GetTabHWND() == NULL)
	{
		return;
	}


}


#endif // _PLUGIN_DOM_TRAVERSER_BASE_H_
