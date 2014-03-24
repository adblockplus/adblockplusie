/**
 * \file abp_ca.cpp Top-level source for custom actions. Includes DLL initialization.
 */
#include "DLL.h"
#include <stdexcept>

/**
 * DllMain is the standard entry point call when the DLL is loaded or unloaded.
 *
 * \param[in] module_handle 
 *    Handle for this instance of the DLL; same as the module handle.
 *    This handle allows us to get the DLL file name for logging.
 * \param[in] reason 
 *    The point in the DLL life cycle at which this call is made. Called "reason code" by Microsoft.
 * \param[in] reserved 
 *    No longer reserved, since it contains a point in the thread life cycle.
 *    We aren't using it, though.

 * \sa { http://msdn.microsoft.com/en-us/library/windows/desktop/ms682583%28v=vs.85%29.aspx }
 * Documentation on DLL entry points in Windows.
 */
extern "C" BOOL WINAPI DllMain(
  IN HINSTANCE module_handle,
  IN ULONG reason,
  IN LPVOID reserved )
{
  /*
   * Because this is an external API, we must ensure that there is a catch-all block for each execution path. There are two of these below.
   */
  switch ( reason )
  {
  case DLL_PROCESS_ATTACH:
    try
    {
      DLL_Module::attach( module_handle );
      return TRUE;
    }
    catch(...)
    {
      // We can't log to the installation log yet, and this couldn't shouldn't be executed except in rare cases such as out-of-memory.
      // Since it's a lot of code to do something useful (such as logging to the Windows system event log), we don't do anything but return a failure.
      return FALSE;
    }
    break;

  case DLL_PROCESS_DETACH:
    try
    {
      DLL_Module::detach();
      return TRUE;
    }
    catch(...)
    {
      // See comment above in parallel catch-block.
      return FALSE;
    }
    break;

  /*
   * This entry point is called for each thread after the first in a process with this DLL loaded. Note "after the first".
   * The process life cycle is always called, and we do our global initialization there.  So even though this DLL
   * doesn't support asynchronous operation, this entry point gets called anyway. We need to ignore these calls.
   */
  case DLL_THREAD_ATTACH:
  case DLL_THREAD_DETACH:
    return TRUE;

  default:
    return FALSE;
  }
}
