#ifndef _PLUGIN_DEBUG_H_
#define _PLUGIN_DEBUG_H_


class CPluginDebug
{

public:

#if (defined ENABLE_DEBUG_INFO)
  static void Debug(const CString& error, DWORD dwProcessId=0, DWORD dwThreadId=0);
  static void DebugClear();
  static void DebugError(const CString& error);
  static void DebugErrorCode(DWORD errorCode, const CString& error, DWORD dwProcessId=0, DWORD dwThreadId=0);
#endif

#if (defined ENABLE_DEBUG_RESULT)
  static void DebugResult(const CString& text);
  static void DebugResultDomain(const CString& domain);
  static void DebugResultBlocking(const CString& type, const CString& src, const CString& domain);
  static void DebugResultHiding(const CString& tag, const CString& id, const CString& filter);
  static void DebugResultClear();
#endif

#if (defined ENABLE_DEBUG_RESULT_IGNORED)
  static void DebugResultIgnoring(const CString& type, const CString& src, const CString& domain);
#endif
};


#endif // _PLUGIN_DEBUG_H_
