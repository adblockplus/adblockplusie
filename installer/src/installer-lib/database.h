/**
* \file database.h MSI database
*/

#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <memory>

#include <Windows.h>
#include <Msi.h>
#include <MsiQuery.h>

#include "installer-lib.h"
#include "handle.h"
#include "session.h"

// Forward declarations
class View ;

//-------------------------------------------------------
// Database
//-------------------------------------------------------
/**
* A Windows Installer database as contained in an MSI file.
*
* The API for MSI databases is shared between installation and non-installation contexts.
* Roughly speaking, outside an installation the database supports both read and write,
*   but inside an installation the database is read-only.
* The life cycle functions are not shared, in addition.
* Outside of these restrictions, however, the API is mostly common.
* This class is the base class for the common API.
* Subclasses provide public constructors and provide access to API calls not in common.
*/
class Database
{
protected:
  typedef Handle< MSIHANDLE, DisallowNull, GenericMsiDestruction > HandleType ;

  /**
  * Protected constructor. 
  *
  * An MSI database handle is an overloaded type, used both for installation databases and one opened outside an installation.
  * These database handles, while both databases, have different capabilities and are thus defined in subclasses.
  * Each subclass has the responsibility for obtaining a database handle appropriate to its circumstance.
  * 
  * \sa MSDN "Obtaining a Database Handle"
  *    http://msdn.microsoft.com/en-us/library/windows/desktop/aa370541(v=vs.85).aspx
  */
  Database( MSIHANDLE handle )
    : handle( handle )
  {}

  /**
  */
  HandleType handle ;

private:
  /**
  * Private copy constructor is declared but not defined.
  */
  Database( const Database & ) ;

  /**
  * Private assignment operator is declared but not defined.
  */
  Database & operator=( const Database & ) ;

  /**
  * Open a new view for this database.
  *
  * \param query
  *    An SQL query using the restricted MSI syntax
  *
  * \sa
  *   - MSDN [MsiDatabaseOpenView function](http://msdn.microsoft.com/en-us/library/aa370082%28v=vs.85%29.aspx)
  */
  MsiHandle OpenView( const wchar_t * query ) ;

  friend class View ;
} ;

/**
* A Windows Installer database in an installation context.
*/
class InstallationDatabase : public Database
{
public:
  /**
  * The constructor of a database in an installation context has no arguments because the database is a part of that context.
  */
  InstallationDatabase( ImmediateSession & session ) ;
} ;

//-------------------------------------------------------
//
//-------------------------------------------------------
/**
* A Windows Installer database outside of an installation context, opened as a file from the file system.
*
* This is a read-only version of a file-system database.
* Refactor the class to obtain other open-modes.
*
*/
class FileSystemDatabase : public Database
{
  /**
  * Open function is separate to enable initializing base class before constructor body.
  *
  * \sa
  *   - MSDN [MsiOpenDatabase function](http://msdn.microsoft.com/en-us/library/aa370338%28v=vs.85%29.aspx)
  */
  MsiHandle HandleFromPathname( const wchar_t * pathname )
  {
    MSIHANDLE handle ;
    UINT x = MsiOpenDatabaseW( pathname, MSIDBOPEN_READONLY, & handle ) ;
    if ( x != ERROR_SUCCESS )
    {
      throw WindowsApiError( "MsiOpenDatabaseW", x, "MSI database on file system" ) ;
    }
    return MsiHandle( handle ) ;
  }

public:
  FileSystemDatabase( const wchar_t * pathname )
    : Database( HandleFromPathname( pathname ) )
  {}
} ;

//-------------------------------------------------------
// View
//-------------------------------------------------------
/*
* The MSI database is accessible through a cut-down version of SQL.
* There's no distinction between view and query in this dialect.
*
* \sa
*   - MSDN [Working with Queries](http://msdn.microsoft.com/en-us/library/aa372879%28v=vs.85%29.aspx)
*/
class View
{
  typedef Handle< MSIHANDLE, DisallowNull, GenericMsiDestruction > HandleType ;

  /**
  * Handle for the MSI view object
  */
  HandleType handle;

public:
  /**
  * Ordinary constructor
  */
  View( Database & db, wchar_t * query )
    : handle( db.OpenView( query ) )
  {}

  /**
  * Execute the query and return the first record in its results.
  * 
  * \param arguments
  *    List of parameters to supply as the query arguments (question marks).
  */
  Record First( Record & arguments ) ;

  /**
  * Execute the query and return the first record in its results.
  * 
  * With no arguments, this version of the function may only be used with a query that takes no arguments.
  */
  Record First() ;

  /**
  * Retrieve the next record.
  */
  Record Next() ;

  /**
  * End marker
  */
  inline Record End()
  {
    return Record( Record::NullType() ) ;
  }
} ;


#endif
