#include "PluginStdAfx.h"

#include "Plugin.h"
#if (defined PRODUCT_SIMPLEADBLOCK)
 #include "../AdBlocker/AdBlocker_i.c"
#elif (defined PRODUCT_DOWNLOADHELPER)
 #include "../DownloadHelper/DownloadHelper_i.c"
#endif

#include "PluginClass.h"
#include "PluginClient.h"
#include "PluginSystem.h"
#include "PluginSettings.h"
#include "PluginDictionary.h"
#ifdef SUPPORT_FILTER
 #include "PluginFilter.h"
#endif
#ifdef SUPPORT_CONFIG
 #include "PluginConfig.h"
#endif


CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
	OBJECT_ENTRY(CLSID_PluginClass, CPluginClass)
END_OBJECT_MAP()


class CPluginApp : public CWinApp
{

public:

	CPluginApp();

	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CPluginApp, CWinApp)	
END_MESSAGE_MAP()



CPluginApp theApp;

CPluginApp::CPluginApp()
{
}

BOOL CPluginApp::InitInstance()
{
	TCHAR szFilename[MAX_PATH];
	GetModuleFileName(NULL, szFilename, MAX_PATH);
	_tcslwr_s(szFilename);

	if (_tcsstr(szFilename, _T("explorer.exe")))
	{
		return FALSE;
	}

    _Module.Init(ObjectMap, AfxGetInstanceHandle(), &LIBID_PluginLib);

	CWinApp::InitInstance();

	return TRUE;
}


STDAPI DllCanUnloadNow(void)
{
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

	settings->SetMainProcessId();
    settings->EraseTab();

    settings->Remove(SETTING_PLUGIN_SELFTEST);
	settings->SetValue(SETTING_PLUGIN_INFO_PANEL, isInstall ? 1 : 2);
    settings->Write();

	if (isInstall)
	{
		::MessageBoxA(::GetDesktopWindow(), "OnInstall", "info", MB_OK);
		DEBUG_GENERAL(
			L"================================================================================\nINSTALLER " + 
			CString(IEPLUGIN_VERSION) + 
			L"\n================================================================================")
	}
	else
	{
		::MessageBoxA(::GetDesktopWindow(), "OnUpdate", "info", MB_OK);
		DEBUG_GENERAL(
			L"================================================================================\nUPDATER " + 
			CString(IEPLUGIN_VERSION) + L" (UPDATED FROM " + settings->GetString(SETTING_PLUGIN_VERSION) + L")"
			L"\n================================================================================")
	}

    // Create default filters
#ifdef SUPPORT_FILTER
    DEBUG_GENERAL(L"*** Generating default filters")
    CPluginFilter::CreateFilters();
#endif

    // Force creation of default dictionary
    CPluginDictionary* dictionary = CPluginDictionary::GetInstance(true);   
	dictionary->Create(true);

    // Force creation of default config file
#ifdef SUPPORT_CONFIG
    DEBUG_GENERAL("*** Generating config file")
    CPluginConfig* config = CPluginConfig::GetInstance();
	config->Create(true);
#endif

	HKEY hKey = NULL;
	DWORD dwDisposition = 0;

	DWORD dwResult = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\IE Download Helper", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, &dwDisposition);
	if (dwResult == ERROR_SUCCESS)
	{
		CString pluginId = system->GetPluginId();

		::RegSetValueEx(hKey, L"PluginId", 0, REG_SZ, (const BYTE*)pluginId.GetBuffer(), 2*(pluginId.GetLength() + 1));

		::RegCloseKey(hKey);
	}

	// Post async plugin error
    CPluginError pluginError;
    while (CPluginClientBase::PopFirstPluginError(pluginError))
    {
        CPluginClientBase::LogPluginError(pluginError.GetErrorCode(), pluginError.GetErrorId(), pluginError.GetErrorSubid(), pluginError.GetErrorDescription(), true, pluginError.GetProcessId(), pluginError.GetThreadId());
    }
}

// Called from installer
EXTERN_C void STDAPICALLTYPE OnInstall(void)
{
	InitPlugin(true);
}

// Called from updater
EXTERN_C void STDAPICALLTYPE OnUpdate(void)
{
	InitPlugin(false);
}
