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
class View;

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
* Every constructor has a postcondition that the handle member points to an open record.
* The destructor closes the record.
* The copy constructor syntax is used as a move constructor (since no C++11 yet).
* Analogously, copy assignment has move semantics.
*
* \par Invariant
*   - handle is not null implies handle points to a record open in the Windows Installer subsystem
*
* \sa http://msdn.microsoft.com/en-us/library/windows/desktop/aa372881%28v=vs.85%29.aspx
*    Windows Installer on MSDN: "Working with Records"
*/
class Record
{
  /**
  *
  */
  typedef Handle<MSIHANDLE, SpecialNull, GenericMsiDestruction> RecordHandleType;

  /**
  * The handle for the record as a Windows Installer resource.
  */
  MSIHANDLE handle;

  /**
  * Construct a record from its handle as returned by some MSI call.
  */
  Record(MsiHandle handle)
    : handle(handle)
  {}

  /**
  * Internal validation guard for operations that require a non-null handle.
  *
  * \post
  *   - if handle is zero, throw an exception
  *   - if handle is non-zero, nothing
  */
  void OnlyNonNull();

  /**
  * Proxy class used to implement move semantics, prior to use of C++11.
  *
  * /sa
  *   - Wikibooks [More C++ Idioms/Move Constructor](http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Move_Constructor)
  */
  struct ProxyRecord
  {
    MSIHANDLE handle;

    ProxyRecord(MSIHANDLE handle)
      : handle(handle)
    {}
  };

  /**
  * Tag class for null record constructor
  */
  class NullType {};

  /**
  * Null record constructor.
  *
  * The null record constructor avoids the ordinary check that an external handle not be zero.
  * It's declared private so that only friends can instantiate them.
  */
  Record(NullType)
    : handle(0)
  {}

  /**
  * View class needs access to constructor-from-handle.
  */
  friend class View;

public:
  /**
  * Ordinary constructor creates a free-standing record.
  * Use this for creating argument vectors.
  *
  * \post handle points to a record obtained from MsiCreateRecord
  *
  * \param[in] nFields
  *    Number of fields in the created record.
  */
  Record(unsigned int nFields);

  /**
  * Destructor
  */
  ~Record();

  /**
  * Copy constructor syntax used as a move constructor.
  */
  Record(Record& r)
    : handle(r.handle)
  {
    r.handle = 0;
  }

  /**
  * Proxy move constructor.
  */
  Record(ProxyRecord r)
    : handle(r.handle)
  {
    r.handle = 0;
  }

  /**
  * Copy assignment syntax has move assignment semantics.
  */
  Record& operator=(Record& r)
  {
    this -> ~Record();
    handle = r.handle;
    r.handle = 0;
    return * this;
  }

  /**
  * Proxy move assignment.
  */
  Record& operator=(ProxyRecord pr)
  {
    this -> ~Record();
    handle = pr.handle;
    pr.handle = 0;
    return * this;
  }

  /**
  * Proxy conversion operator
  */
  operator ProxyRecord()
  {
    ProxyRecord pr(handle);
    handle = 0;
    return pr;
  }

  /**
  * Two records are equal exactly when their handles are equal.
  */
  inline bool operator==(const Record& x) const
  {
    return handle == x.handle;
  }

  /**
  * Standard inequality operator defined by negating the equality operator.
  */
  inline bool operator!=(const Record& x) const
  {
    return ! operator==(x);
  }

  /**
  * Assign a string to a record, (regular) character pointer.
  *
  * \param[in] fieldIndex
  *    Index into the record as a vector of fields
  * \param[in] value
  *    String to write into the field
  */
  void AssignString(unsigned int fieldIndex, const char* value);

  /**
  * Assign a string to a record, regular string version.
  *
  * \param[in] fieldIndex
  *    Index into the record as a vector of fields
  * \param[in] value
  *    String to write into the field
  */
  void AssignString(unsigned int fieldIndex, const std::string value)
  {
    AssignString(fieldIndex, value.c_str());
  }

  /**
  * Assign a string to a record, wide character pointer version.
  *
  * \param[in] fieldIndex
  *    Index into the record as a vector of fields
  * \param[in] value
  *    String to write into the field
  */
  void AssignString(unsigned int fieldIndex, const wchar_t* value);

  /**
  * Assign a string to a record, wide string version.
  *
  * \param[in] fieldIndex
  *    Index into the record as a vector of fields
  * \param[in] value
  *    String to write into the field
  */
  void AssignString(unsigned int fieldIndex, const std::wstring value)
  {
    AssignString(fieldIndex, value.c_str());
  }

  /**
  * Retrieve a wide string value from a record
  */
  std::wstring ValueString(unsigned int fieldIndex);

  /**
  * The number of fields in the record.
  */
  size_t NumberOfFields() const;

  /**
  * Handle accessor.
  */
  MSIHANDLE Handle()
  {
    return handle ;
  }
};

#endif
