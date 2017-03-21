/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2017 eyeo GmbH
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
#include <gtest/gtest.h>
#include "../../src/plugin/PluginUtil.h"

namespace
{
  template<class CharT, size_t M, size_t N>
  bool BeginsWithForTesting(const CharT(&s)[M], const CharT(&beginning)[N])
  {
    return BeginsWith(std::basic_string<CharT>(s), beginning);
  }
}

TEST(BeginsWith, EmptyIsEmpty)
{
  ASSERT_TRUE(BeginsWithForTesting(L"", L""));
}

TEST(BeginsWith, EmptyAlwaysBegins)
{
  ASSERT_TRUE(BeginsWithForTesting(L"foo", L""));
}

TEST(BeginsWith, EmptyNeverHasABeginning)
{
  ASSERT_FALSE(BeginsWithForTesting(L"", L"foo"));
}

TEST(BeginsWith, PartGood)
{
  ASSERT_TRUE(BeginsWithForTesting(L"foo", L"f"));
  ASSERT_TRUE(BeginsWithForTesting(L"foo", L"fo"));
}

TEST(BeginsWith, AllGood)
{
  ASSERT_TRUE(BeginsWithForTesting(L"foo", L"foo"));
}

TEST(BeginsWith, PartBad)
{
  ASSERT_FALSE(BeginsWithForTesting(L"foo", L"b"));
}

TEST(BeginsWith, AllBad)
{
  ASSERT_FALSE(BeginsWithForTesting(L"foo", L"bar"));
}

TEST(BeginsWith, ExtraBad)
{
  ASSERT_FALSE(BeginsWithForTesting(L"foo", L"foobar"));
}

TEST(BeginsWith, LongerBad)
{
  ASSERT_FALSE(BeginsWithForTesting(L"foo", L"barfoo"));
}

TEST(ToLowerString, Empty)
{
  ASSERT_EQ(L"", ToLowerString(L""));
}

TEST(ToLowerString, NotEmpty)
{
  ASSERT_EQ(L"foobar", ToLowerString(L"FooBAR"));
}

TEST(ToLowerString, NullWithExtra)
{
  std::wstring input(L"\0BAR", 4);
  ASSERT_EQ(4, input.length());

  std::wstring actual;
  ASSERT_NO_THROW({ actual = ToLowerString(input); });
  // White-box tests for further verification of no-crash behavior
  ASSERT_EQ(4, actual.length());
  ASSERT_EQ(L"", std::wstring(actual.c_str()));
}

TEST(ToLowerString, InternalNull)
{
  std::wstring input(L"Foo\0BAR", 7);
  ASSERT_EQ(7, input.length());

  std::wstring actual;
  ASSERT_NO_THROW({ actual = ToLowerString(input); });
  // White-box tests for further verification of no-crash behavior
  ASSERT_EQ(7, actual.length());
  ASSERT_EQ(L"foo", std::wstring(actual.c_str()));
}
