/**
 * \file process.h
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "handle.h"

#include <string>
#include <cctype>
#include <vector>
#include <set>
#include <algorithm>
#include <memory>

#include <Windows.h>
#include <TlHelp32.h>

//-------------------------------------------------------
// wstring_ci: case-insensitive wide string
//-------------------------------------------------------

/**
 * Traits class for case-insensitive strings.
 */
template< class T >
struct ci_traits: std::char_traits< T >
{
  static bool eq( T c1, T c2 )
  {
    return std::tolower( c1 ) == std::tolower( c2 ) ;
  }

  static bool lt( T c1, T c2 )
  {
    return std::tolower( c1 ) < std::tolower( c2 ) ;
  }

  /**
   * Trait comparison function.
   *
   * Note that this is not a comparison of C-style strings.
   * In particular, there's no concern over null characters '\0'.
   * The argument 'n' is the minimum length of the two strings being compared.
   * We may assume that the intervals p1[0..n) and p2[0..n) are both valid substrings.
   */
  static int compare( const T * p1, const T * p2, size_t n )
  {
    while ( n-- > 0 )
    {
      T l1 = std::tolower( * p1 ++ ) ;
      T l2 = std::tolower( * p2 ++ ) ;
      if ( l1 == l2 )
      {
        continue ;
      }
      return ( l1 < l2 ) ? -1 : +1 ;
    }
    return 0  ;
  }
} ;

typedef std::basic_string< wchar_t, ci_traits< wchar_t > > wstring_ci ;


//-------------------------------------------------------
// file_name_set: case-insensitive wide-string set
//-------------------------------------------------------
struct file_name_set
  : public std::set< wstring_ci >
{
  /**
   * Empty set constructor.
   */
  file_name_set()
  {}

  /**
   * Constructor initialization from an array.
   */
  template< size_t n_file_names >
  file_name_set( const wchar_t * ( & file_name_list )[ n_file_names ] )
  {
    for ( unsigned int j = 0 ; j < n_file_names ; ++ j )
    {
      insert( wstring_ci( file_name_list[ j ] ) ) ;
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
  /**
   * Set of file names from which to match candidate process names.
   *
   * This is a reference to, not a copy of, the set.
   * The lifetime of this object must be subordinate to that of its referent.
   * The set used to instantiate this class is a member of Process_Closer,
   *   and so also is this class.
   * Hence the lifetimes are coterminous, and the reference is not problematic.
   */
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
  /**
   * Function to be applied to each enumerated window.
   */
  F & f ;

  /**
   * Completion status of the enumeration.
   */
  enumerate_windows_state status ;

  /**
   * An exception to be transported across the callback.
   *
   * The enumerator and the callback are not guaranteed to share a call stack,
   *   nor need they even share compatible exception conventions,
   *   and might not even be in the same thread.
   * Thus, if the applied function throws an exception, 
   *   we catch it in the callback and re-throw it in the enumerator.
   * This member holds such an exception.
   *
   * This member holds an exception only if 'status' has the value 'exception'.
   * Otherwise it's a null pointer.
   */
  std::unique_ptr< std::exception > ee ;

  /**
   * Ordinary constructor.
   */
  ew_data( F & f )
    : f( f ), status( started )
  {}
} ;

/**
 * Callback function for EnumWindows.
 *
 * This function provides two standard behaviors.
 * It records early termination of the enumeration, should that happen by the applied function returning false.
 * It captures any exception thrown for transport back to the enumerator.
 */
template< class F >
BOOL CALLBACK enumeration_callback( HWND window, LPARAM x )
{
  // LPARAM is always the same size as a pointer
  ew_data< F > * data = reinterpret_cast< ew_data< F > * >( x ) ;
  /*
   * Top-level try statement prevents exception from propagating back to system.
   */
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
    data -> ee = std::unique_ptr< std::exception >() ;
    data -> status = exception ;
    return FALSE ;
  }
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
  file_name_set process_names ;

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
    : snapshot(snapshot), process_names(file_name_list), module_names(module_name_list), filter(process_names, module_names)
  {
    update() ;
  }
  template <size_t n_file_names>
  Process_Closer(Snapshot & snapshot, const wchar_t * (&file_name_list)[n_file_names])
    : snapshot(snapshot), process_names(file_name_list), module_names(), filter(process_names, module_names)
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
