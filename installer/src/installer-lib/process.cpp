#include <stdexcept>
#include <functional>
#include <wctype.h>
// <thread> is C++11, but implemented in VS2012
#include <thread>

#include "process.h"

//-------------------------------------------------------
// Windows_Handle
//-------------------------------------------------------
Windows_Handle::Windows_Handle( HANDLE h ) 
  : handle( h ) 
{
  validate_handle() ;
}

Windows_Handle::~Windows_Handle()
{
  CloseHandle( handle ) ;
}

void Windows_Handle::operator=( HANDLE h )
{
  this -> ~Windows_Handle() ;
  handle = h ;
  validate_handle() ;
}

void Windows_Handle::validate_handle()
{
  if ( handle == INVALID_HANDLE_VALUE )
  {
    throw std::runtime_error( "Invalid handle" ) ;
  }
}

//-------------------------------------------------------
// process_by_name_CI
//-------------------------------------------------------
process_by_name_CI::process_by_name_CI( const wchar_t * name )
  : name( name ), length( wcslen( name ) )
{}

bool process_by_name_CI::operator()( const PROCESSENTRY32W & process )
{
  return 0 == wcsncmpi( process.szExeFile, name, length ) ;
}

//-------------------------------------------------------
// process_by_any_exe_name_CI
//-------------------------------------------------------
bool process_by_any_exe_name_CI::operator()( const PROCESSENTRY32W & process )
{
  return names.find( process.szExeFile ) != names.end() ;
}

//-------------------------------------------------------
// wcscmpi
//-------------------------------------------------------
int wcscmpi( const wchar_t * s1, const wchar_t * s2 )
{
  // Note: Equality of character sequences is case-insensitive in all predicates below.
  // Loop invariant: s1[0..j) == s2[0..j)
  const size_t LIMIT( 4294967295 ) ; // Runaway limit of 2^32 - 1 should be acceptably long.
  for ( size_t j = 0 ; j < LIMIT ; ++j )
  {
    wchar_t c1 = towupper( *s1++ ) ;
    wchar_t c2 = towupper( *s2++ ) ;
    if ( c1 != c2 )
    {
      // Map to -1/+1 because c2 - c1 may not fit into an 'int'.
      return ( c1 < c2 ) ? -1 : 1 ;
    }
    else
    {
      if ( c1 == L'\0' )
      {
	// Assert length( s1 ) == length( s2 ) == j
	// Assert strings are equal at length < n
	return 0 ;
      }
    }
  }
  // Assert j == LIMIT
  // Assert s1[0..LIMIT) == s2[0..LIMIT)
  // Both strings are longer than 64K, which violates the precondition
  throw std::runtime_error( "String arguments too long for wcscmpi" ) ;
}

//-------------------------------------------------------
// wcsncmpi
//-------------------------------------------------------
int wcsncmpi( const wchar_t * s1, const wchar_t * s2, unsigned int n )
{
  // Note: Equality of character sequences is case-insensitive in all predicates below.
  // Loop invariant: s1[0..j) == s2[0..j)
  for ( unsigned int j = 0 ; j < n ; ++j )
  {
    wchar_t c1 = towupper( *s1++ ) ;
    wchar_t c2 = towupper( *s2++ ) ;
    if ( c1 != c2 )
    {
      // Map to -1/+1 because c2 - c1 may not fit into an 'int'.
      return ( c1 < c2 ) ? -1 : 1 ;
    }
    else
    {
      if ( c1 == L'\0' )
      {
	// Assert length( s1 ) == length( s2 ) == j
	// Assert strings are equal at length < n
	return 0 ;
      }
    }
  }
  // Assert j == n
  // Assert s1[0..n) == s2[0..n)
  // The semantics of n-compare ignore everything after the first 'n' characters.
  return 0 ;
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
    throw std::runtime_error( "" ) ;
  }
  return pid ;
}

//-------------------------------------------------------
// Snapshot
//-------------------------------------------------------
Snapshot::Snapshot()
  : handle( ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) )
{
  process.dwSize = sizeof( PROCESSENTRY32W ) ;
}

PROCESSENTRY32W * Snapshot::begin()
{
  return ::Process32FirstW( handle, & process ) ? ( & process ) : 0 ;
}

PROCESSENTRY32W * Snapshot::next()
{
  return ::Process32NextW( handle, & process ) ? ( & process ) : 0 ;
}

void Snapshot::refresh()
{
  handle = ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) ;
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
  : public std::binary_function< DWORD_PTR, BOOL, bool >
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
    BOOL rv = SendMessageTimeoutW( window, message, p1, p2, SMTO_BLOCK, timeout, & result ) ;
    /*
     * If we have no message accumulator, the default behavior is to iterate everything.
     * If we do have one, we delegate to it the decision whether to break or to continue.
     */
    if ( ! f )
    {
      return true ;
    }
    return ( * f )( result, rv ) ;
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
  bool operator()( DWORD_PTR result, BOOL return_value )
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
// Process_Closer
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
bool Process_Closer::shut_down()
{
  /*
   * If we're not running, we don't need to shut down.
   */
  if ( ! is_running() )
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
	  send_message m2( WM_ENDSESSION, 0, 0 ) ;
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

    default :
      /*
       * We're out of ways to try to shut down.
       */
      return false ;
    }

    /*
     * Wait loop.
     */
    for ( unsigned int j = 0 ; j < 50 ; ++ j )
    {
      std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) ) ;
      refresh() ;
      if ( ! is_running() )
      {
	return true ;
      }
    }
    // Assert is_running()
  }
  // No control path leaves the for-loop.
} ;

