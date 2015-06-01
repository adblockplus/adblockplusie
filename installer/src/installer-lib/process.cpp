#include <stdexcept>
#include <functional>
#include <wctype.h>
// <thread> is C++11, but implemented in VS2012
#include <thread>

#include "installer-lib.h"
#include "process.h"
#include "handle.h"
#include "session.h"

//-------------------------------------------------------
//-------------------------------------------------------
typedef int (__stdcall *IsImmersiveDynamicFunc)(HANDLE);
bool process_by_any_exe_not_immersive::operator()( const PROCESSENTRY32W & process )
{
  // If the name is not found in our list, it's filtered out
  if (processNames.find(process.szExeFile) == processNames.end()) return false;

  // Make sure the process is still alive
  HANDLE tmpHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process.th32ProcessID);
  if (tmpHandle == NULL) return false;
  Windows_Handle procHandle(tmpHandle);
  DWORD exitCode;
  if (!GetExitCodeProcess(procHandle, &exitCode)) return false;
  if (exitCode != STILL_ACTIVE) return false;

  // Check if this is a Windows Store app process (we don't care for IE in Modern UI)
  HMODULE user32Dll = LoadLibrary(L"user32.dll");
  if (!user32Dll) return true;
  IsImmersiveDynamicFunc IsImmersiveDynamicCall = (IsImmersiveDynamicFunc)GetProcAddress(user32Dll, "IsImmersiveProcess");
  if (!IsImmersiveDynamicCall) return true;
  return !IsImmersiveDynamicCall(procHandle);
}

//-------------------------------------------------------
// creator_process
//-------------------------------------------------------
DWORD creator_process( HWND window )
{
  DWORD pid ;
  DWORD r = GetWindowThreadProcessId( window, & pid ) ;
  if ( r == 0 )
  {
    // Assert GetWindowThreadProcessId returned an error
    // If the window handle is invalid, we end up here.
    throw windows_api_error( "GetWindowThreadProcessId", r ) ;
  }
  return pid ;
}

//-------------------------------------------------------
// send_message, send_endsession_messages
//-------------------------------------------------------
/**
* Default process exit wait time (per message) 5000 ms
*
* 5 seconds is time that the system will wait before it considers a process non-responsive.
*/
static const unsigned int timeout = 5000 ;    // milliseconds

/**
* An function object to process the results of sending window messages in send_message.
*
* We are using send_message within a system iteration over windows.
* The system has its own convention for continuing/breaking the iteration.
* This convention is assured consistently in send_message, which also provides default behavior.
* This class provides the base for any variation from the default behavior.
*/
struct message_accumulator
  : public std::binary_function< DWORD_PTR, bool, bool >
{
  virtual result_type operator()( first_argument_type result, second_argument_type return_value ) = 0 ;
  virtual ~message_accumulator() {} ;
} ;

/**
* Iteration action to send a message to a window and accumulate results.
*
* An error sending the message is not a failure for the function a whole.
* The goal is to close the process, and if the window is no longer present, then the process may have already closed.
* Therefore, we're ignoring both the return value and the result.
*/
class send_message
{
  UINT message ;		///< Message type for windows message
  WPARAM p1 ;			///< Generic parameter 1 for windows message
  LPARAM p2 ;			///< Generic parameter 2 for windows message
  message_accumulator * f ;	///< Processor for results of sending the message.

public:
  /**
  * Full contructor gathers message parameters and a message accumulator.
  */
  send_message( UINT message, WPARAM p1, LPARAM p2, message_accumulator & f )
    : message( message ), p1( p1 ), p2( p2 ), f( & f )
  {}

  /**
  * Abbreviated contructor gathers only message parameters.
  * The message accumulator is absent.
  */
  send_message( UINT message, WPARAM p1, LPARAM p2 )
    : message( message ), p1( p1 ), p2( p2 ), f( 0 )
  {}

  /*
  * Enumeration function applied to each window.
  */
  bool operator()( HWND window )
  {
    DWORD_PTR result ;
    LRESULT rv = SendMessageTimeoutW( window, message, p1, p2, SMTO_BLOCK, timeout, & result ) ;
    /*
    * If we have no message accumulator, the default behavior is to iterate everything.
    * If we do have one, we delegate to it the decision whether to break or to continue.
    */
    if ( ! f )
    {
      return true ;
    }
    return ( * f )( result, (rv != 0) ) ;
  }
} ;

/**
* Send WM_QUERYENDSESSION and WM_ENDSESSION to a window.
*
* This window processor tries to shut down each application individually.
* The alternative, gathering all the query results first and only then ending sessions, cannot be done with a single window enumeration.
*/
class send_endsession_messages
{
public:
  /*
  * Enumeration function applied to each window.
  */
  bool operator()( HWND window )
  {
    DWORD_PTR result ;
    if ( ! SendMessageTimeoutW( window, WM_QUERYENDSESSION, 0, ENDSESSION_CLOSEAPP, SMTO_BLOCK, timeout, & result ) )
    {
      // Assert sending the message failed
      // Ignore failure, just as with send_message().
      return true ;
    }
    // Assert result is FALSE if the process has refused notice that it should shut down.
    if ( ! result )
    {
      /*
      * Returning false terminates iteration over windows.
      * Since this process is refusing to shut down, we can't close all the processes and the operation fails.
      */
      return false ;
    }
    SendMessageTimeoutW( window, WM_ENDSESSION, 0, ENDSESSION_CLOSEAPP, SMTO_BLOCK, timeout, 0 ) ;
    return true ;
  }
} ;

/**
* Accumulator for query-endsession message.
*
* Implements a conditional-conjunction of the query results.
* All answers must be true in order for this result to be true,
*   and the calculation is terminated at the first answer 'false'.
* As usual, errors sending messages are ignored.
*/
struct endsession_accumulator :
  public message_accumulator
{
  bool permit_end_session ; ///< Accumulator variable yields final result.

  /**
  * Enumeration function applied to each window.
  */
  bool operator()( DWORD_PTR result, bool return_value )
  {
    if ( ( ! return_value ) || result )
    {
      // 1. If the result is true, then the process will permit WM_ENDSESSION
      // 2. An error sending the message counts as "no new information"
      return true ;
    }
    // The first false is the result of the calculation.
    // The second false means to terminate enumeration early.
    permit_end_session = false ;
    return false ;
  }

  /**
  * Ordinary constructor.
  */
  endsession_accumulator()
    : permit_end_session( true )
  {}
} ;

//-------------------------------------------------------
// ProcessCloser
//-------------------------------------------------------
/**
* Shut down all the processes in the pid_list.
*
* The method used here uses blocking system calls to send messages to target processes.
* Message processing delays, therefore, are sequential and the total delay is their sum.
* Windows has non-blocking message calls available, and using a multi-threaded implementation would shorten that delay.
* The code, hwoever, is significantly simpler without multi-threading.
* The present use of this method is not closing dozens of applications, so delay performance is not critical.
*
* \return
*   The negation of is_running.
*   If is_running() was true at the beginning, then this function will have run refresh() before returning.
*
* \sa
*   - MSDN [WM_QUERYENDSESSION message](http://msdn.microsoft.com/en-us/library/windows/desktop/aa376890%28v=vs.85%29.aspx)
*   - MSDN [WM_ENDSESSION message](http://msdn.microsoft.com/en-us/library/windows/desktop/aa376889%28v=vs.85%29.aspx)
*/
bool ProcessCloser::ShutDown(ImmediateSession& session)
{
  /*
  * If we're not running, we don't need to shut down.
  */
  if ( ! IsRunning() )
  {
    return true ;
  }

  /*
  * Shutting down is a structure as an escalating series of attempts to shut down.
  * After each one, we wait to see if the shut down has completed.
  * Even though we're using a blocking call to send messages, applications need not block before exiting.
  * Internet Explorer, in particular, does not.
  *
  * Note that termination occurs inside the default case within the switch statement
  */
  for ( unsigned int stage = 1 ; ; ++ stage )
  {
    // Assert is_running()
    switch( stage )
    {
    case 1 :
      /*
      * Send WM_QUERYENDSESSION to every admissible window.
      * Send WM_ENDSESSION if all processes are ready to shut down.
      * We try this technique first, since this allows an application to restore its application state when it starts up again.
      */
      {
        endsession_accumulator acc ;
        send_message m1( WM_QUERYENDSESSION, 0, ENDSESSION_CLOSEAPP, acc ) ;
        iterate_our_windows( m1 ) ;

        if ( acc.permit_end_session )
        {
          send_message m2( WM_ENDSESSION, 0, ENDSESSION_CLOSEAPP ) ;
          iterate_our_windows( m2 ) ;
        }
      }
      break ;

    case 2 :
      {
        /*
        * Send WM_QUERYENDSESSION and WM_ENDSESSION to every admissible window singly, not accumulating results.
        */
        send_endsession_messages m ;
        iterate_our_windows( m ) ;
      }
      break ;

    case 3 :
      {
        /*
        * Send WM_CLOSE to every admissible window.
        */
        send_message m( WM_CLOSE, 0, 0 ) ;
        iterate_our_windows( m ) ;
      }
      break ;

    case 4:
      /*
      * Oh well. Take cover. It gets violent here. Try to kill all matching processes.
      */
      for (auto it = pid_set.begin(); it != pid_set.end(); ++it)
      {
        HANDLE tmpHandle = OpenProcess(PROCESS_TERMINATE, FALSE, *it);
        if (!tmpHandle) 
        {
          std::ostringstream stream;
          stream << "Can't open process for termination. Error: " << GetLastError();
          session.Log(stream.str());
          continue;
        }
        Windows_Handle procHandle(tmpHandle);
        if (!TerminateProcess(tmpHandle, 0))
        {
          std::ostringstream stream;
          stream << "Can't terminate process. Error: " << GetLastError();
          session.Log(stream.str());
        }
      }
      break;

    default:
      // We're out of ways to try to shut down. 
      return false;
    }

    /*
    * Wait loop.
    */
    for ( unsigned int j = 0 ; j < 50 ; ++ j )
    {
      std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) ) ;
      Refresh() ;
      if ( ! IsRunning() )
      {
        return true ;
      }
    }
    // Assert is_running()
  }
  // No control path leaves the for-loop.
} ;

