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
  static void DebugResultBlocking(const CString& type, const std::wstring& src, const std::wstring& domain);
  static void DebugResultHiding(const CString& tag, const CString& id, const CString& filter);
  static void DebugResultClear();
#endif

#if (defined ENABLE_DEBUG_RESULT_IGNORED)
  static void DebugResultIgnoring(const CString& type, const std::wstring& src, const std::wstring& domain);
#endif
};


#endif // _PLUGIN_DEBUG_H_
