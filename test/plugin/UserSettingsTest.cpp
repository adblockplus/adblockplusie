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
#include <gtest/gtest.h>

#include <OAIdl.h>
#include "../../src/plugin/PluginUserSettings.h"

//----------------------------------
// GetIDsOfNames
//----------------------------------

namespace
{
  void SingleMethodNameFound(std::wstring name, DISPID expected_id)
  {
    CPluginUserSettings x;
    wchar_t* names[] = {const_cast<wchar_t*>(name.c_str())};
    DISPID ids[1];
    HRESULT h = x.GetIDsOfNames(IID_NULL, names, 1, 0, ids);
    ASSERT_EQ(S_OK, h);
    DISPID id = ids[0];
    ASSERT_EQ(expected_id, id);
  }

  void SingleMethodNameNotFound(std::wstring name)
  {
    CPluginUserSettings x;
    wchar_t* names[] = {const_cast<wchar_t*>(name.c_str())};
    DISPID ids[1];
    HRESULT h = x.GetIDsOfNames(IID_NULL, names, 1, 0, ids);
    ASSERT_NE(S_OK, h);
    EXPECT_EQ(DISP_E_UNKNOWNNAME, h);
  }
}

TEST(CPluginUserSettingsGetIDsOfNames, AllDefinedMethodsMustBeFound)
{
  CPluginUserSettings x;
  SingleMethodNameFound(L"GetMessage", 0);
  SingleMethodNameFound(L"GetLanguageCount", 1);
  SingleMethodNameFound(L"GetLanguageByIndex", 2);
  SingleMethodNameFound(L"GetLanguageTitleByIndex", 3);
  SingleMethodNameFound(L"SetLanguage", 4);
  SingleMethodNameFound(L"GetLanguage", 5);
  SingleMethodNameFound(L"GetWhitelistDomains", 6);
  SingleMethodNameFound(L"AddWhitelistDomain", 7);
  SingleMethodNameFound(L"RemoveWhitelistDomain", 8);
  SingleMethodNameFound(L"GetAppLocale", 9);
  SingleMethodNameFound(L"GetDocumentationLink", 10);
  SingleMethodNameFound(L"IsAcceptableAdsEnabled", 11);
  SingleMethodNameFound(L"SetAcceptableAdsEnabled", 12);
  SingleMethodNameFound(L"IsUpdate", 13);
}

TEST(CPluginUserSettingsGetIDsOfNames, UndefinedMethodsMustNotBeFound)
{
  SingleMethodNameNotFound(L"");
  SingleMethodNameNotFound(L"clearly unknown");
  SingleMethodNameNotFound(L"GETMESSAGE");
}

//----------------------------------
// Invoke
//----------------------------------

namespace
{
  void InvokeInvalidDispatchId(DISPID id)
  {
    CPluginUserSettings x;
    DISPPARAMS params;
    params.rgvarg = nullptr;
    params.rgdispidNamedArgs = nullptr;
    params.cArgs = 0;
    params.cNamedArgs = 0;
    EXCEPINFO ex;
    HRESULT h = x.Invoke(id, IID_NULL, 0, DISPATCH_METHOD, &params, nullptr, &ex, nullptr);
    ASSERT_NE(S_OK, h);
    EXPECT_EQ(DISP_E_MEMBERNOTFOUND, h);
  }
}

/**
 * Verify that a negative Dispatch ID returns the proper error code.
 */
TEST(CPluginUserSettingsInvoke, InvalidDispatchIdShouldUnderflow)
{
  InvokeInvalidDispatchId(-1);
}

/**
 * Verify that a positive Dispatch ID that's too large returns the proper error code.
 */
TEST(CPluginUserSettingsInvoke, InvalidDispatchIdShouldOverflow)
{
  InvokeInvalidDispatchId(14);
}
