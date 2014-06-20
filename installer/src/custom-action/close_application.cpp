/**
 * \file close_application.cpp
 */

#include <algorithm>

#include "session.h"
#include "property.h"
#include "database.h"
#include "process.h"
#include "interaction.h"
#include "custom-i18n.h"

const wchar_t * ie_names[] = { L"IExplore.exe" } ;
const wchar_t * engine_names[] = { L"AdblockPlusEngine.exe" } ;

//-------------------------------------------------------
//-------------------------------------------------------
class IE_Closer
{
  Process_Snapshot snapshot ;

  Process_Closer ie_closer ;

  Process_Closer engine_closer ;

public:
  IE_Closer()
    : snapshot(), ie_closer( snapshot, ie_names), engine_closer( snapshot, engine_names )
  {}

  void refresh()
  {
    snapshot.refresh() ;
    ie_closer.refresh() ;
    engine_closer.refresh() ;
  }

  bool is_running()
  {
    return ie_closer.is_running() || engine_closer.is_running() ;
  }

  bool shut_down()
  {
    if ( ie_closer.is_running() && ! ie_closer.shut_down() )
    {
      // Assert IE is still running
      // This is after we've tried to shut it down, so we fail
      return false ;
    }
    if ( engine_closer.is_running() && ! engine_closer.shut_down() )
    {
      // Assert the engine is still running
      // This is after IE has shut down itself and after we've tried to shut down the engine. Whatever.
      return false ;
    }
    return true ;
  }
} ;


//-------------------------------------------------------
// abp_close_ie
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
abp_close_ie( MSIHANDLE session_handle )
{
  // Utility typedef to shorten the class name.
  typedef Installer_Message_Box IMB ;

  /*
   * Immediate_Session cannot throw, so it can go outside the try-block.
   * It's needed in the catch-all block to write an error message to the log.
   */
  Immediate_Session session( session_handle, "abp_close_ie" ) ;
    
  // The body of an entry point function must have a catch-all.
  try {

    // MSI property BROWSERRUNNING is one of the return values of this function.
    Property browser_running( session, L"BROWSERRUNNING" ) ;
    Property browser_closed( session, L"BROWSERCLOSED" ) ;

    // Instantiation of Process_Closer takes a snapshot.
    IE_Closer iec ;

    /*
     * We take the short path through this function if neither IE nor engine is not running at the outset.
     */
    if ( ! iec.is_running() )
    {
      browser_running = L"0" ;	    // The browser is not running.
      browser_closed = L"0" ;	    // We didn't close the browser (and we couldn't have).
      session.log( "IE not running. No issue with reboot policy." ) ;
      return ERROR_SUCCESS ;
    }

    /*
     * As a (potentially) user-driven function, a state machine manages control flow.
     * The states are organized around the policy stances.
     */
    enum Policy_State {
      // Non-terminal states
      not_known,	  // We don't know the user's stance at all
      part_known,	  // The user has indicated either ACTIVE or AUTOMATIC
      active,		  // Actively avoid reboot
      automatic,          // Automatically avoid reboot
      // Terminal states
      success,
      abort,
      // Aliases for what would ordinarily be non-terminal states.
      // They're terminal because of implementation details.
      allow = success,	  // Allow reboot. 
      passive = abort,	  // Passively avoid reboot, that is, don't try to close IE.
    };
    Policy_State state ;

    /*
     * Use the AVOIDREBOOT property, if present, to set an initial state.
     */
    std::wstring avoid_reboot = Property( session, L"AVOIDREBOOT" ) ;
    std::transform( avoid_reboot.begin(), avoid_reboot.end(), avoid_reboot.begin(), ::towupper ) ;
    if ( avoid_reboot == L"" )
    {
      state = not_known ;
    }
    else if ( avoid_reboot == L"NO" )
    {
      state = allow ;
      session.log( "Reboot allowed on command line." ) ;
    }
    else if ( avoid_reboot == L"PASSIVE" )
    {
      state = passive ;
      session.log( "Reboot avoided on command line." ) ;
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
      // Assert state is one of { not_known, allow, passive, active, automatic }
    }
    else if ( uilevel == L"3" || uilevel == L"2" )
    {
      // Assert installer is running without user interaction.
      interactive = false ;
      if ( state == not_known )
      {
	// Assert AVOIDREBOOT was not specified
	/*
	 * This is where we specify default behavior for non-interactive operation.
	 * The choice of "allow" makes it act like other installers, which is to make no effort to avoid a reboot after installation.
	 */
	state = allow ;
	session.log( "Reboot allowed by default in non-interactive session." ) ;
      }
      else if ( state == active )
      {
	throw std::runtime_error( "AVOIDREBOOT=ACTIVE in non-interative session is not consistent" ) ;
      }
      // Assert state is one of { allow, passive, automatic }
    }
    else
    {
      throw std::runtime_error( "unrecognized value for UILevel" ) ;
    }

    /*
     * Now that preliminaries are over, we set up the accessors for UI text.
     * We only use the object 'message_text' for interactive sessions, but it's cheap to set up and a hassle to conditionalize.
     *
     * The key "close_ie" is the component name within the file "close_ie.wxi" that defines rows in the localization table.
     * The identifiers for the message_text.text() function are defined within that file.
     */
    Installation_Database db( session ) ;
    custom_message_text message_text( db, L"close_ie" ) ;

    /*
     * State machine: Loop through non-terminal states.
     *
     * Loop invariant: IE was running at last check, that is, iec.is_running() would return true.
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
	{
	  int x = session.write_message( IMB( message_text.text( L"dialog_unknown" ), IMB::warning_box, IMB::yes_no_cancel, IMB::default_button_three ) ) ;
	  switch ( x )
	  {
	  case IDYES:
	    state = part_known ;
	    break ;
	  case IDNO:
	    state = allow ;
	    session.log( "User chose to allow reboot" ) ;
	    break ;
	  case IDCANCEL:
	    state = abort ;
	    session.log( "User cancelled installation" ) ;
	    break ;
	  default:
	    throw unexpected_return_value_from_message_box() ;
	  }
	}
	break ;

      case part_known:
	/*
	 * Precondition: interactive session
	 *
	 * Ask the user "Would you like the installer to close IE for you?"
	 * Yes -> Goto automatic
	 * No -> Goto active
	 * Cancel -> Goto not_known
	 */
	{
	  int x = session.write_message( IMB( message_text.text( L"dialog_part_known" ), IMB::warning_box, IMB::yes_no_cancel, IMB::default_button_three ) ) ;
	  switch ( x )
	  {
	  case IDYES:
	    state = automatic ;
	    break ;
	  case IDNO:
	    state = active ;
	    break ;
	  case IDCANCEL:
	    state = not_known ;
	    break ;
	  default:
	    throw unexpected_return_value_from_message_box() ;
	  }
	}
	break ;

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
	{
	  int x = session.write_message( IMB( message_text.text( L"dialog_active_retry" ), IMB::warning_box, IMB::ok_cancel, IMB::default_button_one ) ) ;
	  switch ( x )
	  {
	  case IDOK:
	    /*
	     * Refresh our knowledge of whether IE is running.
	     * If it is, we display the dialog again. The state doesn't change, so we just iterate again.
	     * If it's not, then the user has closed IE and we're done.
	     */
	    iec.refresh() ;
	    if ( ! iec.is_running() )
	    {
	      state = success ;
	      session.log( "User shut down IE manually." ) ;
	    }
	    break ;
	  case IDCANCEL:
	    state = not_known ;
	    break ;
	  default:
	    throw unexpected_return_value_from_message_box() ;
	  }
	}
	break ;

      case automatic:
	/*
	 * Close all known IE instances.
	 * Unlike other cases, this state starts with an action and not a user query.
	 * We first shut down IE, or at least attempt to.
	 *
	 * Succeeded -> Goto success
	 * Failed && interactive ->
	 *   Ask user if they would like to try again
	 *   Retry -> re-enter this state
	 *   Cancel -> Goto not_known
	 * Failed && not interactive -> Goto abort
	 */
	{
	  bool IE_was_closed = iec.shut_down() ;
	  if ( iec.is_running() )
	  {
	    session.log( "Attempt to shut down IE automatically failed." ) ;
	    if ( interactive )
	    {
	      // Assert Interactive session and IE did not shut down.
	      int x = session.write_message( IMB( message_text.text( L"dialog_automatic_retry" ), IMB::warning_box, IMB::retry_cancel, IMB::default_button_one ) ) ;
	      switch ( x )
	      {
	      case IDRETRY:
		// Don't change the state. Iterate again.
		break ;
	      case IDCANCEL:
		state = not_known ;
		break ;
	      default:
		throw unexpected_return_value_from_message_box() ;
	      }
	    }
	    else
	    {
	      // Assert Non-interactive session and IE did not shut down.
	      state = abort ;
	      session.log( "Failed to shut down IE automatically." ) ;
	    }
	  }
	  else
	  {
	    // Assert IE is not running, so shut_down() succeeded.
	    state = success ;
	    session.log( "Automatically shut down IE." ) ;
	  }
	}
	break;
      }
    }
    /*
     * State machine: Actions for terminal states.
     */
    switch ( state )
    {
      case success:
	if ( iec.is_running() )
	{
	  browser_running = L"1" ;
	  browser_closed = L"0" ;
	}
	else
	{
	  browser_running = L"0" ;
	  browser_closed = L"1" ;
	}
	return ERROR_SUCCESS ;
	break;
      case abort:
	return ERROR_INSTALL_USEREXIT ;
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
 */
class Window_List {
public:
  void enumerate_top_level();
};
