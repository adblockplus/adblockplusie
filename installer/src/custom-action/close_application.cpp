/**
 * \file close_application.cpp
 */

#include "session.h"
#include "property.h"
#include "database.h"
#include "process.h"
#include "interaction.h"

#include <algorithm>

#include <TlHelp32.h>

//-------------------------------------------------------
// IE_List
//-------------------------------------------------------
/**
 * Filter by the fixed name "IExplore.exe", case-insensitive.
 */
struct IE_by_name
  : std::unary_function< PROCESSENTRY32W, bool >
{
  bool operator()( const PROCESSENTRY32W & process ) 
  {
    static const wchar_t IE_name[] = L"IExplore.exe" ;
    return 0 == wcsncmpi( process.szExeFile, IE_name, sizeof( IE_name ) / sizeof( wchar_t ) ) ;
  } ;
} ;

/**
 * A list of the process ID's of all IE processes running on the system.
 *
 * The list is derived from a process snapshot made at construction.
 */
class IE_List
  : public Process_List< DWORD, IE_by_name, copy_PID >
{
public:
  bool is_running() { return v.size() > 0 ; } ;
} ;

//-------------------------------------------------------
// abp_close_applications
//-------------------------------------------------------
/**
 * Exposed DLL entry point for custom action. 
 * The function signature matches the calling convention used by Windows Installer.
 *
 * This function supports four policy stances with respect to a running IE process.
 *
 * - Allow reboot. 
 *   IE is running, so what? I'm willing to reboot after installation.
 * - Avoid reboot passively. 
 *   I don't want to affect any other running processes, but I don't want to reboot. I'll abort the installation.
 * - Avoid reboot actively. 
 *   I want to shut down IE in order to avoid a reboot.
 *   I'll do it manually when the time is right.
 * - Avoid reboot automatically. 
 *   I want to shut down IE automatically in order to avoid a reboot. 
 *
 * In a silent installation, the default stance is "allow reboot", which is to say, to act like most installers.
 * In an interactive installation, the stance is gathered from the user through dialog boxes.
 * If the MSI property AVOIDREBOOT is set to one of the values NO, PASSIVE, ACTIVE, or AUTOMATIC, the policy is set accordingly.
 * In a silent installation, this is the only way getting a stance other than the default.
 * In an interactive installation, AVOIDREBOOT skips the dialogs.
 *
 * \param[in] session_handle
 *     Windows installer session handle
 *
 * \return 
 *    An integer interpreted as a custom action return value.
 *   
 * \post
 *   + The return value is one of the following:
 *     - ERROR_INSTALL_USEREXIT if the user cancelled installation.
 *     - ERROR_INSTALL_FAILURE if something unexpected happened, usually if the top level try-block caught an exception.
 *     - ERROR_SUCCESS otherwise.
 *   + The function performed at least one check that Internet Explorer was running.
 *   + If ERROR_SUCCESS, the MSI property RUNNINGBROWSER is set and has one of the following values:
 *     - 1 if Internet Explorer was running upon the last check.
 *     - 0 otherwise.
 * \post
 *   Note that this function cannot provide any assurance that Internet Explorer stays either stays running or stays not running.
 *
 * \sa
 *   - MSDN [Custom Action Return Values](http://msdn.microsoft.com/en-us/library/aa368072%28v=vs.85%29.aspx)
 */
extern "C" UINT __stdcall 
abp_close_applications( MSIHANDLE session_handle )
{
  /*
   * Immediate_Session cannot throw, so it can go outside the try-block.
   * It's needed in the catch-all block to write an error message to the log.
   */
  Immediate_Session session( session_handle, "abp_close_applications" ) ;
    
  // The body of an entry point function must have a catch-all.
  try {
    
    // Instantiation of IE_List takes a snapshot.
    IE_List iel ; 
    // MSI property BROWSERRUNNING is one of the return values of this function.
    Property browser_running( session, L"BROWSERRUNNING" ) ;

    /*
     * We take the short path through this function if IE is not running at the outset.
     */
    if ( false && ! iel.is_running() )
    {
      browser_running = L"0" ;
      return ERROR_SUCCESS ;
    }

    /* TEST CODE */
    /*
     * We need to see if we can put up dialog boxes when in "Basic UI" mode.
     * If not, we have a pretty large problem with updates.
     */
    /*
     * First thing is to put up a dialog box at all.
     */
    int x = session.write_message( Installer_Message_Box( L"This is the initial dialog box." ) ) ;

    return ERROR_SUCCESS ;

    /*
     * As a (potentially) user-driven function, a state machine manages control flow.
     * The states are organized around the policy stances.
     */
    enum Policy_State {
      // Non-terminal states
      not_known,	  // We don't know the user's stance at all
      part_known,	  // The user has indicated either ACTIVE or AUTOMATIC
      allow,		  // Allow reboot
      passive,		  // Passively avoid reboot
      active,		  // Actively avoid reboot
      automatic,          // Automatically avoid reboot
      // Terminal states
      success,
      abort
    };
    Policy_State state = not_known;

    /*
     * Use the AVOIDREBOOT property, if present, to set an initial state.
     */
    std::wstring avoid_reboot = Property( session, L"AVOIDREBOOT" ) ;
    std::transform( avoid_reboot.begin(), avoid_reboot.end(), avoid_reboot.begin(), ::towupper ) ;
    if ( avoid_reboot == L"NO" )
    {
      state = allow ;
    }
    else if ( avoid_reboot == L"PASSIVE" )
    {
      state = passive ;
    }
    else if ( avoid_reboot == L"ACTIVE" )
    {
      state = active ;
    }
    else if ( avoid_reboot == L"AUTOMATIC" )
    {
      state = automatic ;
    }
    else
    {
      // It's an error to specify an unrecognized value for AVOIDREBOOT.
      throw std::runtime_error( "unrecognized value for AVOIDREBOOT" ) ;
    }

    /*
     * When running as an update (see Updater.cpp), this installer receives the command line option "/qb",
     *   which sets the UI level to "Basic UI".
     * When running as an initial install, we cannot expect what command line options this installer receives.
     */
    /*
     * The UILevel property indicates whether we have the ability to put dialog boxes up.
     * Levels 2 (silent) and 3 (basic) do not have this ability.
     * Levels 4 (reduced) and 5 (full) do.
     *
     * MSDN [UILevel property](http://msdn.microsoft.com/en-us/library/windows/desktop/aa372096%28v=vs.85%29.aspx)
     */
    std::wstring uilevel = Property( session, L"UILevel" ) ;
    bool interactive ;
    if ( uilevel == L"5" || uilevel == L"4" )
    {
      interactive = true ;
      // Assert state is any of the non-terminal states except part_known
    }
    else if ( uilevel == L"3" || uilevel == L"2" )
    {
      interactive = false ;
      if ( state == not_known )
      {
	// Assert AVOIDREBOOT was not specified
	state = allow ;
      }
      else if ( state == active )
      {
	throw std::runtime_error( "may not specify AVOIDREBOOT=ACTIVE in a non-interative session" ) ;
      }
      // Assert state is one of { allow, passive, automatic }
    }
    else
    {
      throw std::runtime_error( "unrecognized value for UILevel" ) ;
    }

    /*
     * State machine: Loop through non-terminal states.
     */
    while ( state <= automatic )	  // "automatic" is the non-terminal state with the highest value
    {
      switch ( state )
      {
      case not_known:
	/*
	 * Precondition: interactive session
	 *
	 * Ask the user "Would you like to close IE and avoid reboot?"
	 * Yes -> Close IE somehow. Goto part_known.
	 * No -> Install with reboot. Goto allow.
	 * Cancel -> terminate installation. Goto abort.
	 */
	break;
      case part_known:
	/*
	 * Precondition: interactive session
	 *
	 * Ask the user "Would you like the installer to close IE for you?"
	 * Yes -> Goto automatic
	 * No -> Goto active
	 * Cancel -> Goto not_known
	 */
	break;
      case allow:
	/*
	 * We are allowing a reboot.
	 *
	 * All -> Goto success
	 */
	iel = IE_List() ;   // refresh to set BROWSERRUNNING correctly.
	state = success;
	break;
      case passive:
	/*
	 * We enter this state only if AVOIDREBOOT=PASSIVE, thus only within the first loop iteration.
	 * We only enter the loop if IE is running.
	 * (Hence) Assert IE is still running.
	 */
	state = abort;
	break;
      case active:
	/*
	 * Precondition: interactive session
	 *
	 * IE is no longer running -> Goto success
	 * IE is still running ->
	 *   Ask the user to close IE manually
	 *   OK -> re-enter this state
	 *   Cancel -> Goto not_known
	 */
	iel = IE_List() ;
	if ( iel.is_running() )
	{
	  // show the dialog
	}
	else
	{
	  state = success;
	}
	break;
      case automatic:
	/*
	 * Close all known IE instances
	 *
	 * Succeeded -> Goto success
	 * Failed && interactive ->
	 *   Ask user if they would like to try again
	 *   Retry -> re-enter this state
	 *   Cancel -> Goto not_known
	 * Failed && not interactive -> Goto abort
	 */
	break;
      }
    }
    /*
     * State machine: Actions for terminal states.
     */
    switch ( state )
    {
      case success:
	browser_running = iel.is_running() ? L"1" : L"0" ;
	return ERROR_SUCCESS ;
	break;
      case abort:
	break;
    }
  }
  catch( std::exception & e )
  {
    session.log_noexcept( "terminated by exception: " + std::string( e.what() ) ) ;
    return ERROR_INSTALL_FAILURE ;
  }
  catch( ... )
  {
    session.log_noexcept( "terminated by unknown exception" ) ;
    return ERROR_INSTALL_FAILURE ;
  }
  // Should be unreachable.
  return ERROR_INSTALL_FAILURE ;
}

/*
 * EnumWindows system call: http://msdn.microsoft.com/en-us/library/windows/desktop/ms633497%28v=vs.85%29.aspx
 */
/**
 * 
 * Callback function for EnumWindows.
 */
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
  return TRUE ;
}

/**
 * Windows_List
 *
 * 
 */
class Window_List {
public:
  void enumerate_top_level();
};
