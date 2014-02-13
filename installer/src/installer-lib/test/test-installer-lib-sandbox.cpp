/**
 * \file close_application.cpp
 */

#include "session.h"
#include "property.h"
#include "database.h"
#include "process.h"
#include "interaction.h"

//-------------------------------------------------------
// abp_close_applications
//-------------------------------------------------------
/**
 * Exposed DLL entry point for custom action. 
 * The function signature matches the calling convention used by Windows Installer.

 * \param[in] session_handle
 *     Windows installer session handle
 *
 * \return 
 *    An integer interpreted as a custom action return value.
 *   
 * \sa
 *   - MSDN [Custom Action Return Values](http://msdn.microsoft.com/en-us/library/aa368072%28v=vs.85%29.aspx)
 */
extern "C" UINT __stdcall 
sandbox( MSIHANDLE session_handle )
{
  Immediate_Session session( session_handle, "sandbox" ) ;

  try
  {
    session.log( "Log point A" ) ;
    session.write_message( Installer_Message_Box( "Test Box 1" ) ) ;
    session.log( L"Log point B" ) ;
    session.write_message( Installer_Message_Box( L"Test Box 2" ) ) ;
    session.log_noexcept( "Log point C" ) ;
  }
  catch( std::exception & e )
  {
    session.log_noexcept( "terminated by exception: " + std::string( e.what() ) ) ;
    return ERROR_INSTALL_FAILURE ;
  }
  catch( ... )
  {
    session.log_noexcept( "Caught an exception" ) ;
    return ERROR_INSTALL_FAILURE ;
  }

  return ERROR_INSTALL_FAILURE ;
}
