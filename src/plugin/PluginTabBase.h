#ifndef _PLUGIN_TAB_BASE_H_
#define _PLUGIN_TAB_BASE_H_


#ifdef SUPPORT_DOM_TRAVERSER
class CPluginDomTraverser;
#endif

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

  CString m_documentDomain;
  CString m_documentUrl;
  CPluginUserSettings m_pluginUserSettings;
public:
  CPluginClass* m_plugin;
protected:
  bool m_isActivated;

  std::thread m_thread;
  std::atomic<bool> m_continueThreadRunning;

#ifdef SUPPORT_DOM_TRAVERSER
  CPluginDomTraverser* m_traverser;
#endif

  static int s_dictionaryVersion;
  static int s_settingsVersion;
#ifdef SUPPORT_FILTER
  static int s_filterVersion;
public:
  std::auto_ptr<CPluginFilter> m_filter;
private:
#endif
#ifdef SUPPORT_WHITELIST
  static int s_whitelistVersion;
#endif
#ifdef SUPPORT_CONFIG
  static int s_configVersion;
#endif

  void ThreadProc();

#ifdef SUPPORT_FRAME_CACHING
  CComAutoCriticalSection m_criticalSectionCache;
  std::set<CString> m_cacheFrames;
  CString m_cacheDomain;
#endif

  void SetDocumentUrl(const CString& url);
  void InjectABP(IWebBrowser2* browser);
public:

  CPluginTabBase(CPluginClass* plugin);
  ~CPluginTabBase();

  CString GetDocumentDomain();
  CString GetDocumentUrl();

  virtual void OnActivate();
  virtual void OnUpdate();
  virtual void OnNavigate(const CString& url);
  virtual void OnDownloadComplete(IWebBrowser2* browser);
  virtual void OnDocumentComplete(IWebBrowser2* browser, const CString& url, bool isDocumentBrowser);

  static DWORD WINAPI TabThreadProc(LPVOID pParam);

#ifdef SUPPORT_FRAME_CACHING
  void CacheFrame(const CString& url);
  bool IsFrameCached(const CString& url);
  void ClearFrameCache(const CString& domain="");
#endif

};


#endif // _PLUGIN_TAB_BASE_H_
