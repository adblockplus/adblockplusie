/**
* \file DLL.h The DLL as a Windows system module.
*/

#ifndef DLL_H
#define DLL_H

#include <memory>
#include <string>

#include "windows.h"

/**
* Singleton representing the DLL module. This class is the source of the file name for the custom action library, used in logging.
* The choice to use a singleton reflects the design of the Windows API, which treats the module handle as a global for the DLL instance,
* only appearing during the calls that manage the lifetime of the DLL.
*/
class DllModule
{
public:
  /**
  * Accessor function for the singleton.
  */
  static DllModule & module();

  /**
  * Hook function to call on DLL attach.
  */
  static void Attach( HINSTANCE handle );

  /**
  * Hook function to call on DLL detach.
  */
  static void Detach();

  /**
  * Textual name of the DLL as an OS module.
  */
  std::wstring name();

private:
  /**
  * The singleton value.
  */
  static std::shared_ptr< DllModule > singleton;

  /**
  * Private constructor ensures use of accessor function only.
  */
  DllModule( HINSTANCE handle );

  /**
  * Windows handle for the instance of the DLL.
  */
  HINSTANCE handle;

  /**
  * The text name of the module.
  *
  * Implemented as a smart pointer for deferred evaluation of the system call to get the module name.
  */
  std::shared_ptr< std::wstring > _name;
};

#endif
