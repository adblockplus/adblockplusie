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
// Message
//-----------------------------------------------------------------------------------------
/**
* Wrapper class for arguments to MsiProcessMessage.
*
* The "user interface" for custom actions includes both interactive dialog boxes as well as the installation log.
* All of them use the same call, MsiProcessMessage.
* This class encapsulates its arguments.
*
* \sa 
*    * MSDN [MsiProcessMessage function](http://msdn.microsoft.com/en-us/library/windows/desktop/aa370354%28v=vs.85%29.aspx)
*    * MSDN [Sending Messages to Windows Installer Using MsiProcessMessage](http://msdn.microsoft.com/en-us/library/windows/desktop/aa371614%28v=vs.85%29.aspx)
*/
class Message
{
protected:
  /**
  * The flags used by MsiProcessMessage as the box type.
  */
  INSTALLMESSAGE message_type ;

  /**
  * The record argument to MsiProcessMessage
  */
  Record r ;

  Message( std::string message, INSTALLMESSAGE message_type ) ;

  Message( std::wstring message, INSTALLMESSAGE message_type ) ;

  /**
  * This class is a helper for Session, mustering all the arguments for MsiProcessMessage except for the session handle.
  */
  friend Session ;
} ;

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
  * Write a message to the installation log, regular string version.
  */
  void log( std::string message ) ;

  /**
  * Write a message to the installation log, wide string version.
  */
  void log( std::wstring message ) ;

  /**
  * Write a message to the installation log without raising an exception.
  *
  * Use this function only in the three circumstances when an exception cannot be caught by an entry point catch-all.
  * First and second, there's the constructor and destructor of a Session instance.
  * These log entry into and exit from the custom action, respectively.
  * Third, there's the top level catch-blocks of the CA.
  * The scope of the Session object cannot be within the try-block in order for it to be in scope for the catch-block.
  * The session must be in scope in the catch-block to allow logging error messages.
  * In all other cases, use the exception mechanism.
  */
  void log_noexcept( std::string message ) ;

  /**
  * Write to a MessageBox dialog.
  */
  int write_message( Message & ) ;

protected:
  /**
  * Ordinary constructor is protected; public constructors are all in subclasses.
  * The MSI system uses a single handle type for all types of sessions. This handle is here in this base class.
  *
  * \param[in] handle
  *    Handle for the Windows Installer session provided as an argument to a custom action.
  * \param[in] name
  *    The name of the custom action, used for logging.
  *    This string must be ASCII characters only, so that its wide-character version displays identically.
  */
  Session( MSIHANDLE handle, std::string name ) ;

  /**
  * Handle for the Windows Installer session.
  *
  * The life cycle of the session handle is not the responsibility of the base class.
  * In an interactive session, the handle is provided as an argument to the custom action entry point, and we do not manage its life cycle.
  * In an offline session, the handle is created in the (subclass) constructor.
  */
  MSIHANDLE handle ;

private:
  /**
  * Prefix for log messages, regular string. Contains the name of the custom action.
  */
  std::string log_prefix ;

  /**
  * Prefix for log messages, wide string. Contains the name of the custom action.
  */
  std::wstring log_prefix_w ;

  /**
  * Private copy constructor is declared but not defined.
  *
  * C++11: declare with <b>= delete</b>.
  */
  Session( const Session & ) ;

  /**
  * Write a message with MsiProcessMessage and throw no exceptions.
  *
  * This is declared private because there are very few cases in which no-exception behavior is required.
  *
  * C++11: declare with **noexcept**.
  */
  int write_message_noexcept( Message & m ) ;

  /**
  * Private assignment operator is declared but not defined.
  *
  * C++11: declare with <b>= delete</b>.
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
  * 
  * **noexcept** declaration to be added for C++11.
  */
  Immediate_Session( MSIHANDLE handle, std::string name ) ;

private:
  /*
  * Allow helper function for Installation_Database constructor to have access to the handle.
  */
  friend msi_handle get_active_database( Immediate_Session & session ) ;
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
