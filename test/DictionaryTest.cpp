/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2016 Eyeo GmbH
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
#include <Windows.h>
#include <gtest/gtest.h>

#include "../src/shared/Dictionary.h"
#include "../src/shared/Utils.h"

namespace
{
  void WriteLocale(const std::wstring& basePath, const std::wstring& locale, const std::string& data)
  {
    std::wstring filePath = basePath + locale + L".ini";
    std::ofstream stream(filePath);
    if (stream.fail())
      throw std::runtime_error("Failed creating locale file " + ToUtf8String(filePath));

    stream << data;
    if (stream.fail())
      throw std::runtime_error("Failed writing to file " + ToUtf8String(filePath));
  }

  void RemoveLocale(const std::wstring& basePath, const std::wstring& locale)
  {
    std::wstring filePath = basePath + locale + L".ini";
    ::DeleteFileW(filePath.c_str());
  }
}

class DictionaryTest : public ::testing::Test
{
protected:
  std::wstring basePath;

  void SetUp()
  {
    basePath = GetDllDir() + L"locales\\";
    ::CreateDirectoryW(basePath.c_str(), NULL);

    WriteLocale(basePath, L"en", "[general]\nfoo=bar\n[x]\nx=y\n");
    WriteLocale(basePath, L"es-ES", "[general]\n#comment=nada\nfoo=esbar");
    WriteLocale(basePath, L"ru", "[general]\ntrash\nfoo=\xD1\x82\xD0\xB5\xD1\x81\xD1\x82\n");
  }
  void TearDown()
  {
    if (Dictionary::instance)
    {
      delete Dictionary::instance;
      Dictionary::instance = 0;
    }

    RemoveLocale(basePath, L"en");
    RemoveLocale(basePath, L"es-ES");
    RemoveLocale(basePath, L"ru");
    ::RemoveDirectoryW(basePath.c_str());
  }
};

TEST_F(DictionaryTest, DefaultLocale)
{
  Dictionary::Create(L"en");
  Dictionary* dict = Dictionary::GetInstance();

  ASSERT_TRUE(dict);
  ASSERT_EQ(L"bar", dict->Lookup("general", "foo"));
  ASSERT_EQ(L"y", dict->Lookup("x", "x"));
  ASSERT_NE(L"bar", dict->Lookup("", "foo"));
  ASSERT_NE(L"bar", dict->Lookup("x", "foo"));
  ASSERT_NE(L"bar", dict->Lookup("generalsomething", "foo"));
  ASSERT_FALSE(dict->Lookup("foo", "bar").empty());
}

TEST_F(DictionaryTest, LocaleFallback)
{
  Dictionary::Create(L"es-ES");
  Dictionary* dict = Dictionary::GetInstance();

  ASSERT_TRUE(dict);
  ASSERT_EQ(L"esbar", dict->Lookup("general", "foo"));
  ASSERT_EQ(L"y", dict->Lookup("x", "x"));
}

TEST_F(DictionaryTest, LocaleFallbackShort)
{
  Dictionary::Create(L"ru-RU");
  Dictionary* dict = Dictionary::GetInstance();

  ASSERT_TRUE(dict);
  ASSERT_EQ(L"\u0442\u0435\u0441\u0442", dict->Lookup("general", "foo"));
  ASSERT_EQ(L"y", dict->Lookup("x", "x"));
}
