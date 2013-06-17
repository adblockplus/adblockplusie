#ifndef _ADBLOCK_PLUS_CLIENT_H_
#define _ADBLOCK_PLUS_CLIENT_H_


#include "PluginTypedef.h"
#include "PluginClientBase.h"


class CPluginFilter;

struct SubscriptionDescription
{
  std::string url;
  std::string title;
  std::string specialization;
  bool listed;
};

class CAdblockPlusClient : public CPluginClientBase
{

private:

  std::auto_ptr<CPluginFilter> m_filter;

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
  std::vector<SubscriptionDescription> FetchAvailableSubscriptions();
  std::vector<SubscriptionDescription> GetListedSubscriptions();
  void SetSubscription(std::string url);
  void UpdateAllSubscriptions();
  std::vector<std::string> GetExceptionDomains();
  void AddFilter(const std::string& text);
  void RemoveFilter(const std::string& text);
};

#endif // _ADBLOCK_PLUS_CLIENT_H_
