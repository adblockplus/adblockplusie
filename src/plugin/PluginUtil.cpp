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

void UnescapeUrl(std::wstring& url)
{
  try
  {
    DWORD result_length = INTERNET_MAX_URL_LENGTH;
    std::unique_ptr<wchar_t[]> result(new wchar_t[result_length]);
    HRESULT hr = UrlUnescapeW(const_cast<wchar_t*>(url.c_str()), result.get(), &result_length, 0);
    if (hr == S_OK)
    {
      url = std::wstring(result.get(), result_length);
    }
    /*
     * Do nothing. This masks error return values from UrlUnescape without logging the error.
     */
  }
  catch(std::bad_alloc e)
  {
    /*
     * When the code has a systematic way of handling bad_alloc, we'll rethrow (probably).
     * Until then, we mask the exception and make no modification.
     */
  }
  catch(...)
  {
    // no modification if any other exception
  }
}
