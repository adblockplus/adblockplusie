#ifndef _PLUGIN_SYSTEM_H_
#define _PLUGIN_SYSTEM_H_


class CPluginSystem
{
private:

  static CComAutoCriticalSection s_criticalSection;

  // Private constructor used by the singleton pattern
  CPluginSystem();

public:
  static CPluginSystem* s_instance;

  static CPluginSystem* CPluginSystem::GetInstance();

  ~CPluginSystem();

  CString GetBrowserLanguage() const;
};

#endif // _PLUGIN_SYSTEM_H_
