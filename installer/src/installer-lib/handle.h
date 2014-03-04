/**
 * \file handle.h The "install session" is the context for all custom installation behavior.
 */

#ifndef HANDLE_H
#define HANDLE_H

#include "windows.h"
#include "msi.h"

//-------------------------------------------------------
// msi_handle
//-------------------------------------------------------
/**
 * Disambiguation class holding an MSIHANDLE.
 *
 * We need constructors for Record that accept both handles and record counts.
 * Since the underlying type of a handle is integral, without its own type these constructors are ambiguous.
 */
class msi_handle
{
  MSIHANDLE _handle ;

public:
  /**
   * Ordinary constructor is explicit to avoid inadvertent conversions.
   */
  explicit msi_handle( MSIHANDLE handle )
    : _handle( handle )
  {}

  operator MSIHANDLE()
  {
    return _handle ;
  }
} ;

//-------------------------------------------------------
// Handle Policies
//-------------------------------------------------------
/**
 * Policy class that indicates that a raw handle may not be zero.
 */
template< class T >
struct Disallow_Null
{
  inline static bool prohibited_always() { return true ; }
  inline static bool prohibited_from_outside() { return true ; }
} ;

/**
 * Policy class that indicates that a raw handle may be zero only when constructed internally.
 */
template< class T >
struct Special_Null
{
  inline static bool prohibited_always() { return false ; }
  inline static bool prohibited_from_outside() { return true ; }
} ;

/**
 * Policy class that indicates that a raw handle is permitted to be zero.
 */
template< class T >
struct Allow_Null
{
  inline static bool prohibited_always() { return false ; }
  inline static bool prohibited_from_outside() { return false ; }
} ;

/**
 * Policy class that does not close a file handle when it goes out of scope
 */
template< class T >
class No_Destruction
{
public:
  inline static void close( T handle ) {} ;
} ;

/**
 * Policy class that does not close a file handle when it goes out of scope
 */
template< class T >
class MSI_Generic_Destruction
{
public:
  inline static void close( T handle )
  {
    MsiCloseHandle( handle ) ;
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
class handle_raw
{
protected:
  /**
   * The underlying handle.
   *
   * This is the only data member of the class.
   * Everything else is for life cycle and type conversion.
   */
  T _handle ;

  /**
   * Basic constructor is protected to cede creation policy entirely to subclasses.
   */
  handle_raw( T _handle )
    : _handle( _handle )
  {}
  
public:
  /**
   * Conversion operator to underlying handle type.
   */
  operator T()
  {
    return _handle ;
  }

  struct null_handle_error
    : public std::runtime_error
  {
    null_handle_error()
      : std::runtime_error( "May not initialize with null handle" ) 
    {}
  } ;
} ;

/*
 * Handle class
 */
template<
  class T,
  template <class> class Null_Policy,
  template <class> class Destruction_Policy = No_Destruction
>
class handle
  : public handle_raw< T >
{
protected:
  /**
   * Tag class for null record constructor
   */
  class null_t {} ;

  /**
   * Null handle constructor initializes its handle to zero.
   *
   * The null record constructor avoids the ordinary check that an external handle not be zero.
   * It's declared protected so that it's not ordinarily visible.
   * To use this constructor, derive from it and add a friend declaration.
   */
  handle( null_t )
    : handle_raw( 0 )
  {
    if ( Null_Policy< T >::prohibited_always() )
    {
      throw null_handle_error() ;
    }
  }

public:
  /**
   * Ordinary constructor.
   *
   * A check for a non-zero handle compiles in conditionally based on the Null_Policy.
   */
  handle( T _handle )
    : handle_raw( _handle )
  {
    if ( Null_Policy< T >::prohibited_from_outside() && _handle == 0 )
    {
      throw null_handle_error() ;
    }
  }

  /**
   * Destructor
   *
   * The check for a non-zero handle compiles out conditionally based on Null_Policy.
   * The specific function used to close the handle is given by the Destruction_Policy.
   */
  ~handle()
  {
    if ( Null_Policy< T >::prohibited_always() || ( _handle != 0 ) ) {
      Destruction_Policy< T >::close( _handle ) ;
    }
  }

} ;

#endif
