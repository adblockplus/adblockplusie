#include <gtest/gtest.h>
#include "../process.h"
#include <functional>


/**
 * A promiscuous filter admits everything.
 */
struct always_true
  : public std::unary_function< PROCESSENTRY32W, bool >
{
  bool operator()( const PROCESSENTRY32W & ) { return true ; } ;
} ;

struct copy_all
  : public std::unary_function< PROCESSENTRY32W, PROCESSENTRY32W >
{
  PROCESSENTRY32W operator()( const PROCESSENTRY32W & process ) { return process ; }
} ;

/**
 * Construction test ensures that we don't throw and that at least one process shows up.
 */
TEST( Process_List_Test, construct )
{
  Process_List< PROCESSENTRY32W, always_true, copy_all > pl ;
  ASSERT_GE( pl.v.size(), 1u );
}

/**
 * Filter by a fixed name.
 */
struct our_process_by_name
  : std::unary_function< PROCESSENTRY32W, bool >
{
  bool operator()( const PROCESSENTRY32W & process ) { return 0 == wcscmp( process.szExeFile, L"installer-ca-tests.exe" ); } ;
};

/**
 * The only process we are really guaranteed to have is this test process itself.
 */
TEST( Process_List_Test, find_our_process )
{
  Process_List< PROCESSENTRY32W, our_process_by_name, copy_all > pl ;
  unsigned int size( pl.v.size() );
  EXPECT_EQ( 1u, size );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( 1u, size );
}


struct our_process_by_name_CI
  : std::unary_function< PROCESSENTRY32W, bool >
{
  bool operator()( const PROCESSENTRY32W & process ) 
  {
    static const wchar_t s2[] = L"Installer-CA-Tests.exe";
    return 0 == wcsncmpi( process.szExeFile, s2, sizeof( s2 ) / sizeof( wchar_t ) ) ;
  } ;
} ;

/**
 * The only process we are really guaranteed to have is this test process itself.
 */
TEST( Process_List_Test, find_our_process_CI )
{
  Process_List< PROCESSENTRY32W, our_process_by_name_CI, copy_all > pl ;
  unsigned int size( pl.v.size() );
  EXPECT_EQ( 1u, size );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( 1u, size );
}


/*
 * Extractor that copies only the PID.
 */
struct copy_PID
  : public std::unary_function< PROCESSENTRY32W, DWORD >
{
  DWORD operator()( const PROCESSENTRY32W & process ) { return process.th32ProcessID ; }
} ;


/**
 * Locate the PID of our process.
 */
TEST( Process_List_Test, find_our_PID )
{
  Process_List< DWORD, our_process_by_name, copy_PID > pl ;
  unsigned int size( pl.v.size() );
  EXPECT_EQ( size, 1u );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( size, 1u );
}

