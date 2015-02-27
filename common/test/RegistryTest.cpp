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
#include "Registry.h"

using namespace AdblockPlus;

TEST(RegistryTest, Simple00)
{
  ASSERT_NO_THROW({ auto r = RegistryKey(HKEY_CLASSES_ROOT, L"CLSID"); });
}

TEST(RegistryTest, Simple01)
{
  ASSERT_ANY_THROW({ auto r = RegistryKey(HKEY_CLASSES_ROOT, L"IREALLYHOPENOBODYHASREGISTEREDTHISKEY"); });
}

TEST(RegistryTest, ConstructorIllegalArgument00)
{
  // Empty string arguments are illegal by fiat; there's an explicit check
  ASSERT_ANY_THROW({ auto r = RegistryKey(HKEY_CLASSES_ROOT, L""); });
}

TEST(RegistryTest, ValueNotFound00)
{
  auto r = RegistryKey(HKEY_CLASSES_ROOT, L"CLSID");
  ASSERT_ANY_THROW({ r.value_wstring(L"nonexistent"); });
}
