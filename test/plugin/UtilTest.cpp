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
