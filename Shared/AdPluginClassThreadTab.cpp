#include "AdPluginStdAfx.h"

#include <ctime>

#include "AdPluginClass.h"
#include "AdPluginDictionary.h"
#include "AdPluginSettings.h"
#ifdef SUPPORT_FILTER
#include "AdPluginFilterClass.h"
#endif
#ifdef SUPPORT_CONFIG
#include "AdPluginConfig.h"
#endif
#include "AdPluginMimeFilterClient.h"

#include "AdPluginClient.h"
#include "AdPluginClientFactory.h"
#include "AdPluginWbPassThrough.h"
#include "AdPluginHttpRequest.h"

#include "ProtocolImpl.h"
#include "ProtocolCF.h"


HANDLE CPluginClass::s_hTabThread = NULL;
bool CPluginClass::s_isTabThreadDone = false;


DWORD WINAPI CPluginClass::TabThreadProc(LPVOID pParam)
{
    // Force loading/creation of settings
    CPluginSettings* settings = CPluginSettings::GetInstance();

    settings->SetWorkingThreadId();

    CString threadInfo;
    threadInfo.Format(L"%d.%d", ::GetCurrentProcessId(), ::GetCurrentThreadId());
    
    CString debugText;
    
    debugText += L"================================================================================";
    debugText += L"\nTAB THREAD " + threadInfo;
    debugText += L"\n================================================================================";

    DEBUG_GENERAL(debugText)

    HANDLE hTabThread = GetTabThreadHandle();

	CPluginClient* client = NULL;
        
    // Force loading/creation of dictionary
    CPluginDictionary::GetInstance();

	// --------------------------------------------------------------------
	// Initialize local client
	// --------------------------------------------------------------------

	if (!IsTabThreadDone(hTabThread))
	{
        DEBUG_THREAD("Thread::Initialize local client");

	    // Loop here until the client are intialized
	    // the first time a user runs the bho, we will try to contact the server to generate a plugin
	    // If the server is down we need to send requests with a decreasing frequency to avoid hitting the server to hard
	    // we cannot sleep for increasing periods of time, because then the browser can hang if the user exists

	    while (!IsTabThreadDone(hTabThread))
	    {
            DEBUG_THREAD("Thread::Initialize local client (action)");

		    try 
		    {
			    client = CPluginClientFactory::GetClientInstance();
			    // The client has been initialized, we can continue
			    break;
		    }
		    catch (std::runtime_error)
		    {
    		    DEBUG_ERROR("Thread::Init local client failed")
		    }
		    catch (...)
		    {
    		    DEBUG_ERROR("Thread::Init local client failed")
		    }

		    Sleep(50); 
	    }
    }

	// --------------------------------------------------------------------
	// Tab loop
	// --------------------------------------------------------------------

	DWORD loopCount = 0;
	DWORD tabLoopIteration = 1;
			
	while (!IsTabThreadDone(hTabThread))
	{
#ifdef ENABLE_DEBUG_THREAD
	    CStringA sTabLoopIteration;
	    sTabLoopIteration.Format("%u", tabLoopIteration);
	    
        DEBUG_THREAD("--------------------------------------------------------------------------------")
        DEBUG_THREAD("Loop iteration " + sTabLoopIteration);
        DEBUG_THREAD("--------------------------------------------------------------------------------")
#endif
        // Update settings from file
        if (s_isTabActivated)
        {
            bool isChanged = false;

            settings->RefreshTab();

            int newSettingsVersion = settings->GetTabVersion(SETTING_TAB_SETTINGS_VERSION);
            if (s_settingsVersion != newSettingsVersion)
            {
                s_settingsVersion = newSettingsVersion;
                if (!settings->IsMainProcess())
                {
                    settings->Read();
                }
            }

            int newDictionaryVersion = settings->GetTabVersion(SETTING_TAB_DICTIONARY_VERSION);
            if (s_dictionaryVersion != newDictionaryVersion)
            {
                s_dictionaryVersion = newDictionaryVersion;
                client->SetLocalization();
                isChanged = true;
            }

#ifdef SUPPORT_CONFIG
            int newConfigVersion = settings->GetTabVersion(SETTING_TAB_CONFIG_VERSION);
            if (s_configVersion != newConfigVersion)
            {
                s_configVersion = newConfigVersion;
                client->ClearCache();
                isChanged = true;
#ifdef SUPPORT_DOM_TRAVERSER
#ifdef PRODUCT_DOWNLOADHELPER
				UpdateConfig();
#endif
#endif
			}
#endif // SUPPORT_CONFIG

#ifdef SUPPORT_WHITELIST
            int newWhitelistVersion = settings->GetTabVersion(SETTING_TAB_WHITELIST_VERSION);
            if (s_whitelistVersion != newWhitelistVersion)
            {
                s_whitelistVersion = newWhitelistVersion;
                settings->RefreshWhitelist();
                client->ClearCache();
                isChanged = true;
            }
#endif // SUPPORT_WHITELIST

#ifdef SUPPORT_FILTER
            int newFilterVersion = settings->GetTabVersion(SETTING_TAB_FILTER_VERSION);
            if (s_filterVersion != newFilterVersion)
            {
                s_filterVersion = newFilterVersion;
                client->ReadFilters();
                isChanged = true;
            }
#endif
            if (isChanged)
            {
                UpdateStatusBar();
            }

            s_isTabActivated = false;
        }

	    // --------------------------------------------------------------------
	    // End loop
	    // --------------------------------------------------------------------

	    // Sleep loop
	    while (!IsTabThreadDone(hTabThread) && !s_isTabActivated && (++loopCount % (TIMER_THREAD_SLEEP_TAB_LOOP / 50)) != 0)
	    {
            // Post async plugin error
            CPluginError pluginError;
            if (CPluginClient::PopFirstPluginError(pluginError))
            {
                CPluginClient::LogPluginError(pluginError.GetErrorCode(), pluginError.GetErrorId(), pluginError.GetErrorSubid(), pluginError.GetErrorDescription(), true, pluginError.GetProcessId(), pluginError.GetThreadId());
            }

		    // Non-hanging sleep
		    Sleep(50);
	    }
	    
	    tabLoopIteration++;
	}

	return 0;
}


HANDLE CPluginClass::GetTabThreadHandle()
{
    HANDLE handle = NULL;

    s_criticalSectionLocal.Lock();
    {
        handle = s_hTabThread;
    }
    s_criticalSectionLocal.Unlock();
    
    return handle;
}


bool CPluginClass::IsTabThreadDone(HANDLE tabThread)
{
    bool isDone = false;

    s_criticalSectionLocal.Lock();
    {
        isDone = s_isTabThreadDone || tabThread != s_hTabThread;
    }
    s_criticalSectionLocal.Unlock();

    return isDone;
}
