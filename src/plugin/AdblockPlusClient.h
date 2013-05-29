#ifndef _SIMPLE_ADBLOCK_CLIENT_H_
#define _SIMPLE_ADBLOCK_CLIENT_H_


#include "PluginTypedef.h"
#include "PluginClientBase.h"
#include "AdblockPlus.h"


using namespace AdblockPlus;

class CPluginFilter;

class CAdblockPlusClient : public CPluginClientBase
{

private:

  std::auto_ptr<CPluginFilter> m_filter;
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine;

  CComAutoCriticalSection m_criticalSectionFilter;
  CComAutoCriticalSection m_criticalSectionCache;

  std::map<CString,bool> m_cacheBlockedSources;


  // Private constructor used by the singleton pattern
  CAdblockPlusClient();

public:

  static CAdblockPlusClient* s_instance;

  ~CAdblockPlusClient();

  static CAdblockPlusClient* GetInstance();

  // Removes the url from the list of whitelisted urls if present
  // Only called from ui thread
  bool ShouldBlock(CString src, int contentType, const CString& domain, bool addDebug=false);

  bool IsElementHidden(const CString& tag, IHTMLElement* pEl, const CString& domain, const CString& indent, CPluginFilter* filter);
  bool IsUrlWhiteListed(const CString& url);

  int GetIEVersion();

  bool Matches(const std::string& url, const std::string& contentType, const std::string& domain);
  std::vector<std::string> GetElementHidingSelectors(const std::string& domain);
  std::vector<AdblockPlus::SubscriptionPtr> FetchAvailableSubscriptions();
  std::vector<AdblockPlus::FilterPtr> GetListedFilters();
  AdblockPlus::FilterPtr GetFilter(std::string text);
  std::vector<AdblockPlus::SubscriptionPtr> GetListedSubscriptions();
  AdblockPlus::SubscriptionPtr GetSubscription(std::string url);
};

#endif // _SIMPLE_ADBLOCK_CLIENT_H_
