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
#include <ExDisp.h>

std::wstring HtmlFolderPath();
std::wstring UserSettingsFileUrl();
std::wstring FirstRunPageFileUrl();
std::wstring FileUrl(const std::wstring& url);
std::wstring GetLocationUrl(IWebBrowser2& browser);

template<class CharT, class Traits, class Alloc, size_t N>
bool BeginsWith(const std::basic_string<CharT, Traits, Alloc>& s, const CharT(&beginning)[N])
{
  return 0 == s.compare(0, N - 1, beginning);
}

/**
 * Return a lower-case version of the argument string.
 * Current version uses 'CString::MakeLower()' to preserve legacy behavior during refactoring.
 */
std::wstring ToLowerString(const std::wstring& s);
