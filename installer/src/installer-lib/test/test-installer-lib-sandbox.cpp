/**
 * \file test-installer-lib-sandbox.cpp
 */

#include <sstream>

#include "session.h"
#include "property.h"
#include "database.h"
#include "process.h"
#include "interaction.h"

//-------------------------------------------------------
// Log all window handles
//-------------------------------------------------------
class log_single_window_handle
{
  Immediate_Session & session ;

public:
  log_single_window_handle( Immediate_Session & session )
    : session( session )
  {
  }

  bool operator()( HWND window )
  {
    std::stringstream s ;
    s << "Window handle 0x" << std::hex << window ;
    session.log( s.str() ) ;
    return true ;
  }
} ;

void log_all_window_handles( Immediate_Session & session )
{
  session.log( "log_all_window_handles" ) ;
  log_single_window_handle lp( session ) ;
  iterate_top_windows< log_single_window_handle > iter( lp ) ;
  bool x = iter() ;
}

//-------------------------------------------------------
// Log all window handles
//-------------------------------------------------------
class log_single_window_handle_only_if_IE
{
  Immediate_Session & session ;

  std::set< DWORD > PID_set ;

public:
  log_single_window_handle_only_if_IE( Immediate_Session & session, std::set< DWORD > PID_set )
    : session( session ), PID_set( PID_set )
  {
  }

  bool operator()( HWND window )
  {
    DWORD pid = creator_process( window ) ;
    if ( PID_set.find( pid ) != PID_set.end() )
    {
      std::stringstream s ;
      s << "Window handle 0x" << std::hex << window ;
      session.log( s.str() ) ;
    }
    return true ;
  }
} ;

void log_IE_window_handles( Immediate_Session & session )
{
  session.log( "log_IE_window_handles" ) ;
  const wchar_t * IE_names[] = { L"IExplore.exe", L"AdblockPlusEngine.exe" } ;
  Process_Closer iec( IE_names, 2 ) ;
  log_single_window_handle_only_if_IE lp( session, iec.pid_set ) ;
  iterate_top_windows< log_single_window_handle_only_if_IE > iter( lp ) ;
  bool x = iter() ;
}

//-------------------------------------------------------
// sandbox
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
    log_IE_window_handles( session ) ;
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

