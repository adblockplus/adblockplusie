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
#include "PluginClientBase.h"
#include "../shared/Utils.h"
#include <iomanip>
#include <memory>

namespace
{
  class CPluginDebugLock
    : public CPluginMutex
  {
  private:
    static CComAutoCriticalSection s_criticalSectionDebugLock;

  public:
    CPluginDebugLock()
      : CPluginMutex(L"DebugFile", PLUGIN_ERROR_MUTEX_DEBUG_FILE)
    {
      s_criticalSectionDebugLock.Lock();
    }

    ~CPluginDebugLock()
    {
      s_criticalSectionDebugLock.Unlock();
    }
  };

  CComAutoCriticalSection CPluginDebugLock::s_criticalSectionDebugLock;

  class LogText
  {
  public:
    virtual std::string Text() const = 0;
    virtual ~LogText() {};
  };

  class LogTextFixed
    : public LogText
  {
  protected:
    const std::string fixedText;

  public:
    explicit LogTextFixed(const std::string& text)
      : fixedText(text)
    {}

    explicit LogTextFixed(const std::exception& ex)
      : fixedText(std::string("!!! Exception: ") + ex.what())
    {}

    virtual std::string Text() const override
    {
      return fixedText;
    }
  };

  class LogTextErrorCode
    : public LogTextFixed
  {
  protected:
    const DWORD errorCode;

  public:
    LogTextErrorCode(DWORD errorCode, const std::string& text)
      : LogTextFixed(text), errorCode(errorCode)
    {}

    virtual std::string Text() const override
    {
      std::ostringstream ss;
      ss << fixedText << ". error = " << errorCode << " (0x";
      ss << std::setfill('0') << std::setw(2 * sizeof(DWORD)) << std::hex << errorCode;
      ss << ")";
      return ss.str();
    }
  };

  /**
   * Wrapper around SYSTEMTIME allows initialization of 'const' instances.
   */
  struct SystemTime
    : public SYSTEMTIME
  {
    SystemTime()
    {
      ::GetSystemTime(static_cast<SYSTEMTIME*>(this));
    }
  };

  class LogEntry
  {
    const std::unique_ptr<LogText> text;

    std::string InitialPrefix() const
    {
      std::stringstream ss;
      ss << std::setfill('0') << std::setw(2) << st.wHour;
      ss << ":";
      ss << std::setfill('0') << std::setw(2) << st.wMinute;
      ss << ":";
      ss << std::setfill('0') << std::setw(2) << st.wSecond;
      ss << ".";
      ss << std::setfill('0') << std::setw(3) << st.wMilliseconds;
      ss << " [";
      ss << std::setfill(' ') << std::setw(5) << threadId;
      ss << "] - ";
      return ss.str();
    }

    std::string SubsequentPrefix() const
    {
      return "                     + ";
    }

  public:
    /**
     * The time at which the log-generating statement executes.
     */
    const SystemTime st;

    /**
     * The process within which the log-generating statement executes.
     */
    const DWORD processId;

    /**
     * The thread within which the log-generating statement executes.
     */
    const DWORD threadId;

    explicit LogEntry(LogText* text)
      : processId(::GetCurrentProcessId()), threadId(::GetCurrentThreadId()), text(text)
    {}

    LogEntry(LogText* text, DWORD processId, DWORD threadId)
      : processId(processId), threadId(threadId), text(text)
    {}

    void Write(std::ostream& out) const
    {
      CPluginDebugLock lock;
      if (lock.IsLocked())
      {
        auto lines = text->Text();
        size_t linePosition = 0;
        while (true)
        {
          auto eolPosition = lines.find('\n', linePosition);
          auto prefix = linePosition == 0 ? InitialPrefix() : SubsequentPrefix();
          out << prefix;
          if (eolPosition == std::string::npos)
          {
            out << lines.substr(linePosition) << "\n";
            break;
          }
          else
          {
            out << lines.substr(linePosition, eolPosition - linePosition) << "\n";
            linePosition = eolPosition + 1;
          }
        }
        out.flush();
      }
    }

    void WriteFile(const std::wstring& logFileName) const
    {
      std::ofstream out;
      out.open(logFileName, std::ios::app);
      Write(out);
    }
  };

  std::wstring GetDataPath(const std::wstring& filename)
  {
    return GetAppDataPath() + L"\\" + filename;
  }

  void LogWriteDefault(const LogEntry& le)
  {
    std::wstring debugFileName = GetDataPath(L"debug_" + std::to_wstring(le.processId) + L".txt");
    le.WriteFile(debugFileName);
  }

  void LogWriteResult(const LogEntry& le)
  {
    std::wstring debugFileName = GetDataPath(L"debug_result.txt");
    le.WriteFile(debugFileName);
  }
}
  
#ifdef ENABLE_DEBUG_INFO

void CPluginDebug::Debug(const std::string& text)
{
  LogWriteDefault(LogEntry(new LogTextFixed(text)));
}

void CPluginDebug::Debug(const std::wstring& text)
{
  Debug(ToUtf8String(text));
}

#endif

void CPluginDebug::DebugSystemException(const std::system_error& ex, int errorId, int errorSubid, const std::string& description)
{
  std::string message = description + ", " + ex.code().message() + ", " + ex.what();
  DEBUG_ERROR_LOG(ex.code().value(), errorId, errorSubid, message);
}

#if (defined ENABLE_DEBUG_INFO)

void CPluginDebug::DebugException(const std::exception& ex)
{
  auto lt = new LogTextFixed(ex);
  LogEntry le(lt);
#ifdef ENABLE_DEBUG_ERROR
  LogWriteDefault(le);
#endif
  DEBUG_SELFTEST(
    "********************************************************************************\n"
    + lt->text() + "\n"
    "********************************************************************************")
}

void CPluginDebug::DebugErrorCode(DWORD errorCode, const std::string& error, DWORD processId, DWORD threadId)
{
  auto lt = new LogTextErrorCode(errorCode, error);
  LogEntry le(lt, processId, threadId);
#ifdef ENABLE_DEBUG_ERROR
  LogWriteDefault(le);
#endif
  DEBUG_SELFTEST(
    "********************************************************************************\n"
    + lt->text() + "\n"
    "********************************************************************************")
}

#endif

// ============================================================================
// Debug result
// ============================================================================

#ifdef ENABLE_DEBUG_RESULT

void CPluginDebug::DebugResult(const std::wstring& text)
{
  LogWriteResult(LogEntry(new LogTextFixed(ToUtf8String(text))));
}

void CPluginDebug::DebugResultDomain(const std::wstring& domain)
{
  DebugResult(
    L"=========================================================================================\n"
    + domain + L"\n"
    L"=========================================================================================");
}

namespace
{
  void DebugResultFormat(const std::wstring& action, const std::wstring& type, const std::wstring& param1, const std::wstring& param2)
  {
    std::wostringstream ss;
    ss << std::setw(7) << std::setiosflags(std::ios::left) << action;
    ss << L"  ";
    ss << std::setw(12) << std::setiosflags(std::ios::left) << type;
    ss << L"  " << param1 << L"  " << param2;
    CPluginDebug::DebugResult(ss.str());
  }

  std::wstring Shorten(const std::wstring& s)
  {
    auto n = s.length();
    if (n <= 100) return s;
    auto r = s.substr(0, 67);
    r += L"...";
    r += s.substr(n - 30, 30);
    return r;
  }
}

void CPluginDebug::DebugResultBlocking(const std::wstring& type, const std::wstring& src, const std::wstring& domain)
{
  DebugResultFormat(L"Blocked", type, domain.empty() ? L"-" : domain, Shorten(src));
}


void CPluginDebug::DebugResultHiding(const std::wstring& tag, const std::wstring& id, const std::wstring& filter)
{
  DebugResultFormat(L"Hidden", tag, L"- " + Shorten(id), filter);
}

#endif // ENABLE_DEBUG_RESULT


#ifdef ENABLE_DEBUG_RESULT_IGNORED

void CPluginDebug::DebugResultIgnoring(const std::wstring& type, const std::wstring& src, const std::wstring& domain)
{
  DebugResultFormat(L"Ignored", type, domain.empty() ? L"-" : domain, Shorten(src));
}

#endif // ENABLE_DEBUG_RESULT_IGNORED

namespace
{
  /*
   * To convert a pointer to a hexadecimal number, we need an integral type that has the same size as that of the pointer.
   */
#if defined(_WIN64)
  typedef uint64_t voidIntegral;
  static_assert(sizeof(void*)==sizeof(voidIntegral),"WIN64: sizeof(uint64_t) is not the same as sizeof(void*)");
#elif defined(_WIN32)
  typedef uint32_t voidIntegral;
  static_assert(sizeof(void*)==sizeof(voidIntegral),"WIN32: sizeof(uint32_t) is not the same as sizeof(void*)");
#else
#error Must compile with either _WIN32 or _WIN64
#endif
}

std::wstring ToHexLiteral(void const* p)
{
  std::wstringstream ss;
  ss << L"0x";
  ss.width(sizeof(p) * 2);
  ss.fill(L'0');
  ss << std::hex << reinterpret_cast<voidIntegral>(p);
  return ss.str();
}
