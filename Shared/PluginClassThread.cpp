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

  while (!IsMainThreadDone(hMainThread))
  {
    CString sMainLoopIteration;
    sMainLoopIteration.Format(L"%u", mainLoopIteration);

    CString debugText;

    debugText += L"--------------------------------------------------------------------------------";
    debugText += L"\nLoop iteration " + sMainLoopIteration;
    debugText += L"\n--------------------------------------------------------------------------------";

    DEBUG_GENERAL(debugText)

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

          if (configuration->IsValidPluginInfoPanel())
          {
            settings->SetValue(SETTING_PLUGIN_INFO_PANEL, configuration->GetPluginInfoPanel());
          }

#ifdef SUPPORT_FILTER
          // Update filter URL list
          if (configuration->IsValidFilter())
          {
            isNewFilterVersion = true;
          }
#endif // SUPPORT_FILTER

          settings->Write();

          configuration->Invalidate();

#ifdef SUPPORT_FILTER
          DEBUG_GENERAL("*** before isNewFilterVersion");

          // Update filters, if needed (5 days * (random() * 0.4 + 0.8))
          if (isNewFilterVersion)
          {

            DEBUG_GENERAL("*** before CheckFilterAndDownload");
            settings->CheckFilterAndDownload();

            settings->MakeRequestForUpdate();


            settings->Write();

            tab->OnUpdate();
          }
#endif // SUPPORT_FILTER
        }
      }


#ifndef ENTERPRISE
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
          CString updatePath = L"";
          if (updateUrl.Find(L".exe") == updateUrl.GetLength() - 4)
          {
            updatePath = CPluginSettings::GetTempPath(INSTALL_EXE_FILE);
            // Delete old installer
            ::DeleteFile(CPluginSettings::GetTempPath(INSTALL_EXE_FILE));
          }
          else
          {
            updatePath = CPluginSettings::GetTempPath(INSTALL_MSI_FILE);
            // Delete old installer
            ::DeleteFile(CPluginSettings::GetTempPath(INSTALL_MSI_FILE));
          }

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
        catch (std::runtime_error& er)
        {
          DEBUG_ERROR(er.what());
        }
      }
#endif
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
          Sleep(5000);

          if (sleepLoopIteration++ % (TIMER_THREAD_SLEEP_USER_REGISTRATION) == 0)
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
