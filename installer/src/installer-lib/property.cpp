/**
 * \file property.cpp Implementation of Property class etc.
 */

#include "property.h"
#include "session.h"
#include "msiquery.h"
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
   * The screwy logic below arises from how the API works.
   * MsiGetProperty insists on copying into your buffer, but you don't know how long that buffer needs to be in advance.
   * The first call gets the size, but also the actual value if it's short enough.
   * A second call, if necessary, gets the actual value after allocat
   */
  // We only need a modest fixed-size buffer here, because we handle arbitrary-length property values in a second step.
  // It has 'auto' allocation, so we don't want it too large.
  TCHAR buffer1[ 64 ] = { L'\0' } ;
  DWORD length = sizeof( buffer1 ) / sizeof( TCHAR ) ;
  switch ( MsiGetProperty( handle, name.c_str(), buffer1, & length ) ) 
  {
  case ERROR_SUCCESS:
    // This call might succeed, which means the return value was short enough to fit into the buffer.
    return std::wstring( buffer1, length ) ;
  case ERROR_MORE_DATA:
    // Do nothing yet.
    break ;
  default:
    throw std::runtime_error( "Error getting property" ) ;
  }
  // Assert we received ERROR_MORE_DATA
  // unique_ptr handles deallocation transparently
  std::unique_ptr< TCHAR[] > buffer2( new TCHAR[ length ] );
  switch ( MsiGetProperty( handle, name.c_str(), buffer2.get(), & length ) )
  {
  case ERROR_SUCCESS:
    return std::wstring( buffer2.get(), length ) ;
  default:
    throw std::runtime_error( "Error getting property" ) ;
  }
}

/**
 * \par Implementation
 * The center of the implementation is the <a href="http://msdn.microsoft.com/en-us/library/windows/desktop/aa370391%28v=vs.85%29.aspx">MsiSetProperty function</a>.
 */
void Property::operator=( const std::wstring & value )
{
  if ( MsiSetProperty( handle, name.c_str(), value.c_str() ) != ERROR_SUCCESS )
  {
    throw std::runtime_error( "Error setting property" ) ;
  }
}
