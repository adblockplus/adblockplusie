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
  static void DebugSystemException(const std::system_error& ex, int errorId, int errorSubid, const std::string& description); 

#if (defined ENABLE_DEBUG_INFO)
  static void Debug(const std::string& text);
  static void Debug(const std::wstring& text);
  static void DebugException(const std::exception& ex);
  static void DebugErrorCode(DWORD errorCode, const std::string& error, DWORD processId=0, DWORD threadId=0);
#endif

#if (defined ENABLE_DEBUG_RESULT)
  static void DebugResult(const std::wstring& text);
  static void DebugResultDomain(const std::wstring& domain);
  static void DebugResultBlocking(const std::wstring& type, const std::wstring& src, const std::wstring& domain);
  static void DebugResultHiding(const std::wstring& tag, const std::wstring& id, const std::wstring& filter);
#endif

#if (defined ENABLE_DEBUG_RESULT_IGNORED)
  static void DebugResultIgnoring(const std::wstring& type, const std::wstring& src, const std::wstring& domain);
#endif
};


#endif // _PLUGIN_DEBUG_H_
