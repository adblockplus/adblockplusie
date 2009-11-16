#ifndef _PLUGIN_SYSTEM_H_
#define _PLUGIN_SYSTEM_H_


class CPluginSystem
{
private:

	static CComAutoCriticalSection s_criticalSection;
	static CPluginSystem* s_instance;

	CString m_pluginId;

	// Private constructor used by the singleton pattern
	CPluginSystem();

public:

	static CPluginSystem* CPluginSystem::GetInstance();

	~CPluginSystem();

	CString GetBrowserLanguage() const;
	CString GetBrowserVersion() const;
    CString GetUserName() const;
    CString GetComputerName() const;
	CString GetPluginId();
    CString GetMacId(bool addSeparator=false) const;

private:

	CString GeneratePluginId();
};

#endif // _PLUGIN_SYSTEM_H_
