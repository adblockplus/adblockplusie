/**
 * \file session.h The "install session" is the context for all custom installation behavior.
 */

#ifndef RECORD_H
#define RECORD_H

#include <string>
#include "windows.h"
#include "msi.h"

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
 * The copy constructor and copy assignment are disabled.
 *
 * \par Invariant
 *   - _handle is not null
 *   - _handle points to a record open in the Windows Installer subsystem
 *
 * \sa http://msdn.microsoft.com/en-us/library/windows/desktop/aa372881%28v=vs.85%29.aspx
 *    Windows Installer on MSDN: "Working with Records"
 */
class Record {
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
   * Handle accessor.
   */
  MSIHANDLE handle() { return _handle ; }

private:
  /**
   * The handle for the record as a Windows Installer resource.
   */
  MSIHANDLE _handle ;

  /** 
   * The number of fields in the record.
   */
  unsigned int n_fields ;

  /**
   * Private copy constructor is not implemented.
   *
   * C++11 declare as <b>= delete</b>.
   */
  Record( const Record & ) ;

  /**
   * Private copy assignment is not implemented.
   *
   * C++11 declare as <b>= delete</b>.
   */
  Record & operator=( const Record & ) ;
};

#endif
