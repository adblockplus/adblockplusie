#ifndef _PLUGIN_DEBUG_H_
#define _PLUGIN_DEBUG_H_


class CPluginDebug
{

public:

#if (defined ENABLE_DEBUG_INFO)
    static void Debug(const CStringA& error, DWORD dwProcessId=0, DWORD dwThreadId=0);
    static void DebugClear();
#endif

#if (defined ENABLE_DEBUG_INFO || defined ENABLE_DEBUG_SELFTEST)
    static void DebugError(const CStringA& error);
    static void DebugErrorCode(DWORD errorCode, const CStringA& error, DWORD dwProcessId=0, DWORD dwThreadId=0);
#endif

#if (defined ENABLE_DEBUG_RESULT)
    static void DebugResult(const CStringA& text);
    static void DebugResultDomain(const CStringA& domain);
    static void DebugResultBlocking(const CStringA& type, const CStringA& src, const CStringA& filter, const CStringA& filterFile);
    static void DebugResultHiding(const CStringA& tag, const CStringA& id, const CStringA& filter, const CStringA& filterFile);
    static void DebugResultClear();
#endif

#if (defined ENABLE_DEBUG_RESULT_IGNORED)
    static void DebugResultIgnoring(const CStringA& type, const CStringA& src);
#endif
};


#endif // _PLUGIN_DEBUG_H_
