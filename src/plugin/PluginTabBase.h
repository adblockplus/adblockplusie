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
  CString m_documentUrl;
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
  std::set<CString> m_cacheFrames;
  std::wstring m_cacheDomain;
  void SetDocumentUrl(const CString& url);
  void InjectABP(IWebBrowser2* browser);
public:

  CPluginTabBase(CPluginClass* plugin);
  ~CPluginTabBase();

  std::wstring GetDocumentDomain();
  CString GetDocumentUrl();
  virtual void OnActivate();
  virtual void OnUpdate();
  virtual void OnNavigate(const CString& url);
  virtual void OnDownloadComplete(IWebBrowser2* browser);
  virtual void OnDocumentComplete(IWebBrowser2* browser, const CString& url, bool isDocumentBrowser);
  static DWORD WINAPI TabThreadProc(LPVOID pParam);
  void CacheFrame(const CString& url);
  bool IsFrameCached(const CString& url);
  void ClearFrameCache(const std::wstring& domain=L"");

};


#endif // _PLUGIN_TAB_BASE_H_
