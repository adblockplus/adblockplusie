#include <windows.h>
#include <TlHelp32.h>

#include <vector>

class Windows_Handle
{
public:
  /**
   * Ordinary constructor.
   *
   * Validates argument against INVALID_HANDLE_VALUE. No other checks performed.
   */
  Windows_Handle( HANDLE h ) ;

  /**
   * Destructor
   */
  ~Windows_Handle() ;

  /**
   * Copy constructor declared but not defined.
   *
   * \par Implementation
   *   Add "= delete" for C++11.
   */
  Windows_Handle( const Windows_Handle & ) ;

  /**
   * Copy assignment declared but not defined.
   *
   * \par Implementation
   *   Add "= delete" for C++11.
   */
  Windows_Handle operator=( const Windows_Handle & ) ;

  /**
   * Conversion operator to underlying HANDLE.
   */
  operator HANDLE() const { return handle ; } ;

private:
  /**
   * \invariant The handle is an open handle to some system resource.
   */
  HANDLE handle ;
};

/**
 * \tparam Predicate A unary predicate type, either function pointer or function object. Ordinarily inferred from the parameter.
 */
template< class T >
class Process_List
{
public:
  /**
   * \tparam Predicate Function pointer or function object. Generally inferred from the argument.
   * \param admit A selection filter predicate. 
   *	A process appears in the list only if the predicate returns true. 
   *	The use of this predicate is analogous to that in std::copy_if.
   * \tparam Converter Function pointer or function object. Generally inferred from the argument.
   * \param convert A conversion function that takes a PROCESSENTRY32W as input argument and returns an element of type T.
   */
  template< class Predicate, class Converter >
  Process_List( Predicate admit, Converter convert );

  /**
   */
  ~Process_List() {};

  /**
   * This class is principally a way of initializing a vector by filtering and extracting a process list.
   * There's no point in keeping the underlying vector private.
   */
  std::vector< T > v;
};

/**
 * \par Implementation
 *
 * CreateToolhelp32Snapshot function http://msdn.microsoft.com/en-us/library/windows/desktop/ms682489%28v=vs.85%29.aspx
 * Process32First function http://msdn.microsoft.com/en-us/library/windows/desktop/ms684834%28v=vs.85%29.aspx
 * Process32Next function http://msdn.microsoft.com/en-us/library/windows/desktop/ms684836%28v=vs.85%29.aspx
 * PROCESSENTRY32 structure http://msdn.microsoft.com/en-us/library/windows/desktop/ms684839%28v=vs.85%29.aspx
 */
template< class T > template< class Predicate, class Converter > 
Process_List< T >::Process_List( Predicate admit, Converter convert )
{
  /*
   * Take a snapshot only of all processes on the system, and not all the other data available through the CreateToolhelp32Snapshot.
   * Second argument is ignored for flag TH32CS_SNAPPROCESS.
   */
  Windows_Handle handle( ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) );
  // Size initialization required for Process32FirstW to succeed.
  PROCESSENTRY32W process;
  process.dwSize = sizeof( PROCESSENTRY32W );
  /*
   * Process32FirstW and Process32NextW iterate through the process snapshot.
   */
  BOOL have_process = ::Process32FirstW( handle, & process );
  while ( have_process )
  {
    if ( admit( process ) )
    {
      /*
       * We don't have C++11 emplace_back, which can construct the element in place.
       * Instead, we copy the return value of the converter.
       */
      v.push_back( convert( process ) );
    }
    have_process = ::Process32NextW( handle, & process );
  }
}

