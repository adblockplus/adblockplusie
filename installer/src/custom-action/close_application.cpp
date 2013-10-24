/**
 * \file close_application.cpp
 */

#include "session.h"
#include "property.h"
#include "database.h"

/**
 * Exposed DLL entry point for custom action. 
 * The function signature matches the calling convention used by Windows Installer.
 *
 * \param[in] session_handle
 *     Windows installer session handle
 */
extern "C" UINT __stdcall 
abp_close_applications( MSIHANDLE session_handle )
{
  // Always supply an externally-exposed function with a catch-all block
  try {
    Immediate_Session session( session_handle, L"abp_close_applications" ) ;
    session.log( L"Have session object" ) ;

    Installation_Database db( session ) ;
    session.log( L"Have database object" ) ;

    session.log( L"Still with new Property operator+ implementations!" ) ;
    session.log( L"VersionMsi = " + Property( session, L"VersionMsi" ) ) ;

    Property tv( session, L"TESTVARIABLE" ) ;
    session.log( L"TESTVARIABLE = " + tv ) ;
    session.log( L"Setting TESTVARIABLE to 'testvalue'" ) ;
    tv = L"testvalue" ;
    session.log( L"TESTVARIABLE = " + tv ) ;
  }
  catch( ... )
  {
    return ERROR_INSTALL_FAILURE ;
  }

  /*
   * While we're working on infrastructure (and not the CA itself), fail the action.
   */
  return ERROR_INSTALL_FAILURE ;
}
