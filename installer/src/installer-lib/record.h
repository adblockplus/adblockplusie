/**
* \file record.h Definition of Record class.
*/

#ifndef RECORD_H
#define RECORD_H

#include <string>

#include <Windows.h>
#include <Msi.h>

#include "handle.h"

// Forward
class View ;

/**
* An abstract record entity. 
* It represents both records in the installation database and as argument vectors for API functions.
*
* The ordinary constructor creates a free-standing record.
* It takes only the number of fields in the created record. 
* The fields of the record are dynamically typed according to how they're assigned.
* Other constructors will be required to encapsulate records that are bound to databases.
*
* This class has exclusive-ownership semantics for the API handle to the record.
* Every constructor has a postcondition that the _handle member points to an open record.
* The destructor closes the record.
* The copy constructor syntax is used as a move constructor (since no C++11 yet).
* Analogously, copy assignment has move semantics.
*
* \par Invariant
*   - _handle is not null implies _handle points to a record open in the Windows Installer subsystem
*
* \sa http://msdn.microsoft.com/en-us/library/windows/desktop/aa372881%28v=vs.85%29.aspx
*    Windows Installer on MSDN: "Working with Records"
*/
class Record {
  /**
  * 
  */
  typedef handle< MSIHANDLE, Special_Null, MSI_Generic_Destruction > record_handle_type ;

  /**
  * The handle for the record as a Windows Installer resource.
  */
  MSIHANDLE _handle ;

  /**
  * Construct a record from its handle as returned by some MSI call.
  */
  Record( msi_handle handle )
    : _handle( handle )
  {} 

  /**
  * Internal validation guard for operations that require a non-null handle.
  *
  * \post
  *   - if _handle is zero, throw an exception
  *   - if _handle is non-zero, nothing
  */
  void only_non_null() ;

  /**
  * Proxy class used to implement move semantics, prior to use of C++11.
  *
  * /sa
  *   - Wikibooks [More C++ Idioms/Move Constructor](http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Move_Constructor)
  */
  struct Proxy_Record
  {
    MSIHANDLE _handle ;

    Proxy_Record( MSIHANDLE handle )
      : _handle( handle )
    {}
  } ;

  /**
  * Tag class for null record constructor
  */
  class null_t {} ;

  /**
  * Null record constructor.
  *
  * The null record constructor avoids the ordinary check that an external handle not be zero.
  * It's declared private so that only friends can instantiate them.
  */
  Record( null_t )
    : _handle( 0 )
  {}

  /**
  * View class needs access to constructor-from-handle.
  */
  friend class View ;

public:
  /**
  * Ordinary constructor creates a free-standing record.
  * Use this for creating argument vectors.
  *
  * \post _handle points to a record obtained from MsiCreateRecord
  *
  * \param[in] n_fields
  *    Number of fields in the created record.
  */
  Record( unsigned int n_fields ) ;

  /**
  * Destructor
  */
  ~Record() ;

  /**
  * Copy constructor syntax used as a move constructor.
  */
  Record( Record & r ) 
    : _handle( r._handle )
  {
    r._handle = 0 ;
  }

  /**
  * Proxy move constructor.
  */
  Record( Proxy_Record r )
    : _handle( r._handle )
  {
    r._handle = 0 ;
  }

  /**
  * Copy assignment syntax has move assignment semantics. 
  */
  Record & operator=( Record & r )
  {
    this -> ~Record() ;
    _handle = r._handle ;
    r._handle = 0 ;
    return * this ;
  }

  /**
  * Proxy move assignment.
  */
  Record & operator=( Proxy_Record pr )
  {
    this -> ~Record() ;
    _handle = pr._handle ;
    pr._handle = 0 ;
    return * this ;
  }

  /**
  * Proxy conversion operator
  */
  operator Proxy_Record()
  {
    Proxy_Record pr( _handle ) ;
    _handle = 0 ;
    return pr ;
  }

  /**
  * Two records are equal exactly when their handles are equal.
  */
  inline bool operator==( const Record & x ) const
  {
    return _handle == x._handle ;
  }

  /**
  * Standard inequality operator defined by negating the equality operator.
  */
  inline bool operator!=( const Record & x ) const
  {
    return ! operator==( x ) ;
  }

  /**
  * Assign a string to a record, (regular) character pointer.
  *
  * \param[in] field_index
  *    Index into the record as a vector of fields
  * \param[in] value
  *    String to write into the field
  */
  void assign_string( unsigned int field_index, const char *value ) ;

  /**
  * Assign a string to a record, regular string version.
  *
  * \param[in] field_index
  *    Index into the record as a vector of fields
  * \param[in] value
  *    String to write into the field
  */
  void assign_string( unsigned int field_index, const std::string value )
  {
    assign_string( field_index, value.c_str() );
  }

  /**
  * Assign a string to a record, wide character pointer version.
  *
  * \param[in] field_index
  *    Index into the record as a vector of fields
  * \param[in] value
  *    String to write into the field
  */
  void assign_string( unsigned int field_index, const wchar_t *value ) ;

  /**
  * Assign a string to a record, wide string version.
  *
  * \param[in] field_index
  *    Index into the record as a vector of fields
  * \param[in] value
  *    String to write into the field
  */
  void assign_string( unsigned int field_index, const std::wstring value )
  {
    assign_string( field_index, value.c_str() );
  }

  /**
  * Retrieve a wide string value from a record
  */
  std::wstring value_string( unsigned int field_index ) ;

  /**
  * The number of fields in the record.
  */
  size_t n_fields() const ;

  /**
  * Handle accessor.
  */
  MSIHANDLE handle() { return _handle ; }
};

#endif
