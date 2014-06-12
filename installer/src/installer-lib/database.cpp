/**
* \file database.h MSI database
*/

#include "database.h"
#include "msiquery.h"

//-----------------------------------------------------------------------------------------
// Database
//-----------------------------------------------------------------------------------------
msi_handle Database::open_view( const wchar_t * query )
{
  MSIHANDLE view_handle ;
  UINT x = MsiDatabaseOpenView( handle, query, & view_handle ) ;
  if ( x == ERROR_BAD_QUERY_SYNTAX )
  {
    throw windows_api_error( "MsiDatabaseOpenView", "ERROR_BAD_QUERY_SYNTAX" ) ;
  }
  else if ( x == ERROR_INVALID_HANDLE )
  {
    throw windows_api_error( "MsiDatabaseOpenView", "ERROR_INVALID_HANDLE" ) ;
  }
  return msi_handle( view_handle ) ;
}

//-----------------------------------------------------------------------------------------
// Installation_Database
//-----------------------------------------------------------------------------------------

/**
* Helper function for Installation_Database constructor.
*
* \par Resource Allocator
*    Return value of this function, a handle, must be released in order to avoid a resource leak.
*    Passing it as an argument to the Database constructor is adequate.
*/
msi_handle get_active_database( Immediate_Session & session )
{
  MSIHANDLE h( MsiGetActiveDatabase( session.handle ) ) ;
  if ( h == 0 )
  {
    throw windows_api_error( "MsiGetActiveDatabase", 0 ) ;
  }
  return msi_handle( h ) ;
}

/**
* \par Implementation Notes
*    The only thing this constructor needs to do is to initialize the base class.
*/
Installation_Database::Installation_Database( Immediate_Session & session )
  : Database( get_active_database( session ) )
{
  // empty body
} ;

//-----------------------------------------------------------------------------------------
// View
//-----------------------------------------------------------------------------------------
/**
* Implementation function for View::first().
*/
void view_first_body( UINT x )
{
  if ( x != ERROR_SUCCESS )
  {
    throw windows_api_error( "MsiViewExecute", x ) ;
  }
}

Record View::first()
{
  view_first_body( MsiViewExecute( _handle, 0 ) ) ;
  return next() ;
}

Record View::first( Record & arguments )
{
  view_first_body( MsiViewExecute( _handle, arguments._handle ) ) ;
  return next() ;
}

Record View::next()
{
  MSIHANDLE h ;
  UINT x = MsiViewFetch( _handle, & h ) ;
  if ( x == ERROR_NO_MORE_ITEMS )
  {
    return Record( Record::null_t() ) ;
  }
  else if ( x == ERROR_SUCCESS )
  {
    return Record( msi_handle( h ) ) ;
  }
  throw windows_api_error( "MsiViewFetch", x ) ;
}

