/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2016 Eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

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

  explicit CPluginDomTraverser(const PluginFilterPtr& pluginFilter);

protected:

  bool OnIFrame(IHTMLElement* pEl, const std::wstring& url, const std::wstring& indent);
  bool OnElement(IHTMLElement* pEl, const std::wstring& tag, CPluginDomTraverserCache* cache, bool isDebug, const std::wstring& indent);

  bool IsEnabled();

  void HideElement(IHTMLElement* pEl, const std::wstring& type, const std::wstring& url, bool isDebug, const std::wstring& indent);

};


#endif // _ADBLOCK_PLUS_DOM_TRAVERSER_H_
