/**
 * \file session.cpp Implementation of Session class.
 */

#include "session.h"
#include "property.h"
#include "msiquery.h"

//-----------------------------------------------------------------------------------------
// Session
//-----------------------------------------------------------------------------------------
Session::Session( MSIHANDLE handle, std::wstring name )
  : handle( handle ), 
  log_prefix( name + L": " )
{
  log( L"Entering custom action" ) ;
}

/**
 * \par Implementation Notes
 *    The session handle doesn't need to be closed.
 *    It's provided as an argument to the custom action at the outset, and we do not manage its life cycle.
 */
Session::~Session()
{
  log( L"Exiting custom action" ) ;
}

/**
 * \par Implementation Notes
 *    To write to the installation log, we use a call to MsiProcessMessage with message type INSTALLMESSAGE_INFO.
 *    The text to be written needs to go in a "record" (yes, a database record) that acts as an argument vector.
 *    For the message type we're using, we need only a record with a single field.
 *
 * \sa MSDN "MsiProcessMessage function"
 *    http://msdn.microsoft.com/en-us/library/windows/desktop/aa370354%28v=vs.85%29.aspx
 *    MsiProcessMessage is mostly for user interaction with message boxes, but it's also the access to the installation log.
 */
void Session::log( std::wstring message )
{
  Record r = Record( 1 );
  r.assign_string( 0, log_prefix + message );
  int e = MsiProcessMessage( handle, INSTALLMESSAGE_INFO, r.handle() ) ;
  if ( e != IDOK )
  {
    throw std::runtime_error( "Did not succeed writing to log." ) ;
  }
}

Immediate_Session::Immediate_Session( MSIHANDLE handle, std::wstring name )
  : Session( handle, name )
{
}
