#ifndef _PLUGIN_SELFTEST_H_
#define _PLUGIN_SELFTEST_H_


#if (defined ENABLE_DEBUG_SELFTEST)

class CPluginSelftest
{

private:

	static CComAutoCriticalSection s_criticalSectionLocal;
	
	static bool s_isSupported;

	// Private constructor used by the singleton pattern
	CPluginSelftest();

public:

	~CPluginSelftest();

    static void AddText(const CStringA& text);
    static void Clear();
    static bool Send();
    static bool IsFileTooLarge();

    static void SetSupported(bool isSupported=true);
};

#endif // ENABLE_DEBUG_SELFTEST

#endif // _PLUGIN_SELFTEST_H_
