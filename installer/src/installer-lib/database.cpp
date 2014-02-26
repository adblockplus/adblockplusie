/**
 * \file database.h MSI database
 */

#include "database.h"
#include "msiquery.h"

//-----------------------------------------------------------------------------------------
// Database
//-----------------------------------------------------------------------------------------
/**
 * \par Implementation Notes
 *    An MSI database handle is an overloaded type, used both for installation databases and one opened outside an installation.
 *    Hence this base constructor initializes with that handle.
 * 
 * \sa MSDN "Obtaining a Database Handle"
 *    http://msdn.microsoft.com/en-us/library/windows/desktop/aa370541(v=vs.85).aspx
 */
Database::Database( MSIHANDLE handle )
  : handle( handle )
{
}

Database::~Database()
{
  MsiCloseHandle( handle ) ;
}

msi_handle Database::open_view( const wchar_t * query )
{
  MSIHANDLE view_handle ;
  UINT x = MsiDatabaseOpenView( handle, query, & view_handle ) ;
  if ( x == ERROR_BAD_QUERY_SYNTAX )
  {
    throw std::runtime_error( "Bad MSI query syntax" ) ;
  }
  else if ( x == ERROR_INVALID_HANDLE )
  {
    throw std::runtime_error( "Invalid handle" ) ;
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
MSIHANDLE get_active_database( Immediate_Session & session )
{
  MSIHANDLE h( MsiGetActiveDatabase( session.handle ) ) ;
  if ( h == 0 )
  {
    throw std::runtime_error( "Failed to retrieve active databases" ) ;
  }
  return h ;
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
    throw std::runtime_error( "MsiViewExecute call failed" ) ;
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
  throw std::runtime_error( "Error fetch record from view" ) ;
}

