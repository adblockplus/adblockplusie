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
  MSIHANDLE h = MsiGetActiveDatabase( session.handle ) ;
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
}