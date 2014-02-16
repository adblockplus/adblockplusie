#include <stdexcept>
#include <wctype.h>

#include "process.h"

//-------------------------------------------------------
// Windows_Handle
//-------------------------------------------------------
Windows_Handle::Windows_Handle( HANDLE h ) 
  : handle( h ) 
{
  if ( handle == INVALID_HANDLE_VALUE )
  {
    throw std::runtime_error( "Invalid handle" ) ;
  }
}

Windows_Handle::~Windows_Handle()
{
  CloseHandle( handle ) ;
}

//-------------------------------------------------------
// wcsncmpi
//-------------------------------------------------------
int wcsncmpi( const wchar_t * s1, const wchar_t * s2, unsigned int n )
{
  // Note: Equality of character sequences is case-insensitive in all predicates below.
  // Loop invariant: s1[0..j) == s2[0..j)
  for ( unsigned int j = 0 ; j < n ; ++j )
  {
    wchar_t c1 = towupper( *s1++ ) ;
    wchar_t c2 = towupper( *s2++ ) ;
    if ( c1 != c2 )
    {
      // Map to -1/+1 because c2 - c1 may not fit into an 'int'.
      return ( c1 < c2 ) ? -1 : 1 ;
    }
    else
    {
      if ( c1 == L'\0' )
      {
	// Assert length( s1 ) == length( s2 ) == j
	// Assert strings are equal at length < n
	return 0 ;
      }
    }
  }
  // Assert j == n
  // Assert s1[0..n) == s2[0..n)
  // The semantics of n-compare ignore everything after the first 'n' characters.
  return 0 ;
}

//-------------------------------------------------------
// Snapshot
//-------------------------------------------------------
Snapshot::Snapshot()
  : handle( ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) )
{
  process.dwSize = sizeof( PROCESSENTRY32W ) ;
}

PROCESSENTRY32W * Snapshot::begin()
{
  return ::Process32FirstW( handle, & process ) ? ( & process ) : 0 ;
}

PROCESSENTRY32W * Snapshot::next()
{
  return ::Process32NextW( handle, & process ) ? ( & process ) : 0 ;
}