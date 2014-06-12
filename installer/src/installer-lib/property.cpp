/**
* \file property.cpp Implementation of Property class etc.
*/

#include "installer-lib.h"
#include "property.h"
#include "session.h"
#include <msiquery.h>
#include <memory>

//-----------------------------------------------------------------------------------------
// Property
//-----------------------------------------------------------------------------------------
Property::Property( Session & session, std::wstring name )
  // VSE 2012 shows an IntelliSense error here. Ignore it. The compiler properly sees the 'friend' declaration.
  : handle( session.handle ), name( name )
{}

/**
* \par Implementation
* The center of the implementation is the <a href="http://msdn.microsoft.com/en-us/library/windows/desktop/aa370134%28v=vs.85%29.aspx">MsiGetProperty function</a>.
*/
Property::operator std::wstring() const
{
  /*
  * The first call gets the size, but also the actual value if it's short enough.
  * A second call, if necessary, allocates a sufficiently-long buffer and then gets the full value.
  * We use only a modest fixed-size buffer for the first step, because we handle arbitrary-length property values in a second step.
  */
  // This buffer allocates on the stack, so we don't want it too large; 64 characters is enough for most properties anyway.
  WCHAR buffer1[ 64 ] = { L'\0' } ;
  DWORD length = sizeof( buffer1 ) / sizeof( WCHAR ) ;
  UINT x = MsiGetPropertyW( handle, name.c_str(), buffer1, & length ) ;
  switch ( x )
  {
  case ERROR_SUCCESS:
    // This call might succeed, which means the return value was short enough to fit into the buffer.
    return std::wstring( buffer1, length ) ;
  case ERROR_MORE_DATA:
    // Do nothing yet.
    break ;
  default:
    throw windows_api_error( "MsiGetPropertyW", x, "fixed buffer" ) ;
  }
  // Assert we received ERROR_MORE_DATA
  // unique_ptr handles deallocation transparently
  std::unique_ptr< WCHAR[] > buffer2( new WCHAR[ length ] ) ;
  x = MsiGetPropertyW( handle, name.c_str(), buffer2.get(), & length ) ;
  switch ( x )
  {
  case ERROR_SUCCESS:
    return std::wstring( buffer2.get(), length ) ;
  default:
    throw windows_api_error( "MsiGetPropertyW", x, "allocated buffer" ) ;
  }
}

/**
* \par Implementation
* The center of the implementation is the <a href="http://msdn.microsoft.com/en-us/library/windows/desktop/aa370391%28v=vs.85%29.aspx">MsiSetProperty function</a>.
*/
void Property::operator=( const std::wstring & value )
{
  UINT x = MsiSetPropertyW( handle, name.c_str(), value.c_str() ) ;
  if ( x != ERROR_SUCCESS )
  {
    throw windows_api_error( "MsiSetPropertyW", x ) ;
  }
}
