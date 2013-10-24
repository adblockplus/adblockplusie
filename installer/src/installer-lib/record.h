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
 *
 * Other constructors encapsulate records that are bound to databases.
 *
 * \par Invariant
 *   - _handle is not null
 *   - _handle is represents an open record obtained from MsiCreateRecord
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
   * \param[in] n_fields
   *    Number of fields in the created record.
   */
  Record( unsigned int n_fields ) ;

  /**
   * Destructor
   */
  ~Record() ;

  /**
   * Assign a string to a record
   *
   * \param[in] field_index
   *    Index into the record as a vector of fields
   * \param[in] value
   *    String to write into the field
   */
  void assign_string( unsigned int field_index, std::wstring value ) ;

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
};

#endif
