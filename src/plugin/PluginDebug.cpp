/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PluginStdAfx.h"
#include "PluginDebug.h"
#include "PluginClientBase.h"
#include "PluginMutex.h"
#include "PluginSettings.h"

class CPluginDebugLock : public CPluginMutex
{

private:

  static CComAutoCriticalSection s_criticalSectionDebugLock;

public:

  CPluginDebugLock() : CPluginMutex(L"DebugFile", PLUGIN_ERROR_MUTEX_DEBUG_FILE)
  {
    s_criticalSectionDebugLock.Lock();
  }

  ~CPluginDebugLock()
  {
    s_criticalSectionDebugLock.Unlock();
  }
};

CComAutoCriticalSection CPluginDebugLock::s_criticalSectionDebugLock;

void CPluginDebug::DebugSystemException(const std::system_error& ex, int errorId, int errorSubid, const std::string& description)
{
  std::string message = description + ", " + ex.code().message() + ", " + ex.what();
  DEBUG_ERROR_LOG(ex.code().value(), errorId, errorSubid, message);
}

#ifdef ENABLE_DEBUG_INFO

void DebugLegacy(const CString& text, DWORD dwProcessId, DWORD dwThreadId)
{
#ifdef USE_CONSOLE
  CONSOLE("%s", CT2A(text.GetString(), CP_UTF8));
#endif

  if (CPluginSettings::HasInstance())
  {
#ifdef ENABLE_DEBUG_SPLIT_FILE
    CPluginSettings* settings = CPluginSettings::GetInstance();

    bool isWorkingThread = settings->IsWorkingThread(dwThreadId);

    std::wstring processor;
    wchar_t tmp[10];
    _itow_s(::GetCurrentProcessId(), tmp, 10);
    if (isWorkingThread)
      processor = L"tab" + std::wstring(tmp) + L"_thread";
    else
      processor = L"tab" + std::wstring(tmp) + L"_ui";
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
      debugFile.open(GetDataPath(L"debug_" + processor + L".txt"), std::ios::app);
#else
      debugFile.open(GetDataPath(L"debug.txt"), std::ios::app);
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

void CPluginDebug::Debug(const std::string& text, DWORD processId, DWORD threadId)
{
  DebugLegacy(CString(text.c_str()), processId, threadId);
}

void CPluginDebug::Debug(const std::wstring& text, DWORD processId, DWORD threadId)
{
  DebugLegacy(ToCString(text), processId, threadId);
}

#endif

#if (defined ENABLE_DEBUG_INFO)

void CPluginDebug::DebugException(const std::exception& ex)
{
  auto error = std::string("!!! Exception:") + ex.what();
#ifdef ENABLE_DEBUG_ERROR
  Debug(error);
#endif

  DEBUG_SELFTEST("********************************************************************************\n" + error + "\n********************************************************************************")
}

void DebugErrorCodeLegacy(DWORD errorCode, const CString& error, DWORD dwProcessId, DWORD dwThreadId)
{
  CString errorCodeText;
  errorCodeText.Format(L"%u (0x%8.8x)", errorCode, errorCode);

  CString finalError = error + L". error=" + errorCodeText;

#ifdef ENABLE_DEBUG_ERROR
  DebugLegacy(finalError, dwProcessId, dwThreadId);
#endif

  DEBUG_SELFTEST(L"********************************************************************************\n" + finalError + "\n********************************************************************************")
}

void CPluginDebug::DebugErrorCode(DWORD errorCode, const std::string& error, DWORD processId, DWORD threadId)
{
  DebugErrorCodeLegacy(errorCode, CString(error.c_str()), processId, threadId);
}

#endif

// ============================================================================
// Debug result
// ============================================================================

#ifdef ENABLE_DEBUG_RESULT

void DebugResultLegacy(const CString& text)
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

    debugFile.open(GetDataPath(L"debug_result.txt"), std::ios::app);
    debugFile.write(sysTime.GetBuffer(), sysTime.GetLength());
    debugFile.write(LPCSTR(textA), textA.GetLength());
    debugFile.write("\n", 1);
    debugFile.flush();
  }
}

void CPluginDebug::DebugResult(const std::wstring& text)
{
  DebugResultLegacy(ToCString(text));
}

void CPluginDebug::DebugResultDomain(const std::wstring& domain)
{
  DebugResult(L"===========================================================================================================================================================================================");
  DebugResult(domain);
  DebugResult(L"===========================================================================================================================================================================================");
}


void CPluginDebug::DebugResultBlocking(const std::wstring& type, const std::wstring& src, const std::wstring& domain)
{
  CString srcTrunc = ToCString(src);
  if (src.length() > 100)
  {
    srcTrunc = srcTrunc.Left(67) + L"..." + srcTrunc.Right(30);
  }

  CString blocking;
  blocking.Format(L"Blocked  %-12s  %-20s  %s", ToCString(type), domain.empty()? L"-" : ToCString(domain), srcTrunc);

  DebugResultLegacy(blocking);
}


void CPluginDebug::DebugResultHiding(const std::wstring& tag, const std::wstring& id, const std::wstring& filter)
{
  CString srcTrunc = ToCString(id);
  if (srcTrunc.GetLength() > 100)
  {
    srcTrunc = srcTrunc.Left(67) + L"..." + srcTrunc.Right(30);
  }

  CString blocking;
  blocking.Format(L"Hidden   %-12s  - %s  %s", ToCString(tag), srcTrunc, ToCString(filter));

  DebugResultLegacy(blocking);
}

#endif // ENABLE_DEBUG_RESULT


#ifdef ENABLE_DEBUG_RESULT_IGNORED

void CPluginDebug::DebugResultIgnoring(const std::wstring& type, const std::wstring& src, const std::wstring& domain)
{
  CString srcTrunc = ToCString(src);
  if (src.length() > 100)
  {
    srcTrunc = srcTrunc.Left(67) + L"..." + srcTrunc.Right(30);
  }

  CString blocking;
  blocking.Format(L"Ignored  %-12s  %s  %s", ToCString(type), domain.empty()? L"-" : ToCString(domain), srcTrunc);

  DebugResultLegacy(blocking);
}

#endif // ENABLE_DEBUG_RESULT_IGNORED
