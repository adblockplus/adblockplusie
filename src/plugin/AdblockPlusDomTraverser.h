#ifndef _ADBLOCK_PLUS_DOM_TRAVERSER_H_
#define _ADBLOCK_PLUS_DOM_TRAVERSER_H_


#include "PluginDomTraverserBase.h"


class CPluginTab;


class CPluginDomTraverserCache : public CPluginDomTraverserCacheBase
{
public:

  bool m_isHidden;

  CPluginDomTraverserCache() : CPluginDomTraverserCacheBase(), m_isHidden(false) {}

  void Init() { CPluginDomTraverserCacheBase::Init(); m_isHidden = false; }
};


class CPluginDomTraverser : public CPluginDomTraverserBase<CPluginDomTraverserCache>
{

public:

  CPluginDomTraverser(CPluginTab* tab);

protected:

  bool OnIFrame(IHTMLElement* pEl, const CString& url, CString& indent);
  bool OnElement(IHTMLElement* pEl, const CString& tag, CPluginDomTraverserCache* cache, bool isDebug, CString& indent);

  bool IsEnabled();

  void HideElement(IHTMLElement* pEl, const CString& type, const CString& url, bool isDebug, CString& indent);

};


#endif // _ADBLOCK_PLUS_DOM_TRAVERSER_H_
