#include <gtest/gtest.h>

#include "../process.h"


/**
 * A promiscuous filter admits everything.
 */
bool trivial_true( PROCESSENTRY32W & )
{
  return true;
}

/*
 * Converter that copies everything, by reference.
 */
PROCESSENTRY32W & copy_all( PROCESSENTRY32W & process )
{
  return process ;
}

/**
 * Construction test ensures that we don't throw and that at least one process shows up.
 */
TEST( Process_List_Test, construct )
{
  Process_List< PROCESSENTRY32W > pl( trivial_true, copy_all );
  ASSERT_GE( pl.v.size(), 1u );
}

/**
 * Filter by a fixed name.
 */
bool our_process_by_name( PROCESSENTRY32W & process )
{
  return 0 == wcscmp( process.szExeFile, L"installer-ca-tests.exe" );
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 */
TEST( Process_List_Test, find_our_process )
{
  Process_List< PROCESSENTRY32W > pl( our_process_by_name, copy_all );
  unsigned int size( pl.v.size() );
  EXPECT_EQ( size, 1u );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( size, 1u );
}

/*
 * Converter that copies only the PID, by value.
 */
DWORD copy_PID( PROCESSENTRY32W & process )
{
  return process.th32ProcessID ;
}

/**
 * Locate the PID of our process.
 */
TEST( Process_List_Test, find_our_PID )
{
  Process_List< DWORD > pl( our_process_by_name, copy_PID );
  unsigned int size( pl.v.size() );
  EXPECT_EQ( size, 1u );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( size, 1u );
}

