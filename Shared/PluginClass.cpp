#include "PluginStdAfx.h"

#include "PluginClass.h"
#include "PluginDictionary.h"
#include "PluginSettings.h"
#include "PluginSystem.h"
#ifdef SUPPORT_FILTER
#include "PluginFilter.h"
#endif
#include "PluginMimeFilterClient.h"
#include "PluginClient.h"
#include "PluginClientFactory.h"
#include "PluginHttpRequest.h"
#include "PluginMutex.h"
#include "PluginProfiler.h"
#include "DownloadSource.h"
#include "sddl.h"

#ifdef PRODUCT_DOWNLOADHELPER
#include "..\DownloadHelper\SettingsDialog.h"
#include "pluginConfig.h"
#endif


#ifdef DEBUG_HIDE_EL
DWORD profileTime = 0;
#endif

typedef HANDLE (WINAPI *OPENTHEMEDATA)(HWND, LPCWSTR);
typedef HRESULT (WINAPI *DRAWTHEMEBACKGROUND)(HANDLE, HDC, INT, INT, LPRECT, LPRECT);
typedef HRESULT (WINAPI *CLOSETHEMEDATA)(HANDLE);


HICON CPluginClass::s_hIcons[ICON_MAX] = { NULL, NULL, NULL };
DWORD CPluginClass::s_hIconTypes[ICON_MAX] = { IDI_ICON_DISABLED, IDI_ICON_ENABLED, IDI_ICON_DEACTIVATED };

CPluginMimeFilterClient* CPluginClass::s_mimeFilter = NULL;

CLOSETHEMEDATA pfnClose = NULL;
DRAWTHEMEBACKGROUND pfnDrawThemeBackground = NULL; 
OPENTHEMEDATA pfnOpenThemeData = NULL;

ATOM CPluginClass::s_atomPaneClass = NULL;
HINSTANCE CPluginClass::s_hUxtheme = NULL;
CSimpleArray<CPluginClass*> CPluginClass::s_instances;
std::map<DWORD, CPluginClass*> CPluginClass::s_threadInstances;

CComAutoCriticalSection CPluginClass::s_criticalSectionLocal;
CComAutoCriticalSection CPluginClass::s_criticalSectionBrowser;
CComAutoCriticalSection CPluginClass::s_criticalSectionWindow;

CComQIPtr<IWebBrowser2> CPluginClass::s_asyncWebBrowser2;

#ifdef SUPPORT_WHITELIST
std::map<UINT,CString> CPluginClass::s_menuDomains;
#endif

#ifdef SUPPORT_FILE_DOWNLOAD
TMenuDownloadFiles CPluginClass::s_menuDownloadFiles;
#endif

bool CPluginClass::s_isPluginToBeUpdated = false;

CPluginTab* CPluginClass::s_activeTab = NULL;


CPluginClass::CPluginClass()
{
//	_CrtDumpMemoryLeaks();

    m_isAdviced = false;
    m_nConnectionID = 0;
    m_hTabWnd = NULL;
    m_hStatusBarWnd = NULL;
    m_hPaneWnd = NULL;
    m_nPaneWidth = 0;
    m_pWndProcStatus = NULL;
    m_hTheme = NULL;
	m_isInitializedOk = false;


	m_tab = new CPluginTab(this);

    // Load / create settings
    CPluginSettings* settings = CPluginSettings::GetInstance();

    CPluginSystem* system = CPluginSystem::GetInstance();

    bool isMainTab = settings->IncrementTabCount();

    if (isMainTab)
    {
        // Prepare settings
        settings->SetMainProcessId();
        settings->SetMainUiThreadId();

        // Ensure plugin version
        if (!settings->Has(SETTING_PLUGIN_VERSION))
        {
            settings->SetString(SETTING_PLUGIN_VERSION, IEPLUGIN_VERSION);
		    settings->SetFirstRunUpdate();
        }

        // First run or deleted settings file)
        if (!settings->Has(SETTING_PLUGIN_ID))
        {
            settings->SetString(SETTING_PLUGIN_ID, system->GetPluginId());
            settings->SetFirstRun();
        }
		
/*
#ifdef _DEBUG
settings->SetString(SETTING_PLUGIN_UPDATE_VERSION, "9.9.9");
settings->SetString(SETTING_PLUGIN_UPDATE_URL, "http://ie-downloadhelper.com/download/downloadhelper_update_test 2.1.msi");
#endif
*/
        // Update?
        CString oldVersion = settings->GetString(SETTING_PLUGIN_VERSION);
	    if (settings->IsFirstRunUpdate() || settings->GetString(SETTING_PLUGIN_UPDATE_VERSION) == IEPLUGIN_VERSION || oldVersion != IEPLUGIN_VERSION)
        {
            settings->SetString(SETTING_PLUGIN_VERSION, IEPLUGIN_VERSION);

            settings->Remove(SETTING_REG_DATE);
            settings->Remove(SETTING_PLUGIN_UPDATE_TIME);
            settings->Remove(SETTING_PLUGIN_UPDATE_VERSION);
            settings->Remove(SETTING_PLUGIN_UPDATE_URL);

		    settings->SetFirstRunUpdate();
	    }

        // Ensure max REGISTRATION_MAX_ATTEMPTS registration attempts today
        CString regDate = settings->GetString(SETTING_REG_DATE);

        SYSTEMTIME systemTime;
        ::GetSystemTime(&systemTime);

        CString today;
        today.Format(L"%d-%d-%d", systemTime.wYear, systemTime.wMonth, systemTime.wDay);
        
        if (regDate != today)
        {
            settings->SetString(SETTING_REG_DATE, today);
            settings->SetValue(SETTING_REG_ATTEMPTS, 0);
            settings->Remove(SETTING_REG_SUCCEEDED);
        }
        // Only allow one trial, if settings or whitelist changes
        else if (settings->GetForceConfigurationUpdateOnStart())
        {
            settings->SetValue(SETTING_REG_ATTEMPTS, REGISTRATION_MAX_ATTEMPTS - 1);
            settings->Remove(SETTING_REG_SUCCEEDED);

            settings->RemoveForceConfigurationUpdateOnStart();
        }

        int info = settings->GetValue(SETTING_PLUGIN_INFO_PANEL, 0);


		CString ffmpegLocation = settings->GetString(SETTING_FFMPEG, L"");
		if (ffmpegLocation.IsEmpty())
		{
			if (!settings->GetBool(SETTING_FFMPEG_CHECK, false))
			{
				LPTSTR  strDLLPath1 = new TCHAR[_MAX_PATH];
				::GetModuleFileName((HINSTANCE)&__ImageBase, strDLLPath1, _MAX_PATH);
				CString dllPath;
				dllPath.Format(L"%s", strDLLPath1);
				dllPath = dllPath.Left(dllPath.GetLength() - CString(L"DownloadHelper.dll").GetLength());
				CString ffmpegLocation = dllPath + CString("ffmpeg.exe");
				CFileStatus fileStatus;
				if (CFile::GetStatus(ffmpegLocation, fileStatus))
				{
					settings->SetString(SETTING_FFMPEG, ffmpegLocation);					
				}
				settings->SetBool(SETTING_FFMPEG_CHECK, true);
			}
		}

#ifdef ENABLE_DEBUG_RESULT
        CPluginDebug::DebugResultClear();
#endif

#ifdef ENABLE_DEBUG_INFO
        if (info == 0 || info > 2)
        {
            CPluginDebug::DebugClear();
        }
#endif // ENABLE_DEBUG_INFO

        settings->Write(false);
    }
}

CPluginClass::~CPluginClass()
{
	delete m_tab;

    CPluginSettings* settings = CPluginSettings::GetInstance();
    
    settings->DecrementTabCount();

//	if (s_instances.GetSize() <= 1)
//	{
//		CPluginClientFactory::ReleaseMimeFilterClientInstance();		
//	}

}


/////////////////////////////////////////////////////////////////////////////
// Initialization

HRESULT CPluginClass::FinalConstruct()
{
	return S_OK;
}

void CPluginClass::FinalRelease()
{
    s_criticalSectionBrowser.Lock();
    {
    	m_webBrowser2.Release();
	}
    s_criticalSectionBrowser.Unlock();
}


// This method tries to get a 'connection point' from the stored browser, which can be
// used to attach or detach from the stream of browser events
CComPtr<IConnectionPoint> CPluginClass::GetConnectionPoint()
{
	CComQIPtr<IConnectionPointContainer, &IID_IConnectionPointContainer> pContainer(GetBrowser());
	if (!pContainer)
	{
		return NULL;
	}

	CComPtr<IConnectionPoint> pPoint;
	HRESULT hr = pContainer->FindConnectionPoint(DIID_DWebBrowserEvents2, &pPoint);
	if (FAILED(hr))
	{
	    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_FIND_CONNECTION_POINT, "Class::GetConnectionPoint - FindConnectionPoint")
		return NULL;
	}

	return pPoint;
}

// This method tries to get a 'connection point' from the stored browser, which can be
// used to attach or detach from the stream of browser events
CComPtr<IConnectionPoint> CPluginClass::GetConnectionPointPropSink()
{
	CComQIPtr<IConnectionPointContainer, &IID_IConnectionPointContainer> pContainer(GetBrowser());
	if (!pContainer)
	{
		return NULL;
	}

	CComPtr<IConnectionPoint> pPoint;
	HRESULT hr = pContainer->FindConnectionPoint(IID_IPropertyNotifySink, &pPoint);
	if (FAILED(hr))
	{
	    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_FIND_CONNECTION_POINT, "Class::GetConnectionPoint - FindConnectionPoint")
		return NULL;
	}

	return pPoint;
}


HWND CPluginClass::GetBrowserHWND() const
{
	SHANDLE_PTR hBrowserWndHandle = NULL;

    CComQIPtr<IWebBrowser2> browser = GetBrowser();    
	if (browser)
	{
	    HRESULT hr = browser->get_HWND(&hBrowserWndHandle);
	    if (FAILED(hr))
	    {
	        DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_BROWSER_WINDOW, "Class::GetBrowserHWND - failed")
	    }
    }

	return (HWND)hBrowserWndHandle;
}


CComQIPtr<IWebBrowser2> CPluginClass::GetBrowser() const
{
    CComQIPtr<IWebBrowser2> browser;
    
    s_criticalSectionBrowser.Lock();
    {
        browser = m_webBrowser2;
    }
    s_criticalSectionBrowser.Unlock();
    
    return browser;
}


CComQIPtr<IWebBrowser2> CPluginClass::GetAsyncBrowser()
{
    CComQIPtr<IWebBrowser2> browser;
    
    s_criticalSectionLocal.Lock();
    {
        browser = s_asyncWebBrowser2;
    }
    s_criticalSectionLocal.Unlock();
    
    return browser;
}

CString CPluginClass::GetBrowserUrl() const
{
	CString url;

    CComQIPtr<IWebBrowser2> browser = GetBrowser();
	if (browser)
	{
	    CComBSTR bstrURL;

	    if (SUCCEEDED(browser->get_LocationURL(&bstrURL)))
	    {
		    url = bstrURL;
		    CPluginClient::UnescapeUrl(url);
	    }
    }
    else
	{
		url = m_tab->GetDocumentUrl();
	}

	return url;
}

void CPluginClass::LaunchUpdater(const CString& strPath)
{
	PROCESS_INFORMATION pi;
	::ZeroMemory(&pi, sizeof(pi));

	STARTUPINFO si;
	::ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.wShowWindow = FALSE;

	CString cpath = _T("\"msiexec.exe\" /i \"") + strPath + _T("\" UPDATEPLUGIN=\"True\"");

	if (!::CreateProcess(NULL, cpath.GetBuffer(), NULL, NULL, FALSE, CREATE_BREAKAWAY_FROM_JOB, NULL, NULL, &si, &pi))
	{
		DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UPDATER, PLUGIN_ERROR_UPDATER_CREATE_PROCESS, "Class::Updater - Failed to start process");
		return;
	}
#ifndef AUTOMATIC_SHUTDOWN
	else 
	{
		::WaitForSingleObject(pi.hProcess, INFINITE);
	}
#endif // not AUTOMATIC_SHUTDOWN

	::CloseHandle(pi.hProcess);
	::CloseHandle(pi.hThread);
}


// This gets called when a new browser window is created (which also triggers the
// creation of this object). The pointer passed in should be to a IWebBrowser2
// interface that represents the browser for the window. 
// it is also called when a tab is closed, this unknownSite will be null
// so we should handle that it is called this way several times during a session
STDMETHODIMP CPluginClass::SetSite(IUnknown* unknownSite)
{

    CPluginSettings* settings = CPluginSettings::GetInstance();

	if (unknownSite) 
	{
//		s_activeTab = m_tab;

        if (settings->IsMainProcess() && settings->IsMainUiThread())
        {
            DEBUG_GENERAL(L"================================================================================\nMAIN TAB UI\n================================================================================")
        }
        else
        {
            DEBUG_GENERAL(L"================================================================================\nNEW TAB UI\n================================================================================")
        }

		HRESULT hr = ::CoInitialize(NULL);
		if (FAILED(hr))
		{
		    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_COINIT, "Class::SetSite - CoInitialize");
		}

        s_criticalSectionBrowser.Lock();
        {
    		m_webBrowser2 = unknownSite;
        }
        s_criticalSectionBrowser.Unlock();

		//register the mimefilter 
		//and only mimefilter
		//on some few computers the mimefilter does not get properly registered when it is done on another thread

		s_criticalSectionLocal.Lock();
        {
			if (settings->GetPluginEnabled())
			{
				s_mimeFilter = CPluginClientFactory::GetMimeFilterClientInstance();
			}

			s_asyncWebBrowser2 = unknownSite;
		    s_instances.Add(this);
	    }
        s_criticalSectionLocal.Unlock();

		try 
		{
			// Check if loaded as BHO
			// niels question - what is the difference on these two modes
			if (GetBrowser())
			{
				CComPtr<IConnectionPoint> pPoint = GetConnectionPoint();
				if (pPoint)
				{
					HRESULT hr = pPoint->Advise((IDispatch*)this, &m_nConnectionID);
					if (SUCCEEDED(hr))
					{
						m_isAdviced = true;

						//TODO: Lets try and load the status bar only when it's first opened

						if (!InitObject(true))
						{
							Unadvice();
						}
						if ((m_hPaneWnd == NULL) || (!IsStatusBarEnabled()))
						{
							ShowStatusBar();
						}
					}
					else
					{
            		    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_ADVICE, "Class::SetSite - Advice");
					}
				}
			}
			else // Check if loaded as toolbar handler
			{
				CComPtr<IServiceProvider> pServiceProvider;
				
				HRESULT hr = unknownSite->QueryInterface(&pServiceProvider);
				if (SUCCEEDED(hr))
				{
				    if (pServiceProvider)
				    {
                        s_criticalSectionBrowser.Lock();
                        {
					        HRESULT hr = pServiceProvider->QueryService(IID_IWebBrowserApp, &m_webBrowser2);
					        if (SUCCEEDED(hr))
					        {
					            if (m_webBrowser2)
					            {
    						        InitObject(false);
						        }
					        }
					        else
					        {
                    		    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_QUERY_BROWSER, "Class::SetSite - QueryService (IID_IWebBrowserApp)");
					        }
                        }
                        s_criticalSectionBrowser.Unlock();
                    }
				}
		        else
		        {
    		        DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_QUERY_SERVICE_PROVIDER, "Class::SetSite - QueryInterface (service provider)");
		        }
			}
		}
		catch (std::runtime_error e) 
		{
			Unadvice();
		}
	}
	else 
	{
		// Unadvice
		Unadvice();

		// Destroy window
		if (m_pWndProcStatus)
		{
			::SetWindowLongPtr(m_hStatusBarWnd, GWLP_WNDPROC, (LPARAM)(WNDPROC)m_pWndProcStatus);

			m_pWndProcStatus = NULL;
		}

		if (m_hPaneWnd)
		{
			DestroyWindow(m_hPaneWnd);
			m_hPaneWnd = NULL;
		}

		m_hTabWnd = NULL;
		m_hStatusBarWnd = NULL;

		// Remove instance from the list, shutdown threads
		HANDLE hMainThread = NULL;
		HANDLE hTabThread = NULL;

		s_criticalSectionLocal.Lock();
		{
		    s_instances.Remove(this);

		    if (s_instances.GetSize() == 0)
		    {
				if (settings->IsMainProcess() && settings->IsMainUiThread())
		        {
			        hMainThread = s_hMainThread;
			        s_hMainThread = NULL;
		        }
		    }

			std::map<DWORD,CPluginClass*>::iterator it = s_threadInstances.find(::GetCurrentThreadId());
			if (it != s_threadInstances.end())
			{
				s_threadInstances.erase(it);
			}
			if (s_instances.GetSize() == 0)
			{
				CPluginClientFactory::ReleaseMimeFilterClientInstance();
			}
		}
		s_criticalSectionLocal.Unlock();
 
		if (hMainThread != NULL)
		{
		    s_isMainThreadDone = true;

			::WaitForSingleObject(hMainThread, INFINITE);
			::CloseHandle(hMainThread);
		}

		// Release browser interface
        s_criticalSectionBrowser.Lock();
        {
    		m_webBrowser2.Release();
		}
        s_criticalSectionBrowser.Unlock();

        if (settings->IsMainProcess() && settings->IsMainUiThread())
        {
            DEBUG_GENERAL("================================================================================\nMAIN TAB UI - END\n================================================================================")
        }
        else
        {
            DEBUG_GENERAL("================================================================================\nNEW TAB UI - END\n================================================================================")
        }

		::CoUninitialize();
	}

	return IObjectWithSiteImpl<CPluginClass>::SetSite(unknownSite);
}

bool CPluginClass::IsStatusBarEnabled()
{
	HKEY pHkey;
	HKEY pHkeySub;
	RegOpenCurrentUser(KEY_QUERY_VALUE, &pHkey);
	DWORD trueth = 1;
	DWORD truethSize = sizeof(DWORD);
	RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\Main", &pHkeySub);
	LONG res = RegQueryValueEx(pHkeySub, L"StatusBarWeb", NULL, NULL, (BYTE*)&trueth, &truethSize);
	RegCloseKey(pHkey);
	if (res != ERROR_SUCCESS)
	{
		RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\MINIE", &pHkeySub);
		LONG res = RegQueryValueEx(pHkeySub, L"ShowStatusBar", NULL, NULL, (BYTE*)&trueth, &truethSize);
		RegCloseKey(pHkey);
	}
	return trueth == 1;
}

void CPluginClass::ShowStatusBar()
{
	VARIANT_BOOL isVisible;

    CPluginSettings* settings = CPluginSettings::GetInstance();

	CComQIPtr<IWebBrowser2> browser = GetAsyncBrowser();
	if (browser)
	{
		HRESULT hr = S_OK; 
	
		hr = browser->get_StatusBar(&isVisible);
		if (SUCCEEDED(hr))
		{
			if (!isVisible)
			{
				if (!settings->GetBool("statusbarasked", false))
				{
					SHANDLE_PTR pBrowserHWnd;
					browser->get_HWND((SHANDLE_PTR*)&pBrowserHWnd);
					LRESULT res = MessageBox((HWND)pBrowserHWnd, L"The plugin menu is located in the statusbar, would you like to enable Internet Explorer's statusbar?", L"Enable status bar?", MB_YESNO);
					settings->SetBool("statusbarasked", true);
					settings->Write();
					if (res == IDYES)
					{
						HKEY pHkey;
						HKEY pHkeySub;
						RegOpenCurrentUser(KEY_WRITE, &pHkey);
						DWORD trueth = 1;
						RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\MINIE", &pHkeySub);
						RegSetValueEx(pHkeySub, L"ShowStatusBar", 0, REG_DWORD, (BYTE*)&trueth, sizeof(DWORD));
						RegCloseKey(pHkeySub);
						RegOpenKey(pHkey, L"Software\\Microsoft\\Internet Explorer\\Main", &pHkeySub);
						RegSetValueEx(pHkeySub, L"StatusBarWeb", 0, REG_DWORD, (BYTE*)&trueth, sizeof(DWORD));
						RegCloseKey(pHkeySub);
						hr = browser->put_StatusBar(TRUE);
						if (FAILED(hr))
						{
							DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_PUT_STATUSBAR, "Class::Enable statusbar");
						}
						CreateStatusBarPane();
					}
				}
			}
		}
		else 
		{
			DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_STATUSBAR, "Class::Get statusbar state");
		}
	}
}

void CPluginClass::DisplayActivateMessage()
{
	CPluginSettings* settings = CPluginSettings::GetInstance();
	
	CString messageString;
	messageString.Format(L"The daily adblocklimit has been reached and no-more ads are blocked today.\nThe free version of Simple Adblock only blocks %d adrequests a day.\n\nTo enjoy unlimited adblocking please upgrade.", settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0));

	//Adblockcount=1000000 when Activationmessage has been displayed
	settings->SetValue(SETTING_PLUGIN_ADBLOCKCOUNT, settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0) + 1);
	settings->Write();

	LRESULT res = MessageBox(NULL, messageString, L"Upgrade to Simple Adblock Pro", MB_OKCANCEL);
	if (res == IDOK)
	{
		CPluginSettings* settings = CPluginSettings::GetInstance();
		CPluginHttpRequest httpRequest(USERS_SCRIPT_UPGRADE);
		CPluginSystem* system = CPluginSystem::GetInstance();
		httpRequest.Add(L"plugin", system->GetPluginId());
		httpRequest.Add(L"user", settings->GetString(SETTING_USER_ID));
		httpRequest.Add(L"version", settings->GetString(SETTING_PLUGIN_VERSION));
		CString url = httpRequest.GetUrl();

		CPluginTab* tab = CPluginClass::GetTab(::GetCurrentThreadId());							
		CComQIPtr<IWebBrowser2> browser = tab->m_plugin->GetBrowser();    
		if (!url.IsEmpty() && browser)
		{
			VARIANT vFlags;
			vFlags.vt = VT_I4;
			vFlags.intVal = navOpenInNewTab;

			HRESULT hr = browser->Navigate(CComBSTR(url), &vFlags, NULL, NULL, NULL);
			if (FAILED(hr))
			{
				vFlags.intVal = navOpenInNewWindow;

				hr = browser->Navigate(CComBSTR(url), &vFlags, NULL, NULL, NULL);
			}
		}
	}
}
void CPluginClass::BeforeNavigate2(DISPPARAMS* pDispParams)
{

	if (pDispParams->cArgs < 7)
	{
    	return; 
	}
	CPluginSettings* settings = CPluginSettings::GetInstance();
#ifndef PRODUCT_DOWNLOADHELPER
	//Reset adblockcount every day
	SYSTEMTIME stNow;
	GetSystemTime(&stNow);
	WORD limitDay = settings->GetValue(SETTING_PLUGIN_LIMITDAY, 0);
	if (limitDay != stNow.wDay)
	{
		settings->SetValue(SETTING_PLUGIN_ADBLOCKCOUNT, 0);
		settings->SetValue(SETTING_PLUGIN_LIMITDAY, stNow.wDay);
		settings->Write();
		settings->Read();

		//Also register a mime filter if it's not registered yet
		if (s_mimeFilter == NULL)
		{
			s_mimeFilter = CPluginClientFactory::GetMimeFilterClientInstance();
		}

	}

	if (!settings->GetBool(SETTING_PLUGIN_REGISTRATION, false))
	{
		if ((settings->GetValue(SETTING_PLUGIN_ADBLOCKCOUNT, 0) == settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0)) 
			&& (settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0) > 0))
		{
			DisplayActivateMessage();
		}
	}
#endif

	// Get the IWebBrowser2 interface
	CComQIPtr<IWebBrowser2, &IID_IWebBrowser2> WebBrowser2Ptr;
	VARTYPE vt = pDispParams->rgvarg[6].vt; 
	if (vt == VT_DISPATCH)
    {
	    WebBrowser2Ptr = pDispParams->rgvarg[6].pdispVal;
	}
	else 
	{
		// Wrong type, return.
		return;
	}

	// Get the URL
	CString url;
	vt = pDispParams->rgvarg[5].vt; 
	if (vt == VT_BYREF + VT_VARIANT)
	{
        url = pDispParams->rgvarg[5].pvarVal->bstrVal;

        CPluginClient::UnescapeUrl(url);
	}
	else
	{
		// Wrong type, return.
		return;
	}

    // If webbrowser2 is equal to top level browser (as set in SetSite), we are navigating new page
	CPluginClient* client = CPluginClient::GetInstance();

	if (url.Find(L"javascript") == 0)
	{
	}
	else if (GetBrowser().IsEqualObject(WebBrowser2Ptr))
	{
		m_tab->OnNavigate(url);

        DEBUG_GENERAL(L"================================================================================\nBegin main navigation url:" + url + "\n================================================================================")

#ifdef ENABLE_DEBUG_RESULT
        CPluginDebug::DebugResultDomain(url);
#endif
	
		UpdateStatusBar();
	}
	else
	{
		DEBUG_NAVI(L"Navi::Begin navigation url:" + url)

#ifdef SUPPORT_FRAME_CACHING
        m_tab->CacheFrame(url);
#endif
	}
}
STDMETHODIMP CPluginClass::OnTabChanged(DISPPARAMS* pDispParams, WORD wFlags)
{
	bool newtabshown = pDispParams->rgvarg[1].intVal==3;
	if (newtabshown)
	{
		std::map<DWORD,CPluginClass*>::const_iterator it = s_threadInstances.find(GetCurrentThreadId());
		if (it == s_threadInstances.end())
		{
			s_threadInstances[::GetCurrentThreadId()] = this;
			
			
			if (!m_isInitializedOk)
			{
				m_isInitializedOk = true;
				if (!InitObject(true))
				{
//					Unadvice();
				}
				UpdateStatusBar();
			}
		}
	}
	return VARIANT_TRUE;
}

// This gets called whenever there's a browser event
STDMETHODIMP CPluginClass::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pvarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
	switch (dispidMember)
	{

	case DISPID_WINDOWSTATECHANGED:
		return OnTabChanged(pDispParams, wFlags);
		break;
	case DISPID_HTMLDOCUMENTEVENTS2_ONBEFOREUPDATE:
		return VARIANT_TRUE;
		break;

	case DISPID_HTMLDOCUMENTEVENTS2_ONCLICK:
		return VARIANT_TRUE;
		break;

	case DISPID_EVMETH_ONLOAD:
		DEBUG_NAVI("Navi::OnLoad")
		return VARIANT_TRUE;
		break;

	case DISPID_EVMETH_ONCHANGE:
		return VARIANT_TRUE;

	case DISPID_EVMETH_ONMOUSEDOWN:
		return VARIANT_TRUE;

	case DISPID_EVMETH_ONMOUSEENTER:
		return VARIANT_TRUE;

	case DISPID_IHTMLIMGELEMENT_START:
		return VARIANT_TRUE;

	case STDDISPID_XOBJ_ERRORUPDATE:
		return VARIANT_TRUE;

	case STDDISPID_XOBJ_ONPROPERTYCHANGE:
		return VARIANT_TRUE;

	case DISPID_READYSTATECHANGE:
		DEBUG_NAVI("Navi::ReadyStateChange")
		return VARIANT_TRUE;

	case DISPID_BEFORENAVIGATE:
		DEBUG_NAVI("Navi::BeforeNavigate")
		return VARIANT_TRUE;
    case DISPID_COMMANDSTATECHANGE:
		if (m_hPaneWnd == NULL)
		{
			CreateStatusBarPane();
		}
		break;    

    case DISPID_STATUSTEXTCHANGE:
		break;

	case DISPID_BEFORENAVIGATE2:
		BeforeNavigate2(pDispParams);
		break;

	case DISPID_DOWNLOADBEGIN:
		{
			DEBUG_NAVI("Navi::Download Begin")
		}
		break;

	case DISPID_DOWNLOADCOMPLETE:
		{
			DEBUG_NAVI("Navi::Download Complete")

            CComQIPtr<IWebBrowser2> browser = GetBrowser();
			if (browser)
			{
				m_tab->OnDownloadComplete(browser);
			}
		}
		break;

	case DISPID_DOCUMENTCOMPLETE:
		{
			DEBUG_NAVI("Navi::Document Complete")

            CComQIPtr<IWebBrowser2> browser = GetBrowser();

			if (browser && pDispParams->cArgs >= 2 && pDispParams->rgvarg[1].vt == VT_DISPATCH)
			{
				CComQIPtr<IWebBrowser2> pBrowser = pDispParams->rgvarg[1].pdispVal;
				if (pBrowser)
				{
    				CString url;
					CComBSTR bstrUrl;
					if (SUCCEEDED(pBrowser->get_LocationURL(&bstrUrl)) && ::SysStringLen(bstrUrl) > 0)
					{
						url = bstrUrl;
						CPluginClient::UnescapeUrl(url);

						m_tab->OnDocumentComplete(browser, url, browser.IsEqualObject(pBrowser));
					}
				}
			}
		}
		break;

	case DISPID_ONQUIT:
	case DISPID_QUIT:
		{
		    Unadvice();
		}
		break;

	default:
        {
            CString did;
            did.Format(L"DispId:%u", dispidMember);
            
            DEBUG_NAVI(L"Navi::Default " + did)
        }

		// do nothing
		break;
	}

	return VARIANT_TRUE;
}

bool CPluginClass::InitObject(bool bBHO)
{
	// Load theme module
	s_criticalSectionLocal.Lock();
	{
	    if (!s_hUxtheme)
	    {
		    s_hUxtheme = ::GetModuleHandle(_T("uxtheme.dll"));
		    if (s_hUxtheme)
		    {
			    pfnClose = (CLOSETHEMEDATA)::GetProcAddress(s_hUxtheme, "CloseThemeData");
			    if (!pfnClose)
			    {
    		        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_UXTHEME_CLOSE, "Class::InitObject - GetProcAddress(CloseThemeData)");
			    }

			    pfnDrawThemeBackground = (DRAWTHEMEBACKGROUND)::GetProcAddress(s_hUxtheme, "DrawThemeBackground"); 
			    if (!pfnDrawThemeBackground)
			    {
    		        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_UXTHEME_DRAW_BACKGROUND, "Class::InitObject - GetProcAddress(DrawThemeBackground)");
			    }

			    pfnOpenThemeData = (OPENTHEMEDATA)::GetProcAddress(s_hUxtheme, "OpenThemeData");
			    if (!pfnOpenThemeData)
			    {
    		        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_UXTHEME_OPEN, "Class::InitObject - GetProcAddress(pfnOpenThemeData)");
			    }
		    }
		    else
		    {
		        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_UXTHEME, "Class::InitObject - GetModuleHandle(uxtheme.dll)");
		    }
	    }
    }
    s_criticalSectionLocal.Unlock();

	// Register pane class
	if (!GetAtomPaneClass())
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX); 
		wcex.style = 0;
		wcex.lpfnWndProc = (WNDPROC)PaneWindowProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = _Module.m_hInst;
		wcex.hIcon = NULL;
		wcex.hCursor = NULL;
		wcex.hbrBackground = NULL;
		wcex.lpszMenuName = NULL;
		wcex.lpszClassName = _T(STATUSBAR_PANE_NAME);
		wcex.hIconSm = NULL;

	    s_criticalSectionLocal.Lock();
	    {        
		    s_atomPaneClass = ::RegisterClassEx(&wcex);
	    }
	    s_criticalSectionLocal.Unlock();

        if (!GetAtomPaneClass())
        {
	        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_REGISTER_PANE_CLASS, "Class::InitObject - RegisterClassEx");
		    return false;
        }
	}

	// Create status pane
	if (bBHO)
	{
		if (!CreateStatusBarPane())
		{
			return false;
		}
	}

    CPluginSettings* settings = CPluginSettings::GetInstance();

	// Create main thread
    if (GetMainThreadHandle() == NULL && settings->IsMainProcess() && settings->IsMainUiThread())
    {
		DWORD id;
        HANDLE handle = ::CreateThread(NULL, 0, MainThreadProc, (LPVOID)m_tab, CREATE_SUSPENDED, &id);
		if (handle == NULL)
		{
			DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_THREAD, PLUGIN_ERROR_MAIN_THREAD_CREATE_PROCESS, "Class::Thread - Failed to create main thread");
		}

        s_hMainThread = handle;

	    ::ResumeThread(handle);
    }

	return true;
}

bool CPluginClass::CreateStatusBarPane()
{
	TCHAR szClassName[MAX_PATH];

	// Get browser window and url
	HWND hBrowserWnd = GetBrowserHWND();
	if (!hBrowserWnd)
	{
        DEBUG_ERROR_LOG(0, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_NO_STATUSBAR_BROWSER, "Class::CreateStatusBarPane - No status bar")
		return false;
	}

	// Looking for a TabWindowClass window in IE7
	// the last one should be parent for statusbar
	HWND hWndStatusBar = NULL;

	HWND hTabWnd = ::GetWindow(hBrowserWnd, GW_CHILD);
	UINT amoundOfNewTabs = 0;
	HWND uniqueNewTab = NULL;
	while (hTabWnd)
	{
		memset(szClassName, 0, MAX_PATH);
		GetClassName(hTabWnd, szClassName, MAX_PATH);

		if (_tcscmp(szClassName, _T("TabWindowClass")) == 0 || _tcscmp(szClassName,_T("Frame Tab")) == 0)
		{
			// IE8 support
			HWND hTabWnd2 = hTabWnd;
			if (_tcscmp(szClassName,_T("Frame Tab")) == 0)
			{
				hTabWnd2 = ::FindWindowEx(hTabWnd2, NULL, _T("TabWindowClass"), NULL);
			}

			if (hTabWnd2)
			{
				DWORD nProcessId;
				::GetWindowThreadProcessId(hTabWnd2, &nProcessId);
				if (::GetCurrentProcessId() == nProcessId)
				{
					bool bExistingTab = false;

                    s_criticalSectionLocal.Lock();
                    {
					    for (int i = 0; i < s_instances.GetSize(); i++)
					    {
						    if (s_instances[i]->m_hTabWnd == hTabWnd2) 
						    {

									bExistingTab = true;
									break;
						    }
					    }
                    }
                    s_criticalSectionLocal.Unlock();

					if (!bExistingTab)
					{
						amoundOfNewTabs ++;
						uniqueNewTab = hTabWnd2;
						if (GetCurrentThreadId() == GetWindowThreadProcessId(hTabWnd2, NULL))
						{
							hBrowserWnd = hTabWnd = hTabWnd2;
							break;
						}

					}
				}
			}
		}

		hTabWnd = ::GetWindow(hTabWnd, GW_HWNDNEXT);
	}


	HWND hWnd = ::GetWindow(hBrowserWnd, GW_CHILD);
	while (hWnd)
	{
		memset(szClassName, 0, MAX_PATH);
		::GetClassName(hWnd, szClassName, MAX_PATH);

		if (_tcscmp(szClassName,_T("msctls_statusbar32")) == 0)
		{
			hWndStatusBar = hWnd;
			break;
		}

		hWnd = ::GetWindow(hWnd, GW_HWNDNEXT);
	}

	if (!hWndStatusBar)
	{
        DEBUG_ERROR_LOG(0, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_NO_STATUSBAR_WIN, "Class::CreateStatusBarPane - No status bar")
		return true;
	}

	// Calculate pane height
	CRect rcStatusBar;
	::GetClientRect(hWndStatusBar, &rcStatusBar);

	if (rcStatusBar.Height() > 0)
	{
#ifdef _DEBUG
		m_nPaneWidth = 70;
#else
		m_nPaneWidth = min(rcStatusBar.Height(), 22);
#endif
	}
	else
	{
#ifdef _DEBUG
		m_nPaneWidth = 70;
#else
		m_nPaneWidth = 22;
#endif
	}

/*
	// ****TEST
	HWND hWndTest = ::GetWindow(GetParent(GetParent(hBrowserWnd)), GW_CHILD);
	HWND hWndToolbar = NULL;
	TCHAR szTitle[MAX_PATH];
	while (hWndTest)
	{
		memset(szClassName, 0, MAX_PATH);
		memset(szTitle, 0, MAX_PATH);
		::GetClassName(hWndTest, szClassName, MAX_PATH);
		::GetWindowText(hWndTest, szTitle, MAX_PATH);
		if (_tcscmp(szClassName,_T("WorkerW")) == 0)
		{
			hWndTest = ::GetWindow(hWndTest, GW_CHILD);

			while (hWndTest)
			{
				memset(szClassName, 0, MAX_PATH);
				memset(szTitle, 0, MAX_PATH);
				::GetClassName(hWndTest, szClassName, MAX_PATH);
				::GetWindowText(hWndTest, szTitle, MAX_PATH);
				if (_tcscmp(szClassName,_T("ReBarWindow32")) == 0)
				{
					hWndTest = ::GetWindow(hWndTest, GW_CHILD);

					while (hWndTest)
					{
						memset(szClassName, 0, MAX_PATH);
						memset(szTitle, 0, MAX_PATH);
						::GetClassName(hWndTest, szClassName, MAX_PATH);
						::GetWindowText(hWndTest, szTitle, MAX_PATH);
						if (_tcscmp(szClassName,_T("TabBandClass")) == 0)
						{
							WINDOWPLACEMENT wndpl;
							memset(&wndpl, 0, sizeof(WINDOWPLACEMENT));
							GetWindowPlacement(hWndTest, &wndpl);
							wndpl.rcNormalPosition.right = wndpl.rcNormalPosition.right - 70;
							SetWindowPlacement(hWndTest, &wndpl);
						}
						if (_tcscmp(szClassName,_T("ControlBandClass")) == 0)
						{
							WINDOWPLACEMENT wndpl;
							memset(&wndpl, 0, sizeof(WINDOWPLACEMENT));
							GetWindowPlacement(hWndTest, &wndpl);
							wndpl.rcNormalPosition.left = wndpl.rcNormalPosition.left - 70;
							SetWindowPlacement(hWndTest, &wndpl);

							hWndToolbar = hWndTest;
						}
						hWndTest = ::GetWindow(hWndTest, GW_HWNDNEXT);
					}
				}
				hWndTest = ::GetWindow(hWndTest, GW_HWNDNEXT);
			}

			break;
		}

		hWndTest = ::GetWindow(hWndTest, GW_HWNDNEXT);
	}

	// Calculate pane height
	CRect rcToolBar;
	::GetClientRect(hWndToolbar, &rcToolBar);
	
	WINDOWPLACEMENT wndpl;
	memset(&wndpl, 0, sizeof(WINDOWPLACEMENT));
	GetWindowPlacement(hWndToolbar, &wndpl);
//	wndpl.rcNormalPosition.left = wndpl.rcNormalPosition.left - 70;
	wndpl.rcNormalPosition.right = wndpl.rcNormalPosition.right + 70;

	SetWindowPlacement(hWndToolbar, &wndpl);

	// Create pane window
	HWND hWndNewPane2 = ::CreateWindowEx(
		NULL,
		MAKEINTATOM(GetAtomPaneClass()),
		_T(""),
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		rcToolBar.Width() - 70,0,70,rcToolBar.Height(),
		hWndToolbar,
		(HMENU)3671,
		_Module.m_hInst,
		NULL);


	// *** END TEST
*/

	// Create pane window
	HWND hWndNewPane = ::CreateWindowEx(
		NULL,
		MAKEINTATOM(GetAtomPaneClass()),
		_T(""),
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		rcStatusBar.Width() - 200,0,m_nPaneWidth,rcStatusBar.Height(),
		hWndStatusBar,
		(HMENU)3671,
		_Module.m_hInst,
		NULL);

	if (!hWndNewPane)
	{
        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_CREATE_STATUSBAR_PANE, "Class::CreateStatusBarPane - CreateWindowEx")
		return false;
	}

	m_hTabWnd = hTabWnd;
	m_hStatusBarWnd = hWndStatusBar;
	m_hPaneWnd = hWndNewPane;

	UpdateTheme();

	// Subclass status bar
	m_pWndProcStatus = (WNDPROC)SetWindowLongPtr(hWndStatusBar, GWLP_WNDPROC, (LPARAM)(WNDPROC)NewStatusProc);

	// Adjust pane
	UINT nPartCount = ::SendMessage(m_hStatusBarWnd, SB_GETPARTS, 0, 0);

	if (nPartCount > 1)
	{
		INT *pData = new INT[nPartCount];

		::SendMessage(m_hStatusBarWnd, SB_GETPARTS, nPartCount, (LPARAM)pData);
		::SendMessage(m_hStatusBarWnd, SB_SETPARTS, nPartCount, (LPARAM)pData);

		delete[] pData;
	}  
	HDC hdc = GetWindowDC(m_hStatusBarWnd);
	SendMessage(m_hStatusBarWnd, WM_PAINT, (WPARAM)hdc, 0);
	ReleaseDC(m_hStatusBarWnd, hdc);
	return true;
}/////////////////////////////////////////////////////////////////////////////
// Implementation

void CPluginClass::CloseTheme()
{
	if (m_hTheme)
	{
		if (pfnClose)
		{
			pfnClose(m_hTheme);
		}

		m_hTheme = NULL;
	}
}

void CPluginClass::UpdateTheme()
{
	CloseTheme();		

	if (pfnOpenThemeData)
	{
		m_hTheme = pfnOpenThemeData(m_hPaneWnd, L"STATUS");
		if (!m_hTheme)
		{
		}
	}
}


CPluginClass* CPluginClass::FindInstance(HWND hStatusBarWnd)
{
    CPluginClass* instance = NULL;

    s_criticalSectionLocal.Lock();
    {
	    for (int i = 0; i < s_instances.GetSize(); i++)
	    {
		    if (s_instances[i]->m_hStatusBarWnd == hStatusBarWnd)
		    {
			    instance = s_instances[i];
			    break;
		    }
	    }
    }
    s_criticalSectionLocal.Unlock();

	return instance;
}

CPluginTab* CPluginClass::GetTab()
{
	return m_tab;
}

CPluginTab* CPluginClass::GetTab(DWORD dwThreadId)
{
    CPluginTab* tab = NULL;

    s_criticalSectionLocal.Lock();
    {
		std::map<DWORD,CPluginClass*>::const_iterator it = s_threadInstances.find(dwThreadId);
		if (it != s_threadInstances.end())
		{
			tab = it->second->m_tab;
		}
    }
    s_criticalSectionLocal.Unlock();

	return tab;
}


STDMETHODIMP CPluginClass::QueryStatus(const GUID* pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT* pCmdText)
{
	if (cCmds == 0) return E_INVALIDARG;
	if (prgCmds == 0) return E_POINTER;

	prgCmds[0].cmdf = OLECMDF_ENABLED;

	return S_OK;
}

HMENU CPluginClass::CreatePluginMenu(const CString& url)
{
	HINSTANCE hInstance = _AtlBaseModule.GetModuleInstance();

	HMENU hMenu = ::LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1));

	HMENU hMenuTrackPopup = GetSubMenu(hMenu, 0);

	SetMenuBar(hMenuTrackPopup, url);

	return hMenuTrackPopup;
}
BOOL CreateLowProcess(WCHAR* wszProcessName, WCHAR* cmdLine)
{
 
    BOOL                  fRet;
    HANDLE                hToken        = NULL;
    HANDLE                hNewToken     = NULL;
    PSID                  pIntegritySid = NULL;
    TOKEN_MANDATORY_LABEL TIL           = {0};
    PROCESS_INFORMATION   ProcInfo      = {0};
    STARTUPINFO           StartupInfo   = {0};



 // Low integrity SID
 WCHAR wszIntegritySid[20] = L"S-1-16-4096";


    fRet = OpenProcessToken(GetCurrentProcess(),
                            TOKEN_DUPLICATE |
                              TOKEN_ADJUST_DEFAULT |
                              TOKEN_QUERY |
                              TOKEN_ASSIGN_PRIMARY,
                            &hToken);

    if (!fRet)
    {
        goto CleanExit;
    }

    fRet = DuplicateTokenEx(hToken,
                            0,
                            NULL,
                            SecurityImpersonation,
                            TokenPrimary,
                            &hNewToken);

    if (!fRet)
    {
        goto CleanExit;
    }

    fRet = ConvertStringSidToSid(wszIntegritySid, &pIntegritySid);

    if (!fRet)
    {
        goto CleanExit;
    }


    TIL.Label.Attributes = SE_GROUP_INTEGRITY;
    TIL.Label.Sid        = pIntegritySid;


    //
    // Set the process integrity level
    //

    fRet = SetTokenInformation(hNewToken,
                               TokenIntegrityLevel,
                               &TIL,
                               sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(pIntegritySid));

    if (!fRet)
    {
        goto CleanExit;
    }

    //
    // Create the new process at Low integrity
    //

    fRet  = CreateProcessAsUser(hNewToken,
                                wszProcessName,
                                cmdLine,
                                NULL,
                                NULL,
                                FALSE,
                                0,
                                NULL,
                                NULL,
                                &StartupInfo,
                                &ProcInfo);


CleanExit:

    if (ProcInfo.hProcess != NULL)
    {
        CloseHandle(ProcInfo.hProcess);
    }

    if (ProcInfo.hThread != NULL)
    {
        CloseHandle(ProcInfo.hThread);
    }

    LocalFree(pIntegritySid);

    if (hNewToken != NULL)
    {
        CloseHandle(hNewToken);
    }

    if (hToken != NULL)
    {
        CloseHandle(hToken);
    }

    return fRet;
}

void CPluginClass::DisplayPluginMenu(HMENU hMenu, int nToolbarCmdID, POINT pt, UINT nMenuFlags)
{
	CPluginClient* client = CPluginClient::GetInstance();

	CPluginSystem* system = CPluginSystem::GetInstance();

	CString url;
	int navigationErrorId = 0;

	// Create menu parent window
	HWND hMenuWnd = ::CreateWindowEx(
		NULL,
		MAKEINTATOM(GetAtomPaneClass()),
		_T(""),
		0,
		0,0,0,0,
		NULL,
		NULL,
		_Module.m_hInst,
		NULL);

	if (!hMenuWnd)
	{
		DestroyMenu(hMenu);    
		return;
	}

	// Display menu
	nMenuFlags |= TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTBUTTON;

	int nCommand = ::TrackPopupMenu(hMenu, nMenuFlags, pt.x, pt.y, 0, hMenuWnd, 0);

	::DestroyMenu(hMenu);    
	::DestroyWindow(hMenuWnd);

	switch (nCommand)
	{
	case ID_PLUGIN_UPDATE:
        {
            s_isPluginToBeUpdated = true;
		}
        break;

	case ID_PLUGIN_ACTIVATE:
		{
			url = CPluginHttpRequest::GetStandardUrl(USERS_SCRIPT_ACTIVATE);
			navigationErrorId = PLUGIN_ERROR_NAVIGATION_ACTIVATE;
		}        
		break;

	case ID_PLUGIN_ENABLE:
		{
			CPluginSettings* settings = CPluginSettings::GetInstance();
#ifndef PRODUCT_DOWNLOADHELPER
			//Display activation menu if enabling expired plugin
			if (!settings->GetPluginEnabled())
			{
				if (!settings->GetBool(SETTING_PLUGIN_REGISTRATION, false) && 
					(settings->GetValue(SETTING_PLUGIN_ADBLOCKCOUNT, 0) >=settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0)) && 
					(settings->GetValue(SETTING_PLUGIN_ADBLOCKLIMIT, 0) > 0))
				{		
					DisplayActivateMessage();
					return;
				}
			}
#endif
			settings->TogglePluginEnabled();

			// Enable / disable mime filter
			s_criticalSectionLocal.Lock();
			{
				//Display activation menu if enabling expired plugin
				if (settings->GetPluginEnabled())
				{
					s_mimeFilter = CPluginClientFactory::GetMimeFilterClientInstance();
				}
				else
				{
					s_mimeFilter = NULL;

					CPluginClientFactory::ReleaseMimeFilterClientInstance();
				}
			}
			s_criticalSectionLocal.Unlock();

#ifdef SUPPORT_WHITELIST
			client->ClearWhiteListCache();
#endif
		}
		break;
#ifdef PRODUCT_DOWNLOADHELPER
	case ID_ENTERLICENSE:
		{
			url = CPluginHttpRequest::GetStandardUrl(USERS_SCRIPT_ENTERLICENSE);
			CPluginSettings* settings = CPluginSettings::GetInstance();
			CPluginHttpRequest httpRequest(USERS_SCRIPT_ENTERLICENSE);
			httpRequest.Add(L"plugin", system->GetPluginId());
			httpRequest.Add(L"username", system->GetUserNameW());
			httpRequest.Add(L"user", settings->GetString(SETTING_USER_ID));
			httpRequest.Add(L"version", settings->GetString(SETTING_PLUGIN_VERSION));
			CString url = httpRequest.GetUrl();
			navigationErrorId = PLUGIN_ERROR_NAVIGATION_ENTERLICENSE;
			break;
		}
	case ID_UPGRADE:
		{
			url = CPluginHttpRequest::GetStandardUrl(USERS_SCRIPT_UPGRADE);
			CPluginSettings* settings = CPluginSettings::GetInstance();
			CPluginHttpRequest httpRequest(USERS_SCRIPT_UPGRADE);
			httpRequest.Add(L"plugin", system->GetPluginId());
			httpRequest.Add(L"username", system->GetUserNameW());
			httpRequest.Add(L"user", settings->GetString(SETTING_USER_ID));
			httpRequest.Add(L"version", settings->GetString(SETTING_PLUGIN_VERSION));
			CString url = httpRequest.GetUrl();
			navigationErrorId = PLUGIN_ERROR_NAVIGATION_UPGRADE;
			break;
		}
	case ID_ENABLE_CONVERSSION:
		{
			url = ENABLE_CONVERSSION_URL;
			navigationErrorId = PLUGIN_ERROR_NAVIGATION_ENABLE_CONVERSSION;			
			break;
		}
#endif
#ifndef ENTERPRISE
	case ID_SETTINGS:
		{
		    // Update settings server side on next IE start, as they have possibly changed
	        CPluginSettings* settings = CPluginSettings::GetInstance();

			settings->ForceConfigurationUpdateOnStart();

#ifdef PRODUCT_DOWNLOADHELPER

			CSettingsDialog settingsDialog;

			CPluginConfig* config = CPluginConfig::GetInstance();
			std::vector<CString> extenssions = config->GetConversionExtenssions();
			for (std::vector<CString>::const_iterator ext = extenssions.begin(); ext != extenssions.end(); ++ext)
			{
				settingsDialog.m_extenssions.push_back(ext->GetString());
			}
			settingsDialog.m_defDirVal = settings->GetString(SETTING_DEFAULT_DIR);
			CString fo = settings->GetString(SETTING_DEFAULT_FORMAT, "-1").GetString();
			settingsDialog.m_defFormatVal = _wtoi(settings->GetString(SETTING_DEFAULT_FORMAT, "-1").GetString());
			settingsDialog.m_ffmpegPath = settings->GetString(SETTING_FFMPEG);
			settingsDialog.m_defaultFfmpegPath = settings->GetString(SETTING_FFMPEG);
			if (settings->GetString(SETTING_CLOSE_WHEN_FINISHED, "false") == "true")
			{
				settingsDialog.m_closeWhenFinishedVal = TRUE;
			} 
			else 
			{
				settingsDialog.m_closeWhenFinishedVal = FALSE;
			}
			INT_PTR response = settingsDialog.DoModal();
			if (response == IDOK)
			{
				settings->SetString(SETTING_DEFAULT_DIR, settingsDialog.m_defDirVal);
				CString defFormat;
				if (settingsDialog.m_defFormatVal != -1)
				{
					settingsDialog.m_defFormatVal--;
				}
				defFormat.Format(L"%d", settingsDialog.m_defFormatVal);
				settings->SetString(SETTING_DEFAULT_FORMAT, defFormat);
				if (settingsDialog.m_closeWhenFinishedVal)
				{
					settings->SetString(SETTING_CLOSE_WHEN_FINISHED, "true");
				}
				else
				{
					settings->SetString(SETTING_CLOSE_WHEN_FINISHED, "false");
				}

				if (settingsDialog.m_ffmpegPath.IsEmpty())
				{
					settings->Remove(SETTING_FFMPEG);
				}
				else
				{
					settings->SetString(SETTING_FFMPEG, settingsDialog.m_ffmpegPath);
				}
				settings->Write(false);


			}
#endif
#ifdef PRODUCT_SIMPLEADBLOCK

            CPluginHttpRequest httpRequest(USERS_SCRIPT_USER_SETTINGS);
            
            httpRequest.AddPluginId();
            httpRequest.Add("username", system->GetUserName(), false);
			
			url = httpRequest.GetUrl();

			navigationErrorId = PLUGIN_ERROR_NAVIGATION_SETTINGS;
#endif
		}
		break;
#endif
	case ID_INVITEFRIENDS:
		{
			url = CPluginHttpRequest::GetStandardUrl(USERS_SCRIPT_INVITATION);
			navigationErrorId = PLUGIN_ERROR_NAVIGATION_INVITATION;
		}
		break;

	case ID_FAQ:
        {
			url = CPluginHttpRequest::GetStandardUrl(USERS_SCRIPT_FAQ);
			navigationErrorId = PLUGIN_ERROR_NAVIGATION_FAQ;
        }
        break;

    case ID_FEEDBACK:
        {
            CPluginHttpRequest httpRequest(USERS_SCRIPT_FEEDBACK);
            
            httpRequest.AddPluginId();
			httpRequest.Add("reason", 0);
			httpRequest.Add(L"url", m_tab->GetDocumentUrl(), false);
			
			url = httpRequest.GetUrl();
			navigationErrorId = PLUGIN_ERROR_NAVIGATION_FEEDBACK;
        }
        break;

	case ID_ABOUT:
		{
			url = CPluginHttpRequest::GetStandardUrl(USERS_SCRIPT_ABOUT);
			navigationErrorId = PLUGIN_ERROR_NAVIGATION_ABOUT;
		}
		break;

	default:

#ifdef SUPPORT_WHITELIST
        {
            if (nCommand >= WM_WHITELIST_DOMAIN && nCommand <= WM_WHITELIST_DOMAIN_MAX)
		    {
	            CPluginSettings* settings = CPluginSettings::GetInstance();

			    CString domain;
    		
			    s_criticalSectionLocal.Lock();
			    {
				    domain = s_menuDomains[nCommand];
			    }
			    s_criticalSectionLocal.Unlock();

			    if (settings->IsWhiteListedDomain(domain)) 
			    {
        		    settings->AddWhiteListedDomain(domain, 3, true);
			    }
			    else
			    {
	                settings->AddWhiteListedDomain(domain, 1, true);
			    }
    			
			    client->ClearWhiteListCache();
		    }
	    }
#endif // SUPPORT_WHITELIST


#ifdef SUPPORT_FILE_DOWNLOAD
        {
            if (nCommand >= WM_DOWNLOAD_FILE && nCommand <= WM_DOWNLOAD_FILE_MAX)
		    {
			    SDownloadFile downloadFile;
    		
			    s_criticalSectionLocal.Lock();
			    {
				    downloadFile = s_menuDownloadFiles[nCommand];
			    }
			    s_criticalSectionLocal.Unlock();

				STARTUPINFO si;
				::ZeroMemory(&si, sizeof(si));
				si.cb = sizeof(si);

				PROCESS_INFORMATION pi;
				::ZeroMemory(&pi, sizeof(pi));

				LPTSTR  strDLLPath1 = new TCHAR[_MAX_PATH];
				::GetModuleFileName((HINSTANCE)&__ImageBase, strDLLPath1, _MAX_PATH);
				CString dllPath;
				dllPath.Format(L"%s", strDLLPath1);
				dllPath = dllPath.Left(dllPath.GetLength() - CString(L"DownloadHelper.dll").GetLength());

				CPluginSettings* settings = CPluginSettings::GetInstance();
				if (settings->GetValue(SETTING_DOWNLOAD_LIMIT) > 0)
				{
					int fileSizeInMb = downloadFile.fileSize / 1024000;
					if (fileSizeInMb >= settings->GetValue(SETTING_DOWNLOAD_LIMIT))
					{
						CPluginDictionary* dictionary = CPluginDictionary::GetInstance();
						CString message;
						message.Format(dictionary->Lookup(L"DOWNLOAD_LIMIT_MESSAGE"), settings->GetValue(SETTING_DOWNLOAD_LIMIT));
						int res = MessageBox(this->m_hBrowserWnd, message, dictionary->Lookup(L"DOWNLOAD_LIMIT_TITLE"), MB_OKCANCEL);
						if (res == IDOK)
						{
							url = CPluginHttpRequest::GetStandardUrl(USERS_SCRIPT_UPGRADE);
							CPluginSettings* settings = CPluginSettings::GetInstance();
							CPluginHttpRequest httpRequest(USERS_SCRIPT_UPGRADE);
							httpRequest.Add(L"plugin", system->GetPluginId());
							httpRequest.Add(L"username", system->GetUserNameW());
							httpRequest.Add(L"user", settings->GetString(SETTING_USER_ID));
							httpRequest.Add(L"version", settings->GetString(SETTING_PLUGIN_VERSION));
							CString url = httpRequest.GetUrl();
							navigationErrorId = PLUGIN_ERROR_NAVIGATION_UPGRADE;
							break;
						}
						else
						{
							return;
						}
					}
				}
				CPluginChecksum checksum;

				checksum.Add("/url", downloadFile.downloadUrl);
				checksum.Add("/type", downloadFile.properties.content);
				checksum.Add("/file", downloadFile.downloadFile);
				checksum.Add("/cookie", downloadFile.cookie);
				checksum.Add("/referer", downloadFile.properties.properties.referer);
				CString size;
				size.Format(L"%d", downloadFile.fileSize);
				checksum.Add("/size", size);
				checksum.Add("/path", settings->GetString("defaultDir"));
				checksum.Add("/format", settings->GetString("defaultFormat", L"-1"));
				checksum.Add("/autoclose", settings->GetString("closeWhenFinished"));


				CString args =  CString(L"\"") + CString(dllPath) + CString(L"DownloadHelper.exe\" /url:") + downloadFile.downloadUrl + " /type:" + downloadFile.properties.content + " /file:" + downloadFile.downloadFile  + " /size:" + size + " /cookie:" + downloadFile.cookie + " /referer:" + downloadFile.properties.properties.referer + " /checksum:" + checksum.GetAsString() + " /path:" + settings->GetString("defaultDir") + " /format:" + settings->GetString("defaultFormat", L"-1") + " /autoclose:" + settings->GetString("closeWhenFinished");

				LPWSTR szCmdline = _wcsdup(args);

				if (!::CreateProcess(NULL, szCmdline, NULL, NULL, FALSE, CREATE_PRESERVE_CODE_AUTHZ_LEVEL, NULL, NULL, &si, &pi))
				{
					DWORD dwError = ::CommDlgExtendedError();
					DEBUG_ERROR_LOG(dwError, PLUGIN_ERROR_DOWNLOAD, PLUGIN_ERROR_DOWNLOAD_CREATE_PROCESS, "Download::create process failed");
				}

				::CloseHandle(pi.hProcess);
				::CloseHandle(pi.hThread);
			}
        }
#endif // SUPPORT_FILE_DOWNLOAD

		break;
	}

	// Invalidate and redraw the control
	UpdateStatusBar();

    CComQIPtr<IWebBrowser2> browser = GetBrowser();    
    if (!url.IsEmpty() && browser)
    {
        VARIANT vFlags;
        vFlags.vt = VT_I4;
        vFlags.intVal = navOpenInNewTab;

		HRESULT hr = browser->Navigate(CComBSTR(url), &vFlags, NULL, NULL, NULL);
		if (FAILED(hr))
		{
		    vFlags.intVal = navOpenInNewWindow;

            hr = browser->Navigate(CComBSTR(url), &vFlags, NULL, NULL, NULL);
		    if (FAILED(hr))
		    {
				DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_NAVIGATION, navigationErrorId, "Navigation::Failed")
			}
		}
	}
}


bool CPluginClass::SetMenuBar(HMENU hMenu, const CString& url) 
{
	CString ctext;

	s_criticalSectionLocal.Lock();
	{
#ifdef SUPPORT_WHITELIST
    	s_menuDomains.clear();
#endif
#ifdef SUPPORT_FILE_DOWNLOAD
    	s_menuDownloadFiles.clear();
#endif
	}
	s_criticalSectionLocal.Unlock();

	CPluginTab* tab = GetTab(::GetCurrentThreadId());
	if (!tab)
	{
		return false;
	}

    CPluginDictionary* dictionary = CPluginDictionary::GetInstance();

	MENUITEMINFO fmii;
	memset(&fmii, 0, sizeof(MENUITEMINFO));
	fmii.cbSize = sizeof(MENUITEMINFO);

	MENUITEMINFO miiSep;
	memset(&miiSep, 0, sizeof(MENUITEMINFO));
	miiSep.cbSize = sizeof(MENUITEMINFO);
	miiSep.fMask = MIIM_TYPE | MIIM_FTYPE;
	miiSep.fType = MFT_SEPARATOR;

	CPluginClient* client = CPluginClient::GetInstance();

    CPluginSettings* settings = CPluginSettings::GetInstance();
    
    settings->RefreshTab();

    // Update settings
	m_tab->OnUpdateSettings(false);

#ifndef ENTERPRISE
    // Plugin activate
    if (!settings->GetBool(SETTING_PLUGIN_REGISTRATION, false))
    {
        ctext = dictionary->Lookup("MENU_ACTIVATE");
	    fmii.fMask  = MIIM_STATE | MIIM_STRING;
	    fmii.fState = MFS_ENABLED;
        fmii.dwTypeData = ctext.GetBuffer();
    	fmii.cch = ctext.GetLength();
        ::SetMenuItemInfo(hMenu, ID_PLUGIN_ACTIVATE, FALSE, &fmii);
    }
    else
    {
        ::DeleteMenu(hMenu, ID_PLUGIN_ACTIVATE, FALSE);
    }
#else
	::DeleteMenu(hMenu, ID_PLUGIN_ACTIVATE, FALSE);
#endif
#ifndef ENTERPRISE
    // Plugin update
    if (settings->IsPluginUpdateAvailable())
    {
        ctext = dictionary->Lookup("MENU_UPDATE");
	    fmii.fMask  = MIIM_STATE | MIIM_STRING;
	    fmii.fState = MFS_ENABLED;
        fmii.dwTypeData = ctext.GetBuffer();
    	fmii.cch = ctext.GetLength();
        ::SetMenuItemInfo(hMenu, ID_PLUGIN_UPDATE, FALSE, &fmii);
    }
    else
    {
        ::DeleteMenu(hMenu, ID_PLUGIN_UPDATE, FALSE);
    }
#else 
	::DeleteMenu(hMenu, ID_PLUGIN_UPDATE, FALSE);
#endif
    #ifdef SUPPORT_WHITELIST
    {
	    // White list domain
	    ctext = dictionary->Lookup("MENU_DISABLE_ON");
        fmii.fMask = MIIM_STRING | MIIM_STATE;
        fmii.fState = MFS_DISABLED;
	    fmii.dwTypeData = ctext.GetBuffer();
	    fmii.cch = ctext.GetLength();

	    UINT index = WM_WHITELIST_DOMAIN;

	    // Add domains from history
	    if (client)
	    {
		    bool isFirst = true;

		    TDomainHistory domainHistory = settings->GetDomainHistory();
    		
		    CString documentDomain = tab->GetDocumentDomain();

		    if (CPluginClient::IsValidDomain(documentDomain))
		    {
    		    CString documentDomainT = documentDomain;

			    for (TDomainHistory::const_reverse_iterator it = domainHistory.rbegin(); it != domainHistory.rend(); ++it)
			    {
				    if (documentDomain == it->first)
				    {
					    if (isFirst)
					    {
						    fmii.fState = MFS_ENABLED;
						    fmii.fMask |= MIIM_SUBMENU;
						    fmii.hSubMenu = ::CreateMenu();
					    }

					    MENUITEMINFO smii;
					    memset(&smii, 0, sizeof(MENUITEMINFO));
					    smii.cbSize = sizeof(MENUITEMINFO);

					    smii.fMask = MIIM_STRING | MIIM_ID;
					    smii.dwTypeData = documentDomainT.GetBuffer();
					    smii.cch = documentDomainT.GetLength();
					    smii.wID = index;

					    bool isWhitelisted = settings->IsWhiteListedDomain(it->first);
					    if (isWhitelisted)
					    {
						    smii.fMask |= MIIM_STATE;
						    smii.fState |= MFS_CHECKED;
					    }

					    if (isFirst)
					    {
						    smii.fMask |= MIIM_STATE;
						    smii.fState |= MFS_DEFAULT;

						    isFirst = false;
					    }

					    InsertMenuItem(fmii.hSubMenu, index, FALSE, &smii);

    				    s_criticalSectionLocal.Lock();
    				    {
					        s_menuDomains[index++] = documentDomain;
                        }
    				    s_criticalSectionLocal.Unlock();
				    }
			    }
		    }

            // Add last domains
            for (TDomainHistory::const_reverse_iterator it = domainHistory.rbegin(); it != domainHistory.rend(); ++it)
		    {
			    if (it->first != documentDomain)
			    {
                    if (isFirst)
                    {
                        fmii.fMask |= MIIM_STATE | MIIM_SUBMENU;
                        fmii.fState = MFS_ENABLED;
                        fmii.hSubMenu = CreateMenu();

					    isFirst = false;
                    }

				    CString domain = it->first;

				    MENUITEMINFO smii;
				    memset(&smii, 0, sizeof(MENUITEMINFO));
				    smii.cbSize = sizeof(MENUITEMINFO);
				    smii.fMask = MIIM_STRING | MIIM_ID;
				    smii.dwTypeData = domain.GetBuffer();
				    smii.cch = domain.GetLength();
				    smii.wID = index;

				    bool isWhitelisted = settings->IsWhiteListedDomain(it->first);
				    if (isWhitelisted)
				    {
					    smii.fMask |= MIIM_STATE;
					    smii.fState |= MFS_CHECKED;
				    }

				    ::InsertMenuItem(fmii.hSubMenu, index, FALSE, &smii);

				    s_criticalSectionLocal.Lock();
				    {
				        s_menuDomains[index++] = it->first;
                    }
				    s_criticalSectionLocal.Unlock();                 
                }
		    }
	    }

	    ::SetMenuItemInfo(hMenu, ID_WHITELISTDOMAIN, FALSE, &fmii);
    }
	#else
	{
	    ::DeleteMenu(hMenu, ID_WHITELISTDOMAIN, FALSE);
	}
    #endif // SUPPORT_WHITELIST

	// Invite friends
	ctext = dictionary->Lookup("MENU_INVITE_FRIENDS");
	fmii.fMask  = MIIM_STATE | MIIM_STRING;
	fmii.fState = MFS_ENABLED;
	fmii.dwTypeData = ctext.GetBuffer();
	fmii.cch = ctext.GetLength();
	::SetMenuItemInfo(hMenu, ID_INVITEFRIENDS, FALSE, &fmii);

	// FAQ
    ctext = dictionary->Lookup("MENU_FAQ");
	fmii.fMask  = MIIM_STATE | MIIM_STRING;
	fmii.fState = MFS_ENABLED;
	fmii.dwTypeData = ctext.GetBuffer();
	fmii.cch = ctext.GetLength();
	::SetMenuItemInfo(hMenu, ID_FAQ, FALSE, &fmii);
    
	// About
	ctext = dictionary->Lookup("MENU_ABOUT");
	fmii.fMask = MIIM_STATE | MIIM_STRING;
	fmii.fState = MFS_ENABLED;
	fmii.dwTypeData = ctext.GetBuffer();
	fmii.cch = ctext.GetLength();
	::SetMenuItemInfo(hMenu, ID_ABOUT, FALSE, &fmii);

	// Feedback
    ctext = dictionary->Lookup("MENU_FEEDBACK");
	fmii.fMask = MIIM_STATE | MIIM_STRING;
	fmii.fState = MFS_ENABLED;
	fmii.dwTypeData = ctext.GetBuffer();
	fmii.cch = ctext.GetLength();
	::SetMenuItemInfo(hMenu, ID_FEEDBACK, FALSE, &fmii);

	// Plugin enable
    if (settings->GetPluginEnabled())
    {
        ctext = dictionary->Lookup("MENU_DISABLE");
    }
    else
    {
        ctext = dictionary->Lookup("MENU_ENABLE");
    }
    fmii.fMask  = MIIM_STATE | MIIM_STRING;
    fmii.fState = client ? MFS_ENABLED : MFS_DISABLED;
    fmii.dwTypeData = ctext.GetBuffer();
	fmii.cch = ctext.GetLength();
    ::SetMenuItemInfo(hMenu, ID_PLUGIN_ENABLE, FALSE, &fmii);

#ifdef PRODUCT_DOWNLOADHELPER
	if (settings->GetBool(SETTING_PLUGIN_REGISTRATION, false))
	{	
		DeleteMenu(hMenu, ID_UPGRADE, FALSE);
		DeleteMenu(hMenu, ID_ENTERLICENSE, FALSE);
		if (settings->IsPluginUpdateAvailable())
		{
			RemoveMenu(hMenu, 6, MF_BYPOSITION);	
		}
		else
		{
			RemoveMenu(hMenu, 5, MF_BYPOSITION);	
		}
//		RemoveMenu(hMenu, 5, MF_BYPOSITION);	
		//RemoveMenu(hMenu, 5, MF_BYPOSITION);	
		//RemoveMenu(hMenu, 5, MF_BYPOSITION);			
	}
	else
	{
		// Upgrade
		ctext = dictionary->Lookup("MENU_UPGRADE");
		fmii.fMask  = MIIM_STATE | MIIM_STRING;
		fmii.dwTypeData = ctext.GetBuffer();
		fmii.fState = MFS_ENABLED;
		fmii.cch = ctext.GetLength();
		::SetMenuItemInfo(hMenu, ID_UPGRADE, FALSE, &fmii);

			// Enter license key
		ctext = dictionary->Lookup("MENU_ENTERLICENSE");
		fmii.fMask  = MIIM_STATE | MIIM_STRING;
		fmii.fState = MFS_ENABLED;
		fmii.dwTypeData = ctext.GetBuffer();
		fmii.cch = ctext.GetLength();
		::SetMenuItemInfo(hMenu, ID_ENTERLICENSE, FALSE, &fmii);
	}
	
	if (settings->GetString(SETTING_FFMPEG).IsEmpty())
	{
		ctext = dictionary->Lookup("MENU_ENABLE_CONVERSION");
		fmii.fMask  = MIIM_STATE | MIIM_STRING;
		fmii.fState = MFS_ENABLED;
		fmii.dwTypeData = ctext.GetBuffer();
		fmii.cch = ctext.GetLength();
		::SetMenuItemInfo(hMenu, ID_ENABLE_CONVERSSION, FALSE, &fmii);
	} 
	else
	{
		DeleteMenu(hMenu, ID_ENABLE_CONVERSSION, FALSE);
		int index = 6;
		if (!settings->GetBool(SETTING_PLUGIN_REGISTRATION, false))
		{	
			index += 3;
		}
		if (settings->IsPluginUpdateAvailable())
		{
			index += 1;
		}
		RemoveMenu(hMenu, index, MF_BYPOSITION);
	}
	DeleteMenu(hMenu, ID_PLUGIN_ACTIVATE, FALSE);
#endif
 

	// Settings
#ifndef ENTERPRISE
	ctext = dictionary->Lookup("MENU_SETTINGS");
	fmii.fMask  = MIIM_STATE | MIIM_STRING;
	fmii.fState = MFS_ENABLED;
	fmii.dwTypeData = ctext.GetBuffer();
	fmii.cch = ctext.GetLength();
	::SetMenuItemInfo(hMenu, ID_SETTINGS, FALSE, &fmii);
#else
	RemoveMenu(hMenu, ID_SETTINGS, MF_BYCOMMAND);
	RemoveMenu(hMenu, 5, MF_BYPOSITION);
#endif	



    // Download files
    #ifdef SUPPORT_FILE_DOWNLOAD
    {
	    UINT index = WM_DOWNLOAD_FILE;

        TDownloadFiles downloadFiles = tab->GetDownloadFiles();
        if (downloadFiles.empty())
        {
	        ctext = dictionary->Lookup("DOWNLOAD_FILE_NO_FILES");

            MENUITEMINFO smii;
            memset(&smii, 0, sizeof(MENUITEMINFO));
            smii.cbSize = sizeof(MENUITEMINFO);
    		
            smii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
            smii.dwTypeData = ctext.GetBuffer();
            smii.cch = ctext.GetLength();
            smii.wID = index;
            smii.fState = MFS_DISABLED;

            InsertMenuItem(hMenu, index, FALSE, &smii);
        }
        else
        {
            for (TDownloadFiles::iterator it = downloadFiles.begin(); it != downloadFiles.end() && index < WM_DOWNLOAD_FILE_MAX; ++it)
            {
				CString fileItem;
				CString download = dictionary->Lookup("GENERAL_DOWNLOAD");

				if (it->second.fileSize > 1024000L)
				{
					fileItem.Format(L"%s - %s (%.1f Mb)", download.GetBuffer(), it->second.downloadFile.GetBuffer(), (float)it->second.fileSize / (float)1024000L);
				}
				else if (it->second.fileSize > 1024L)
				{
					fileItem.Format(L"%s - %s (%.1f Kb)", download.GetBuffer(), it->second.downloadFile.GetBuffer(), (float)it->second.fileSize / (float)1024L);
				}
				else if (it->second.fileSize > 0)
				{
					fileItem.Format(L"%s - %s (%u bytes)", download.GetBuffer(), it->second.downloadFile.GetBuffer(), it->second.fileSize);
				}
				else
				{
					fileItem.Format(L"%s - %s", download.GetBuffer(), it->second.downloadFile.GetBuffer());
				}

	            MENUITEMINFO smii;
	            memset(&smii, 0, sizeof(MENUITEMINFO));
	            smii.cbSize = sizeof(MENUITEMINFO);
        		
	            smii.fMask = MIIM_STRING | MIIM_ID;
	            smii.dwTypeData = fileItem.GetBuffer();
	            smii.cch = fileItem.GetLength();
	            smii.wID = index;

	            InsertMenuItem(hMenu, index, FALSE, &smii);
    	        
			    s_criticalSectionLocal.Lock();
			    {
		            s_menuDownloadFiles[index++] = it->second;
                }
			    s_criticalSectionLocal.Unlock();                 
            }
        }
    }
    #endif // SUPPORT_FILE_DOWNLOAD

	ctext.ReleaseBuffer();

	return true;
}


STDMETHODIMP CPluginClass::Exec(const GUID*, DWORD nCmdID, DWORD, VARIANTARG*, VARIANTARG*)
{
	HWND hBrowserWnd = GetBrowserHWND();
	if (!hBrowserWnd)
	{
		return E_FAIL;
	}

	// Create menu
	HMENU hMenu = CreatePluginMenu(m_tab->GetDocumentUrl());
	if (!hMenu)
	{
		return E_FAIL;
	}

	// Check if button in toolbar was pressed
	int nIDCommand = -1;
	BOOL bRightAlign = FALSE;

	POINT pt;
	GetCursorPos(&pt);

	HWND hWndToolBar = ::WindowFromPoint(pt);

	DWORD nProcessId;
	::GetWindowThreadProcessId(hWndToolBar, &nProcessId);

	if (hWndToolBar && ::GetCurrentProcessId() == nProcessId)
	{
		::ScreenToClient(hWndToolBar, &pt);
		int nButton = (int)::SendMessage(hWndToolBar, TB_HITTEST, 0, (LPARAM)&pt);

		if (nButton > 0)
		{
			TBBUTTON pTBBtn;
			memset(&pTBBtn, 0, sizeof(TBBUTTON));

			if (SendMessage(hWndToolBar, TB_GETBUTTON, nButton, (LPARAM)&pTBBtn))
			{
				RECT rcButton;
				nIDCommand = pTBBtn.idCommand;

				if (SendMessage(hWndToolBar, TB_GETRECT, nIDCommand, (LPARAM)&rcButton))
				{
					pt.x = rcButton.left;
					pt.y = rcButton.bottom;
					ClientToScreen(hWndToolBar, &pt);

					RECT rcWorkArea;
					SystemParametersInfo(SPI_GETWORKAREA, 0, (LPVOID)&rcWorkArea, 0);
					if (rcWorkArea.right - pt.x < 150)
					{
						bRightAlign = TRUE;
						pt.x = rcButton.right;
						pt.y = rcButton.bottom;
						ClientToScreen(hWndToolBar, &pt);
					}
				}
			}
		}
		else
		{
			GetCursorPos(&pt);
		}
	}

	// Display menu
	UINT nFlags = 0;
	if (bRightAlign)
	{
		nFlags |= TPM_RIGHTALIGN;
	}
	else
	{
		nFlags |= TPM_LEFTALIGN;
	}

	DisplayPluginMenu(hMenu, nIDCommand, pt, nFlags);

	return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Window procedures

LRESULT CALLBACK CPluginClass::NewStatusProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Find tab
	CPluginClass *pClass = FindInstance(hWnd);
	if (!pClass)
	{
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	// Process message 
	switch (message)
	{
	case SB_SIMPLE:
		{
			ShowWindow(pClass->m_hPaneWnd, !wParam);
			break;
		}

	case WM_SYSCOLORCHANGE:
		{
			pClass->UpdateTheme();
			break;
		}

	case SB_SETPARTS:
		{
			if (!lParam || !wParam || wParam > 30 || !IsWindow(pClass->m_hPaneWnd))
			{
				return CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, lParam);
			}

			int nParts = wParam;
			if (STATUSBAR_PANE_NUMBER >= nParts)
			{
				return CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, lParam);
			}

			HLOCAL hLocal = LocalAlloc(LHND, sizeof(int) * (nParts+1));
			LPINT lpParts = (LPINT)LocalLock(hLocal);
			memcpy(lpParts, (void*)lParam, wParam*sizeof(int));

			for (unsigned i = 0; i < STATUSBAR_PANE_NUMBER; i++)
			{
				lpParts[i] -= pClass->m_nPaneWidth;
			}
			LRESULT hRet = CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, (LPARAM)lpParts);

			CRect rcPane;
			::SendMessage(hWnd, SB_GETRECT, STATUSBAR_PANE_NUMBER, (LPARAM)&rcPane);

			CRect rcClient;
			::GetClientRect(hWnd, &rcClient);

			::MoveWindow(
				pClass->m_hPaneWnd,
				lpParts[STATUSBAR_PANE_NUMBER] - pClass->m_nPaneWidth,
				0,
				pClass->m_nPaneWidth,
				rcClient.Height(),
				TRUE);

			::LocalFree(hLocal);


			return hRet;
		}

	default:
		break;
	}

	LRESULT result = CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, lParam);


	return result;

}


HICON CPluginClass::GetStatusBarIcon(const CString& url)
{
	// use the disable icon as defualt, if the client doesn't exists
	HICON hIcon = GetIcon(ICON_PLUGIN_DEACTIVATED);

	CPluginTab* tab = GetTab(::GetCurrentThreadId());
	if (tab)
	{
		CPluginClient* client = CPluginClient::GetInstance();

#ifdef PRODUCT_SIMPLEADBLOCK
		if (!CPluginSettings::GetInstance()->IsPluginEnabled())
		{
		}
#ifdef SUPPORT_WHITELIST
		else if (client->IsUrlWhiteListed(url))
		{
			hIcon = GetIcon(ICON_PLUGIN_DISABLED);
		}
#endif // SUPPORT_WHITELIST
		else
		{
			//Deativate adblock icon if adblock limit reached
			CPluginSettings* settings = CPluginSettings::GetInstance();
			if (!settings->GetPluginEnabled()) {
				hIcon = GetIcon(ICON_PLUGIN_DEACTIVATED);
				return hIcon;
			}
			hIcon = GetIcon(ICON_PLUGIN_ENABLED);
		}

#endif // PRODUCT_SIMPLEADBLOCK

#ifdef PRODUCT_DOWNLOADHELPER
 #ifdef SUPPORT_WHITELIST
	    if (CPluginSettings::GetInstance()->IsPluginEnabled() && !client->IsUrlWhiteListed(url))
 #else
		if (CPluginSettings::GetInstance()->IsPluginEnabled())
 #endif
		{
			if (tab->HasDownloadFiles())
			{
    			hIcon = GetIcon(ICON_PLUGIN_ENABLED);
			}
			else
			{
				hIcon = GetIcon(ICON_PLUGIN_DISABLED);
			}
		}
#endif
	}

	return hIcon;
}	


LRESULT CALLBACK CPluginClass::PaneWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Find tab
	CPluginClass *pClass = FindInstance(GetParent(hWnd));
	if (!pClass) 
	{
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}

	CPluginSystem* system = CPluginSystem::GetInstance();

	// Process message 
	switch (message)
	{

	case WM_SETCURSOR:
		{
			::SetCursor(::LoadCursor(NULL, IDC_ARROW));
			return TRUE;
		}
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hDC = ::BeginPaint(hWnd, &ps);

			CRect rcClient;
			::GetClientRect(hWnd, &rcClient);

			int nDrawEdge = 0;

			// Old Windows background drawing
			if (pClass->m_hTheme == NULL)
			{
				::FillRect(hDC, &rcClient, (HBRUSH)(COLOR_BTNFACE + 1));
				::DrawEdge(hDC, &rcClient, BDR_RAISEDINNER, BF_LEFT);

				nDrawEdge = 3;
				rcClient.left += 3;

				::DrawEdge(hDC, &rcClient, BDR_SUNKENOUTER, BF_RECT);
			}
			// Themed background drawing
			else
			{
				// Draw background
				if (pfnDrawThemeBackground)
				{
					CRect rc = rcClient;
					rc.right -= 2;
					pfnDrawThemeBackground(pClass->m_hTheme, hDC, 0, 0, &rc, NULL);
				}

				// Copy separator picture to left side
				int nHeight = rcClient.Height();
				int nWidth = rcClient.Width() - 2;

				for (int i = 0; i < 2; i++)
				{
					for (int j = 0; j < nHeight; j++)
					{
						COLORREF clr = ::GetPixel(hDC, i + nWidth, j);

						// Ignore black boxes (if source is obscured by other windows)
						if (clr != -1 && (GetRValue(clr) > 8 || GetGValue(clr) > 8 || GetBValue(clr) > 8))
						{
							::SetPixel(hDC, i, j, clr);
						}
					}
				}
			}

			// Draw icon
			if (CPluginClient::GetInstance())
			{
				HICON hIcon = GetStatusBarIcon(pClass->GetTab()->GetDocumentUrl());

				int offx = (rcClient.Height() - 16)/2 + nDrawEdge;
				if (hIcon) 
				{
					::DrawIconEx(hDC, offx, (rcClient.Height() - 16)/2 + 2, hIcon, 16, 16, NULL, NULL, DI_NORMAL);
					offx += 22;
				}
#ifdef _DEBUG
				// Display version
				HFONT hFont = (HFONT)::SendMessage(pClass->m_hStatusBarWnd, WM_GETFONT, 0, 0);
				HGDIOBJ hOldFont = ::SelectObject(hDC,hFont);

				CRect rcText = rcClient;
				rcText.left += offx;
				::SetBkMode(hDC, TRANSPARENT);
				::DrawText(hDC, _T(IEPLUGIN_VERSION), -1, &rcText, DT_WORD_ELLIPSIS|DT_LEFT|DT_SINGLELINE|DT_VCENTER);

				::SelectObject(hDC, hOldFont);
#endif // _DEBUG
			}

			// Done!
			EndPaint(hWnd, &ps);

			return 0;
		}

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		{
			CString strURL = pClass->GetBrowserUrl();
			if (strURL != pClass->GetTab()->GetDocumentUrl())
			{
				pClass->GetTab()->SetDocumentUrl(strURL);
			}

			// Create menu
			HMENU hMenu = pClass->CreatePluginMenu(strURL);
			if (!hMenu)
			{
				return 0;
			}

			// Display menu
			POINT pt;
			::GetCursorPos(&pt);

			RECT rc;
			::GetWindowRect(hWnd, &rc);

			if (rc.left >= 0 && rc.top >= 0) 
			{
				pt.x = rc.left;
				pt.y = rc.top;
			}
			pClass->DisplayPluginMenu(hMenu, -1, pt, TPM_LEFTALIGN|TPM_BOTTOMALIGN);
		}
		break;

#ifndef ENTERPRISE

	case WM_LAUNCH_INFO:
        {
	        // Set the status bar visible, if it isn't
	        // Otherwise the user won't see the icon the first time

	        if (wParam == 1)
	        {
                // Redirect to welcome page
	            VARIANT_BOOL isVisible;
                CComQIPtr<IWebBrowser2> browser = GetAsyncBrowser();
                if (browser)
                {
					HRESULT hr = S_OK;
				
					hr = browser->get_StatusBar(&isVisible);
					if (SUCCEEDED(hr))
					{
						if (!isVisible)
						{
							LRESULT res = MessageBox(NULL, L"The plugin menu is located in the statusbar, would you like to enable Internet Explorer's statusbar?", L"Enable status bar?", MB_YESNO);
							if (res == IDYES)
							{
								hr = browser->put_StatusBar(TRUE);
								if (FAILED(hr))
								{
									DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_PUT_STATUSBAR, "Class::Enable statusbar");
								}
							}
						}
					}
					else
					{
						DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_GET_STATUSBAR, "Class::Get statusbar state");
					}

                    CPluginSettings* settings = CPluginSettings::GetInstance();

                    CPluginHttpRequest httpRequest(USERS_SCRIPT_WELCOME);

                    httpRequest.AddPluginId();
                    httpRequest.Add("username", system->GetUserName(), false);
                    httpRequest.Add("errors", settings->GetErrorList());
//                    httpRequest.Add("src", DOWNLOAD_SOURCE);


			        hr = browser->Navigate(CComBSTR(httpRequest.GetUrl() + "&src=" + DOWNLOAD_SOURCE), NULL, NULL, NULL, NULL);
					if (FAILED(hr))
					{
						DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_NAVIGATION, PLUGIN_ERROR_NAVIGATION_WELCOME, "Navigation::Welcome page failed")
					}

		            // Update settings server side on next IE start, as they have possibly changed
			        settings->ForceConfigurationUpdateOnStart();
		        }
		    }
	        else
	        {
                // Redirect to info page
                CComQIPtr<IWebBrowser2> browser = GetAsyncBrowser();
                if (browser)
                {
                    CPluginHttpRequest httpRequest(USERS_SCRIPT_INFO);
                    
                    httpRequest.AddPluginId();
    			    httpRequest.Add("info", wParam);

                    VARIANT vFlags;
                    vFlags.vt = VT_I4;
                    vFlags.intVal = navOpenInNewTab;
                    
			        HRESULT hr = browser->Navigate(CComBSTR(httpRequest.GetUrl()), &vFlags, NULL, NULL, NULL);
					if (FAILED(hr))
					{
					    vFlags.intVal = navOpenInNewWindow;

			            hr = browser->Navigate(CComBSTR(httpRequest.GetUrl()), &vFlags, NULL, NULL, NULL);
					    if (FAILED(hr))
					    {
    						DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_NAVIGATION, PLUGIN_ERROR_NAVIGATION_INFO, "Navigation::Info page failed")
						}
					}
		        }
		    }
		}
		break;
#endif
		
    case WM_DESTROY:
        break;
	case SC_CLOSE:
		break;

    case WM_UPDATEUISTATE:
        {
			CPluginTab* tab = GetTab(::GetCurrentThreadId());
			if (tab)
			{
				tab->OnActivate();
			}
        }
        break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}


void CPluginClass::UpdateStatusBar()
{
    DEBUG_GENERAL("*** Updating statusbar")

	if (m_hPaneWnd == NULL)
	{
		CreateStatusBarPane();
	}
    if (!::InvalidateRect(m_hPaneWnd, NULL, FALSE))
	{
		DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_INVALIDATE_STATUSBAR, "Class::Invalidate statusbar");	
	}
}


void CPluginClass::Unadvice()
{
    s_criticalSectionLocal.Lock();
    {
        if (m_isAdviced)
        {
	        CComPtr<IConnectionPoint> pPoint = GetConnectionPoint();
	        if (pPoint)
	        {
		        HRESULT hr = pPoint->Unadvise(m_nConnectionID);
		        if (FAILED(hr))
		        {
//??        		    DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_SET_SITE, PLUGIN_ERROR_SET_SITE_UNADVICE, "Class::Unadvice - Unadvise");
		        }
	        }

	        m_isAdviced = false;
        }
    }
    s_criticalSectionLocal.Unlock();
}

HICON CPluginClass::GetIcon(int type)
{
    HICON icon = NULL;

    s_criticalSectionLocal.Lock();
    {
        if (!s_hIcons[type])
        {
		    s_hIcons[type] = ::LoadIcon(_Module.m_hInst, MAKEINTRESOURCE(s_hIconTypes[type]));
		    if (!s_hIcons[type])
		    {
		        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_LOAD_ICON, "Class::GetIcon - LoadIcon")
		    }
        }
        
        icon = s_hIcons[type];
	}
    s_criticalSectionLocal.Unlock();

    return icon;
}

ATOM CPluginClass::GetAtomPaneClass()
{
    return s_atomPaneClass;
}

HWND CPluginClass::GetTabHWND() const
{

	TCHAR szClassName[MAX_PATH];
	// Get browser window and url
	HWND hBrowserWnd = GetBrowserHWND();
	if (!hBrowserWnd)
	{
        DEBUG_ERROR_LOG(0, PLUGIN_ERROR_UI, PLUGIN_ERROR_UI_NO_STATUSBAR_BROWSER, "Class::GetTabWindow - No tab window")
                    s_criticalSectionWindow.Unlock();

		return false;
	}

	// Looking for a TabWindowClass window in IE7

	HWND hTabWnd = ::GetWindow(hBrowserWnd, GW_CHILD);
	while (hTabWnd)
	{
		memset(szClassName, 0, MAX_PATH);
		GetClassName(hTabWnd, szClassName, MAX_PATH);

		if (_tcscmp(szClassName, _T("TabWindowClass")) == 0 || _tcscmp(szClassName,_T("Frame Tab")) == 0)
		{
			// IE8 support
			HWND hTabWnd2 = hTabWnd;
			if (_tcscmp(szClassName,_T("Frame Tab")) == 0)
			{
				hTabWnd2 = ::FindWindowEx(hTabWnd2, NULL, _T("TabWindowClass"), NULL);
			}

			if (hTabWnd2)
			{
				DWORD nProcessId;
				::GetWindowThreadProcessId(hTabWnd2, &nProcessId);
				if (::GetCurrentProcessId() == nProcessId)
				{
					bool bExistingTab = false;
					s_criticalSectionLocal.Lock();

                    {
					    for (int i = 0; i < s_instances.GetSize(); i++)
					    {
						    if (s_instances[i]->m_hTabWnd == hTabWnd2) 
						    {
							    bExistingTab = true;
							    break;
						    }
					    }
                    }

					if (!bExistingTab)
					{
						hBrowserWnd = hTabWnd2;
						hTabWnd = hTabWnd2;
						s_criticalSectionLocal.Unlock();
						break;
					}
					s_criticalSectionLocal.Unlock();

				}
			}
		}

		hTabWnd = ::GetWindow(hTabWnd, GW_HWNDNEXT);
	}

	return hTabWnd;

}
