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
#include "../src/shared/Utils.h"

// Tests are in the following order
// x ? x # x
// 1 0 0 0 0
// 1 0 0 0 1 - the same as 10000
// 1 0 0 1 0
// 1 0 0 1 1
// 1 0 1 0 0 - the same as 10000
// 1 0 1 0 1 - the same as 10000
// 1 0 1 1 0 - the same as 10010
// 1 0 1 1 1 - the same as 10011
// 1 1 0 0 0
// 1 1 0 0 1 - the same as 11100
// 1 1 0 1 0
// 1 1 0 1 1
// 1 1 1 0 0
// 1 1 1 0 1 - the same as 11100
// 1 1 1 1 0
// 1 1 1 1 1

TEST(GetQueryStringTest, URL_10000)
{
  EXPECT_EQ(L"", GetQueryString(L"schema://host/path1/path2"));
}

TEST(GetQueryStringTest, URL_10010)
{
  EXPECT_EQ(L"", GetQueryString(L"schema://host/path1/path2#"));
}

TEST(GetQueryStringTest, URL_10011)
{
  EXPECT_EQ(L"", GetQueryString(L"schema://host/path1/path2#fragment"));
}

TEST(GetQueryStringTest, URL_11000)
{
  EXPECT_EQ(L"", GetQueryString(L"schema://host/path1/path2?"));
}

TEST(GetQueryStringTest, URL_11010)
{
  EXPECT_EQ(L"", GetQueryString(L"schema://host/path1/path2?#"));
}

TEST(GetQueryStringTest, URL_11011)
{
  EXPECT_EQ(L"", GetQueryString(L"schema://host/path1/path2?#fragment"));
}

TEST(GetQueryStringTest, URL_11100)
{
  EXPECT_EQ(L"queryString", GetQueryString(L"schema://host/path1/path2?queryString"));
}

TEST(GetQueryStringTest, URL_11110)
{
  EXPECT_EQ(L"queryString=value", GetQueryString(L"schema://host/path1/path2?queryString=value#"));
}

TEST(GetQueryStringTest, URL_11111)
{
  EXPECT_EQ(L"queryString=value", GetQueryString(L"schema://host/path1/path2?queryString=value#fragment"));
}

