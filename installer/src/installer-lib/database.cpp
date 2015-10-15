/**
* \file database.h MSI database
*/

#include "database.h"
#include "msiquery.h"

//-----------------------------------------------------------------------------------------
// Database
//-----------------------------------------------------------------------------------------
MsiHandle Database::OpenView(const wchar_t* query)
{
  MSIHANDLE viewHandle;
  UINT x = MsiDatabaseOpenView(handle, query, & viewHandle);
  if (x == ERROR_BAD_QUERY_SYNTAX)
  {
    throw WindowsApiError("MsiDatabaseOpenView", "ERROR_BAD_QUERY_SYNTAX");
  }
  else if (x == ERROR_INVALID_HANDLE)
  {
    throw WindowsApiError("MsiDatabaseOpenView", "ERROR_INVALID_HANDLE");
  }
  return MsiHandle(viewHandle);
}

//-----------------------------------------------------------------------------------------
// InstallationDatabase
//-----------------------------------------------------------------------------------------

/**
* Helper function for InstallationDatabase constructor.
*
* \par Resource Allocator
*    Return value of this function, a handle, must be released in order to avoid a resource leak.
*    Passing it as an argument to the Database constructor is adequate.
*/
MsiHandle GetActiveDatabase(ImmediateSession& session)
{
  MSIHANDLE h(MsiGetActiveDatabase(session.handle));
  if (h == 0)
  {
    throw WindowsApiError("MsiGetActiveDatabase", 0);
  }
  return MsiHandle(h);
}

/**
* \par Implementation Notes
*    The only thing this constructor needs to do is to initialize the base class.
*/
InstallationDatabase::InstallationDatabase(ImmediateSession& session)
  : Database(GetActiveDatabase(session))
{
  // empty body
};

//-----------------------------------------------------------------------------------------
// View
//-----------------------------------------------------------------------------------------
/**
* Implementation function for View::First().
*/
void ViewFirstBody(UINT x)
{
  if (x != ERROR_SUCCESS)
  {
    throw WindowsApiError("MsiViewExecute", x);
  }
}

Record View::First()
{
  ViewFirstBody(MsiViewExecute(handle, 0));
  return Next();
}

Record View::First(Record& arguments)
{
  ViewFirstBody(MsiViewExecute(handle, arguments.handle));
  return Next();
}

Record View::Next()
{
  MSIHANDLE h;
  UINT x = MsiViewFetch(handle, & h);
  if (x == ERROR_NO_MORE_ITEMS)
  {
    return Record(Record::NullType());
  }
  else if (x == ERROR_SUCCESS)
  {
    return Record(MsiHandle(h));
  }
  throw WindowsApiError("MsiViewFetch", x);
}

