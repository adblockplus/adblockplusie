#ifndef _ADBLOCK_PLUS_CLIENT_H_
#define _ADBLOCK_PLUS_CLIENT_H_


#include "PluginTypedef.h"
#include "PluginClientBase.h"
#include "../shared/Communication.h"
#include "../shared/CriticalSection.h"


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

  bool IsFirstRun();
};

#endif // _ADBLOCK_PLUS_CLIENT_H_
