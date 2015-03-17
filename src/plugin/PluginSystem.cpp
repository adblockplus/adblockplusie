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
#include "PluginSystem.h"
#include "PluginClientBase.h"
#include <array>

std::wstring GetBrowserLanguage()
{
  LANGID lcid = GetUserDefaultLangID();
  std::wstring lang;
  // According to http://msdn.microsoft.com/en-us/library/windows/desktop/dd373848(v=vs.85).aspx
  // The maximum number of characters allowed for this string is nine, including a terminating null character.
  {
    std::array<wchar_t, 9> localeLanguage;
    int res = GetLocaleInfoW(lcid, LOCALE_SISO639LANGNAME, localeLanguage.data(), localeLanguage.size());
    if (res == 0)
    {
      DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_BROWSER_LANGUAGE, "System::GetBrowserLang - Failed");
    }
    else
    {
      lang += localeLanguage.data();
    }
  }
  lang += L"-";
  {
    std::array<wchar_t, 9> localeCountry;
    int res = GetLocaleInfoW(lcid, LOCALE_SISO3166CTRYNAME, localeCountry.data(), localeCountry.size());
    if (res == 0)
    {
      DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_BROWSER_LANGUAGE, "System::GetBrowserLang - failed to retrieve country");
    }
    else
    {
      lang += localeCountry.data();
    }
  }
  return lang;
}
