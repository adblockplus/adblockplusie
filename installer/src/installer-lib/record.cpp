/**
* \file record.cpp Implementation of Record class.
*/

#include "installer-lib.h"
#include "record.h"
#include "msiquery.h"

//-----------------------------------------------------------------------------------------
// Record
//-----------------------------------------------------------------------------------------
Record::Record( unsigned int n_fields )
{
  _handle = MsiCreateRecord( n_fields ) ;
  if ( ! _handle )
  {
    throw windows_api_error( "MsiCreateRecord", 0 ) ;
  }
}

Record::~Record()
{
  if ( _handle != 0 )
  {
    MsiCloseHandle( _handle ) ;
  }
}

void Record::only_non_null()
{
  if ( _handle == 0 )
  {
    throw std::runtime_error( "Operation only permitted for non-null objects" ) ;
  }
}

void Record::assign_string( unsigned int field_index, const char *value )
{
  only_non_null() ;
  MsiRecordSetStringA( _handle, field_index, value ) ;
}

void Record::assign_string( unsigned int field_index, const wchar_t *value )
{
  only_non_null() ;
  MsiRecordSetStringW( _handle, field_index, value ) ;
}

/**
* \par Implementation
*    - MSDN [MsiRecordGetString](http://msdn.microsoft.com/en-us/library/aa370368%28v=vs.85%29.aspx)
*/
std::wstring Record::value_string( unsigned int field_index )
{
  static wchar_t initial_buffer[ 1024 ] = L"" ;
  DWORD length = 1023 ; // one less than the buffer length to hold a terminating null character
  UINT x = MsiRecordGetStringW( _handle, field_index, initial_buffer, & length ) ;
  if ( x == ERROR_SUCCESS )
  {
    return std::wstring( initial_buffer ) ;
  }
  if ( x == ERROR_MORE_DATA )
  {
    // Future: handle longer strings.
    /*
    * The present custom action only uses this function for strings that appear in dialog boxes.
    * A thousand characters is about a dozen lines of text, which is far more than enough.
    */
    throw not_yet_supported( "retrieving string values longer than 1023 from a record" ) ;
  }
  throw windows_api_error( "MsiRecordGetStringW", x ) ;
}

size_t Record::n_fields() const
{
  unsigned int x = MsiRecordGetFieldCount( _handle ) ;
  if ( x == 0xFFFFFFFF )
  {
    throw windows_api_error( "MsiRecordGetFieldCount", x, "invalid handle" ) ;
  }
  return x ;
}