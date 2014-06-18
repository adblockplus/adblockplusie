#include "PluginStdAfx.h"

#include "Plugin.h"
#ifdef _WIN64
#include "../../build/x64/AdblockPlus_i.c"
#else
#include "../../build/ia32/AdblockPlus_i.c"
#endif

#include "PluginClass.h"
#include "PluginClient.h"
#include "PluginSystem.h"
#include "PluginSettings.h"
#include "PluginMimeFilterClient.h"
#include "Msiquery.h"

#ifdef SUPPORT_FILTER
#include "PluginFilter.h"
#endif
#ifdef SUPPORT_CONFIG
#include "PluginConfig.h"
#endif

#include "../shared/Dictionary.h"

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
    TCHAR szFilename[MAX_PATH];
    GetModuleFileName(NULL, szFilename, MAX_PATH);
    _tcslwr_s(szFilename);

    if (_tcsstr(szFilename, _T("explorer.exe")))
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


    if (CPluginSystem::s_instance != NULL)
    {
      delete CPluginSystem::s_instance;
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

void InitPlugin(bool isInstall)
{
  CPluginSystem* system = CPluginSystem::GetInstance();

  CPluginSettings* settings = CPluginSettings::GetInstance();

  if (isInstall)
  {
    DEBUG_GENERAL(
      L"================================================================================\nINSTALLER " +
      CString(IEPLUGIN_VERSION) +
      L"\n================================================================================")
  }
  else
  {
    DEBUG_GENERAL(
      L"================================================================================\nUPDATER " +
      CString(IEPLUGIN_VERSION) +
      L"\n================================================================================")
  }

  // Post async plugin error
  CPluginError pluginError;
  while (CPluginClientBase::PopFirstPluginError(pluginError))
  {
    CPluginClientBase::LogPluginError(pluginError.GetErrorCode(), pluginError.GetErrorId(), pluginError.GetErrorSubid(), pluginError.GetErrorDescription(), true, pluginError.GetProcessId(), pluginError.GetThreadId());
  }
}

// Called from installer
EXTERN_C void STDAPICALLTYPE OnInstall(MSIHANDLE hInstall, MSIHANDLE tmp)
{
  InitPlugin(true);
}

// Called from updater
EXTERN_C void STDAPICALLTYPE OnUpdate(void)
{
  InitPlugin(false);
}
