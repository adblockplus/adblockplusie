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

const wchar_t * browserNames[] = { L"IExplore.exe" } ;
const wchar_t * engineNames[] = { L"AdblockPlusEngine.exe" } ;

//-------------------------------------------------------
//-------------------------------------------------------
class InternetExplorerCloser
{
  ProcessSnapshot snapshot ;

  ProcessCloser browserCloser ;

  ProcessCloser engineCloser ;

public:
  InternetExplorerCloser()
    : snapshot(), browserCloser( snapshot, browserNames ), engineCloser( snapshot, engineNames )
  {}

  void Refresh()
  {
    snapshot.Refresh() ;
    browserCloser.Refresh() ;
    engineCloser.Refresh() ;
  }

  bool IsRunning()
  {
    return browserCloser.IsRunning() || engineCloser.IsRunning() ;
  }

  bool ShutDown(ImmediateSession& session)
  {
    if ( browserCloser.IsRunning() && ! browserCloser.ShutDown(session) )
    {
      // Assert IE is still running
      // This is after we've tried to shut it down, so we fail
      return false ;
    }
    if ( engineCloser.IsRunning() && ! engineCloser.ShutDown(session) )
    {
      // Assert the engine is still running
      // This is after IE has shut down itself and after we've tried to shut down the engine. Whatever.
      return false ;
    }
    return true ;
  }
} ;


//-------------------------------------------------------
// AbpCloseIe
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
 * \param[in] sessionHandle
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
AbpCloseIe( MSIHANDLE sessionHandle )
{
  // Utility typedef to shorten the class name.
  typedef InstallerMessageBox IMB ;

  /*
   * ImmediateSession cannot throw, so it can go outside the try-block.
   * It's needed in the catch-all block to write an error message to the log.
   */
  ImmediateSession session( sessionHandle, "AbpCloseIe" ) ;
    
  // The body of an entry point function must have a catch-all.
  try {

    // MSI property BROWSERRUNNING is one of the return values of this function.
    Property browserRunning( session, L"BROWSERRUNNING" ) ;
    Property browserClosed( session, L"BROWSERCLOSED" ) ;

    // Instantiation of ProcessCloser takes a snapshot.
    InternetExplorerCloser iec ;

    /*
     * We take the short path through this function if neither IE nor engine is not running at the outset.
     */
    if ( ! iec.IsRunning() )
    {
      browserRunning = L"0" ;	    // The browser is not running.
      browserClosed = L"0" ;	    // We didn't close the browser (and we couldn't have).
      session.Log( "IE not running. No issue with reboot policy." ) ;
      return ERROR_SUCCESS ;
    }

    /*
     * As a (potentially) user-driven function, a state machine manages control flow.
     * The states are organized around the policy stances.
     */
    enum PolicyState {
      // Non-terminal states
      notKnown,	  // We don't know the user's stance at all
      partKnown,	  // The user has indicated either ACTIVE or AUTOMATIC
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
    PolicyState state ;

    /*
     * Use the AVOIDREBOOT property, if present, to set an initial state.
     */
    std::wstring avoidReboot = Property( session, L"AVOIDREBOOT" ) ;
    std::transform( avoidReboot.begin(), avoidReboot.end(), avoidReboot.begin(), ::towupper ) ;
    if ( avoidReboot == L"" )
    {
      state = notKnown ;
    }
    else if ( avoidReboot == L"NO" )
    {
      state = allow ;
      session.Log( "Reboot allowed on command line." ) ;
    }
    else if ( avoidReboot == L"PASSIVE" )
    {
      state = passive ;
      session.Log( "Reboot avoided on command line." ) ;
    }
    else if ( avoidReboot == L"ACTIVE" )
    {
      state = active ;
    }
    else if ( avoidReboot == L"AUTOMATIC" )
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
      // Assert state is one of { notKnown, allow, passive, active, automatic }
    }
    else if ( uilevel == L"3" || uilevel == L"2" )
    {
      // Assert installer is running without user interaction.
      interactive = false ;
      if ( state == notKnown )
      {
	// Assert AVOIDREBOOT was not specified
	/*
	 * This is where we specify default behavior for non-interactive operation.
	 * The choice of "allow" makes it act like other installers, which is to make no effort to avoid a reboot after installation.
	 */
	state = allow ;
	session.Log( "Reboot allowed by default in non-interactive session." ) ;
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
     * We only use the object 'messageText' for interactive sessions, but it's cheap to set up and a hassle to conditionalize.
     *
     * The key "close_ie" is the component name within the file "close_ie.wxi" that defines rows in the localization table.
     * The identifiers for the messageText.text() function are defined within that file.
     */
    InstallationDatabase db( session ) ;
    CustomMessageText messageText( db, L"close_ie" ) ;

    /*
     * State machine: Loop through non-terminal states.
     *
     * Loop invariant: IE was running at last check, that is, iec.IsRunning() would return true.
     */
    while ( state <= automatic )	  // "automatic" is the non-terminal state with the highest value
    {
      switch ( state )
      {
      case notKnown:
	/*
	 * Precondition: interactive session
	 *
	 * Ask the user "Would you like to close IE and avoid reboot?"
	 * Yes -> Close IE somehow. Goto partKnown.
	 * No -> Install with reboot. Goto allow.
	 * Cancel -> terminate installation. Goto abort.
	 */
        {
          int x = session.WriteMessage(IMB( 
            messageText.Text(L"dialog_unknown"), 
            IMB::Box::warning, IMB::ButtonSet::yesNoCancel, IMB::DefaultButton::three
            )) ;
	  switch ( x )
	  {
	  case IDYES:
	    state = partKnown ;
	    break ;
	  case IDNO:
	    state = allow ;
	    session.Log( "User chose to allow reboot" ) ;
	    break ;
	  case IDCANCEL:
	    state = abort ;
	    session.Log( "User cancelled installation" ) ;
	    break ;
	  default:
	    throw UnexpectedReturnValueFromMessageBox() ;
	  }
	}
	break ;

      case partKnown:
	/*
	 * Precondition: interactive session
	 *
	 * Ask the user "Would you like the installer to close IE for you?"
	 * Yes -> Goto automatic
	 * No -> Goto active
	 * Cancel -> Goto notKnown
	 */
	{
	  int x = session.WriteMessage(IMB(
            messageText.Text(L"dialog_part_known"), 
            IMB::Box::warning, IMB::ButtonSet::yesNoCancel, IMB::DefaultButton::three
            )) ;
	  switch ( x )
	  {
	  case IDYES:
	    state = automatic ;
	    break ;
	  case IDNO:
	    state = active ;
	    break ;
	  case IDCANCEL:
	    state = notKnown ;
	    break ;
	  default:
	    throw UnexpectedReturnValueFromMessageBox() ;
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
	 *   Cancel -> Goto notKnown
	 */
	{
	  int x = session.WriteMessage(IMB(
            messageText.Text(L"dialog_active_retry"), 
            IMB::Box::warning, IMB::ButtonSet::okCancel, IMB::DefaultButton::one
            )) ;
	  switch ( x )
	  {
	  case IDOK:
	    /*
	     * Refresh our knowledge of whether IE is running.
	     * If it is, we display the dialog again. The state doesn't change, so we just iterate again.
	     * If it's not, then the user has closed IE and we're done.
	     */
	    iec.Refresh() ;
	    if ( ! iec.IsRunning() )
	    {
	      state = success ;
	      session.Log( "User shut down IE manually." ) ;
	    }
	    break ;
	  case IDCANCEL:
	    state = notKnown ;
	    break ;
	  default:
	    throw UnexpectedReturnValueFromMessageBox() ;
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
	 *   Cancel -> Goto notKnown
	 * Failed && not interactive -> Goto abort
	 */
	{
	  bool ieWasClosed = iec.ShutDown(session) ;
	  if ( iec.IsRunning() )
	  {
	    session.Log( "Attempt to shut down IE automatically failed." ) ;
	    if ( interactive )
	    {
	      // Assert Interactive session and IE did not shut down.
	      int x = session.WriteMessage(IMB(
                messageText.Text(L"dialog_automatic_retry"),
                IMB::Box::warning, IMB::ButtonSet::retryCancel, IMB::DefaultButton::one
                )) ;
	      switch ( x )
	      {
	      case IDRETRY:
		// Don't change the state. Iterate again.
		break ;
	      case IDCANCEL:
		state = notKnown ;
		break ;
	      default:
		throw UnexpectedReturnValueFromMessageBox() ;
	      }
	    }
	    else
	    {
	      // Assert Non-interactive session and IE did not shut down.
	      state = abort ;
	      session.Log( "Failed to shut down IE automatically." ) ;
	    }
	  }
	  else
	  {
	    // Assert IE is not running, so ShutDown() succeeded.
	    state = success ;
	    session.Log( "Automatically shut down IE." ) ;
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
	if ( iec.IsRunning() )
	{
	  browserRunning = L"1" ;
	  browserClosed = L"0" ;
	}
	else
	{
	  browserRunning = L"0" ;
	  browserClosed = L"1" ;
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
    session.LogNoexcept( "terminated by exception: " + std::string( e.what() ) ) ;
    return ERROR_INSTALL_FAILURE ;
  }
  catch( ... )
  {
    session.LogNoexcept( "terminated by unknown exception" ) ;
    return ERROR_INSTALL_FAILURE ;
  }
  // Should be unreachable.
  return ERROR_INSTALL_FAILURE ;
}
