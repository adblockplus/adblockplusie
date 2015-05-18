/**
* \file handle.h The "install session" is the context for all custom installation behavior.
*/

#ifndef HANDLE_H
#define HANDLE_H

#include "windows.h"
#include "msi.h"

//-------------------------------------------------------
// MsiHandle
//-------------------------------------------------------
/**
* Disambiguation class holding an MSIHANDLE.
*
* We need constructors for Record that accept both handles and record counts.
* Since the underlying type of a handle is integral, without its own type these constructors are ambiguous.
*/
class MsiHandle
{
  MSIHANDLE handle ;

public:
  /**
  * Ordinary constructor is explicit to avoid inadvertent conversions.
  */
  explicit MsiHandle( MSIHANDLE handle )
    : handle( handle )
  {}

  operator MSIHANDLE()
  {
    return handle ;
  }
} ;

//-------------------------------------------------------
// Handle Policies
//-------------------------------------------------------
/**
* Policy class that indicates that a raw handle may not be zero.
*/
template< class T >
struct DisallowNull
{
  inline static bool ProhibitedAlways() { return true ; }
  inline static bool ProhibitedFromOutside() { return true ; }
} ;

/**
* Policy class that indicates that a raw handle may be zero only when constructed internally.
*/
template< class T >
struct SpecialNull
{
  inline static bool ProhibitedAlways() { return false ; }
  inline static bool ProhibitedFromOutside() { return true ; }
} ;

/**
* Policy class that indicates that a raw handle is permitted to be zero.
*/
template< class T >
struct AllowNull
{
  inline static bool ProhibitedAlways() { return false ; }
  inline static bool ProhibitedFromOutside() { return false ; }
} ;

/**
* Policy class that does not close a handle at all.
*/
template< class T >
class NoDestruction
{
public:
  inline static void Close( T handle ) {} ;
} ;

/**
* Policy class that closes an MSI handle when it goes out of scope.
*/
template< class T >
class GenericMsiDestruction
{
public:
  inline static void Close( T handle )
  {
    MsiCloseHandle( handle ) ;
  } ;
} ;

/**
* Policy class that closes a Windows handle when it goes out of scope.
*/
template< class T >
class GenericWindowsDestruction
{
public:
  inline static void Close( T handle )
  {
    CloseHandle( handle ) ;
  } ;
} ;


//-------------------------------------------------------
// Handle
//-------------------------------------------------------
/**
* Raw handle is the base class for the generic handle.
*
* Raw handles always allow null, so that generic handles may allow or disallow them as they please.
*/
template< class T >
class HandleRaw
{
protected:
  /**
  * The underlying handle.
  *
  * This is the only data member of the class.
  * Everything else is for life cycle and type conversion.
  */
  T handle ;

  /**
  * Basic constructor is protected to cede creation policy entirely to subclasses.
  */
  HandleRaw( T handle )
    : handle( handle )
  {}

public:
  /**
  * Conversion operator to underlying handle type.
  */
  operator T()
  {
    return handle ;
  }

  /**
  * Error thrown when initialize or assigning a null handle against policy.
  *
  * Note that this error is a logic_error, not a runtime error.
  * If it's against policy for a handle to be null, it's an error for the caller to try to make it null.
  * Policy enforcment here is not a substitute for good error handling by the caller.
  * In many cases, the caller ought to be throwing WindowsApiError.
  */
  struct NullHandleError
    : public std::logic_error
  {
    NullHandleError()
      : std::logic_error( "May not initialize with null handle" ) 
    {}
  } ;
} ;

/*
* Handle class
*/
template<
  class T,
  template <class> class NullPolicy,
  template <class> class DestructionPolicy = NoDestruction
>
class Handle
  : public HandleRaw< T >
{
  /**
   * Copy constructor prohibited.
   *
   * This class represents an external resource and is responsible for its lifecycle.
   * As such, the semantics here require a one-to-one match between instances and resource.
   * Copy construction violates these semantics.
   *
   * \par Implementation
   *   Currently, declared private and not defined.
   *   Add "= delete" for C++11.
   */
  Handle( Handle & ) ;

  /**
   * Copy assignment not implemented.
   *
   * It's not used anywhere yet.
   *
   * \par Implementation
   *   Currently, declared private and not defined.
   */
  Handle operator=( const Handle & ) ;

  /**
   * Validate initial handle values, both for construction and reinitialization assignment.
   */
  T ValidateHandle( T handle )
  {
    if ( NullPolicy< T >::ProhibitedFromOutside() && handle == 0 )
    {
      throw NullHandleError() ;
    }
    return handle ;
  }

protected:
  /**
   * Tag class for null record constructor
   */
  class NullType {} ;

  /**
   * Null handle constructor initializes its handle to zero.
   *
   * The null record constructor avoids the ordinary check that an external handle not be zero.
   * It's declared protected so that it's not ordinarily visible.
   * To use this constructor, derive from it and add a friend declaration.
   */
  Handle( NullType )
    : HandleRaw( 0 )
  {
    if ( NullPolicy< T >::ProhibitedAlways() )
    {
      throw NullHandleError() ;
    }
  }

public:
  /**
   * Ordinary constructor.
   *
   * A check for a non-zero handle compiles in conditionally based on the NullPolicy.
   */
  Handle( T handle )
    : HandleRaw( ValidateHandle( handle ) )
  {}

  /**
   * Reinitialization Assignment.
   *
   * If we had C++11 move constructors, we wouldn't need this, since this acts exactly as construct-plus-move would.
   */
  Handle & operator=( T handle )
  {
    ValidateHandle( handle ) ;
    this -> ~Handle() ;
    this -> handle = handle ;
    return * this ;
  }

  /**
   * Destructor
   *
   * The check for a non-zero handle compiles out conditionally based on NullPolicy.
   * The specific function used to close the handle is given by the DestructionPolicy.
   */
  ~Handle()
  {
    if ( NullPolicy< T >::ProhibitedAlways() || ( handle != 0 ) ) {
      DestructionPolicy< T >::Close( handle ) ;
    }
  }

  /**
   * Expose the underlying handle type.
   */
  typedef T HandleType ;
} ;

//-------------------------------------------------------
// Common instantiations of handle
//-------------------------------------------------------
typedef Handle< HANDLE, DisallowNull, GenericWindowsDestruction > WindowsHandle ;

#endif
