/**
 * \file test-installer-lib-sandbox.cpp
 *
 * Automatic testing of many of the units within the custom action is infeasible.
 * In one case, they rely on the execution environment within an installation session.
 * In another, they rely on the operation system environment as a whole.
 * In these cases, it's easier to verify behavior manually.
 *
 * This file contains a custom action function sandbox() as well as a number of test functions.
 * At any given time, not all of the test functions need to be referenced within the body of custom action.
 */

#include <sstream>
#include <functional>

#include "session.h"
#include "property.h"
#include "database.h"
#include "process.h"
#include "interaction.h"

//-------------------------------------------------------
// LogAllWindowHandles
//-------------------------------------------------------
class LogSingleWindowHandle
{
  ImmediateSession& session;

public:
  LogSingleWindowHandle(ImmediateSession& session)
    : session(session)
  {
  }

  bool operator()(HWND window)
  {
    std::stringstream s;
    s << "Window handle 0x" << std::hex << window;
    session.Log(s.str());
    return true;
  }
};

void LogAllWindowHandles(ImmediateSession& session)
{
  session.Log("LogAllWindowHandles");
  LogSingleWindowHandle lp(session);
  EnumerateWindows(lp);
}

//-------------------------------------------------------
// LogIeWindowHandles
//-------------------------------------------------------
class LogSingleWindowHandleOnlyIfIe
{
  ImmediateSession& session;

  ProcessCloser& pc;

public:
  LogSingleWindowHandleOnlyIfIe(ImmediateSession& session, ProcessCloser& pc)
    : session(session), pc(pc)
  {
  }

  bool operator()(HWND window)
  {
    DWORD pid = CreatorProcess(window);
    if (pc.Contains(pid))
    {
      std::stringstream s;
      s << "Window handle 0x" << std::hex << window;
      session.Log(s.str());
    }
    return true;
  }
};

void LogIeWindowHandles(ImmediateSession& session)
{
  session.Log("LogIeWindowHandles");
  const wchar_t* IeNames[] = {L"IExplore.exe", L"AdblockPlusEngine.exe"};
  ProcessSnapshot snapshot;
  ProcessCloser iec(snapshot, IeNames);
  LogSingleWindowHandleOnlyIfIe lp(session, iec);
  EnumerateWindows(lp);
}

//-------------------------------------------------------
// LogOnlyWindowHandleInCloser
//-------------------------------------------------------
void LogOnlyWindowHandleInCloser(ImmediateSession& session)
{
  session.Log("LogOnlyWindowHandleInCloser");
  const wchar_t* IeNames[] = {L"IExplore.exe", L"AdblockPlusEngine.exe"};
  ProcessSnapshot snapshot;
  ProcessCloser iec(snapshot, IeNames);
  iec.IterateOurWindows(LogSingleWindowHandle(session));
}

//-------------------------------------------------------
// sandbox
//-------------------------------------------------------
/**
 * Exposed DLL entry point for custom action.
 * The function signature matches the calling convention used by Windows Installer.

 * \param[in] sessionHandle
 *     Windows installer session handle
 *
 * \return
 *    An integer interpreted as a custom action return value.
 *
 * \sa
 *   - MSDN [Custom Action Return Values](http://msdn.microsoft.com/en-us/library/aa368072%28v=vs.85%29.aspx)
 */
extern "C" UINT __stdcall
sandbox(MSIHANDLE sessionHandle)
{
  ImmediateSession session(sessionHandle, "sandbox");

  try
  {
    session.Log("Sandbox timestamp " __TIMESTAMP__);
    LogOnlyWindowHandleInCloser(session);
  }
  catch (std::exception& e)
  {
    session.LogNoexcept("terminated by exception: " + std::string(e.what()));
    return ERROR_INSTALL_FAILURE;
  }
  catch (...)
  {
    session.LogNoexcept("Caught an exception");
    return ERROR_INSTALL_FAILURE;
  }

  return ERROR_SUCCESS;
}
