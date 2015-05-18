/**
 * \file exception_test.cpp Unit tests for the library-wide exception classes in installer-lib.h
 */

#include <gtest/gtest.h>

#include <Windows.h>

#include "../installer-lib.h"

TEST( Exception_Test, empty_two )
{
  ::SetLastError( 0 ) ;
  WindowsApiError e( "", "" ) ;
  ASSERT_STREQ( "<unspecified> returned <unknown> with last error code 0", e.what() ) ;
}

TEST( Exception_Test, empty_three )
{
  ::SetLastError( 1 ) ;
  WindowsApiError e( "", "", "" ) ;
  ASSERT_STREQ( "<unspecified> returned <unknown> with last error code 1", e.what() ) ;
}

TEST( Exception_Test, empty_empty_message )
{
  ::SetLastError( 2 ) ;
  WindowsApiError e( "", "", "message" ) ;
  ASSERT_STREQ( "<unspecified> returned <unknown> with last error code 2: message", e.what() ) ;
}

TEST( Exception_Test, string_number )
{
  ::SetLastError( 3 ) ;
  WindowsApiError e( "Beep", 1 ) ;
  ASSERT_STREQ( "Beep returned 1 with last error code 3", e.what() ) ;
}

TEST( Exception_Test, string_number_message )
{
  ::SetLastError( 4 ) ;
  WindowsApiError e( "Beep", 1, "message" ) ;
  ASSERT_STREQ( "Beep returned 1 with last error code 4: message", e.what() ) ;
}

TEST( Exception_Test, string_string )
{
  ::SetLastError( 5 ) ;
  WindowsApiError e( "GetErrorMode", "SEM_FAILCRITICALERRORS" ) ;
  ASSERT_STREQ( "GetErrorMode returned SEM_FAILCRITICALERRORS with last error code 5", e.what() ) ;
}

TEST( Exception_Test, string_string_message )
{
  ::SetLastError( 6 ) ;
  WindowsApiError e( "GetErrorMode", "SEM_FAILCRITICALERRORS", "message" ) ;
  ASSERT_STREQ( "GetErrorMode returned SEM_FAILCRITICALERRORS with last error code 6: message", e.what() ) ;
}
