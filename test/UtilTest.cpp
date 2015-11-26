/*
 * This file is part of Adblock Plus <http://adblockplus.org/>,
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
#include <gtest/gtest.h>
#include "../src/shared/Utils.h"

namespace
{
  void TrimTestBody(const std::wstring& input, const std::wstring& expected)
  {
    std::wstring trimmed = TrimString(input);
    EXPECT_EQ(expected, trimmed);
  }

  void TrimLeftTestBody(const std::wstring& input, const std::wstring& expected)
  {
    std::wstring trimmed = TrimStringLeft(input);
    EXPECT_EQ(expected, trimmed);
  }

  void TrimRightTestBody(const std::wstring& input, const std::wstring& expected)
  {
    std::wstring trimmed = TrimStringRight(input);
    EXPECT_EQ(expected, trimmed);
  }
}

TEST(TrimTest, Trim00)
{
  const std::wstring x = L"";
  TrimTestBody(x, L"");
  TrimLeftTestBody(x, L"");
  TrimRightTestBody(x, L"");
}

TEST(TrimTest, Trim01)
{
  const std::wstring x = L" ";
  TrimTestBody(x, L"");
  TrimLeftTestBody(x, L"");
  TrimRightTestBody(x, L"");
}

TEST(TrimTest, Trim02)
{
  const std::wstring x = L"\n";
  TrimTestBody(x, L"");
  TrimLeftTestBody(x, L"");
  TrimRightTestBody(x, L"");
}

TEST(TrimTest, Trim03)
{
  const std::wstring x = L"\r";
  TrimTestBody(x, L"");
  TrimLeftTestBody(x, L"");
  TrimRightTestBody(x, L"");
}

TEST(TrimTest, Trim04)
{
  const std::wstring x = L"\t";
  TrimTestBody(x, L"");
  TrimLeftTestBody(x, L"");
  TrimRightTestBody(x, L"");
}

TEST(TrimTest, Trim05)
{
  const std::wstring x = L"foo";
  TrimTestBody(x, L"foo");
  TrimLeftTestBody(x, L"foo");
  TrimRightTestBody(x, L"foo");
}

TEST(TrimTest, Trim06)
{
  const std::wstring x = L" foo";
  TrimTestBody(x, L"foo");
  TrimLeftTestBody(x, L"foo");
  TrimRightTestBody(x, L" foo");
}

TEST(TrimTest, Trim07)
{
  const std::wstring x = L"\r\nfoo";
  TrimTestBody(x, L"foo");
  TrimLeftTestBody(x, L"foo");
  TrimRightTestBody(x, L"\r\nfoo");
}

TEST(TrimTest, Trim08)
{
  const std::wstring x = L"\tfoo";
  TrimTestBody(x, L"foo");
  TrimLeftTestBody(x, L"foo");
  TrimRightTestBody(x, L"\tfoo");
}

TEST(TrimTest, Trim09)
{
  const std::wstring x = L"foo  ";
  TrimTestBody(x, L"foo");
  TrimLeftTestBody(x, L"foo  ");
  TrimRightTestBody(x, L"foo");
}

TEST(TrimTest, Trim10)
{
  const std::wstring x = L"foo\r\n";
  TrimTestBody(x, L"foo");
  TrimLeftTestBody(x, L"foo\r\n");
  TrimRightTestBody(x, L"foo");
}

TEST(TrimTest, Trim11)
{
  const std::wstring x = L"foo\t";
  TrimTestBody(x, L"foo");
  TrimLeftTestBody(x, L"foo\t");
  TrimRightTestBody(x, L"foo");
}

TEST(TrimTest, Trim12)
{
  const std::wstring x = L"foo bar";
  TrimTestBody(x, L"foo bar");
  TrimLeftTestBody(x, L"foo bar");
  TrimRightTestBody(x, L"foo bar");
}

TEST(TrimTest, Trim13)
{
  const std::wstring x = L"foo bar \r\n";
  TrimTestBody(x, L"foo bar");
  TrimLeftTestBody(x, L"foo bar \r\n");
  TrimRightTestBody(x, L"foo bar");
}

TEST(TrimTest, Trim14)
{
  const std::wstring x = L"  foo bar \r\n";
  TrimTestBody(x, L"foo bar");
  TrimLeftTestBody(x, L"foo bar \r\n");
  TrimRightTestBody(x, L"  foo bar");
}

/*
 * First of two tests verifying that TrimString() does not alter its argument.
 * This one tests an ordinary, non-const variable as an argument.
 * It differs from its companion only in the one line that declares the variable used as an argument.
 */
TEST(TrimTest, TrimPassesByValue)
{
  std::wstring x = L"foo bar  "; // not declared 'const', could alter
  const std::wstring y = x;
  ASSERT_EQ(y, x);
  std::wstring trimmed = TrimString(x); // expect here pass by value
  EXPECT_EQ(L"foo bar", trimmed);
  ASSERT_EQ(y, x); // argument variable not altered
}

/*
 * Second of two tests verifying that TrimString() does not alter its argument.
 * This one tests a const variable as an argument.
 */
TEST(TrimTest, TrimBindsOnConstArg)
{
  const std::wstring x = L"foo bar  "; // declared 'const'
  const std::wstring y = x;
  ASSERT_EQ(y, x);
  std::wstring trimmed = TrimString(x);
  EXPECT_EQ(L"foo bar", trimmed);
  ASSERT_EQ(y, x);
}
