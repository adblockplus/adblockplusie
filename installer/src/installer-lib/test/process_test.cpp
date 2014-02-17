#include <gtest/gtest.h>
#include "../process.h"
#include <functional>


/**
 * Single-snapshot version of initialize_process_list, for testing.
 */
template< class T, class Admittance, class Extractor >
void initialize_process_list( std::vector< T > & v, Admittance admit = Admittance(), Extractor extract = Extractor() )
{
  initialize_process_list( v, Snapshot(), admit, extract ) ;
}


/**
 * Construction test ensures that we don't throw and that at least one process shows up.
 */
TEST( Process_List_Test, construct )
{
  std::vector< PROCESSENTRY32W > v ;
  initialize_process_list( v, every_process(), copy_all() ) ;
  ASSERT_GE( v.size(), 1u );
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
  std::vector< PROCESSENTRY32W > v ;
  initialize_process_list( v, our_process_by_name(), copy_all() ) ;
  unsigned int size( v.size() );
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
 * This test uses a filter function class with a special, fixed name comparison.
 */
TEST( Process_List_Test, find_our_process_CI_special )
{
  std::vector< PROCESSENTRY32W > v ;
  initialize_process_list( v, our_process_by_name_CI(), copy_all() ) ;
  unsigned int size( v.size() );
  EXPECT_EQ( 1u, size );    // Please, don't run multiple test executables simultaneously
  ASSERT_GE( 1u, size );
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 * This test uses the generic filter function.
 */
TEST( Process_List_Test, find_our_process_CI_generic )
{
  std::vector< PROCESSENTRY32W > v ;
  initialize_process_list( v, process_by_name_CI( L"Installer-CA-Tests.exe" ), copy_all() ) ;
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

