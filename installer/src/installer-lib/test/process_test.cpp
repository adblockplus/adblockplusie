#include <gtest/gtest.h>
#include "../process.h"
#include <functional>

// Turn off warnings for string copies
#pragma warning( disable : 4996 )

//-------------------------------------------------------
// Comparison objects
//-------------------------------------------------------

const wchar_t exact_file_name[] = L"installer-ca-tests.exe" ;
const wchar_t mixedcase_file_name[] = L"Installer-CA-Tests.exe" ;
const wchar_t * multiple_file_names[] = { mixedcase_file_name, L"non-matching-name" } ;
const wchar_t * multiple_module_names[] = { L"kernel32.dll", L"non-matching-name" } ;
const wchar_t * non_existent_module_names[] = { L"non-matching-name" } ;

/**
 * Compare to our own process name, case-sensitive, no length limit
 */
struct our_process_by_name
  : std::unary_function< PROCESSENTRY32W, bool >
{
  bool operator()( const PROCESSENTRY32W & process )
  {
    return 0 == wcscmp( process.szExeFile, exact_file_name ) ;
  } ;
};

/**
 * Compare to our own process name, case-insensitive, no length limit
 */
struct our_process_by_name_CI
  : std::unary_function< PROCESSENTRY32W, bool >
{
  bool operator()( const PROCESSENTRY32W & process ) 
  {
    return 0 == wcscmpi( process.szExeFile, mixedcase_file_name ) ;
  } ;
} ;

/**
 * Compare to our own process name, case-insensitive, length-limited
 */
struct our_process_by_name_CI_N
  : std::unary_function< PROCESSENTRY32W, bool >
{
  bool operator()( const PROCESSENTRY32W & process ) 
  {
    return 0 == wcsncmpi( process.szExeFile, mixedcase_file_name, sizeof( mixedcase_file_name ) / sizeof( wchar_t ) ) ;
  } ;
} ;


//-------------------------------------------------------
//-------------------------------------------------------
/**
 * Filter by process name. Comparison is case-insensitive.
 */
class process_by_any_file_name_CI
  : public std::unary_function< PROCESSENTRY32W, bool >
{
  const file_name_set & names ;
public:
  bool operator()( const PROCESSENTRY32W & process)
  {
    return names.find( process.szExeFile ) != names.end() ;
  }
  process_by_any_file_name_CI( const file_name_set & names )
    : names( names )
  {}
} ;


//-------------------------------------------------------
// TESTS, no snapshots
//-------------------------------------------------------
PROCESSENTRY32 process_with_name( const wchar_t * s )
{
  PROCESSENTRY32W p ;
  wcsncpy( p.szExeFile, s, MAX_PATH ) ;
  return p ;
}

PROCESSENTRY32 process_empty = process_with_name( L"" ) ;
PROCESSENTRY32 process_exact = process_with_name( exact_file_name ) ;
PROCESSENTRY32 process_mixedcase = process_with_name( mixedcase_file_name ) ;
PROCESSENTRY32 process_explorer = process_with_name( L"explorer.exe" ) ;
PROCESSENTRY32 process_absent = process_with_name( L"no_such_name" ) ;

file_name_set multiple_name_set( multiple_file_names, 2 ) ;
file_name_set multiple_name_set_modules( multiple_module_names, 2 ) ;
file_name_set non_existent_name_set_modules( non_existent_module_names, 1 ) ;
process_by_any_file_name_CI find_in_set( multiple_name_set ) ;
process_by_any_exe_with_any_module find_in_set_w_kernel32( multiple_name_set, multiple_name_set_modules ) ;
process_by_any_exe_with_any_module find_in_set_w_non_existent( multiple_name_set, non_existent_name_set_modules ) ;

TEST( file_name_set, validate_setup )
{
  ASSERT_EQ( 2u, multiple_name_set.size() ) ;
  ASSERT_TRUE( multiple_name_set.find( mixedcase_file_name ) != multiple_name_set.end() ) ;
  ASSERT_TRUE( multiple_name_set.find( exact_file_name ) != multiple_name_set.end() ) ;
  ASSERT_TRUE( multiple_name_set.find( L"" ) == multiple_name_set.end() ) ;
  ASSERT_TRUE( multiple_name_set.find( L"not-in-list" ) == multiple_name_set.end() ) ;
}

TEST( process_by_any_file_name_CI, empty )
{
  const wchar_t * elements[ 1 ] = { 0 } ;   // cheating a bit
  file_name_set s( elements, 0 ) ;
  process_by_any_file_name_CI x( s ) ;

  ASSERT_FALSE( x( process_empty ) ) ;
  ASSERT_FALSE( x( process_exact ) ) ;
  ASSERT_FALSE( x( process_mixedcase ) ) ;
  ASSERT_FALSE( x( process_explorer ) ) ;
  ASSERT_FALSE( x( process_absent ) ) ;
}

TEST( process_by_any_file_name_CI, single_element )
{
  const wchar_t * elements[ 1 ] = { exact_file_name } ;
  file_name_set s( elements, 1 ) ;
  process_by_any_file_name_CI x( s ) ;

  ASSERT_FALSE( x( process_empty ) ) ;
  ASSERT_TRUE( x( process_exact ) ) ;
  ASSERT_TRUE( x( process_mixedcase ) ) ;
  ASSERT_FALSE( x( process_explorer ) ) ;
  ASSERT_FALSE( x( process_absent ) ) ;
}

TEST( process_by_any_file_name_CI, two_elements )
{
  file_name_set s( multiple_file_names, 2 ) ;
  process_by_any_file_name_CI x( s ) ;

  ASSERT_FALSE( find_in_set( process_empty ) ) ;
  ASSERT_TRUE( find_in_set( process_exact ) ) ;
  ASSERT_TRUE( find_in_set( process_mixedcase ) ) ;
  ASSERT_FALSE( find_in_set( process_explorer ) ) ;
  ASSERT_FALSE( find_in_set( process_absent ) ) ;
}

//-------------------------------------------------------
// Single-snapshot version of initializers
//-------------------------------------------------------
/**
 * Single-snapshot version of initialize_process_list, for testing.
 */
template< class T, class Admittance, class Extractor >
void initialize_process_list( std::vector< T > & v, Admittance admit = Admittance(), Extractor extract = Extractor() )
{
  initialize_process_list( v, Snapshot(), admit, extract ) ;
}

/**
 * Single-snapshot version of initialize_process_set, for testing.
 */
template< class T, class Admittance, class Extractor >
void initialize_process_set( std::set< T > & s, Admittance admit = Admittance(), Extractor extract = Extractor() )
{
  initialize_process_set( s, Snapshot(), admit, extract ) ;
}

//-------------------------------------------------------
// TESTS with snapshots
//-------------------------------------------------------
/**
 * Construction test ensures that we don't throw and that at least one process shows up.
 */
TEST( Process_List_Test, construct_vector )
{
  std::vector< PROCESSENTRY32W > v ;
  initialize_process_list( v, every_process(), copy_all() ) ;
  ASSERT_GE( v.size(), 1u );
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 */
TEST( Process_List_Test, find_our_process )
{
  std::vector< PROCESSENTRY32W > v ;
  initialize_process_list( v, our_process_by_name(), copy_all() ) ;
  unsigned int size( v.size() );
  EXPECT_EQ( 1u, size );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( 1u, size );
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 * This test uses a filter function class with a special, fixed name comparison.
 */
TEST( Process_List_Test, find_our_process_CI_N_special )
{
  std::vector< PROCESSENTRY32W > v ;
  initialize_process_list( v, our_process_by_name_CI_N(), copy_all() ) ;
  unsigned int size( v.size() );
  EXPECT_EQ( 1u, size );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( 1u, size );
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 * This test uses the generic filter function.
 */
TEST( Process_List_Test, find_our_process_CI_N_generic )
{
  std::vector< PROCESSENTRY32W > v ;
  initialize_process_list( v, process_by_name_CI( mixedcase_file_name ), copy_all() ) ;
  unsigned int size( v.size() );
  EXPECT_EQ( 1u, size );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( 1u, size );
}

/**
 * Locate the PID of our process.
 */
TEST( Process_List_Test, find_our_PID )
{
  std::vector< DWORD > v ;
  initialize_process_list( v, our_process_by_name(), copy_PID() ) ;
  unsigned int size( v.size() );
  EXPECT_EQ( size, 1u );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( size, 1u );
}

/**
 * Locate the PID of our process using the
 */
TEST( Process_List_Test, find_our_process_in_set )
{
  std::vector< DWORD > v ;
  initialize_process_list( v, find_in_set, copy_PID() ) ;
  unsigned int size( v.size() );
  EXPECT_EQ( size, 1u );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( size, 1u );
}

//-------------------------------------------------------
// TESTS for process ID sets
//-------------------------------------------------------
/*
 * Can't use copy_all without a definition for "less< PROCESSENTRY32W >".
 * Thus all tests only use copy_PID
 */

/**
 * Construction test ensures that we don't throw and that at least one process shows up.
 */
TEST( pid_set, construct_set )
{
  std::set< DWORD > s ;
  initialize_process_set( s, every_process(), copy_PID() ) ;
  ASSERT_GE( s.size(), 1u );
}

TEST( pid_set, find_our_process_in_set )
{
  std::set< DWORD > s ;
  initialize_process_set( s, find_in_set, copy_PID() ) ;
  size_t size( s.size() ) ;
  EXPECT_EQ( size, 1u );
  ASSERT_GE( size, 1u );
}

TEST( pid_set, find_our_process_in_set_w_kernel32 )
{
  std::set< DWORD > s ;
  initialize_process_set( s, find_in_set_w_kernel32, copy_PID() ) ;
  size_t size( s.size() ) ;
  EXPECT_EQ( size, 1u );
  ASSERT_GE( size, 1u );
}
TEST( pid_set, find_our_process_in_set_w_non_existant )
{
  std::set< DWORD > s ;
  initialize_process_set( s, find_in_set_w_non_existent, copy_PID() ) ;
  size_t size( s.size() ) ;
  EXPECT_EQ( size, 0u );
  ASSERT_GE( size, 0u );
}
