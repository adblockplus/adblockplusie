/**
 * \file session.h The "install session" is the context for all custom installation behavior.
 */

#ifndef SESSION_H
#define SESSION_H

#include "property.h"
#include "record.h"

#include <string>
#include "windows.h"
#include "msi.h"

//-----------------------------------------------------------------------------------------
// Session
//-----------------------------------------------------------------------------------------
/**
 * A Windows Installer session
 *
 * Always instantiate an instance of this class at the start of each custom action.
 * Copy and assignment are disabled, so session objects are always passed by reference. 
 *
 * This class is the base for both immediate and deferred custom actions.
 * Immediate custom actions always have an installer database associated with them; deferred actions never do.
 * Both immediate and deferred actions may be executed synchronously or asynchronously; this class is silent about any difference.
 *
 * \par Notes
 *   This class is patterned after WcaInitialize/WcaFinalize of the WiX custom action library.
 *   There are two things that class does that this one does not.
 *   - Extract the file version information from the DLL using GetModuleFileName* and GetFileVersionInfo* system calls.
 *   - Set a "global atom" (a Windows system object) to store the logging state, later to be accessed by deferred actions.
 */
class Session {
public:
  /**
   * Destructor.
   */
  ~Session() ;

  /**
   * Write a message to the installation log.
   */
  void log( std::wstring message ) ;

protected:
  /**
   * Ordinary constructor is protected; public constructors are all in subclasses.
   * The MSI system uses a single handle type for all types of sessions. This handle is here in this base class.
   *
   * \param[in] handle
   *    Handle for the Windows Installer session provided as an argument to a custom action.
   * \param[in] name
   *    The name of the custom action, used for logging.
   */
  Session( MSIHANDLE handle, std::wstring name ) ;

protected:
  /**
   * Handle for the Windows Installer session.
   */
  MSIHANDLE handle ;

private:
  /**
   * Prefix for log messages. Contains the name of the custom action.
   */
  std::wstring log_prefix ;

  /**
   * Private copy constructor is declared but not defined.
   */
  Session( const Session & ) ;

  /**
   * Private assignment operator is declared but not defined.
   */
  Session & operator=( const Session & ) ;

  /**
   * The Property class requires access to the session handle.
   */
  friend Property::Property( Session & session, std::wstring name ) ;
};

//-----------------------------------------------------------------------------------------
// Immediate_Session
//-----------------------------------------------------------------------------------------
/**
 * Session for immediate custom actions.
 *
 * Access to the installer database is by passing a reference to a class of this subtype to a Database constructor.
 */
class Immediate_Session : public Session
{
public:
  /**
   * Ordinary constructor.
   *
   * \param[in] handle
   *    Handle for the Windows Installer session provided as an argument to a custom action.
   * \param[in] name
   *    The name of the custom action, used for logging.
   */
  Immediate_Session( MSIHANDLE handle, std::wstring name ) ;

private:
  /*
   * Allow helper function for Installation_Database constructor to have access to the handle.
   */
  friend MSIHANDLE get_active_database( Immediate_Session & session ) ;
};


//-----------------------------------------------------------------------------------------
// Deferred_Session
//-----------------------------------------------------------------------------------------
/**
 * Session for deferred custom actions.
 *
 * There's much less context information easily available from a deferred custom action.
 *
 * /sa MDSN "Deferred Execution Custom Actions"
 *     http://msdn.microsoft.com/en-us/library/windows/desktop/aa368268%28v=vs.85%29.aspx
 *     for general information.
 *
 * /sa MSDN "Obtaining Context Information for Deferred Execution Custom Actions"
 *     http://msdn.microsoft.com/en-us/library/windows/desktop/aa370543%28v=vs.85%29.aspx
 *     lists the API calls available.
 */
class Deferred_Session : public Session
{
public:
  /**
   * Ordinary constructor.
   *
   * \param[in] handle
   *    Handle for the Windows Installer session provided as an argument to a custom action.
   * \param[in] name
   *    The name of the custom action, used for logging.
   */
  Deferred_Session( MSIHANDLE handle, std::wstring name ) ;
};


//-----------------------------------------------------------------------------------------
// Commit_Session
//-----------------------------------------------------------------------------------------
/**
 * The session for a commit custom action. NOT IMPLEMENTED.
 *
 * \sa MSDN "Commit Custom Actions" http://msdn.microsoft.com/en-us/library/windows/desktop/aa367991%28v=vs.85%29.aspx
 */
class Commit_Session
{
};

//-----------------------------------------------------------------------------------------
// Rollback_Session
//-----------------------------------------------------------------------------------------
/**
 * The session for a rollback custom action. NOT IMPLEMENTED.
 *
 * \sa MSDN "Rollback Custom Actions" http://msdn.microsoft.com/en-us/library/windows/desktop/aa371369%28v=vs.85%29.aspx
 */
class Rollback_Session
{
};

#endif
