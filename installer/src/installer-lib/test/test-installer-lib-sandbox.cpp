/**
 * \file test-installer-lib-sandbox.cpp
 *
 * Automatic testing of many of the units within the custom action is infeasible.
 * In one case, they rely on the execution environment within an installation session.
 * In another, they rely on the operation system environment as a whole.
 * In these cases, it's easier to verify behavior manually.
 *
 * This file contains a custom action function sandbox() as well as a number of test functions.
 * At any given time, not all of the test functions need to be referenced within the body of custom action.
 */

#include <sstream>
#include <functional>

#include "session.h"
#include "property.h"
#include "database.h"
#include "process.h"
#include "interaction.h"

//-------------------------------------------------------
// log_all_window_handles
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
  enumerate_windows( lp ) ;
}

//-------------------------------------------------------
// log_IE_window_handles
//-------------------------------------------------------
class log_single_window_handle_only_if_IE
{
  Immediate_Session & session ;

  Process_Closer & pc ;

public:
  log_single_window_handle_only_if_IE( Immediate_Session & session, Process_Closer & pc )
    : session( session ), pc( pc )
  {
  }

  bool operator()( HWND window )
  {
    DWORD pid = creator_process( window ) ;
    if ( pc.contains( pid ) )
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
  const wchar_t * ABP_names[] = { L"AdblockPlus32.dll", L"AdblockPlus64.dll" } ;
  Snapshot snapshot ;
  Process_Closer iec( snapshot, IE_names,  ABP_names) ;
  log_single_window_handle_only_if_IE lp( session, iec ) ;
  enumerate_windows( lp ) ;
}

//-------------------------------------------------------
// log_only_window_handle_in_closer
//-------------------------------------------------------
void log_only_window_handle_in_closer( Immediate_Session & session )
{
  session.log( "log_only_window_handle_in_closer" ) ;
  const wchar_t * IE_names[] = { L"IExplore.exe", L"AdblockPlusEngine.exe" } ;
  const wchar_t * ABP_names[] = { L"AdblockPlus32.dll", L"AdblockPlus64.dll" } ;
  Snapshot snapshot ;
  Process_Closer iec( snapshot, IE_names, ABP_names) ;
  iec.iterate_our_windows( log_single_window_handle( session ) ) ;
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
    log_only_window_handle_in_closer( session ) ;
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

  return ERROR_SUCCESS ;
}

