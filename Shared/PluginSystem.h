#ifndef _PLUGIN_SYSTEM_H_
#define _PLUGIN_SYSTEM_H_


class CPluginSystem
{
private:

	static CComAutoCriticalSection s_criticalSection;

	CString m_pluginId;

	// Private constructor used by the singleton pattern
	CPluginSystem();

public:
	static CPluginSystem* s_instance;

	static CPluginSystem* CPluginSystem::GetInstance();

	~CPluginSystem();

	CString GetBrowserLanguage() const;
	CString GetBrowserVersion() const;
    CString GetUserName() const;
    CString GetComputerName() const;
	CString GetPluginId();
	void SetPluginId(CString pluginId);
    CString GetMacId(bool addSeparator=false) const;

private:

	CString GeneratePluginId();
};

#endif // _PLUGIN_SYSTEM_H_
