/*
 * This file is part of Adblock Plus <http://adblockplus.org/>,
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
#include "../src/shared/Utils.h"

namespace
{
  void TrimTestBody(std::wstring input, std::wstring expected)
  {
    std::wstring trimmed = TrimString(input);
    ASSERT_EQ(expected, trimmed);
  }
}

TEST(TrimTest, Trim00)
{
  TrimTestBody(L"", L"");
}

TEST(TrimTest, Trim01)
{
  TrimTestBody(L" ", L"");
}

TEST(TrimTest, Trim02)
{
  TrimTestBody(L"\n", L"");
}

TEST(TrimTest, Trim03)
{
  TrimTestBody(L"\r", L"");
}

TEST(TrimTest, Trim04)
{
  TrimTestBody(L"\t", L"");
}

TEST(TrimTest, Trim05)
{
  TrimTestBody(L"foo", L"foo");
}

TEST(TrimTest, Trim06)
{
  TrimTestBody(L" foo", L"foo");
}

TEST(TrimTest, Trim07)
{
  TrimTestBody(L"\r\nfoo", L"foo");
}

TEST(TrimTest, Trim08)
{
  TrimTestBody(L"\tfoo", L"foo");
}

TEST(TrimTest, Trim09)
{
  TrimTestBody(L"foo  ", L"foo");
}

TEST(TrimTest, Trim10)
{
  TrimTestBody(L"foo\r\n", L"foo");
}

TEST(TrimTest, Trim11)
{
  TrimTestBody(L"foo\t", L"foo");
}

TEST(TrimTest, Trim12)
{
  TrimTestBody(L"foo bar", L"foo bar");
}

TEST(TrimTest, Trim13)
{
  TrimTestBody(L"foo bar \r\n", L"foo bar");
}

TEST(TrimTest, Trim14)
{
  TrimTestBody(L"  foo bar \r\n", L"foo bar");
}
