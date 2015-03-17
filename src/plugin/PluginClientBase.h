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

#ifndef _PLUGIN_CLIENT_BASE_H_
#define _PLUGIN_CLIENT_BASE_H_

#include <vector>
#include "ATL_Deprecate.h"

class CPluginError
{

private:

  int m_errorId;
  int m_errorSubid;
  DWORD m_errorCode;
  std::string m_errorDescription;
  DWORD m_processId;
  DWORD m_threadId;

public:

  CPluginError(int errorId, int errorSubid, DWORD errorCode, const std::string& errorDesc) : 
    m_errorId(errorId), m_errorSubid(errorSubid), m_errorCode(errorCode), m_errorDescription(errorDesc)
  {
    m_processId = ::GetCurrentProcessId();
    m_threadId = ::GetCurrentThreadId();
  }

  CPluginError() : 
    m_errorId(0), m_errorSubid(0), m_errorCode(0), m_processId(0), m_threadId(0) {}

  CPluginError(const CPluginError& org) : 
    m_errorId(org.m_errorId), m_errorSubid(org.m_errorSubid), m_errorCode(org.m_errorCode), m_errorDescription(org.m_errorDescription), m_processId(org.m_processId), m_threadId(org.m_threadId) {}

  int GetErrorId() const { return m_errorId; }
  int GetErrorSubid() const { return m_errorSubid; }
  DWORD GetErrorCode() const { return m_errorCode; }
  std::string GetErrorDescription() const { return m_errorDescription; }
  DWORD GetProcessId() const { return m_processId; }
  DWORD GetThreadId() const { return m_threadId; }
};


class LogQueue
{
private:
  static std::vector<CPluginError> s_pluginErrors;
  static CComAutoCriticalSection s_criticalSectionQueue;

public:
  static void LogPluginError(DWORD errorCode, int errorId, int errorSubid, const std::string& description="", bool isAsync=false, DWORD dwProcessId=0, DWORD dwThreadId=0);
  static void PostPluginError(int errorId, int errorSubid, DWORD errorCode, const std::string& errorDescription);
  static bool PopFirstPluginError(CPluginError& pluginError);
};

#endif // _PLUGIN_CLIENT_BASE_H_
