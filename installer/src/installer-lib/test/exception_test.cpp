/**
 * \file exception_test.cpp Unit tests for the library-wide exception classes in installer-lib.h
 */

#include <gtest/gtest.h>

#include <Windows.h>

#include "../installer-lib.h"

TEST(ExceptionTest, EmptyTwo)
{
  ::SetLastError(0);
  WindowsApiError e("", "");
  ASSERT_STREQ("<unspecified> returned <unknown> with last error code 0", e.what());
}

TEST(ExceptionTest, EmptyThree)
{
  ::SetLastError(1);
  WindowsApiError e("", "", "");
  ASSERT_STREQ("<unspecified> returned <unknown> with last error code 1", e.what());
}

TEST(ExceptionTest, EmptyEmptyMessage)
{
  ::SetLastError(2);
  WindowsApiError e("", "", "message");
  ASSERT_STREQ("<unspecified> returned <unknown> with last error code 2: message", e.what());
}

TEST(ExceptionTest, StringNumber)
{
  ::SetLastError(3);
  WindowsApiError e("Beep", 1);
  ASSERT_STREQ("Beep returned 1 with last error code 3", e.what());
}

TEST(ExceptionTest, StringNumberMessage)
{
  ::SetLastError(4);
  WindowsApiError e("Beep", 1, "message");
  ASSERT_STREQ("Beep returned 1 with last error code 4: message", e.what());
}

TEST(ExceptionTest, StringString)
{
  ::SetLastError(5);
  WindowsApiError e("GetErrorMode", "SEM_FAILCRITICALERRORS");
  ASSERT_STREQ("GetErrorMode returned SEM_FAILCRITICALERRORS with last error code 5", e.what());
}

TEST(ExceptionTest, StringStringMessage)
{
  ::SetLastError(6);
  WindowsApiError e("GetErrorMode", "SEM_FAILCRITICALERRORS", "message");
  ASSERT_STREQ("GetErrorMode returned SEM_FAILCRITICALERRORS with last error code 6: message", e.what());
}
