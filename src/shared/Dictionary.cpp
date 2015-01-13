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

#include <fstream>
#include <stdexcept>
#include <Windows.h>

#include "Dictionary.h"
#include "Utils.h"

Dictionary* Dictionary::instance = 0;

const std::wstring baseLocale = L"en";

Dictionary::Dictionary(const std::wstring& locale)
{
  std::wstring basePath = GetDllDir() + L"locales\\";

  // Always load base locale first - that's our fallback
  ReadDictionary(basePath, baseLocale);

  // Now try to load by full locale code
  if (locale != baseLocale && !ReadDictionary(basePath, locale))
  {
    // Fall back to short locale name
    size_t pos = locale.find(L'-');
    if (pos != std::wstring::npos && locale.compare(0, pos, baseLocale) != 0)
      ReadDictionary(basePath, locale.substr(0, pos));
  }
}

void Dictionary::Create(const std::wstring& locale)
{
  if (!instance)
    instance = new Dictionary(locale);
}

Dictionary* Dictionary::GetInstance()
{
  if (!instance)
    throw std::runtime_error("Attempt to access dictionary before creating");

  return instance;
}

bool Dictionary::ReadDictionary(const std::wstring& basePath, const std::wstring& locale)
{
  std::ifstream stream(basePath + locale + L".ini");
  if (stream.fail())
    return false;

  std::string section;
  while (!stream.eof())
  {
    std::string line;
    std::getline(stream, line);
    if (stream.fail())
      return false;

    line = ::TrimString(line);
    if (line.size() >= 2 && line[0] == '[' && line[line.size() - 1] == ']')
    {
      // Section header
      section = line.substr(1, line.size() - 2);
    }
    else if (line.size() >= 1 && line[0] == '#')
    {
      // Comment
      continue;
    }
    else
    {
      // Value
      size_t pos = line.find('=');
      if (pos != std::string::npos)
      {
        std::string key = ::TrimString(line.substr(0, pos));
        std::string value = ::TrimString(line.substr(pos + 1));
        data[KeyType(section, key)] = ToUtf16String(value);
      }
    }
  }
  return true;
}

std::wstring Dictionary::Lookup(const std::string& section, const std::string& key) const
{
  DataType::const_iterator it = data.find(KeyType(section, key));
  if (it == data.end())
    return L"### MISSING STRING [" + ToUtf16String(section) + L", " + ToUtf16String(key) + L"] ###";

  return it->second;
}
