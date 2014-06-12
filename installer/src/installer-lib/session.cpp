/**
* \file session.cpp Implementation of Session class.
*/

#include "installer-lib.h"
#include "session.h"
#include "property.h"
#include "msiquery.h"

//-----------------------------------------------------------------------------------------
// Message
//-----------------------------------------------------------------------------------------
Message::Message( std::string message, INSTALLMESSAGE message_type )
  : r( 1 ), message_type( message_type )
{
  r.assign_string( 0, message ) ;
}

Message::Message( std::wstring message, INSTALLMESSAGE message_type )
  : r( 1 ), message_type( message_type )
{
  r.assign_string( 0, message ) ;
}

//-----------------------------------------------------------------------------------------
// Session
//-----------------------------------------------------------------------------------------
Session::Session( MSIHANDLE handle, std::string name )
  : handle( handle ), 
  log_prefix( name + ": " )
{
  log_prefix_w.assign( name.begin(), name.end() ) ;
  log_prefix_w += L": " ;
  log_noexcept( "Entering custom action" ) ;
}

Session::~Session()
{
  log_noexcept( "Exiting custom action" ) ;
}

/**
* A message for the installation log.
*
* Writing to the installation log uses MsiProcessMessage just like interactive dialog boxes do.
*
* This class is not exposed outside this compilation unit because everything it can do is already exposed by the log functions.
*/
struct Log_Message
  : public Message
{
  Log_Message ( std::wstring message )
    : Message( message, INSTALLMESSAGE_INFO )
  {}

  Log_Message ( std::string message )
    : Message( message, INSTALLMESSAGE_INFO )
  {}
} ;

void Session::log( std::string message )
{
  write_message( Log_Message( log_prefix + message ) ) ;
}

void Session::log( std::wstring message )
{
  write_message( Log_Message( log_prefix_w + message ) ) ;
}

void Session::log_noexcept( std::string message )
{
  write_message_noexcept( Log_Message( log_prefix + message ) ) ;
}

int Session::write_message( Message & m )
{
  int x = write_message_noexcept( m ) ;
  if ( x == -1 )
  {
    throw windows_api_error( "MsiProcessMessage", x, "attempt to write to log file" ) ;
  }
  return x ;
}

int Session::write_message_noexcept( Message & m )
{
  return MsiProcessMessage( handle, m.message_type, m.r.handle() ) ;
}

//-----------------------------------------------------------------------------------------
// Immediate_Session
//-----------------------------------------------------------------------------------------
Immediate_Session::Immediate_Session( MSIHANDLE handle, std::string name )
  : Session( handle, name )
{}
