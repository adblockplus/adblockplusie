/**
 * \file close_application.cpp
 */

#include "session.h"

//-------------------------------------------------------
// Fail
//-------------------------------------------------------
/**
 * A custom action that always and immediately fails.
 * Use during testing to ensure that the installer terminates.
 *
 * \param[in] session_handle
 *     Windows installer session handle
 *
 * \return
 *    An integer interpreted as a custom action return value.
 *
 * \sa
 *   - MSDN [Custom Action Return Values](http://msdn.microsoft.com/en-us/library/aa368072%28v=vs.85%29.aspx)
 */
extern "C" UINT __stdcall
fail(MSIHANDLE session_handle)
{
  // Instantiate the session object in order to get begin/end log entries.
  ImmediateSession session(session_handle, "fail");
  return ERROR_INSTALL_FAILURE;
}
