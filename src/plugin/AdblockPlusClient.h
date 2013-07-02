#ifndef _ADBLOCK_PLUS_CLIENT_H_
#define _ADBLOCK_PLUS_CLIENT_H_


#include "PluginTypedef.h"
#include "PluginClientBase.h"


class CPluginFilter;

struct SubscriptionDescription
{
  std::wstring url;
  std::wstring title;
  std::wstring specialization;
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
  bool IsWhitelistedUrl(const std::wstring& url);

  int GetIEVersion();

  bool Matches(const std::wstring& url, const std::wstring& contentType, const std::wstring& domain);
  std::vector<std::wstring> GetElementHidingSelectors(const std::wstring& domain);
  std::vector<SubscriptionDescription> FetchAvailableSubscriptions();
  std::vector<SubscriptionDescription> GetListedSubscriptions();
  void SetSubscription(const std::wstring& url);
  void UpdateAllSubscriptions();
  std::vector<std::wstring> GetExceptionDomains();
  void AddFilter(const std::wstring& text);
  void RemoveFilter(const std::wstring& text);
};

#endif // _ADBLOCK_PLUS_CLIENT_H_
