/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
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
#include <Windows.h>
#include "../../src/plugin/PluginDebug.h"
#include <memory>

#if defined(_WIN64)
#define expected_length 18
#define expected_nullptr_literal L"0x0000000000000000"
#elif defined(_WIN32)
#define expected_length 10
#define expected_nullptr_literal L"0x00000000"
#endif

TEST(ToHexLiteral, Null)
{
  ASSERT_EQ(expected_nullptr_literal, ToHexLiteral(nullptr));
}

TEST(ToHexLiteral, NotNull)
{
  int p = 0;
  ASSERT_NE(&p, nullptr);
  ASSERT_EQ(expected_length, ToHexLiteral(&p).length());
}
