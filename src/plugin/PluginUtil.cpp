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
#include <algorithm>
#include <stdexcept>
#include <vector>

#include "../shared/Utils.h"
#include "PluginUtil.h"
#include "PluginSettings.h"

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