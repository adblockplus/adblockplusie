#include "PluginStdAfx.h"

#include <ctime>

#include "PluginClass.h"
#include "PluginDictionary.h"
#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginConfiguration.h"
#ifdef SUPPORT_FILTER
 #include "PluginFilter.h"
#endif
#ifdef SUPPORT_CONFIG
 #include "PluginConfig.h"
#endif
#include "PluginMimeFilterClient.h"

#include "PluginUpdateDialog.h"
#include "PluginDownloadDialog.h"
#include "PluginClient.h"
#include "PluginClientFactory.h"
#include "PluginWbPassThrough.h"
#include "PluginHttpRequest.h"

#include "ProtocolImpl.h"
#include "ProtocolCF.h"


HANDLE CPluginClass::s_hMainThread = NULL;
bool CPluginClass::s_isMainThreadDone = false;


DWORD WINAPI CPluginClass::MainThreadProc(LPVOID pParam)
{
	CPluginTab* tab = static_cast<CPluginTab*>(pParam);

    // Force loading/creation of settings
    CPluginSettings* settings = CPluginSettings::GetInstance();

    CPluginSystem* system = CPluginSystem::GetInstance();

    settings->SetMainThreadId();

    CString debugText;

    CString threadInfo;
    threadInfo.Format(L"%d.%d", ::GetCurrentProcessId(), ::GetCurrentThreadId());
    
    debugText += L"================================================================================";
    debugText += L"\nMAIN THREAD " + threadInfo + L" Plugin version:" + CString(IEPLUGIN_VERSION);
    debugText += L"\n================================================================================";

    debugText += L"\nPlugin version:    " + CString(IEPLUGIN_VERSION);
    debugText += L"\nPlugin id:         " + system->GetPluginId();
    debugText += L"\nMAC address:       " + system->GetMacId(true);
    debugText += L"\nComputer name:     " + system->GetComputerName();
    debugText += L"\nUser id:           " + settings->GetString(SETTING_USER_ID, "N/A");
    debugText += L"\nUser name:         " + system->GetUserName();
    debugText += L"\nBrowser version:   " + system->GetBrowserVersion();
    debugText += L"\nBrowser language:  " + system->GetBrowserLanguage();

    DWORD osVersion = ::GetVersion();

    CString ver;
    ver.Format(L"%d.%d", LOBYTE(LOWORD(osVersion)), HIBYTE(LOWORD(osVersion)));

    debugText += L"\nWindows version:   " + ver;
    
    CString proxyName;
    CString proxyBypass;
    
    if (CPluginHttpRequest::GetProxySettings(proxyName, proxyBypass))
    {
        if (!proxyName.IsEmpty())
        {
            debugText += L"\nHTTP proxy name:   " + proxyName;
        }
        if (!proxyBypass.IsEmpty())
        {
            debugText += L"\nHTTP proxy bypass: " + proxyBypass;
        }
    }

    debugText += L"\n================================================================================";

	DEBUG_GENERAL(debugText)

    HANDLE hMainThread = GetMainThreadHandle();

	CPluginClient* client = CPluginClient::GetInstance();
	client->SetLocalization();

	CPluginDictionary::GetInstance();

	// Timer settings for retrieving server client (settings from server)
    DWORD nNextServerClientTimerBase = GetTickCount() / TIMER_INTERVAL_SERVER_CLIENT_INIT + 1;
    DWORD nServerClientTimerBaseStep = 1;

    DWORD nNextUserTimerBase = GetTickCount() / TIMER_INTERVAL_USER_REGISTRATION + 1;
    DWORD nUserTimerBaseStep = 1;
    
    bool isConfigutationLoaded = false;
    
    std::auto_ptr<CPluginConfiguration> configuration = std::auto_ptr<CPluginConfiguration>(new CPluginConfiguration);

    // --------------------------------------------------------------------
    // Welcome / Info page
    // --------------------------------------------------------------------

    DEBUG_THREAD("Thread::Set welcome/info page");

    if (!IsMainThreadDone(hMainThread))
    {
        WORD wInfo = 0;
        WORD wInfoSettings = 0;

        wInfo = wInfoSettings = settings->GetValue(SETTING_PLUGIN_INFO_PANEL, 0);
        if (wInfo == 1)
        {
            DEBUG_GENERAL("*** Display welcome page")
        }
        else if (wInfo == 2)
        {
            DEBUG_GENERAL("*** Display update page")
        }
        else if (wInfo != 0)
        {
            DEBUG_GENERAL("*** Display info page")
        }

        if (wInfo != 0)
        {
            DEBUG_THREAD("Thread::Set info page (action)");

            s_criticalSectionLocal.Lock();
            {
				::PostMessage(tab->m_plugin->m_hPaneWnd, WM_LAUNCH_INFO, wInfo, NULL);
            }
            s_criticalSectionLocal.Unlock();

            if (wInfoSettings == wInfo)
            {
	            settings->Remove(SETTING_PLUGIN_INFO_PANEL);
                settings->Write();
            }
        }
    }

    // --------------------------------------------------------------------
    // Generate list of BHO guids
    // --------------------------------------------------------------------
/*
	if (!IsMainThreadDone(hMainThread))
	{
	    HKEY hKey;

        CString debugText;
        
        debugText += L"--------------------------------------------------------------------------------";
        debugText += L"\nBHO list";
        debugText += L"\n--------------------------------------------------------------------------------";

	    // Open the handler
	    if ((::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Browser Helper Objects"), 0, KEY_ENUMERATE_SUB_KEYS, &hKey)) == ERROR_SUCCESS)
	    {
            TCHAR szKeyName[255];
            DWORD dwIndex = 0;
            DWORD dwResult = 0;

            do
            {
                DWORD dwKeyNameSize = 255;

                dwResult = ::RegEnumKeyEx(hKey, dwIndex++, szKeyName, &dwKeyNameSize, NULL, NULL, NULL, NULL);
                if (dwResult == ERROR_SUCCESS || dwResult == ERROR_MORE_DATA)
                {
                    debugText += L"\n" + CString(szKeyName);
                }
            }
            while (dwResult == ERROR_SUCCESS || dwResult == ERROR_MORE_DATA);
            
            ::RegCloseKey(hKey);
	    }
	    else
	    {
            debugText += L"\nError opening registry";
	    }

        DEBUG_GENERAL(debugText)
    }
*/

	// --------------------------------------------------------------------
    // Should update plugin ?
    // --------------------------------------------------------------------

    if (!IsMainThreadDone(hMainThread) && settings->IsPluginUpdateAvailable())
    {
        DEBUG_THREAD(L"Thread::Should update plugin");

	    CString lastUpdateStr = settings->GetString(SETTING_PLUGIN_UPDATE_TIME);

	    std::time_t today = std::time(NULL);
	    std::time_t lastUpdate = lastUpdateStr.IsEmpty() ? today : _wtoi(lastUpdateStr.GetBuffer());

	    if (today != (std::time_t)(-1) && lastUpdate != (std::time_t)(-1))
	    {
		    if (today == lastUpdate || std::difftime(today, lastUpdate) / (60 * 60 * 24) >= 5.0)
		    {
		        CString updateVersion = settings->GetString(SETTING_PLUGIN_UPDATE_VERSION);

                DEBUG_GENERAL(L"*** Displaying update plugin dialog for version " + updateVersion);

			    // Show update dialog
			    CUpdateDialog uDlg;
			    
			    uDlg.SetVersions(updateVersion, _T(IEPLUGIN_VERSION));

			    if (uDlg.DoModal(::GetDesktopWindow()) == IDOK)
			    {
				    s_isPluginToBeUpdated = true;
			    }

			    settings->SetValue(SETTING_PLUGIN_UPDATE_TIME, (int)today);
			    settings->Write();
		    }
	    }
    }

	// --------------------------------------------------------------------
	// Main loop
	// --------------------------------------------------------------------

	DWORD mainLoopIteration = 1;
	
	bool hasUser = false;
	int regAttempts = 1;
	int regAttemptsThread = 1;
				
	while (!IsMainThreadDone(hMainThread))
	{
	    CString sMainLoopIteration;
	    sMainLoopIteration.Format(L"%u", mainLoopIteration);

        CString debugText;
        
        debugText += L"--------------------------------------------------------------------------------";
        debugText += L"\nLoop iteration " + sMainLoopIteration;
        debugText += L"\n--------------------------------------------------------------------------------";

        DEBUG_GENERAL(debugText)
        
        if (settings->Has(SETTING_REG_SUCCEEDED))
        {
            regAttempts = REGISTRATION_MAX_ATTEMPTS;
        }
        else
        {
            regAttempts = settings->GetValue(SETTING_REG_ATTEMPTS);
        }
        regAttempts = max(regAttempts, regAttemptsThread) + 1;

	    // --------------------------------------------------------------------
	    // Register user
	    // --------------------------------------------------------------------

        if (!IsMainThreadDone(hMainThread))
        {
            DEBUG_THREAD("Thread::Register user");

            hasUser = settings->Has(SETTING_USER_ID);

	        if (!hasUser && regAttempts <= REGISTRATION_MAX_ATTEMPTS)
	        {
		        DWORD nUserTimerBase = GetTickCount() / TIMER_INTERVAL_USER_REGISTRATION;

				if (nUserTimerBase >= nNextUserTimerBase || mainLoopIteration == 1 && !settings->IsFirstRun())
		        {
                    DEBUG_THREAD("Thread::Register user (action)");

			        try 
			        {
			            isConfigutationLoaded = configuration->Download();
			        }
			        catch (...)
			        {
			        }

			        nNextUserTimerBase = nUserTimerBase + nUserTimerBaseStep;
			        nUserTimerBaseStep *= 2;

			        regAttemptsThread++;
		        }
	        }
        }

	    // --------------------------------------------------------------------
	    // Load configuration
	    // --------------------------------------------------------------------

        if (!IsMainThreadDone(hMainThread))
        {
            DEBUG_THREAD("Thread::Load configuration");

	        if (hasUser && !isConfigutationLoaded && regAttempts <= REGISTRATION_MAX_ATTEMPTS)
	        {
		        // Initialize serverClient module
		        // we try an initialization in each loop
		        DWORD nServerClientTimerBase = GetTickCount() / TIMER_INTERVAL_SERVER_CLIENT_INIT;

		        if (nServerClientTimerBase >= nNextServerClientTimerBase || mainLoopIteration == 1)
		        {
                    DEBUG_THREAD("Thread::Load configuration (action)");

			        try 
			        {
			            isConfigutationLoaded = configuration->Download();
			        }
			        catch (...)
			        {
			        }

			        nNextServerClientTimerBase = nServerClientTimerBase + nServerClientTimerBaseStep;
			        nServerClientTimerBaseStep *= 2;

			        regAttemptsThread++;
		        }
	        }
        }

	    // --------------------------------------------------------------------
	    // Update settings
	    // --------------------------------------------------------------------
        
        if (!IsMainThreadDone(hMainThread))
        {
            DEBUG_THREAD("Thread::Update settings");

            if (configuration->IsValid())
            {
                bool isNewDictionaryVersion = false;
#ifdef SUPPORT_FILTER
                bool isNewFilterVersion = false;
#endif
#ifdef SUPPORT_CONFIG
                bool isNewConfig = false;
#endif

                DEBUG_THREAD("Thread::Update settings (action)");
                
                settings->ForceConfigurationUpdateOnStart(false);

                if (configuration->IsValidPluginExpired())
                {
                    settings->SetBool(SETTING_PLUGIN_EXPIRED, configuration->IsPluginExpired());
                }

			    if (configuration->IsValidUserId())
                {
				    settings->SetString(SETTING_USER_ID, configuration->GetUserId());
                }

			    if (configuration->IsValidPluginUpdate() && configuration->GetPluginUpdateVersion() != IEPLUGIN_VERSION)
                {
                    settings->SetString(SETTING_PLUGIN_UPDATE_URL, configuration->GetPluginUpdateUrl());
                    settings->SetString(SETTING_PLUGIN_UPDATE_VERSION, configuration->GetPluginUpdateVersion());
                }
                else
			    {
                    settings->Remove(SETTING_PLUGIN_UPDATE_VERSION);
                    settings->Remove(SETTING_PLUGIN_UPDATE_URL);
			    }

                if (configuration->IsValidPluginInfoPanel())
                {
				    settings->SetValue(SETTING_PLUGIN_INFO_PANEL, configuration->GetPluginInfoPanel());
			    }

                // Update dictionary
                if (configuration->IsValidDictionary())
                {
                    int currentVersion = settings->GetValue(SETTING_DICTIONARY_VERSION);
                    int newVersion = configuration->GetDictionaryVersion();

                    if (newVersion > currentVersion) 
                    {
                        isNewDictionaryVersion = true;
                    }
                }

#ifdef SUPPORT_CONFIG
                // Update config file download info
                if (configuration->IsValidConfig())
                {
					int currentVersion = settings->GetValue(SETTING_CONFIG_VERSION);
                    int newVersion = configuration->GetConfigVersion();

                    if (newVersion > currentVersion) 
                    {
                        isNewConfig = true;
                    }
                }
#endif // SUPPORT_CONFIG
                
#ifdef SUPPORT_FILTER
                // Update filter URL list
                if (configuration->IsValidFilter())
                {
                    isNewFilterVersion = true;
                }
#endif // SUPPORT_FILTER

#ifdef SUPPORT_WHITELIST
                // Update white list
                if (configuration->IsValidWhiteList())
                {
                    settings->ReplaceWhiteListedDomains(configuration->GetWhiteList());
                }
#endif // SUPPORT_WHITELIST

                // Check pluginID
                CString newPluginId = system->GetPluginId();
        	    
                if (newPluginId != settings->GetString(SETTING_PLUGIN_ID))
                {
                    DEBUG_GENERAL(L"*** pluginId has changed from " + settings->GetString(SETTING_PLUGIN_ID) + CString(L" to ") + newPluginId)

                    settings->SetString(SETTING_PLUGIN_ID, newPluginId);                
                }

                settings->SetString(SETTING_REG_SUCCEEDED, "true");     

				settings->SetBool(SETTING_PLUGIN_REGISTRATION, configuration->IsPluginRegistered());
				settings->SetValue(SETTING_PLUGIN_ADBLOCKLIMIT, configuration->GetAdBlockLimit());

                settings->Write();
                
                configuration->Invalidate();

				// Update dictionary
                if (isNewDictionaryVersion)
                {
					CPluginDictionary* dictionary = CPluginDictionary::GetInstance();

                    if (dictionary->Download(configuration->GetDictionaryUrl(), CPluginSettings::GetDataPath(DICTIONARY_INI_FILE)))
                    {
                        settings->SetValue(SETTING_DICTIONARY_VERSION, configuration->GetDictionaryVersion());
                        settings->Write();

                        settings->IncrementTabVersion(SETTING_TAB_DICTIONARY_VERSION);
                    }
                }

#ifdef SUPPORT_CONFIG
                // Update config file
                if (isNewConfig)
                {
					CPluginConfig* config = CPluginConfig::GetInstance();

                    if (config->Download(configuration->GetConfigUrl(), CPluginSettings::GetDataPath(CONFIG_INI_FILE)))
                    {
                        settings->SetValue(SETTING_CONFIG_VERSION, configuration->GetConfigVersion());
                        settings->Write();

                        settings->IncrementTabVersion(SETTING_TAB_CONFIG_VERSION);
                    }
                }
#endif // SUPPORT_CONFIG

#ifdef SUPPORT_FILTER
                // Update filters
                if (isNewFilterVersion)
                {
                    TFilterUrlList filterUrlList = configuration->GetFilterUrlList();
                    TFilterUrlList currentFilterUrlList = settings->GetFilterUrlList();

                    // Compare downloaded URL string with persistent URLs
                    for (TFilterUrlList::iterator it = filterUrlList.begin(); it != filterUrlList.end(); ++it) 
                    {
                        CString downloadFilterName = it->first;

                        CString filename = downloadFilterName.Trim().Right(downloadFilterName.GetLength() - downloadFilterName.ReverseFind('/') - 1).Trim();
                        int version = it->second;

                        TFilterUrlList::const_iterator fi = currentFilterUrlList.find(downloadFilterName);
                        if (fi == currentFilterUrlList.end() || fi->second != version)
                        {
                            CPluginFilter::DownloadFilterFile(downloadFilterName, filename);
                        }
                    }

                    settings->SetFilterUrlList(filterUrlList);
                    settings->SetValue(SETTING_FILTER_VERSION, configuration->GetFilterVersion());
                    settings->Write();

                    settings->IncrementTabVersion(SETTING_TAB_FILTER_VERSION);

					tab->OnUpdate();
                }
#endif // SUPPORT_FILTER
		    }
		    // Plugin not loaded
		    else if (regAttempts <= REGISTRATION_MAX_ATTEMPTS)
		    {
		        settings->SetValue(SETTING_REG_ATTEMPTS, regAttempts);
			    settings->Write();
            } 
        }

	    // --------------------------------------------------------------------
	    // Check filters
	    // --------------------------------------------------------------------
#ifdef SUPPORT_FILTER

        DEBUG_THREAD(L"Thread::Check filters");

        if (!IsMainThreadDone(hMainThread))
        {
            if (client->DownloadFirstMissingFilter())
            {
				tab->OnUpdate();
			}
        }

#endif // SUPPORT_FILTER

	    // --------------------------------------------------------------------
	    // Update plugin
	    // --------------------------------------------------------------------

        if (!IsMainThreadDone(hMainThread) && s_isPluginToBeUpdated)
        {
            DEBUG_GENERAL(L"*** Displaying download plugin dialog");

            s_isPluginToBeUpdated = false;

            try
            {
                CString updateUrl = settings->GetString(SETTING_PLUGIN_UPDATE_URL);
		        CString updatePath = CPluginSettings::GetTempPath(INSTALL_MSI_FILE);

				// Delete old installer
				::DeleteFile(CPluginSettings::GetTempPath(INSTALL_MSI_FILE));

		        CPluginDownloadDialog dlDlg;
		        
		        dlDlg.SetUrlAndPath(updateUrl, updatePath);
		        if (dlDlg.DoModal(::GetDesktopWindow()) == IDC_INSTALLBTN)
		        {
			        LaunchUpdater(updatePath);
#ifdef AUTOMATIC_SHUTDOWN
					settings->EraseTab();
			        ::ExitProcess(0);
#endif // AUTOMATIC_SHUTDOWN
		        }
            }
            catch (std::runtime_error&)
            {
            }
        }

        // ----------------------------------------------------------------
        // End loop
        // ----------------------------------------------------------------

        if (!IsMainThreadDone(hMainThread))
        {
		    bool isDone = false;
	        DWORD sleepLoopIteration = 1;

		    // Sleep loop
		    while (!isDone && !IsMainThreadDone(hMainThread) && !s_isPluginToBeUpdated)
		    {
			    // Non-hanging sleep
			    Sleep(50);

			    if (hasUser || regAttempts >= REGISTRATION_MAX_ATTEMPTS)
			    {
				    if (sleepLoopIteration++ % (TIMER_THREAD_SLEEP_MAIN_LOOP / 50) == 0)
				    {
					    isDone = true;
				    }
			    }
			    else if (sleepLoopIteration++ % (TIMER_THREAD_SLEEP_USER_REGISTRATION / 50) == 0)
			    {
				    isDone = true;
			    }
		    }
		}

	    mainLoopIteration++;
	}

	return 0;
}


HANDLE CPluginClass::GetMainThreadHandle()
{
    HANDLE handle = NULL;

    s_criticalSectionLocal.Lock();
    {
        handle = s_hMainThread;
    }
    s_criticalSectionLocal.Unlock();
    
    return handle;
}


bool CPluginClass::IsMainThreadDone(HANDLE mainThread)
{
    bool isDone = false;

    s_criticalSectionLocal.Lock();
    {
        isDone = s_isMainThreadDone || mainThread != s_hMainThread;
    }
    s_criticalSectionLocal.Unlock();

    return isDone;
}
