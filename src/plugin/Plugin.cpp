/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PluginStdAfx.h"

#include "Plugin.h"
#ifdef _WIN64
#include "../../build/x64/AdblockPlus_i.c"
#else
#include "../../build/ia32/AdblockPlus_i.c"
#endif

#include "PluginClass.h"
#include "PluginMimeFilterClient.h"
#include "PluginSettings.h"

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
  OBJECT_ENTRY(CLSID_PluginClass, CPluginClass)
END_OBJECT_MAP()

//Dll Entry Point
BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD fdwReason, LPVOID reserved)
{
  switch( fdwReason )
  {
  case DLL_PROCESS_ATTACH:
    wchar_t szFilename[MAX_PATH];
    GetModuleFileName(NULL, szFilename, MAX_PATH);
    _wcslwr_s(szFilename);

    if (wcsstr(szFilename, L"explorer.exe"))
    {
      return FALSE;
    }

    _Module.Init(ObjectMap, _Module.GetModuleInstance(), &LIBID_PluginLib);
    break;

  case DLL_THREAD_ATTACH:
    // thread-specific initialization.
    break;

  case DLL_THREAD_DETACH:
    // thread-specific cleanup.
    break;

  case DLL_PROCESS_DETACH:
    // any necessary cleanup.
    break;
  }

  return TRUE;
}


STDAPI DllCanUnloadNow(void)
{
  LONG count = _Module.GetLockCount();
  if (_Module.GetLockCount() == 0)
  {
    if (CPluginSettings::s_instance != NULL)
    {
      delete CPluginSettings::s_instance;
    }

    if (CPluginClass::s_mimeFilter != NULL)
    {
      CPluginClass::s_mimeFilter->Unregister();
      CPluginClass::s_mimeFilter = NULL;
    }

    _CrtDumpMemoryLeaks();
  }
  return (_Module.GetLockCount() == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
  return _Module.GetClassObject(rclsid, riid, ppv);
}

STDAPI DllRegisterServer(void)
{
  return _Module.RegisterServer(TRUE);
}

STDAPI DllUnregisterServer(void)
{
  return _Module.UnregisterServer(TRUE);
}

