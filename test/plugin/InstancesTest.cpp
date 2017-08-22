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
#include "../../src/plugin/Instances.h"

typedef SyncMap<int, int, 0> SyncMapOne;

TEST(SyncMap, Instantiate)
{
  SyncMapOne s;
}

TEST(SyncMap, OrdinaryAdded)
{
  SyncMapOne s;
  ASSERT_TRUE(s.AddIfAbsent(1, 11));
  ASSERT_TRUE(s.Locate(1) == 11);
  ASSERT_TRUE(s.RemoveIfPresent(1));
}

TEST(SyncMap, OrdinaryNotAdded1)
{
  SyncMapOne s;
  ASSERT_TRUE(s.Locate(1) == 0);
}

TEST(SyncMap, OrdinaryNotAdded2)
{
  SyncMapOne s;
  ASSERT_TRUE(s.AddIfAbsent(1, 11));
  ASSERT_TRUE(s.AddIfAbsent(2, 22));
  ASSERT_TRUE(s.Locate(7) == 0);
}

TEST(SyncMap, WrongAddedTwice)
{
  SyncMapOne s;
  ASSERT_TRUE(s.AddIfAbsent(3, 11));
  ASSERT_FALSE(s.AddIfAbsent(3, 22));
}

TEST(SyncMap, AcceptableAddedRemovedAddedAgain)
{
  SyncMapOne s;
  ASSERT_TRUE(s.AddIfAbsent(1, 11));
  ASSERT_TRUE(s.RemoveIfPresent(1));
  ASSERT_TRUE(s.AddIfAbsent(1, 22));
}

TEST(SyncMap, WrongRemovedButNotAdded)
{
  SyncMapOne s;
  ASSERT_FALSE(s.RemoveIfPresent(3));
}

