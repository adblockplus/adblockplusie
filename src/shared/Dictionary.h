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

#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <map>
#include <string>
#include <utility>

class Dictionary
{
  friend class DictionaryTest;

public:
  static void Create(const std::wstring& locale);
  static Dictionary* GetInstance();
  std::wstring Lookup(const std::string& section, const std::string& key) const;

private:
  static Dictionary* instance;

  typedef std::pair<std::string,std::string> KeyType;
  typedef std::map<KeyType,std::wstring> DataType;
  DataType data;

  Dictionary(const std::wstring& locale);
  bool ReadDictionary(const std::wstring& basePath, const std::wstring& locale);
};

#endif
