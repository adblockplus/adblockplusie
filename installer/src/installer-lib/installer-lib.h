/**
* \file handle.h The "install session" is the context for all custom installation behavior.
*/

#ifndef INSTALLER_LIB_H
#define INSTALLER_LIB_H

#include <stdexcept>
#include <sstream>

/**
* Standard runtime error for failure of Windows API calls.
*
* The design purpose of this class is to consistently report the details of a failed API call, with an eye toward logging.
* All the arguments passed to the constructor appear in what().
* In addition the return value of GetLastError() appears.
*
* All the types for the constructors are generic.
* Any type that works with the output operator '<<' of a stream will work.
*
* \par Example
*   For a simple error, where there's not much to add over the API call and the error code itself, just omit the second argument.
*   \code
*     throw WindowsApiError( "MsiDatabaseOpenView", "ERROR_BAD_QUERY_SYNTAX" )
*   \endcode
*
* \par
*   Sometimes you don't have a symbolic error code.
*   This example uses a numeric error and a clarifying message.
*   \code
*     throw WindowsApiError( "MsiOpenDatabaseW", x, "MSI database is on file system" )
*   \endcode
*/
class WindowsApiError
  : public std::runtime_error
{
  template< class T1, class T2, class T3 >
  static std::string MakeMessage( T1 apiFunction, T2 errorCode, T3 message )
  {
    std::ostringstream r, t ;
    std::string s ;

    t << apiFunction ;
    s = t.str() ;
    if ( s.empty() )
    {
      s = "<unspecified>" ;
    }
    r << s <<  " returned " ;

    t = std::ostringstream() ;
    t << errorCode ;
    s = t.str() ;
    if ( s.empty() )
    {
      s = "<unknown>" ;
    }
    r << s << " with last error code " << ::GetLastError() ;

    t = std::ostringstream() ;
    t << message ;
    s = t.str() ;
    if ( ! s.empty() )
    {
      r << ": " << s ;
    }

    return r.str() ;
  }

public:
  /**
  * Constructor with additional message.
  *
  * \param apiFunction
  *	The name of the API function that returned an error code or a null handle.
  * \param errorCode
  *    The error code that the function returned, either symbolic or numeric.
  *    Will be zero when the function returned a null handle.
  * \param message
  *    Extra message to clarify the error
  */
  template< class T1, class T2, class T3 >
  WindowsApiError( T1 apiFunction, T2 errorCode, T3 message  )
    : std::runtime_error( MakeMessage( apiFunction, errorCode, message ) )
  {}

  /**
  * Constructor without anything extra.
  *
  * \param apiFunction
  *	The name of the API function that returned an error code or a null handle.
  * \param errorCode
  *    The error code that the function returned, either symbolic or numeric.
  *    Will be zero when the function returned a null handle.
  */
  template< class T1, class T2 >
  WindowsApiError( T1 apiFunction, T2 errorCode)
    : std::runtime_error( MakeMessage( apiFunction, errorCode, "" ) )
  {}
} ;

/**
*/
class NotYetSupported
  : public std::runtime_error
{
public:
  NotYetSupported( std::string message )
    : std::runtime_error( "Not yet supported: " + message )
  {}
} ;

#endif
