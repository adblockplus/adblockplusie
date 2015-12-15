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
#include "PluginUtil.h"
#include "../shared/Utils.h"
#include <memory>
#include <WinInet.h>

std::wstring HtmlFolderPath()
{
  return GetDllDir() + L"html\\templates\\";
}

std::wstring UserSettingsFileUrl()
{
  return FileUrl(HtmlFolderPath() + L"index.html");
}

std::wstring FirstRunPageFileUrl()
{
  return FileUrl(HtmlFolderPath() + L"firstRun.html");
}

std::wstring FileUrl(const std::wstring& path)
{
  std::wstring url = path;
  std::replace(url.begin(), url.end(), L'\\', L'/');
  return L"file:///" + url;
}

std::wstring GetLocationUrl(IWebBrowser2& browser)
{
  ATL::CComBSTR locationUrl;
  if (FAILED(browser.get_LocationURL(&locationUrl)) || !locationUrl)
  {
    return std::wstring();
  }
  return std::wstring(locationUrl, locationUrl.Length());
}

std::wstring ToLowerString(const std::wstring& s)
{
  std::wstring lower(s); // Copy the argument
  /*
   * Documentation for '_wcslwr_s' https://msdn.microsoft.com/en-us/library/y889wzfw.aspx
   * This documentation is incorrect on an important point.
   * Regarding parameter validation, it says "length of string" where it should say "length of buffer".
   * The call below has argument "length + 1" to include the terminating null character in the buffer.
   */
  auto e = _wcslwr_s(const_cast<wchar_t*>(lower.c_str()), lower.length() + 1); // uses the current locale
  if (e != 0)
  {
    throw std::logic_error("Error code returned from _wcslwr_s()");
  }
  return lower;
}

