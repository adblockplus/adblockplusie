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
 * \tparam T The type into which a PROCESSENTRY32W struture is extracted.
 * \tparam Admittance A unary predicate function class that determines what's included
 */
template< class T, class Admittance, class Extractor >
class Process_List
{
private:
  Admittance admit;
  Extractor extract;
public:
  /**
   * \tparam Predicate Function pointer or function object. Generally inferred from the argument.
   * \param admit A selection filter predicate. 
   *	A process appears in the list only if the predicate returns true. 
   *	The use of this predicate is analogous to that in std::copy_if.
   * \tparam Converter Function pointer or function object. Generally inferred from the argument.
   * \param convert A conversion function that takes a PROCESSENTRY32W as input argument and returns an element of type T.
   *
   * \par Implementation
   *
   * CreateToolhelp32Snapshot function http://msdn.microsoft.com/en-us/library/windows/desktop/ms682489%28v=vs.85%29.aspx
   * Process32First function http://msdn.microsoft.com/en-us/library/windows/desktop/ms684834%28v=vs.85%29.aspx
   * Process32Next function http://msdn.microsoft.com/en-us/library/windows/desktop/ms684836%28v=vs.85%29.aspx
   * PROCESSENTRY32 structure http://msdn.microsoft.com/en-us/library/windows/desktop/ms684839%28v=vs.85%29.aspx
   */
  Process_List()
  {
    /*
     * Take a snapshot only of all processes on the system, and not all the other data available through the 
     * CreateToolhelp32Snapshot. The second argument is ignored when the only flag is TH32CS_SNAPPROCESS.
     */
    Windows_Handle handle( ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) );
    /*
     * Initialization of the size member of the process structure is required for Process32FirstW to succeed.
     */
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
	v.push_back( extract( process ) );
      }
      have_process = ::Process32NextW( handle, & process );
    }
  };

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
 * Case-insensitive wide-character C-style string comparison, fixed-length
 */
int wcsncmpi( const wchar_t * a, const wchar_t * b, unsigned int length ) ;