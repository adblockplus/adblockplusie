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

#pragma once
#include <string>

std::wstring HtmlFolderPath();
std::wstring UserSettingsFileUrl();
std::wstring FirstRunPageFileUrl();
std::wstring FileUrl(const std::wstring& url);
std::wstring GetLocationUrl(IWebBrowser2& browser);

/**
 * Wrapper around Microsoft API 'UrlUnescape'
 *
 * This function has modify-in-place semantics.
 * This behavior matches that of the legacy version of this function declared above.
 * At present, callers of this function have no code to handle error conditions that might arise here.
 * Because there's no error handling, therefore, this masks failures in UrlUnescape.
 */
void UnescapeUrl(std::wstring& url);

