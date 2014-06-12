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
*     throw windows_api_error( "MsiDatabaseOpenView", "ERROR_BAD_QUERY_SYNTAX" )
*   \endcode
*
* \par
*   Sometimes you don't have a symbolic error code.
*   This example uses a numeric error and a clarifying message.
*   \code
*     throw windows_api_error( "MsiOpenDatabaseW", x, "MSI database is on file system" )
*   \endcode
*/
class windows_api_error
  : public std::runtime_error
{
  template< class T1, class T2, class T3 >
  static std::string make_message( T1 api_function, T2 error_code, T3 message )
  {
    std::ostringstream r, t ;
    std::string s ;

    t << api_function ;
    s = t.str() ;
    if ( s.empty() )
    {
      s = "<unspecified>" ;
    }
    r << s <<  " returned " ;

    t = std::ostringstream() ;
    t << error_code ;
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
  * \param api_function
  *	The name of the API function that returned an error code or a null handle.
  * \param error_code
  *    The error code that the function returned, either symbolic or numeric.
  *    Will be zero when the function returned a null handle.
  * \param message
  *    Extra message to clarify the error
  */
  template< class T1, class T2, class T3 >
  windows_api_error( T1 api_function, T2 error_code, T3 message  )
    : std::runtime_error( make_message( api_function, error_code, message ) )
  {}

  /**
  * Constructor without anything extra.
  *
  * \param api_function
  *	The name of the API function that returned an error code or a null handle.
  * \param error_code
  *    The error code that the function returned, either symbolic or numeric.
  *    Will be zero when the function returned a null handle.
  */
  template< class T1, class T2 >
  windows_api_error( T1 api_function, T2 error_code )
    : std::runtime_error( make_message( api_function, error_code, "" ) )
  {}
} ;

/**
*/
class not_yet_supported
  : public std::runtime_error
{
public:
  not_yet_supported( std::string message )
    : std::runtime_error( "Not yet supported: " + message )
  {}
} ;

#endif
