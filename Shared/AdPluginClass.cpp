#include "AdPluginStdAfx.h"

#include "AdPluginClass.h"
#include "AdPluginDictionary.h"
#include "AdPluginSettings.h"
#ifdef SUPPORT_FILTER
#include "AdPluginFilterClass.h"
#endif
#include "AdPluginMimeFilterClient.h"

#include "AdPluginClient.h"
#include "AdPluginClientFactory.h"
#include "AdPluginHttpRequest.h"
#include "AdPluginMutex.h"
#include "AdPluginProfiler.h"


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

CComAutoCriticalSection CPluginClass::s_criticalSectionLocal;
CComAutoCriticalSection CPluginClass::s_criticalSectionBrowser;

CComQIPtr<IWebBrowser2> CPluginClass::s_asyncWebBrowser2;

#ifdef SUPPORT_WHITELIST
std::map<UINT,CString> CPluginClass::s_menuDomains;
#endif

#ifdef SUPPORT_FILE_DOWNLOAD
TMenuDownloadFiles CPluginClass::s_menuDownloadFiles;
#endif

bool CPluginClass::s_isPluginToBeUpdated = false;
bool CPluginClass::s_isTabActivated = true;

int CPluginClass::s_dictionaryVersion = 0;
int CPluginClass::s_settingsVersion = 1;
#ifdef SUPPORT_FILTER
int CPluginClass::s_filterVersion = 0;
#endif
#ifdef SUPPORT_WHITELIST
int CPluginClass::s_whitelistVersion = 0;
#endif
#ifdef SUPPORT_CONFIG
int CPluginClass::s_configVersion = 0;
#endif

CPluginClass::CPluginClass()
{
    m_isRefresh = true;
    m_isAdviced = false;
    m_nConnectionID = 0;
    m_hTabWnd = NULL;
    m_hStatusBarWnd = NULL;
    m_hPaneWnd = NULL;
    m_nPaneWidth = 0;
    m_pWndProcStatus = NULL;
    m_hTheme = NULL;

#ifdef SUPPORT_FILTER
    m_cacheDomElementCount = 0;
    m_cacheIndexLast = 0;
    m_cacheElementsMax= 5000;
    m_cacheElements = new CElementHideCache[m_cacheElementsMax];
#endif

    // Load / create settings
    CPluginSettings* settings = CPluginSettings::GetInstance();

    bool isMainTab = settings->IncrementTabCount();

    if (isMainTab)
    {
        // Prepare settings
        settings->SetMainProcessId();

        // Ensure plugin version
        if (!settings->Has(SETTING_PLUGIN_VERSION))
        {
            settings->SetString(SETTING_PLUGIN_VERSION, IEPLUGIN_VERSION);
		    settings->SetFirstRunUpdate();
        }

        // First run or deleted settings file)
        if (!settings->Has(SETTING_PLUGIN_ID))
        {
            settings->SetString(SETTING_PLUGIN_ID, CPluginClient::GetPluginId());
            settings->SetFirstRun();
        }
/*        
#ifdef _DEBUG
settings->SetString(SETTING_PLUGIN_UPDATE_VERSION, "9.9.9");
settings->SetString(SETTING_PLUGIN_UPDATE_URL, "http://simple-adblock.com/download/simpleadblockupdate.msi");
#endif
*/
        // Update?
        CStringA oldVersion = settings->GetString(SETTING_PLUGIN_VERSION);
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
        CStringA regDate = settings->GetString(SETTING_REG_DATE);

        SYSTEMTIME systemTime;
        ::GetSystemTime(&systemTime);

        CStringA today;
        today.Format("%d-%d-%d", systemTime.wYear, systemTime.wMonth, systemTime.wDay);
        
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

#ifdef ENABLE_DEBUG_SELFTEST
        if (info == 0 || info > 2)
        {
            CPluginSelftest::Clear();
        }
        else
        {
            CPluginSelftest::SetSupported();
        }
#endif // ENABLE_DEBUG_SELFTEST

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
#ifdef SUPPORT_FILTER
    delete [] m_cacheElements;
#endif

    CPluginSettings* settings = CPluginSettings::GetInstance();
    
    settings->DecrementTabCount();
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

CStringA CPluginClass::GetDocumentUrl() const
{
    CStringA url;

    s_criticalSectionLocal.Lock();
    {
        url = m_documentUrl;
    }
    s_criticalSectionLocal.Unlock();

	return url;
}

void CPluginClass::SetDocumentUrl(const CStringA& url)
{
    CStringA domain;

    s_criticalSectionLocal.Lock();
    {
	    m_documentUrl = url;
	    m_documentDomain = CPluginClient::ExtractDomain(url);
    	
	    domain = m_documentDomain;
    }
    s_criticalSectionLocal.Unlock();

#ifdef SUPPORT_WHITELIST
	CPluginSettings::GetInstance()->AddDomainToHistory(domain);
#endif
}

CStringA CPluginClass::GetDocumentDomain() const
{
    CStringA domain;

    s_criticalSectionLocal.Lock();
    {
        domain = m_documentDomain;
    }
    s_criticalSectionLocal.Unlock();

	return domain;
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

CStringA CPluginClass::GetBrowserUrl() const
{
	CStringA url;

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
		url = GetDocumentUrl();
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

	CString cpath = _T("\"msiexec.exe\" /i \"") + strPath + _T("\""); 

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
        if (settings->IsMainProcess())
        {
            DEBUG_GENERAL(
                "================================================================================\nMAIN TAB UI\n================================================================================")
        }
        else
        {
            DEBUG_GENERAL(
                "================================================================================\nNEW TAB UI\n================================================================================")
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

						if (!InitObject(true))
						{
						    Unadvice();
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
			::SetWindowLong(m_hStatusBarWnd, GWL_WNDPROC, (LPARAM)(WNDPROC)m_pWndProcStatus);

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
		        if (settings->IsMainProcess())
		        {
			        hMainThread = s_hMainThread;
			        s_hMainThread = NULL;
		        }

			    hTabThread = s_hTabThread;
			    s_hTabThread = NULL;
		    }
        }
		s_criticalSectionLocal.Unlock();

		if (hMainThread != NULL)
		{
		    s_isMainThreadDone = true;

			::WaitForSingleObject(hMainThread, INFINITE);
			::CloseHandle(hMainThread);
		}

		if (hTabThread != NULL)
		{
		    s_isTabThreadDone = true;

			::WaitForSingleObject(hTabThread, INFINITE);
			::CloseHandle(hTabThread);
		}

		// Release browser interface
        s_criticalSectionBrowser.Lock();
        {
    		m_webBrowser2.Release();
		}
        s_criticalSectionBrowser.Unlock();

        if (settings->IsMainProcess())
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


void CPluginClass::BeforeNavigate2(DISPPARAMS* pDispParams)
{
	if (pDispParams->cArgs < 7)
	{
    	return;
	}
    
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
	CStringA url;
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
	CPluginClient* client = CPluginClientFactory::GetLazyClientInstance();
	if (client)
	{
		if (GetBrowser().IsEqualObject(WebBrowser2Ptr))
		{
			SetDocumentUrl(url);

            DEBUG_GENERAL("================================================================================\nBegin main navigation url:" + url + "\n================================================================================")

#if (defined ENABLE_DEBUG_RESULT)
            CPluginDebug::DebugResultDomain(url);
#endif
			m_isRefresh = false;
			
			client->ClearCache(GetDocumentDomain());
		}
		else
		{
			DEBUG_NAVI("Navi::Begin navigation url:" + url)

#ifdef SUPPORT_WHITELIST
            client->AddCacheFrame(url);
#endif
		}

		client->SetDocumentUrl(GetDocumentUrl());
		client->SetDocumentDomain(GetDocumentDomain());
	}
}


// This gets called whenever there's a browser event
STDMETHODIMP CPluginClass::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, 
									 WORD wFlags, DISPPARAMS* pDispParams, 
									 VARIANT* pvarResult, EXCEPINFO*  pExcepInfo,
									 UINT* puArgErr)
{
	if (!pDispParams)
	{
		return E_INVALIDARG;
	}

	switch (dispidMember)
	{
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
		return S_OK;

    case DISPID_PROGRESSCHANGE:
        break;

    case DISPID_COMMANDSTATECHANGE:
		break;    

    case DISPID_STATUSTEXTCHANGE:
		break;

	case DISPID_BEFORENAVIGATE2:
		BeforeNavigate2(pDispParams);
		break;

	case DISPID_DOWNLOADBEGIN:
		{
/*
			CPluginClient* client = CPluginClientFactory::GetLazyClientInstance();
			if (client)
			{
				client->SetDocumentUrl(GetDocumentUrl());
				client->SetDocumentDomain(GetDocumentDomain());
			}
*/
			DEBUG_NAVI("Navi::Download Begin")
		}
		break;

	case DISPID_DOWNLOADCOMPLETE:
		{
			DEBUG_NAVI("Navi::Download Complete")

            CComQIPtr<IWebBrowser2> browser = GetBrowser();
			if (browser)
			{
#ifdef SUPPORT_FILTER
				HideElements(browser, true, GetDocumentUrl(), GetDocumentDomain(), CStringA(""));
#endif
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
    				CStringA url;
					CComBSTR bstrUrl;
					if (SUCCEEDED(pBrowser->get_LocationURL(&bstrUrl)) && ::SysStringLen(bstrUrl) > 0)
					{
						url = bstrUrl;
						CPluginClient::UnescapeUrl(url);

					    if (browser.IsEqualObject(pBrowser))
					    {
						    if (url != GetDocumentUrl())
						    {
							    SetDocumentUrl(url);

								CPluginClient* client = CPluginClientFactory::GetLazyClientInstance();
								if (client)
								{
									client->SetDocumentUrl(GetDocumentUrl());
									client->SetDocumentDomain(GetDocumentDomain());
								}
							}

						    m_isRefresh = true;			
					    }

#ifdef SUPPORT_FILTER
                        if (url.Left(6) != "res://")
                        {
    				        HideElements(pBrowser, url == GetDocumentUrl(), url, GetDocumentDomain(), CStringA(""));
				        }
#endif
			        }
				}
			}
		}
		break;

	case DISPID_QUIT:
		{
		    Unadvice();
		}
		break;

	default:
        {
            CStringA did;
            did.Format("DispId:%u", dispidMember);
            
            DEBUG_NAVI("Navi::Default " + did)
        }

		// do nothing
		break;
	}

	return S_OK;
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
    
    if (GetMainThreadHandle() == NULL && settings->IsMainProcess())
    {
	    DWORD id;
        HANDLE handle = ::CreateThread(NULL, 0, MainThreadProc, NULL, CREATE_SUSPENDED, &id);
		if (handle == NULL)
		{
			DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_THREAD, PLUGIN_ERROR_MAIN_THREAD_CREATE_PROCESS, "Class::Thread - Failed to create main thread");
		}

        s_hMainThread = handle;

	    ::ResumeThread(handle);
    }

    // Create tab thread
    if (GetTabThreadHandle() == NULL)
    {
	    DWORD id;
        HANDLE handle = ::CreateThread(NULL, 0, TabThreadProc, NULL, CREATE_SUSPENDED, &id);
		if (handle == NULL)
		{
			DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_THREAD, PLUGIN_ERROR_TAB_THREAD_CREATE_PROCESS, "Class::Thread - Failed to create tab thread");
		}

        s_hTabThread = handle;

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
						hBrowserWnd = hTabWnd = hTabWnd2;
						break;
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
		return false;
	}

	// Calculate pane height
	CRect rcStatusBar;
	::GetClientRect(hWndStatusBar, &rcStatusBar);

	if (rcStatusBar.Height() > 0)
	{
#if (defined _DEBUG)
		m_nPaneWidth = 70;
#else
		m_nPaneWidth = min(rcStatusBar.Height(), 22);
#endif
	}
	else
	{
#if (defined _DEBUG)
		m_nPaneWidth = 70;
#else
		m_nPaneWidth = 22;
#endif
	}

	// Create pane window
	HWND hWndNewPane = ::CreateWindowEx(
		NULL,
		MAKEINTATOM(GetAtomPaneClass()),
		_T(""),
		WS_CHILD | WS_VISIBLE,
		0,0,0,0,
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
	m_pWndProcStatus = (WNDPROC)SetWindowLong(hWndStatusBar, GWL_WNDPROC, (LPARAM)(WNDPROC)NewStatusProc);

	// Adjust pane
	UINT nPartCount = ::SendMessage(m_hStatusBarWnd, SB_GETPARTS, 0, 0);

	if (nPartCount > 1)
	{
		INT *pData = new INT[nPartCount];

		::SendMessage(m_hStatusBarWnd, SB_GETPARTS, nPartCount, (LPARAM)pData);
		::SendMessage(m_hStatusBarWnd, SB_SETPARTS, nPartCount, (LPARAM)pData);

		delete[] pData;
	}  

	return true;
}


/////////////////////////////////////////////////////////////////////////////
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


STDMETHODIMP CPluginClass::QueryStatus(const GUID* pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT* pCmdText)
{
	if (cCmds == 0) return E_INVALIDARG;
	if (prgCmds == 0) return E_POINTER;

	prgCmds[0].cmdf = OLECMDF_ENABLED;

	return S_OK;
}

HMENU CPluginClass::CreatePluginMenu(const CStringA& url)
{
	HINSTANCE hInstance = _AtlBaseModule.GetModuleInstance();

	HMENU hMenu = ::LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1));

	HMENU hMenuTrackPopup = GetSubMenu(hMenu, 0);

	SetMenuBar(hMenuTrackPopup, url);

	return hMenuTrackPopup;
}


void CPluginClass::DisplayPluginMenu(HMENU hMenu, int nToolbarCmdID, POINT pt, UINT nMenuFlags)
{
	CPluginClient* client = CPluginClientFactory::GetLazyClientInstance();
	if (!client)
	{
	    return;
    }

	CStringA url;
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

			settings->TogglePluginEnabled();

			// Enable / disable mime filter
			s_criticalSectionLocal.Lock();
			{
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

			client->ClearCache();
		}
		break;

	case ID_SETTINGS:
		{
		    // Update settings server side on next IE start, as they have possibly changed
	        CPluginSettings* settings = CPluginSettings::GetInstance();

			settings->ForceConfigurationUpdateOnStart();

            CPluginHttpRequest httpRequest(USERS_SCRIPT_USER_SETTINGS);
            
            httpRequest.AddPluginId();
            httpRequest.Add("username", CPluginClient::GetUserName(), false);
			
			url = httpRequest.GetUrl();

			navigationErrorId = PLUGIN_ERROR_NAVIGATION_SETTINGS;
		}
		break;

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
			httpRequest.Add("url", GetDocumentUrl(), false);
			
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

			    CStringA domain;
    		
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
    			
			    client->ClearCache();
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

                CPluginDictionary* dictionary = CPluginDictionary::GetInstance();

                // http://msdn.microsoft.com/en-us/library/ms646839(VS.85).aspx
                //#include <cderr.h>
                OPENFILENAME ofn;
                
		        TCHAR szFile[1024] = L"";
		        TCHAR szFilter[1024] = L"";

		        CString extension = CString(downloadFile.properties.extension);
		        CString title = dictionary->Lookup("DOWNLOAD_FILE_SAVE_TITLE");
				CString description = downloadFile.properties.description;

				wsprintf(szFilter, L"%s (*.%s)\0.%s\0\0", description.GetBuffer(), extension.GetBuffer(), extension.GetBuffer());

		        wcscpy(szFile, downloadFile.downloadFile);

                ::ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.hwndOwner = ::GetDesktopWindow();
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = 1024;
                ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
                ofn.lpstrTitle = title;
                ofn.lpstrFilter = szFilter;
                ofn.lpstrDefExt = extension;

                if (!::GetSaveFileName(&ofn))
                {
					DEBUG_ERROR_LOG(::CommDlgExtendedError(), PLUGIN_ERROR_DOWNLOAD, PLUGIN_ERROR_DOWNLOAD_OPEN_SAVE_DIALOG, "Download::opensave dialog failed");
                }
                else
                {
					STARTUPINFO si;
                    ::ZeroMemory(&si, sizeof(si));
                    si.cb = sizeof(si);

                    PROCESS_INFORMATION pi;
                    ::ZeroMemory(&pi, sizeof(pi));

		            char lpData[1024] = "";

			        if (!::SHGetSpecialFolderPathA(NULL, lpData, CSIDL_PROGRAM_FILES, TRUE))
			        {
						DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_GET_SPECIAL_FOLDER_PROGRAM_FILES, "Download::program files folder retrieval failed");
			        }

					CPluginChecksum checksum;

					checksum.Add("/url", downloadFile.downloadUrl);
					checksum.Add(L"/file", CString(szFile));

					CString args = CString(L"\"") + CString(lpData) + CString(L"\\Download Helper\\DownloadHelper.exe\" /url:") + CString(downloadFile.downloadUrl) + " /file:" + szFile + " /checksum:" + CString(checksum.GetAsString());

					LPWSTR szCmdline = _wcsdup(args);

                    if (!::CreateProcess(NULL, szCmdline, NULL, NULL, FALSE, CREATE_PRESERVE_CODE_AUTHZ_LEVEL, NULL, NULL, &si, &pi))
                    {
						DWORD dwError = ::CommDlgExtendedError();
						DEBUG_ERROR_LOG(dwError, PLUGIN_ERROR_DOWNLOAD, PLUGIN_ERROR_DOWNLOAD_CREATE_PROCESS, "Download::create process failed");

#ifdef ADPLUGIN_TEST_MODE
						CString error;
						error.Format(L"Error: %u", dwError);

						::MessageBox(::GetDesktopWindow(), error, L"Create process", MB_OK);
#endif
					}

					::CloseHandle(pi.hProcess);
					::CloseHandle(pi.hThread);
				}
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


bool CPluginClass::SetMenuBar(HMENU hMenu, const CStringA& url) 
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

    CPluginDictionary* dictionary = CPluginDictionary::GetInstance();

	MENUITEMINFO fmii;
	memset(&fmii, 0, sizeof(MENUITEMINFO));
	fmii.cbSize = sizeof(MENUITEMINFO);

	MENUITEMINFO miiSep;
	memset(&miiSep, 0, sizeof(MENUITEMINFO));
	miiSep.cbSize = sizeof(MENUITEMINFO);
	miiSep.fMask = MIIM_TYPE | MIIM_FTYPE;
	miiSep.fType = MFT_SEPARATOR;

	CPluginClient* client = CPluginClientFactory::GetLazyClientInstance();

    CPluginSettings* settings = CPluginSettings::GetInstance();
    
    settings->RefreshTab();

    // Update settings
    if (!settings->IsMainProcess())
    {
        int settingsVersion = settings->GetTabVersion(SETTING_TAB_SETTINGS_VERSION);
        if (s_settingsVersion != settingsVersion)
        {
            s_settingsVersion = settingsVersion;
            settings->Read();
        }
    }

    bool hasUser = settings->Has(SETTING_USER_ID);

    // Plugin activate
    if (settings->GetBool(SETTING_PLUGIN_ACTIVATE_ENABLED,false) && !settings->GetBool(SETTING_PLUGIN_ACTIVATED,false))
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
    		
		    CStringA documentDomain = GetDocumentDomain();

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

	// Settings
	ctext = dictionary->Lookup("MENU_SETTINGS");
	fmii.fMask  = MIIM_STATE | MIIM_STRING;
	fmii.fState = hasUser ? MFS_ENABLED : MFS_DISABLED;
	fmii.dwTypeData = ctext.GetBuffer();
	fmii.cch = ctext.GetLength();
	::SetMenuItemInfo(hMenu, ID_SETTINGS, FALSE, &fmii);

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

    // Download files
    #ifdef SUPPORT_FILE_DOWNLOAD
    {
	    UINT index = WM_DOWNLOAD_FILE;

        std::map<CStringA,SDownloadFile> downloadFiles = client->GetDownloadFiles();
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
            for (std::map<CStringA,SDownloadFile>::iterator it = downloadFiles.begin(); it != downloadFiles.end() && index < WM_DOWNLOAD_FILE_MAX; ++it)
            {
				CString fileItem;
				CString download = dictionary->Lookup("GENERAL_DOWNLOAD");

				if (it->second.fileSize > 1024000L)
				{
					fileItem.Format(L"%s %s (%.1f Mb)", download.GetBuffer(), it->second.downloadFile.GetBuffer(), (float)it->second.fileSize / (float)1024000L);
				}
				else if (it->second.fileSize > 1024L)
				{
					fileItem.Format(L"%s %s (%.1f Kb)", download.GetBuffer(), it->second.downloadFile.GetBuffer(), (float)it->second.fileSize / (float)1024L);
				}
				else if (it->second.fileSize > 0)
				{
					fileItem.Format(L"%s %s (%u bytes)", download.GetBuffer(), it->second.downloadFile.GetBuffer(), it->second.fileSize);
				}
				else
				{
					fileItem.Format(L"%s %s", download.GetBuffer(), it->second.downloadFile.GetBuffer());
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
	HMENU hMenu = CreatePluginMenu(GetDocumentUrl());
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

	return CallWindowProc(pClass->m_pWndProcStatus, hWnd, message, wParam, lParam);
}


HICON CPluginClass::GetStatusBarButton(const CStringA& url)
{
	CPluginClient* client = CPluginClientFactory::GetLazyClientInstance();

	// use the disable icon as defualt, if the client doesn't exists
	HICON hIcon = GetIcon(ICON_PLUGIN_DEACTIVATED);

#if (defined PRODUCT_ADBLOCKER)
    if (!CPluginSettings::GetInstance()->IsPluginEnabled())
	{
    }
#ifdef SUPPORT_WHITELIST
	else if (client && client->IsUrlWhiteListed(url))
	{
		hIcon = GetIcon(ICON_PLUGIN_DISABLED);
	}
#endif
	else if (client)
	{
		hIcon = GetIcon(ICON_PLUGIN_ENABLED);
	}
#elif (defined PRODUCT_DOWNLOADHELPER)
 #ifdef SUPPORT_WHITELIST
    if (CPluginSettings::GetInstance()->IsPluginEnabled() && client && !client->IsUrlWhiteListed(url))
 #else
    if (CPluginSettings::GetInstance()->IsPluginEnabled())
 #endif
	{
	    if (client->HasDownloadFiles())
	    {
    		hIcon = GetIcon(ICON_PLUGIN_ENABLED);
		}
		else
		{
            hIcon = GetIcon(ICON_PLUGIN_DISABLED);
        }
	}
#endif

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
			if (CPluginClientFactory::GetLazyClientInstance())
			{
				HICON hIcon = GetStatusBarButton(pClass->GetDocumentUrl());

				int offx = (rcClient.Height() - 16)/2 + nDrawEdge;
				if (hIcon) 
				{
					::DrawIconEx(hDC, offx, (rcClient.Height() - 16)/2 + 2, hIcon, 16, 16, NULL, NULL, DI_NORMAL);
					offx += 22;
				}
#if (defined _DEBUG)
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
			CStringA strURL = pClass->GetBrowserUrl();
			if (strURL != pClass->GetDocumentUrl())
			{
				pClass->SetDocumentUrl(strURL);
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
	                if (SUCCEEDED(browser->get_StatusBar(&isVisible)) && !isVisible)
	                {
		                browser->put_StatusBar(TRUE);
	                }

                    CPluginSettings* settings = CPluginSettings::GetInstance();

                    CPluginHttpRequest httpRequest(USERS_SCRIPT_WELCOME);

                    httpRequest.AddPluginId();
                    httpRequest.Add("username", CPluginClient::GetUserName(), false);
                    httpRequest.Add("errors", settings->GetErrorList());

			        HRESULT hr = browser->Navigate(CComBSTR(httpRequest.GetUrl()), NULL, NULL, NULL, NULL);
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
		
    case WM_DESTROY:
        break;

    case WM_UPDATEUISTATE:
        {
            s_isTabActivated = true;
        }
        break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}


void CPluginClass::UpdateStatusBar()
{
    DEBUG_GENERAL("*** Updating statusbar")

    s_criticalSectionLocal.Lock();
    {
        for (int i = 0; i < s_instances.GetSize(); i++)
        {
	        ::InvalidateRect(s_instances[i]->m_hPaneWnd, NULL, FALSE);
        }
    }
    s_criticalSectionLocal.Unlock();
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

// ============================================================================
// Element hiding
// ============================================================================

#ifdef SUPPORT_FILTER

void CPluginClass::HideElement(IHTMLElement* pEl, const CStringA& type, const CStringA& url, bool isDebug, CStringA& indent)
{
	CComPtr<IHTMLStyle> pStyle;

	if (SUCCEEDED(pEl->get_style(&pStyle)) && pStyle)
	{
#ifdef ENABLE_DEBUG_RESULT
        CComBSTR bstrDisplay;

        if (SUCCEEDED(pStyle->get_display(&bstrDisplay)) && bstrDisplay && CStringA(bstrDisplay) == "none")
        {
            return;
        }
#endif

		static const CComBSTR sbstrNone(L"none");

		if (SUCCEEDED(pStyle->put_display(sbstrNone)))
		{
            DEBUG_HIDE_EL(indent + "HideEl::Hiding " + type + " url:" + url)

#ifdef ENABLE_DEBUG_SELFTEST
	        DEBUG_SELFTEST("*** Hiding " + type + " url:" + url)
#endif

#ifdef ENABLE_DEBUG_RESULT
            if (isDebug)
            {
                CPluginDebug::DebugResultHiding(type, url, "-", "-");
            }
#endif
		}
	}
}


void CPluginClass::HideElementsLoop(IHTMLElement* pEl, IWebBrowser2* pBrowser, const CStringA& docName, const CStringA& domain, CStringA& indent, bool isCached)
{
    int  cacheIndex = -1;
    long cacheAllElementsCount = -1;
    bool isHidden = false;

    m_criticalSectionHideElement.Lock();
    {
        CComVariant vCacheIndex;
        if (isCached && SUCCEEDED(pEl->getAttribute(L"sab", 0, &vCacheIndex)) && vCacheIndex.vt == VT_I4)
        {
            cacheIndex = vCacheIndex.intVal;
            
            isHidden = m_cacheElements[cacheIndex].m_isHidden;

            cacheAllElementsCount = m_cacheElements[cacheIndex].m_elements;
        }
        else
        {
            isCached = false;

            cacheIndex = m_cacheIndexLast++;

            // Resize cache???            
            if (cacheIndex >= m_cacheElementsMax)
            {
                CElementHideCache* oldCacheElements = m_cacheElements;
                
                m_cacheElements = new CElementHideCache[2*m_cacheElementsMax];

                memcpy(m_cacheElements, oldCacheElements, m_cacheElementsMax*sizeof(CElementHideCache));

                m_cacheElementsMax *= 2;

                delete [] oldCacheElements;                
            }
            
            m_cacheElements[cacheIndex].m_isHidden = false;
            m_cacheElements[cacheIndex].m_elements = 0;

            vCacheIndex.vt = VT_I4;
            vCacheIndex.intVal = cacheIndex;
     
            pEl->setAttribute(L"sab", vCacheIndex);
        }
    }
    m_criticalSectionHideElement.Unlock();

    // Element is hidden - no need to continue
    if (isHidden)
    {
        return;
    }

    // Get number of elements in the scope of pEl
    long allElementsCount = 0;
    
    CComPtr<IDispatch> pAllCollectionDisp;

    if (SUCCEEDED(pEl->get_all(&pAllCollectionDisp)) && pAllCollectionDisp)
    {
        CComPtr<IHTMLElementCollection> pAllCollection;

        if (SUCCEEDED(pAllCollectionDisp->QueryInterface(IID_IHTMLElementCollection, (LPVOID*)&pAllCollection)) && pAllCollection)
        {
            // If number of elements = cached number, return
            if (SUCCEEDED(pAllCollection->get_length(&allElementsCount)) && allElementsCount == cacheAllElementsCount)
            {
                return;
            }
        }
    }

    // Update cache
    m_criticalSectionHideElement.Lock();
    {
        m_cacheElements[cacheIndex].m_elements = allElementsCount;
    }
    m_criticalSectionHideElement.Unlock();

    // Get tag
    CComBSTR bstrTag;
    if (FAILED(pEl->get_tagName(&bstrTag)) || !bstrTag)
    {
        return;
    }

    CStringA tag = bstrTag;
    tag.MakeLower();

    // Check if element is hidden
    CPluginClient* client = CPluginClientFactory::GetLazyClientInstance();

    m_cacheElements[cacheIndex].m_isHidden = client->IsElementHidden(tag, pEl, domain, indent);

    if (m_cacheElements[cacheIndex].m_isHidden)
    {
        HideElement(pEl, tag, "", false, indent);
        return;
    }

    // Special checks - hide already blocked elements

    // Images 
    if (tag == "img")
    {
		CComVariant vAttr;

		if (SUCCEEDED(pEl->getAttribute(L"src", 0, &vAttr)) && vAttr.vt == VT_BSTR && ::SysStringLen(vAttr.bstrVal) > 0)
		{
		    CStringA src = vAttr.bstrVal;
		    CPluginClient::UnescapeUrl(src);

			// If src should be blocked, set style display:none on image
		    m_cacheElements[cacheIndex].m_isHidden = client->ShouldBlock(src, CFilter::contentTypeImage, domain);
		    if (m_cacheElements[cacheIndex].m_isHidden)
		    {
			    HideElement(pEl, "image", src, true, indent);
			    return;
		    }
		}
	}
    // Objects
	else if (tag == "object")
	{
		CComBSTR bstrInnerHtml;

		if (SUCCEEDED(pEl->get_innerHTML(&bstrInnerHtml)) && bstrInnerHtml)
		{
			CStringA sObjectHtml = bstrInnerHtml;
			CStringA src;

		    int posBegin = sObjectHtml.Find("VALUE=\"");
		    int posEnd = posBegin >= 0 ? sObjectHtml.Find('\"', posBegin + 7) : -1;

		    while (posBegin >= 0 && posEnd >= 0)
		    {
			    posBegin += 7;

			    src = sObjectHtml.Mid(posBegin, posEnd - posBegin);

	            // eg. http://w3schools.com/html/html_examples.asp
			    if (src.Left(2) == "//")
			    {
				    src = "http:" + src;
			    }

			    if (!src.IsEmpty())
			    {
			        m_cacheElements[cacheIndex].m_isHidden = client->ShouldBlock(src, CFilter::contentTypeObject, domain);
		            if (m_cacheElements[cacheIndex].m_isHidden)
                    {
	                    HideElement(pEl, "object", src, true, indent);
	                    return;
                    }
			    }

			    posBegin = sObjectHtml.Find("VALUE=\"", posBegin);
			    posEnd = posBegin >= 0 ? sObjectHtml.Find("\"", posBegin + 7) : -1;
		    }
		}
	}	
	else if (tag == "iframe")
    {
        m_criticalSectionHideElement.Lock();
        {
            m_cacheDocumentHasIframes.insert(docName);
        }
        m_criticalSectionHideElement.Unlock();
	}
	else if (tag == "frame")
	{
	    m_criticalSectionHideElement.Lock();
	    {
            m_cacheDocumentHasFrames.insert(docName);
        }
	    m_criticalSectionHideElement.Unlock();
	}

    // Iterate through children of this element
    if (allElementsCount > 0)
    {
        long childElementsCount = 0;

        CComPtr<IDispatch> pChildCollectionDisp;
        if (SUCCEEDED(pEl->get_children(&pChildCollectionDisp)) && pChildCollectionDisp)
        {
            CComPtr<IHTMLElementCollection> pChildCollection;
            if (SUCCEEDED(pChildCollectionDisp->QueryInterface(IID_IHTMLElementCollection, (LPVOID*)&pChildCollection)) && pChildCollection)
            {
                pChildCollection->get_length(&childElementsCount);

	            CComVariant vIndex(0);

	            // Iterate through all children
	            for (long i = 0; i < childElementsCount; i++)
	            {
                    CComPtr<IDispatch> pChildElDispatch;
	                CComVariant vRetIndex;

	                vIndex.intVal = i;

		            if (SUCCEEDED(pChildCollection->item(vIndex, vRetIndex, &pChildElDispatch)) && pChildElDispatch)
		            {
                        CComPtr<IHTMLElement> pChildEl;
			            if (SUCCEEDED(pChildElDispatch->QueryInterface(IID_IHTMLElement, (LPVOID*)&pChildEl)) && pChildEl)
			            {
                            HideElementsLoop(pChildEl, pBrowser, docName, domain, indent, isCached);
			            }
		            }
	            }
            }
        }
    }
}

void CPluginClass::HideElements(IWebBrowser2* pBrowser, bool isMainDoc, const CStringA& docName, const CStringA& domain, CStringA indent)
{
	CPluginClient* client = CPluginClientFactory::GetLazyClientInstance();

	if (!client || !CPluginSettings::GetInstance()->IsPluginEnabled() || client->IsUrlWhiteListed(domain))
	{
	    return;
	}

#ifdef DEBUG_HIDE_EL
    DEBUG_HIDE_EL(indent + "HideEl doc:" + docName)

    indent += "  ";

    CPluginProfiler profiler;
#endif

    VARIANT_BOOL isBusy;    
    if (SUCCEEDED(pBrowser->get_Busy(&isBusy)))
    {
        if (isBusy)
        {
            return;
        }
    }

	// Get document
	CComPtr<IDispatch> pDocDispatch;
	HRESULT hr = pBrowser->get_Document(&pDocDispatch);
	if (FAILED(hr) || !pDocDispatch)
	{
	    return;
    }
    
	CComQIPtr<IHTMLDocument3> pDoc = pDocDispatch;
	if (!pDoc)
	{
	    return;
    }

	CComPtr<IHTMLElement> pBody;
	if (FAILED(pDoc->get_documentElement(&pBody)) || !pBody)
	{
	    return;
    }

    CComPtr<IHTMLElementCollection> pBodyCollection;
    if (FAILED(pDoc->getElementsByTagName(L"body", &pBodyCollection)) || !pBodyCollection)
    {
        return;
    }

    CComPtr<IHTMLElement> pBodyEl;
    {
        CComVariant vIndex(0);        
        CComPtr<IDispatch> pBodyDispatch;
        if (FAILED(pBodyCollection->item(vIndex, vIndex, &pBodyDispatch)) || !pBodyDispatch)
        {
            return;
        }

        if (FAILED(pBodyDispatch->QueryInterface(IID_IHTMLElement, (LPVOID*)&pBodyEl)) || !pBodyEl)
        {
            return;
        }
    }

    // Clear cache (if eg. refreshing) ???
    if (isMainDoc)
    {
        CComVariant vCacheIndex;
        
        if (FAILED(pBodyEl->getAttribute(L"sab", 0, &vCacheIndex)) || vCacheIndex.vt == VT_NULL)
        {
            ClearElementHideCache();
        }
    }

    // Hide elements in body part
    HideElementsLoop(pBodyEl, pBrowser, docName, domain, indent);

    // Check frames and iframes
    bool hasFrames = false;
    bool hasIframes = false;
    
    m_criticalSectionHideElement.Lock();
    {
        hasFrames = m_cacheDocumentHasFrames.find(docName) != m_cacheDocumentHasFrames.end();
        hasIframes = m_cacheDocumentHasIframes.find(docName) != m_cacheDocumentHasIframes.end();
    }
    m_criticalSectionHideElement.Unlock();

    // Frames
    if (hasFrames)
    {
        // eg. http://gamecopyworld.com/
	    long frameCount = 0;
	    CComPtr<IHTMLElementCollection> pFrameCollection;
	    if (SUCCEEDED(pDoc->getElementsByTagName(L"frame", &pFrameCollection)) && pFrameCollection)
	    {
		    pFrameCollection->get_length(&frameCount);
	    }

	    // Iterate through all frames
	    for (long i = 0; i < frameCount; i++)
	    {
		    CComVariant vIndex(i);
		    CComVariant vRetIndex;
		    CComPtr<IDispatch> pFrameDispatch;

		    if (SUCCEEDED(pFrameCollection->item(vIndex, vRetIndex, &pFrameDispatch)) && pFrameDispatch)
		    {
                CComQIPtr<IWebBrowser2> pFrameBrowser = pFrameDispatch;
                if (pFrameBrowser)
                {
                    CComBSTR bstrSrc;
                    CStringA src;

                    if (SUCCEEDED(pFrameBrowser->get_LocationURL(&bstrSrc)))
                    {
                        src = bstrSrc;
				        CPluginClient::UnescapeUrl(src);
                    }

                    if (!src.IsEmpty())
                    {
                        HideElements(pFrameBrowser, false, src, domain, indent);
                    }
                }
	        }
	    }
    }

    // Iframes
    if (hasIframes)
    {
	    long frameCount = 0;
	    CComPtr<IHTMLElementCollection> pFrameCollection;
	    if (SUCCEEDED(pDoc->getElementsByTagName(L"iframe", &pFrameCollection)) && pFrameCollection)
	    {
		    pFrameCollection->get_length(&frameCount);
	    }

	    // Iterate through all iframes
	    for (long i = 0; i < frameCount; i++)
	    {
		    CComVariant vIndex(i);
		    CComVariant vRetIndex;
		    CComPtr<IDispatch> pFrameDispatch;

		    if (SUCCEEDED(pFrameCollection->item(vIndex, vRetIndex, &pFrameDispatch)) && pFrameDispatch)
		    {
                CComQIPtr<IHTMLElement> pFrameEl = pFrameDispatch;
                if (pFrameEl)
                {
                    CComVariant vAttr;

                    if (SUCCEEDED(pFrameEl->getAttribute(L"src", 0, &vAttr)) && vAttr.vt == VT_BSTR && ::SysStringLen(vAttr.bstrVal) > 0)
                    {
		                CStringA src = vAttr.bstrVal;

		                // Some times, domain is missing. Should this be added on image src's as well?''
            		    
		                // eg. gadgetzone.com.au
		                if (src.Left(2) == "//")
		                {
			                src = "http:" + src;
		                }
		                // eg. http://w3schools.com/html/html_examples.asp
		                else if (src.Left(4) != "http" && src.Left(6) != "res://")
		                {
			                src = "http://" + domain + src;
		                }
		                
		                CPluginClient::UnescapeUrl(src);

			            // If src should be blocked, set style display:none on iframe
                        if (client->ShouldBlock(src, CFilter::contentTypeSubdocument, domain))
                        {
		                    HideElement(pFrameEl, "iframe", src, true, indent);
	                    }
	                    else
	                    {
                            CComQIPtr<IWebBrowser2> pFrameBrowser = pFrameDispatch;
                            if (pFrameBrowser)
		                    {
                                HideElements(pFrameBrowser, false, src, domain, indent);
                            }
                        }	                        
		            }
		        }
	        }
	    }
    }

    #ifdef DEBUG_HIDE_EL
    {
        profiler.StopTimer();

        CStringA el;
        el.Format("%u", m_cacheIndexLast);

        DEBUG_HIDE_EL(indent + "HideEl el:" + el + " time:" + profiler.GetElapsedTimeString(profileTime))

        profileTime += profiler.GetElapsedTime();
    }
    #endif

}

void CPluginClass::ClearElementHideCache()
{
    m_criticalSectionHideElement.Lock();
    {
        m_cacheIndexLast = 0;
        m_cacheDocumentHasFrames.clear();
        m_cacheDocumentHasIframes.clear();
        
        #ifdef DEBUG_HIDE_EL
        {
            profileTime = 0;
        }
        #endif
    }
    m_criticalSectionHideElement.Unlock();
}

#endif
