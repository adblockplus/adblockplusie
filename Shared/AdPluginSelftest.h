#ifndef _ADPLUGIN_SELFTEST_H_
#define _ADPLUGIN_SELFTEST_H_


#if (defined ENABLE_DEBUG_SELFTEST)

class CAdPluginSelftest
{

private:

	static CComAutoCriticalSection s_criticalSectionLocal;
	
	static bool s_isSupported;

	// Private constructor used by the singleton pattern
	CAdPluginSelftest();

public:

	~CAdPluginSelftest();

    static void AddText(const CStringA& text);
    static void Clear();
    static bool Send();
    static bool IsFileTooLarge();

    static void SetSupported(bool isSupported=true);
};

#endif // ENABLE_DEBUG_SELFTEST

#endif // _ADPLUGIN_SELFTEST_H_
