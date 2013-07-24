#include "PluginStdAfx.h"

#include "PluginDebug.h"
#include "PluginMutex.h"
#include "PluginSettings.h"


class CPluginDebugLock : public CPluginMutex
{

private:

  static CComAutoCriticalSection s_criticalSectionDebugLock;

public:

  CPluginDebugLock() : CPluginMutex("DebugFile", PLUGIN_ERROR_MUTEX_DEBUG_FILE)
  {
    s_criticalSectionDebugLock.Lock();
  }

  ~CPluginDebugLock()
  {
    s_criticalSectionDebugLock.Unlock();
  }
};

CComAutoCriticalSection CPluginDebugLock::s_criticalSectionDebugLock;


#ifdef ENABLE_DEBUG_INFO

void CPluginDebug::Debug(const CString& text, DWORD dwProcessId, DWORD dwThreadId)
{
#ifdef USE_CONSOLE
  CONSOLE(CT2A(text.GetString(), CP_UTF8));
#endif

  if (CPluginSettings::HasInstance())
  {
#ifdef ENABLE_DEBUG_SPLIT_FILE
    CPluginSettings* settings = CPluginSettings::GetInstance();

    bool isWorkingThread = settings->IsWorkingThread(dwThreadId);

    CString processor;
    wchar_t tmp[10];
    _itow_s(::GetCurrentProcessId(), tmp, 10);
    if (isWorkingThread)
      processor = L"tab" + CString(tmp) + L"_thread";
    else
      processor = L"tab" + CString(tmp) + L"_ui";
#else
    if (dwProcessId == 0)
    {
      dwProcessId = ::GetCurrentProcessId();
    }
    if (dwThreadId == 0)
    {
      dwThreadId = ::GetCurrentThreadId();
    }

    CStringA processInfo;
    processInfo.Format("%4.4u.%4.4u - ", dwProcessId, dwThreadId);
#endif
    SYSTEMTIME st;
    ::GetSystemTime(&st);

    CStringA sysTime;
    sysTime.Format("%2.2d:%2.2d:%2.2d.%3.3d - ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    CPluginDebugLock lock;
    if (lock.IsLocked())
    {
      std::ofstream debugFile;

#ifdef ENABLE_DEBUG_SPLIT_FILE
      debugFile.open(CPluginSettings::GetDataPath(L"debug_" + processor + L".txt"), std::ios::app);
#else
      debugFile.open(CPluginSettings::GetDataPath(L"debug.txt"), std::ios::app);
#endif
      int pos = 0;
      CStringA line = text.Tokenize(L"\n\r", pos);

      while (pos >= 0)
      {
        debugFile.write(sysTime.GetBuffer(), sysTime.GetLength());
#ifndef ENABLE_DEBUG_SPLIT_FILE
        debugFile.write(processInfo.GetBuffer(), processInfo.GetLength());
#endif
        debugFile.write(line.GetBuffer(), line.GetLength());
        debugFile.write("\n", 1);

        line = text.Tokenize(L"\n\r", pos);
      }

      debugFile.flush();
    }
  }
}

void CPluginDebug::DebugClear()
{
  CPluginDebugLock lock;
  if (lock.IsLocked())
  {
    ::DeleteFile(CPluginSettings::GetDataPath(L"debug.txt"));
    ::DeleteFile(CPluginSettings::GetDataPath(L"debug_main_ui.txt"));
    ::DeleteFile(CPluginSettings::GetDataPath(L"debug_main_thread.txt"));

    for (int i = 1; i <= 10; i++)
    {
      CString x;
      x.Format(L"%d", i);

      ::DeleteFile(CPluginSettings::GetDataPath(L"debug_tab" + x + L"_ui.txt"));
      ::DeleteFile(CPluginSettings::GetDataPath(L"debug_tab" + x + L"_thread.txt"));
    }
  }
}

#endif

#if (defined ENABLE_DEBUG_INFO || defined ENABLE_DEBUG_SELFTEST)

void CPluginDebug::DebugError(const CString& error)
{
#ifdef ENABLE_DEBUG_ERROR
  Debug(error);
#endif

  DEBUG_SELFTEST("********************************************************************************\n" + error + "\n********************************************************************************")
}

void CPluginDebug::DebugErrorCode(DWORD errorCode, const CString& error, DWORD dwProcessId, DWORD dwThreadId)
{
  CString errorCodeText;
  errorCodeText.Format(L"%u (0x%8.8x)", errorCode, errorCode);

  CString finalError = error + L". error=" + errorCodeText;

#ifdef ENABLE_DEBUG_ERROR
  Debug(finalError, dwProcessId, dwThreadId);
#endif

  DEBUG_SELFTEST(L"********************************************************************************\n" + finalError + "\n********************************************************************************")
}

#endif

// ============================================================================
// Debug result
// ============================================================================

#ifdef ENABLE_DEBUG_RESULT

void CPluginDebug::DebugResult(const CString& text)
{
  SYSTEMTIME st;
  ::GetSystemTime(&st);

  CStringA sysTime;
  sysTime.Format("%2.2d:%2.2d:%2.2d.%3.3d - ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

  CStringA textA = text;

  CPluginDebugLock lock;
  if (lock.IsLocked())
  {
    std::ofstream debugFile;

    debugFile.open(CPluginSettings::GetDataPath("debug_result.txt"), std::ios::app);
    debugFile.write(sysTime.GetBuffer(), sysTime.GetLength());
    debugFile.write(LPCSTR(textA), textA.GetLength());
    debugFile.write("\n", 1);
    debugFile.flush();
  }
}

void CPluginDebug::DebugResultDomain(const CString& domain)
{
  DebugResult(L"===========================================================================================================================================================================================");
  DebugResult(domain);
  DebugResult(L"===========================================================================================================================================================================================");
}


void CPluginDebug::DebugResultBlocking(const CString& type, const CString& src, const CString& domain)
{
  CString srcTrunc = src;
  if (srcTrunc.GetLength() > 100)
  {
    srcTrunc = src.Left(67) + L"..." + src.Right(30);
  }

  CString blocking;
  blocking.Format(L"Blocked  %-12s  %-20s  %s", type, domain.IsEmpty()? L"-" : domain, srcTrunc);

  DebugResult(blocking);
}


void CPluginDebug::DebugResultHiding(const CString& tag, const CString& src, const CString& filter)
{
  CString srcTrunc = src;
  if (srcTrunc.GetLength() > 100)
  {
    srcTrunc = src.Left(67) + L"..." + src.Right(30);
  }

  CString blocking;
  blocking.Format(L"Hidden   %-12s  - %s  %s", tag, srcTrunc, filter);

  DebugResult(blocking);
}


void CPluginDebug::DebugResultClear()
{
  CPluginDebugLock lock;
  if (lock.IsLocked())
  {
    ::DeleteFile(CPluginSettings::GetDataPath("debug_result.txt"));
  }
}

#endif // ENABLE_DEBUG_RESULT


#ifdef ENABLE_DEBUG_RESULT_IGNORED

void CPluginDebug::DebugResultIgnoring(const CString& type, const CString& src, const CString& domain)
{
  CString srcTrunc = src;
  if (srcTrunc.GetLength() > 100)
  {
    srcTrunc = src.Left(67) + L"..." + src.Right(30);
  }

  CString blocking;
  blocking.Format(L"Ignored  %-12s  %s  %s", type, domain.IsEmpty()? L"-" : domain, srcTrunc);

  DebugResult(blocking);
}

#endif // ENABLE_DEBUG_RESULT_IGNORED
