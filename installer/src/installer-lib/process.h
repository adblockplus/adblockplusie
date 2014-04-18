/**
 * \file process.h
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <vector>
#include <set>
#include <algorithm>
#include <memory>

#include <Windows.h>
#include <TlHelp32.h>

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
   * Conversion operator to underlying HANDLE.
   */
  operator HANDLE() const { return handle ; } ;  

  /**
   * Raw handle assignment.
   *
   * This is equivalent to destroying the old object and constructing a new one in its place.
   * In C++11 this would be handled by the move constructor on an rvalue reference.
   */
  void operator=( HANDLE h ) ;

private:
  /**
   * \invariant The handle is an open handle to some system resource.
   */
  HANDLE handle ;

  /**
   * Validation function for the handle. Invoked at both construction and assignment.
   */
  void validate_handle() ;

  /**
   * Copy constructor declared private and not defined.
   *
   * \par Implementation
   *   Add "= delete" for C++11.
   */
  Windows_Handle( const Windows_Handle & ) ;

  /**
   * Copy assignment declared private and not defined.
   *
   * \par Implementation
   *   Add "= delete" for C++11.
   */
  Windows_Handle operator=( const Windows_Handle & ) ;

};

//-------------------------------------------------------
// file_name_set: case-insensitive wide-string set
//-------------------------------------------------------
int wcscmpi( const wchar_t * s1, const wchar_t * s2 ) ;

struct file_name
{
  /**
   * Pointer to wide-character string, which is supposed to be null-terminated.
   */
  const wchar_t * name ;

  file_name( const wchar_t * name ) : name( name ) {} ;
} ;

template <>
struct std::less< file_name >
  : std::binary_function< file_name, file_name, bool >
{
  bool operator()( const file_name & a, const file_name & b ) const
  {
    return wcscmpi( a.name, b.name ) < 0 ; 
  }
} ;

struct file_name_set
  : public std::set< file_name >
{
  file_name_set(){};

  file_name_set( const wchar_t * file_name_list[], size_t n_file_names )
  {
    for ( unsigned int j = 0 ; j < n_file_names ; ++ j )
    {
      insert( file_name( file_name_list[ j ] ) ) ;
    }
  }
} ;

//-------------------------------------------------------
//-------------------------------------------------------
/**
 * Filter by process name. Comparison is case-insensitive. With ABP module loaded
 */
class process_by_any_exe_with_any_module
  : public std::binary_function< PROCESSENTRY32W, file_name_set, bool >
{
  const file_name_set & processNames ;
  const file_name_set & moduleNames;
public:
  bool operator()( const PROCESSENTRY32W & ) ;
  process_by_any_exe_with_any_module( const file_name_set & names, const file_name_set & moduleNames )
    : processNames( names ), moduleNames( moduleNames )
  {}
} ;


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
 * Filter by process name. Comparison is case-insensitive.
 */
class process_by_name_CI
  : public std::unary_function< PROCESSENTRY32W, bool >
{
  const wchar_t * name ;
  const size_t length ;
  process_by_name_CI() ;
public:
  bool operator()( const PROCESSENTRY32W & ) ;
  process_by_name_CI( const wchar_t * name ) ;
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

/**
 * Retrieve the process ID that created a window.
 *
 * Wrapper around GetWindowThreadProcessId. 
 * Converts an error return from the system call into an exception.
 * The system call can also retrieve the creating thread; we ignore it.
 *
 * \param window
 *   Handle of the window
 * \return
 *   ID of the process that created the argument window
 *
 * \sa
 *   MSDN [GetWindowThreadProcessId function](http://msdn.microsoft.com/en-us/library/windows/desktop/ms633522%28v=vs.85%29.aspx)
 */
DWORD creator_process( HWND window ) ;

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
 * - MSDN [CreateToolhelp32Snapshot function](http://msdn.microsoft.com/en-us/library/windows/desktop/ms682489%28v=vs.85%29.aspx)
 * - MSDN [Process32First function](http://msdn.microsoft.com/en-us/library/windows/desktop/ms684834%28v=vs.85%29.aspx)
 * - MSDN [Process32Next function](http://msdn.microsoft.com/en-us/library/windows/desktop/ms684836%28v=vs.85%29.aspx)
 * - MSDN [PROCESSENTRY32 structure](http://msdn.microsoft.com/en-us/library/windows/desktop/ms684839%28v=vs.85%29.aspx)
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

  /**
   * Copy constructor declared private and not defined.
   *
   * \par Implementation
   *   Add "= delete" for C++11.
   */
  Snapshot( const Snapshot & ) ;

  /**
   * Copy assignment declared private and not defined.
   *
   * \par Implementation
   *   Add "= delete" for C++11.
   */
  Snapshot operator=( const Snapshot & ) ;


public:
  /**
   * Default constructor takes the snapshot.
   */
  Snapshot() ;

  /**
   * Reconstruct the current instance with a new system snapshot.
   */
  void refresh() ;

  /**
   * Return a pointer to the first process in the snapshot.
   */
  PROCESSENTRY32W * first() ;

  /**
   * Return a pointer to the next process in the snapshot.
   * begin() must have been called first.
   */
  PROCESSENTRY32W * next() ;
} ;

class ModulesSnapshot
{
  /**
   * Handle to the process snapshot.
   */
  Windows_Handle handle;

  /**
   * Buffer for reading a single module entry out of the snapshot.
   */
  MODULEENTRY32W module;

  /**
   * Copy constructor declared private and not defined.
   *
   * \par Implementation
   *   Add "= delete" for C++11.
   */
  ModulesSnapshot(const ModulesSnapshot&);

  /**
   * Copy assignment declared private and not defined.
   *
   * \par Implementation
   *   Add "= delete" for C++11.
   */
  ModulesSnapshot operator=(const ModulesSnapshot&);


public:
  /**
   * Default constructor takes the snapshot.
   */
  ModulesSnapshot(DWORD processId);

  /**
   * Return a pointer to the first process in the snapshot.
   */
  MODULEENTRY32W* first();

  /**
   * Return a pointer to the next process in the snapshot.
   * begin() must have been called first.
   */
  MODULEENTRY32W* next();
};


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
template<class T, class Admittance, class Extractor>
void initialize_process_list(std::vector<T>& v, Snapshot& snap, Admittance admit = Admittance(), Extractor extract = Extractor())
{
  PROCESSENTRY32W* p = snap.first();
  while (p != NULL)
  {
    if (admit(*p ))
    {
      /*
	* We don't have C++11 emplace_back, which can construct the element in place.
	* Instead, we copy the return value of the converter.
	*/
      v.push_back(extract(*p));
    }
    p = snap.next();
  }
};

//-------------------------------------------------------
// initialize_process_set
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
template<class T, class Admittance, class Extractor>
void initialize_process_set(std::set< T > & set, Snapshot& snap, Admittance admit = Admittance(), Extractor extract = Extractor())
{
  PROCESSENTRY32W* p = snap.first();
  while (p != NULL)
  {
    if (admit(*p))
    {
      set.insert(extract(*p));
    }
    p = snap.next();
  }
};

//-------------------------------------------------------
// enumerate_windows
//-------------------------------------------------------

/**
 * States of a window enumeration.
 */
typedef enum
{
  started,	  ///< The iteration is currently running
  normal,	  ///< Iteration terminated without error.
  early,	  ///< Callback returned false and terminated iteration early.
  exception,	  ///< Callback threw an exception and thereby terminated iteration.
  error		  ///< Callback always return true but EnumWindows failed.
} enumerate_windows_state ;

/**
 * Data to perform a window enumeration, shared between the main function and the callback function.
 */
template< class F >
struct ew_data
{
  F & f ;

  enumerate_windows_state status ;

  std::unique_ptr< std::exception > ee ;

  ew_data( F & f )
    : f( f ), status( started )
  {}
} ;

/**
 * Callback function for EnumWindows.
 */
template< class F >
BOOL CALLBACK enumeration_callback( HWND window, LPARAM x )
{
  // LPARAM is always the same size as a pointer
  ew_data< F > * data = reinterpret_cast< ew_data< F > * >( x ) ;
  try 
  {
    bool r = data -> f( window ) ;
    if ( ! r )
    {
      data -> status = early ;
    }
    return r ;
  }
  catch ( std::exception e )
  {
    data -> ee = std::unique_ptr< std::exception >( new( std::nothrow ) std::exception( e ) ) ;
    data -> status = exception ;
    return FALSE ;
  }
  catch ( ... )
  {
    data -> status = exception ;
    return FALSE ;
  }
  return TRUE ;
}

/**
 * Enumerate windows, applying a function to each one.
 */
template< class F >
bool enumerate_windows( F f )
{
  ew_data< F > data( f ) ;
  BOOL x( ::EnumWindows( enumeration_callback< F >, reinterpret_cast< LPARAM >( & data ) ) ) ;
  bool r ;
  if ( data.status != started )
  {
    // Assert status was changed within the callback
    if ( data.status == exception )
    {
      /*
       * The callback threw an exception of some sort.
       * We forward it to the extent we are able.
       */
      if ( data.ee )
      {
	throw * data.ee ;
      }
      else
      {
	throw std::runtime_error( "Unknown exception thrown in callback function." ) ;
      }
    }
    r = false ;
  }
  else
  {
    if ( x )
    {
      data.status = normal ;
      r = true ;
    }
    else
    {
      // Assert EnumWindows failed
      data.status = error ;
      r = false ;
    }
  }
  return r ;
}

//-------------------------------------------------------
// Process_Closer
//-------------------------------------------------------
class Process_Closer
{
  /**
   * Set of process identifiers matching one of the executable names.
   */
  std::set< DWORD > pid_set ;

  /**
   * Set of executable names by which to filter.
   *
   * The argument of the filter constructor is a set by reference.
   * Since it does not make a copy for itself, we define it as a class member to provide its allocation.
   */
  file_name_set file_names ;
  /**
   * Set of module (DLL) names by which to filter.
   */
  file_name_set module_names ;

  process_by_any_exe_with_any_module filter ;

  /**
   * Copy function object copies just the process ID.
   */
  copy_PID copy ;

  /** 
   * Snapshot of running processes.
   */
  Snapshot & snapshot ;

  void update()
  {
    initialize_process_set( pid_set, snapshot, filter, copy ) ;
  } ;

  template< class F >
  class only_our_processes
  {
    Process_Closer & self ;

    F f ;

  public:
    only_our_processes( Process_Closer & self, F f )
      : f( f ), self( self )
    {}

    bool operator()( HWND window )
    {
      bool b ;
      try
      {
	b = self.contains( creator_process( window ) ) ;
      }
      catch ( ... )
      {
	// ignore window handles that are no longer valid
	return true ;
      }
      if ( ! b )
      {
	// Assert the process that created the window is not in our pid_set
	return true ;
      }
      return f( window ) ;
    }
  } ;

public:
  template <size_t n_file_names, size_t n_module_names>
  Process_Closer(Snapshot & snapshot, const wchar_t* (&file_name_list)[n_file_names], const wchar_t* (&module_name_list)[n_module_names])
    : snapshot(snapshot), file_names(file_name_list, n_file_names), module_names(module_name_list, n_module_names), filter(file_names, module_names)
  {
    update() ;
  }
  template <size_t n_file_names>
  Process_Closer(Snapshot & snapshot, const wchar_t * (&file_name_list)[n_file_names])
    : snapshot(snapshot), file_names(file_name_list, n_file_names), module_names(), filter(file_names, module_names)
  {
    update() ;
  }

  /**
   * Refresh our state to match the snapshot state.
   */
  void refresh()
  {
    pid_set.clear() ;
    update() ;
  }

  bool is_running() { return ! pid_set.empty() ; } ;

  bool contains( DWORD pid ) const { return pid_set.find( pid ) != pid_set.end() ; } ;

  template< class F >
  bool iterate_our_windows( F f )
  {
    only_our_processes< F > g( * this, f ) ;
    return enumerate_windows( g ) ;
  }

  /*
   * Shut down every process in the pid_set.
   */
  bool shut_down() ;

} ;

#endif
