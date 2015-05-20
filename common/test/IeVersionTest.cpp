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
#include "IeVersion.h"

/*
 * Exact version tests enabled by default.
 * If they are disabled, gtest will report 2 disabled tests.
 */
#if !defined(DISABLE_EXACT_TESTS)
#define exact(x) Exact##x
#else
#define exact(x) DISABLED_Exact##x
#endif
/*
 * Define a default version as the current version of IE in general release.
 * Predefine on the command line to compile tests for non-standard development environments.
 */
#ifndef INSTALLED_IE_VERSION
#define INSTALLED_IE_VERSION 11
#endif

TEST(InstalledVersionStringTest, Sanity)
{
  std::wstring version = AdblockPlus::IE::InstalledVersionString();
  ASSERT_FALSE(version.length() == 0); // separate test for default value
  ASSERT_TRUE(version.length() >= 2);
}

TEST(InstalledMajorVersionTest, Sanity)
{
  int version = AdblockPlus::IE::InstalledMajorVersion();
  ASSERT_NE(version, 0); // separate test for default value
  ASSERT_GE(version, 6);
  ASSERT_LE(version, 12);
  EXPECT_NE(version, 12); // This check will point out when the test needs updating.
}

TEST(InstalledVersionStringTest, exact(PrefixOnly))
{
    std::wstring version = AdblockPlus::IE::InstalledVersionString();
    std::wstring expected = std::to_wstring(INSTALLED_IE_VERSION) + L".";
    ASSERT_EQ(expected, version.substr(0, expected.length()));
}

TEST(InstalledMajorVersionTest, exact(MajorOnly))
{
  int version = AdblockPlus::IE::InstalledMajorVersion();
  ASSERT_EQ(version, INSTALLED_IE_VERSION);
}
