/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
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

#ifndef _PLUGIN_TAB_BASE_H_
#define _PLUGIN_TAB_BASE_H_

class CPluginDomTraverser;

#include "PluginUserSettings.h"
#include "PluginFilter.h"
#include "../shared/CriticalSection.h"
#include <thread>
#include <atomic>

class CPluginClass;


class CPluginTabBase
{

  friend class CPluginClass;

protected:

  CComAutoCriticalSection m_criticalSection;
  CriticalSection m_csInject;

  std::wstring m_documentDomain;
  std::wstring m_documentUrl;
  CPluginUserSettings m_pluginUserSettings;
public:
  CPluginClass* m_plugin;
protected:
  bool m_isActivated;

  std::thread m_thread;
  std::atomic<bool> m_continueThreadRunning;
  CPluginDomTraverser* m_traverser;
  static int s_dictionaryVersion;
  static int s_settingsVersion;
  static int s_filterVersion;
public:
  std::auto_ptr<CPluginFilter> m_filter;
private:
  static int s_whitelistVersion;

  void ThreadProc();
  CComAutoCriticalSection m_criticalSectionCache;
  std::set<std::wstring> m_cacheFrames;
  std::wstring m_cacheDomain;
  void SetDocumentUrl(const std::wstring& url);
  void InjectABP(IWebBrowser2* browser);
public:

  CPluginTabBase(CPluginClass* plugin);
  ~CPluginTabBase();

  std::wstring GetDocumentDomain();
  std::wstring GetDocumentUrl();
  virtual void OnActivate();
  virtual void OnUpdate();
  virtual void OnNavigate(const std::wstring& url);
  virtual void OnDownloadComplete(IWebBrowser2* browser);
  virtual void OnDocumentComplete(IWebBrowser2* browser, const std::wstring& url, bool isDocumentBrowser);
  static DWORD WINAPI TabThreadProc(LPVOID pParam);
  void CacheFrame(const std::wstring& url);
  bool IsFrameCached(const std::wstring& url);
  void ClearFrameCache(const std::wstring& domain=L"");

};


#endif // _PLUGIN_TAB_BASE_H_
