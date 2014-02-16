#include <windows.h>
#include <TlHelp32.h>

#include <vector>

//-------------------------------------------------------
// Windows_Handle
//-------------------------------------------------------
/**
 * A handle to some Windows platform resource. 
 *
 * Note, this is not the same as a Windows Installer handle (MSIHANDLE).
 * The two handles have different underlying types and use different functions to close.
 */
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

//-------------------------------------------------------
// Process utility functions.
//-------------------------------------------------------
/**
 * A promiscuous filter admits everything.
 */
struct every_process
  : public std::unary_function< PROCESSENTRY32W, bool >
{
  bool operator()( const PROCESSENTRY32W & ) { return true ; } ;
} ;

/**
 * Extractor that copies the entire process structure.
 */
struct copy_all
  : public std::unary_function< PROCESSENTRY32W, PROCESSENTRY32W >
{
  PROCESSENTRY32W operator()( const PROCESSENTRY32W & process ) { return process ; }
} ;

/**
 * Extractor that copies only the PID.
 */
struct copy_PID
  : public std::unary_function< PROCESSENTRY32W, DWORD >
{
  inline DWORD operator()( const PROCESSENTRY32W & process ) { return process.th32ProcessID ; }
} ;

/**
 * Case-insensitive wide-character C-style string comparison, fixed-length
 */
int wcsncmpi( const wchar_t * a, const wchar_t * b, unsigned int length ) ;

//-------------------------------------------------------
// Snapshot
//-------------------------------------------------------
/**
 * A snapshot of all the processes running on the system.
 *
 * Unfortunately, we cannot provide standard iterator for this class.
 * Standard iterators must be copy-constructible, which entails the possibility of multiple, coexisting iteration states.
 * The iteration behavior provided by Process32First and Process32Next relies upon state held within the snapshot itself.
 * Thus, there can be only one iterator at a time for the snapshot.
 * The two requirements are not simultaneously satisfiable.
 *
 * As a substitute for a standard iterator, we provide a few functions mimicking the pattern of standard iterators.
 * This class acts as its own iterator.
 * The pointer returned is either one to the member variable "process" or else 0.
 *
 * \par Implementation
 *
 * CreateToolhelp32Snapshot function http://msdn.microsoft.com/en-us/library/windows/desktop/ms682489%28v=vs.85%29.aspx
 * Process32First function http://msdn.microsoft.com/en-us/library/windows/desktop/ms684834%28v=vs.85%29.aspx
 * Process32Next function http://msdn.microsoft.com/en-us/library/windows/desktop/ms684836%28v=vs.85%29.aspx
 * PROCESSENTRY32 structure http://msdn.microsoft.com/en-us/library/windows/desktop/ms684839%28v=vs.85%29.aspx
 */
class Snapshot
{
  /**
   * Handle to the process snapshot.
   */
  Windows_Handle handle ;

  /**
   * Buffer for reading a single process entry out of the snapshot.
   */
  PROCESSENTRY32W process;

public:
  /**
   * Default constructor takes the snapshot.
   */
  Snapshot() ;

  /**
   * Return a pointer to the first process in the snapshot.
   */
  PROCESSENTRY32W * begin() ;

  /**
   * The end pointer is an alias for the null pointer.
   */
  inline PROCESSENTRY32W * end() const { return 0 ; }

  /**
   * Return a pointer to the next process in the snapshot.
   * begin() must have been called first.
   */
  PROCESSENTRY32W * next() ;

  /**
   * Type definition for pointer to underlying structure.
   */
  typedef PROCESSENTRY32W * Pointer ;
} ;

//-------------------------------------------------------
// initialize_process_list
//-------------------------------------------------------
/**
 * \tparam T The type into which a PROCESSENTRY32W struture is extracted.
 * \tparam Admittance Function type for argument 'admit'
 * \tparam Extractor Function type for argument 'extract'
 * \param admit A unary predicate function class that determines what's included
 *	A process appears in the list only if the predicate returns true. 
 *	The use of this predicate is analogous to that in std::copy_if.
 * \param convert A conversion function that takes a PROCESSENTRY32W as input argument and returns an element of type T.
 */
template< class T, class Admittance, class Extractor >
void initialize_process_list( std::vector< T > & v, Admittance admit = Admittance(), Extractor extract = Extractor() )
{
  Snapshot snap ;
  Snapshot::Pointer p = snap.begin() ;
  while ( p != snap.end() )
  {
    if ( admit( * p ) )
    {
      /*
	* We don't have C++11 emplace_back, which can construct the element in place.
	* Instead, we copy the return value of the converter.
	*/
      v.push_back( extract( * p ) );
    }
    p = snap.next() ;
  }
} ;

