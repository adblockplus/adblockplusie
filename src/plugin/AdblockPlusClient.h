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

#ifndef _ADBLOCK_PLUS_CLIENT_H_
#define _ADBLOCK_PLUS_CLIENT_H_

#include <MsHTML.h>
#include "../shared/Communication.h"
#include "../shared/CriticalSection.h"
#include <AdblockPlus/FilterEngine.h>

class CPluginFilter;

struct SubscriptionDescription
{
  std::wstring url;
  std::wstring title;
  std::wstring specialization;
  bool listed;
};

class CAdblockPlusClient
{

private:

  std::auto_ptr<CPluginFilter> m_filter;

  CComAutoCriticalSection m_criticalSectionFilter;
  CComAutoCriticalSection m_criticalSectionCache;
  static CComAutoCriticalSection s_criticalSectionLocal;

  std::map<std::wstring, bool> m_cacheBlockedSources;

  std::shared_ptr<Communication::Pipe> enginePipe;
  CriticalSection enginePipeLock;


  // Private constructor used by the singleton pattern
  CAdblockPlusClient();

  bool CallEngine(Communication::OutputBuffer& message, Communication::InputBuffer& inputBuffer = Communication::InputBuffer());
  bool CallEngine(Communication::ProcType proc, Communication::InputBuffer& inputBuffer = Communication::InputBuffer());
public:

  static CAdblockPlusClient* s_instance;

  ~CAdblockPlusClient();

  static CAdblockPlusClient* GetInstance();

  // Removes the url from the list of whitelisted urls if present
  // Only called from ui thread
  bool ShouldBlock(const std::wstring& src, AdblockPlus::FilterEngine::ContentType contentType, const std::wstring& domain, bool addDebug=false);

  bool IsElementHidden(const std::wstring& tag, IHTMLElement* pEl, const std::wstring& domain, const std::wstring& indent, CPluginFilter* filter);
  bool IsWhitelistedUrl(const std::wstring& url);
  std::string GetWhitelistingFilter(const std::wstring& url);
  bool IsElemhideWhitelistedOnDomain(const std::wstring& url);

  bool Matches(const std::wstring& url, AdblockPlus::FilterEngine::ContentType contentType, const std::wstring& domain);
  std::vector<std::wstring> GetElementHidingSelectors(const std::wstring& domain);
  std::vector<SubscriptionDescription> FetchAvailableSubscriptions();
  std::vector<SubscriptionDescription> GetListedSubscriptions();
  bool IsAcceptableAdsEnabled();
  void SetSubscription(const std::wstring& url);
  void AddSubscription(const std::wstring& url);
  void RemoveSubscription(const std::wstring& url);
  void UpdateAllSubscriptions();
  std::vector<std::wstring> GetExceptionDomains();
  void AddFilter(const std::wstring& text);
  void RemoveFilter(const std::wstring& text);
  void RemoveFilter(const std::string& text);
  void SetPref(const std::wstring& name, const std::wstring& value);
  void SetPref(const std::wstring& name, const int64_t& value);
  void SetPref(const std::wstring& name, bool value);
  std::wstring GetPref(const std::wstring& name, const std::wstring& defaultValue = L"");
  std::wstring GetPref(const std::wstring& name, const wchar_t* defaultValue);
  bool GetPref(const std::wstring& name, bool defaultValue = false);
  int64_t GetPref(const std::wstring& name, int64_t defaultValue = 0);
  void CheckForUpdates(HWND callbackWindow);
  std::wstring GetAppLocale();
  std::wstring GetDocumentationLink();
  bool TogglePluginEnabled();
  std::wstring GetHostFromUrl(const std::wstring& url);
  int CompareVersions(const std::wstring& v1, const std::wstring& v2);

  bool IsFirstRun();
};

typedef CAdblockPlusClient CPluginClient;

#endif // _ADBLOCK_PLUS_CLIENT_H_
