/**
 * \file database.h MSI database
 */

#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include "windows.h"
#include "msi.h"

#include "session.h"

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
  /**
   * Protected constructor. Life cycle depends strongly on context.
   */
  Database( MSIHANDLE handle );

  /**
   * Destructor.
   */
  ~Database();

protected:
  /**
   */
  MSIHANDLE handle;

private:
  /**
   * Private copy constructor is declared but not defined.
   */
  Database( const Database & );

  /**
   * Private assignment operator is declared but not defined.
   */
  Database & operator=( const Database & ) ;
};

/**
 * A Windows Installer database in an installation context.
 */
class Installation_Database : public Database
{
public:
  /**
   * The constructor of a database in an installation context has no arguments because the database is a part of that context.
   */
  Installation_Database( Immediate_Session & session );
};

/**
 * A Windows Installer database in a non-installation context.
 */
class Non_Installation_Database : public Database
{
};

#endif
