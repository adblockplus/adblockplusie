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

#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <locale>
#include <functional>
#include <string>
#include <vector>

#define WM_ALREADY_UP_TO_DATE WM_APP+1
#define WM_UPDATE_CHECK_ERROR WM_APP+2
#define WM_DOWNLOADING_UPDATE WM_APP+3

//
// Application Package Authority.
//

#define SECURITY_APP_PACKAGE_AUTHORITY              {0,0,0,0,0,15}

#define SECURITY_APP_PACKAGE_BASE_RID               (0x00000002L)
#define SECURITY_BUILTIN_APP_PACKAGE_RID_COUNT      (2L)
#define SECURITY_APP_PACKAGE_RID_COUNT              (8L)
#define SECURITY_CAPABILITY_BASE_RID                (0x00000003L)
#define SECURITY_BUILTIN_CAPABILITY_RID_COUNT       (2L)
#define SECURITY_CAPABILITY_RID_COUNT               (5L)

//
// Built-in Packages.
//

#define SECURITY_BUILTIN_PACKAGE_ANY_PACKAGE        (0x00000001L)


bool IsWindowsVistaOrLater();
bool IsWindows8OrLater();

std::string ToUtf8String(const std::wstring& str);
std::wstring ToUtf16String(const std::string& str);
std::vector<std::wstring> ToUtf16Strings(const std::vector<std::string>& value);
std::wstring GetDllDir();
std::wstring GetAppDataPath();
void ReplaceString(std::wstring& input, const std::wstring placeholder, const std::wstring replacement);

/**
 * Returns the beginning of the URL which includes the scheme and hierarchical
 * part according to http://en.wikipedia.org/wiki/URI_scheme.
 */
std::wstring GetSchemeAndHierarchicalPart(const std::wstring& url);

std::wstring GetQueryString(const std::wstring& url);

template<class T>
T TrimString(T text)
{
  // Via http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
  T trimmed(text);
  std::function<bool(T::value_type)> isspace = std::bind(&std::isspace<T::value_type>, std::placeholders::_1, std::locale::classic());
  trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), std::not1(isspace)));
  trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), std::not1(isspace)).base(), trimmed.end());
  return trimmed;
}

#endif // UTILS_H
