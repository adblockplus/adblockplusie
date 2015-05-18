/**
* \file record.cpp Implementation of Record class.
*/

#include "installer-lib.h"
#include "record.h"
#include "msiquery.h"

//-----------------------------------------------------------------------------------------
// Record
//-----------------------------------------------------------------------------------------
Record::Record( unsigned int nFields )
{
  handle = MsiCreateRecord( nFields ) ;
  if ( ! handle )
  {
    throw WindowsApiError( "MsiCreateRecord", 0 ) ;
  }
}

Record::~Record()
{
  if ( handle != 0 )
  {
    MsiCloseHandle( handle ) ;
  }
}

void Record::OnlyNonNull()
{
  if ( handle == 0 )
  {
    throw std::runtime_error( "Operation only permitted for non-null objects" ) ;
  }
}

void Record::AssignString( unsigned int fieldIndex, const char *value )
{
  OnlyNonNull() ;
  MsiRecordSetStringA( handle, fieldIndex, value ) ;
}

void Record::AssignString( unsigned int fieldIndex, const wchar_t *value )
{
  OnlyNonNull() ;
  MsiRecordSetStringW( handle, fieldIndex, value ) ;
}

/**
* \par Implementation
*    - MSDN [MsiRecordGetString](http://msdn.microsoft.com/en-us/library/aa370368%28v=vs.85%29.aspx)
*/
std::wstring Record::ValueString( unsigned int fieldIndex )
{
  static wchar_t initialBuffer[ 1024 ] = L"" ;
  DWORD length = 1023 ; // one less than the buffer length to hold a terminating null character
  UINT x = MsiRecordGetStringW( handle, fieldIndex, initialBuffer, & length ) ;
  if ( x == ERROR_SUCCESS )
  {
    return std::wstring( initialBuffer ) ;
  }
  if ( x == ERROR_MORE_DATA )
  {
    // Future: handle longer strings.
    /*
    * The present custom action only uses this function for strings that appear in dialog boxes.
    * A thousand characters is about a dozen lines of text, which is far more than enough.
    */
    throw NotYetSupported( "retrieving string values longer than 1023 from a record" ) ;
  }
  throw WindowsApiError( "MsiRecordGetStringW", x ) ;
}

size_t Record::NumberOfFields() const
{
  unsigned int x = MsiRecordGetFieldCount( handle ) ;
  if ( x == 0xFFFFFFFF )
  {
    throw WindowsApiError( "MsiRecordGetFieldCount", x, "invalid handle" ) ;
  }
  return x ;
}