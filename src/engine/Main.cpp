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

#pragma comment(linker,"\"/manifestdependency:type='win32' \
  name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
  processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <AdblockPlus.h>
#include <functional>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <Windows.h>

#include "../shared/AutoHandle.h"
#include "../shared/Communication.h"
#include "../shared/Dictionary.h"
#include "../shared/Utils.h"
#include "../shared/Version.h"
#include "../shared/CriticalSection.h"
#include "IeVersion.h"
#include "AdblockPlus.h"
#include "Debug.h"
#include "Updater.h"
#include "Registry.h"
#include "NotificationWindow.h"

namespace
{
  struct ScopedAtlAxInitializer
  {
    ScopedAtlAxInitializer()
    {
      ATL::AtlAxWinInit();
    }
    ~ScopedAtlAxInitializer()
    {
      ATL::AtlAxWinTerm();
    }
  };

  class ABPAtlModule : public ATL::CAtlExeModuleT<ABPAtlModule>
  {
    enum CustomMessages
    {
      TASK_POSTED = WM_USER + 1
    };

  public:
    ABPAtlModule() : m_msgHWnd(nullptr)
    {
    }
    void Finalize();
    HRESULT PreMessageLoop(int showCmd) throw();
    void RunMessageLoop() throw();
    static HRESULT InitializeCom() throw()
    {
      // The default implementation initializes multithreaded version but
      // in this case hosted ActiveX does not properly work.
      return CoInitialize(nullptr);
    }
  private:
    void onNewNotification(const AdblockPlus::NotificationPtr& notification);
    void DispatchTask(std::function<void()>&& task);
    void ProcessTasks();

    ScopedAtlAxInitializer m_scopedAtlAxInit;
    HWND m_msgHWnd;
    std::recursive_mutex m_tasksMutex;
    std::deque<std::function<void()>> m_tasks;
    std::unique_ptr<NotificationBorderWindow> m_notificationWindow;
  } _AtlModule;

  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine;
  std::auto_ptr<Updater> updater;
  int activeConnections = 0;
  CriticalSection activeConnectionsLock;
  HWND callbackWindow;

  void WriteSubscriptions(Communication::OutputBuffer& response,
      const std::vector<AdblockPlus::SubscriptionPtr>& subscriptions)
  {
    int32_t count = static_cast<int32_t>(subscriptions.size());
    response << count;
    for (int32_t i = 0; i < count; i++)
    {
      AdblockPlus::SubscriptionPtr subscription = subscriptions[i];
      response << subscription->GetProperty("url")->AsString()
               << subscription->GetProperty("title")->AsString()
               << subscription->GetProperty("specialization")->AsString()
               << subscription->IsListed();
    }
  }

  bool updateAvailable;
  bool checkingForUpdate = false;
  void UpdateCallback(const std::string res)
  {
    UINT message;
    if (updateAvailable)
    {
      message = WM_DOWNLOADING_UPDATE;
    }
    else if (res.length() == 0)
    {
      message = WM_ALREADY_UP_TO_DATE;
    }
    else 
    {
      message = WM_UPDATE_CHECK_ERROR;
    }
    if (callbackWindow)
    {
      SendMessage(callbackWindow, message, 0, 0);
      checkingForUpdate = false;
      callbackWindow = 0;
    }
    return;
  }


  CriticalSection firstRunLock;
  CriticalSection updateCheckLock;
  bool firstRunActionExecuted = false;
  AdblockPlus::ReferrerMapping referrerMapping;
  Communication::OutputBuffer HandleRequest(Communication::InputBuffer& request)
  {
    Communication::OutputBuffer response;

    Communication::ProcType procedure;
    request >> procedure;
    switch (procedure)
    {
      case Communication::PROC_MATCHES:
      {
        std::string url;
        using namespace AdblockPlus;
        std::string documentUrl;
        int32_t type;
        request >> url >> type >> documentUrl;
        referrerMapping.Add(url, documentUrl);
        auto contentType = static_cast<FilterEngine::ContentType>(type);
        FilterPtr filter = filterEngine->Matches(url, contentType, referrerMapping.BuildReferrerChain(documentUrl));
        response << (filter && filter->GetType() != Filter::TYPE_EXCEPTION);
        break;
      }
      case Communication::PROC_GET_ELEMHIDE_SELECTORS:
      {
        std::string domain;
        request >> domain;
        response << filterEngine->GetElementHidingSelectors(domain);
        break;
      }
      case Communication::PROC_AVAILABLE_SUBSCRIPTIONS:
      {
        WriteSubscriptions(response, filterEngine->FetchAvailableSubscriptions());
        break;
      }
      case Communication::PROC_LISTED_SUBSCRIPTIONS:
      {
        WriteSubscriptions(response, filterEngine->GetListedSubscriptions());
        break;
      }
      case Communication::PROC_SET_SUBSCRIPTION:
      {
        std::string url;
        request >> url;

        AdblockPlus::JsValuePtr valuePtr = filterEngine->GetPref("subscriptions_exceptionsurl");
        std::string aaUrl = "";
        if (!valuePtr->IsNull())
        {
          aaUrl = valuePtr->AsString();
        }
        std::vector<AdblockPlus::SubscriptionPtr> subscriptions = filterEngine->GetListedSubscriptions();

        // Remove all subscriptions, besides the Acceptable Ads
        for (size_t i = 0, count = subscriptions.size(); i < count; i++)
        {
          if (subscriptions[i]->GetProperty("url")->AsString() != aaUrl)
          {
            subscriptions[i]->RemoveFromList();
          }
        }

        filterEngine->GetSubscription(url)->AddToList();
        break;
      }
      case Communication::PROC_ADD_SUBSCRIPTION:
      {
        std::string url;
        request >> url;

        filterEngine->GetSubscription(url)->AddToList();
        break;
      }
      case Communication::PROC_REMOVE_SUBSCRIPTION:
      {
        std::string url;
        request >> url;

        filterEngine->GetSubscription(url)->RemoveFromList();
        break;
      }
      case Communication::PROC_UPDATE_ALL_SUBSCRIPTIONS:
      {
        std::vector<AdblockPlus::SubscriptionPtr> subscriptions = filterEngine->GetListedSubscriptions();
        for (size_t i = 0, count = subscriptions.size(); i < count; i++)
          subscriptions[i]->UpdateFilters();
        break;
      }
      case Communication::PROC_GET_EXCEPTION_DOMAINS:
      {
        std::vector<AdblockPlus::FilterPtr> filters = filterEngine->GetListedFilters();
        std::vector<std::string> domains;
        for (size_t i = 0, count = filters.size(); i < count; i++)
        {
          AdblockPlus::FilterPtr filter = filters[i];
          if (filter->GetType() == AdblockPlus::Filter::TYPE_EXCEPTION)
          {
            std::string text = filter->GetProperty("text")->AsString();

            //@@||example.com^$document
            const char prefix[] = "@@||";
            const char suffix[] = "^$document";
            const size_t prefixLen = strlen(prefix);
            const size_t suffixLen = strlen(suffix);
            if (!text.compare(0, prefixLen, prefix) &&
                !text.compare(text.size() - suffixLen, suffixLen, suffix))
            {
              domains.push_back(text.substr(prefixLen, text.size() - prefixLen - suffixLen));
            }
          }
        }

        response << domains;
        break;
      }
      case Communication::PROC_GET_WHITELISTING_FITER:
      {
        std::string url;
        request >> url;
        AdblockPlus::FilterPtr match = filterEngine->Matches(url,
          AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_DOCUMENT, url);
        std::string filterText;
        if (match && match->GetType() == AdblockPlus::Filter::TYPE_EXCEPTION)
        {
          filterText = match->GetProperty("text")->AsString();
        }
        response << filterText;
        break;
      }
      case Communication::PROC_IS_ELEMHIDE_WHITELISTED_ON_URL:
      {
        std::string url;
        request >> url;
        AdblockPlus::FilterPtr match = filterEngine->Matches(url,
          AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_ELEMHIDE, url);
        response << (match && match->GetType() == AdblockPlus::Filter::TYPE_EXCEPTION);
        break;
      }
      case Communication::PROC_ADD_FILTER:
      {
        std::string text;
        request >> text;

        filterEngine->GetFilter(text)->AddToList();
        break;
      }
      case Communication::PROC_REMOVE_FILTER:
      {
        std::string text;
        request >> text;
        filterEngine->GetFilter(text)->RemoveFromList();
        break;
      }
      case Communication::PROC_SET_PREF:
      {
        std::string prefName;
        request >> prefName;

        Communication::ValueType valueType = request.GetType();
        switch (valueType)
        {
        case Communication::TYPE_STRING:
          {
            std::string prefValue;
            request >> prefValue;
            filterEngine->SetPref(prefName, filterEngine->GetJsEngine()->NewValue(prefValue));
            break;
          }
        case Communication::TYPE_INT64:
          {
            int64_t prefValue;
            request >> prefValue;
            filterEngine->SetPref(prefName, filterEngine->GetJsEngine()->NewValue(prefValue));
            break;
          }
        case Communication::TYPE_INT32:
          {
            int prefValue;
            request >> prefValue;
            filterEngine->SetPref(prefName, filterEngine->GetJsEngine()->NewValue(prefValue));
            break;
          }
        case Communication::TYPE_BOOL:
          {
            bool prefValue;
            request >> prefValue;
            filterEngine->SetPref(prefName, filterEngine->GetJsEngine()->NewValue(prefValue));
            break;
          }
        default:
          break;
        }
        break;
      }
      case Communication::PROC_GET_PREF:
      {
        std::string name;
        request >> name;

        AdblockPlus::JsValuePtr valuePtr = filterEngine->GetPref(name);
        if (valuePtr->IsBool())
        {
          response << true;
          response << valuePtr->AsBool();
        }
        else if (valuePtr->IsNumber())
        {
          response << true;
          response << valuePtr->AsInt();
        }
        else if (valuePtr->IsString())
        {
          response << true;
          response << valuePtr->AsString();
        }
        else
        {
          // Report failure
          response << false;
        }
        break;
      }
      case Communication::PROC_CHECK_FOR_UPDATES:
      {
        request >> (int32_t&)callbackWindow;
        CriticalSection::Lock lock(updateCheckLock);
        if (!checkingForUpdate)
        {
          updateAvailable = false;
          checkingForUpdate = true;
          filterEngine->ForceUpdateCheck(UpdateCallback);
        }
        break;
      }
      case Communication::PROC_IS_FIRST_RUN_ACTION_NEEDED:
      {
        CriticalSection::Lock lock(firstRunLock);
        if (!firstRunActionExecuted && filterEngine->IsFirstRun())
        {
          response << true;
          firstRunActionExecuted = true;
        }
        else
        {
          response << false;
        }
        break;
      }
      case Communication::PROC_COMPARE_VERSIONS:
      {
        std::string v1, v2;
        request >> v1 >> v2;

        response << filterEngine->CompareVersions(v1, v2);
        break;
      }
      case Communication::PROC_GET_DOCUMENTATION_LINK:
      {
        response << filterEngine->GetPref("documentation_link")->AsString();
        break;
      }
      case Communication::PROC_TOGGLE_PLUGIN_ENABLED:
      {
        filterEngine->SetPref("enabled", filterEngine->GetJsEngine()->NewValue(!filterEngine->GetPref("enabled")->AsBool()));
        response << filterEngine->GetPref("enabled")->AsBool();
        break;
      }
      case Communication::PROC_GET_HOST:
      {
        std::string url;
        request >> url;
        std::string host = filterEngine->GetHostFromURL(url);
        if (host.empty())
        {
          response << url;
        }
        else
        {
          response << host;
        }
        break;
      }
    }
    return response;
  }

  void ClientThread(Communication::Pipe* pipe)
  {
    std::stringstream stream;
    stream << GetCurrentThreadId();
    std::string threadId = stream.str();
    std::string threadString = "(Thread ID: " + threadId + ")";
    Debug("Client connected " + threadString);

    {
      CriticalSection::Lock lock(activeConnectionsLock);
      activeConnections++;
    }

    for (;;)
    {
      try
      {
        Communication::InputBuffer message = pipe->ReadMessage();
        Communication::OutputBuffer response = HandleRequest(message);
        pipe->WriteMessage(response);
      }
      catch (const Communication::PipeDisconnectedError&)
      {
        break;
      }
      catch (const std::exception& e)
      {
        DebugException(e);
        break;
      }
    }

    Debug("Client disconnected " + threadString);

    {
      CriticalSection::Lock lock(activeConnectionsLock);
      activeConnections--;
      if (activeConnections < 1)
      {
        Debug("No connections left, shutting down the engine");
        activeConnections = 0;

        // The following exit(0) calls the destructor of _AtlModule from the
        // current thread which results in the disaster because there is a
        // running message loop as well as there can be alive notification
        // window which holds v8::Value as well as m_tasks can hold v8::Value
        // but JS Engine is destroyed before _AtlModule. BTW, various free
        // running threads like Timeout also cause the crash because the engine
        // is already destroyed.
        _AtlModule.Finalize();
        exit(0);
      }
    }

  }

  void OnUpdateAvailable(AdblockPlus::JsValueList& params)
  {
    if (params.size() < 1)
    {
      Debug("updateAvailable event missing URL");
      return;
    }
    updateAvailable = true;

    updater->Update(params[0]->AsString());
  }

  std::wstring PreconfigurationValueFromRegistry(const std::wstring& preconfigName)
  {
    try
    {
      AdblockPlus::RegistryKey regKey(HKEY_CURRENT_USER, L"Software\\AdblockPlus");
      return regKey.value_wstring(preconfigName);
    }
    catch (const std::runtime_error&)
    {
      return L"";
    }
  }

std::auto_ptr<AdblockPlus::FilterEngine> CreateFilterEngine(const std::wstring& locale)
{
  AdblockPlus::AppInfo appInfo;
  appInfo.version = ToUtf8String(IEPLUGIN_VERSION);
  appInfo.name = "adblockplusie";
#ifdef _WIN64
  appInfo.application = "msie64";
#else
  appInfo.application = "msie32";
#endif
  appInfo.applicationVersion = ToUtf8String(AdblockPlus::IE::InstalledVersionString());
  appInfo.locale = ToUtf8String(locale);
#ifdef ADBLOCK_PLUS_TEST_MODE
  appInfo.developmentBuild = true;
#else
  appInfo.developmentBuild = false;
#endif

  AdblockPlus::JsEnginePtr jsEngine = AdblockPlus::JsEngine::New(appInfo);
  jsEngine->SetEventCallback("updateAvailable", &OnUpdateAvailable);

  std::string dataPath = ToUtf8String(GetAppDataPath());
  dynamic_cast<AdblockPlus::DefaultFileSystem*>(jsEngine->GetFileSystem().get())->SetBasePath(dataPath);
  std::map<std::string, AdblockPlus::JsValuePtr> preconfig;
  preconfig["disable_auto_updates"] = jsEngine->NewValue(
    PreconfigurationValueFromRegistry(L"disable_auto_updates") == L"true");
  preconfig["suppress_first_run_page"] = jsEngine->NewValue(
    PreconfigurationValueFromRegistry(L"suppress_first_run_page") == L"true");
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine(new AdblockPlus::FilterEngine(jsEngine, preconfig));
  return filterEngine;
}

void ABPAtlModule::Finalize()
{
  std::condition_variable cv;
  std::mutex cvMutex;
  std::unique_lock<std::mutex> lock(cvMutex);
  DispatchTask([&cvMutex, &cv, this]
  {
    if (m_notificationWindow)
    {
      m_notificationWindow->SendMessage(WM_CLOSE);
    }
    SendMessage(m_msgHWnd, WM_QUIT, 0, 0);
    {
      std::lock_guard<std::recursive_mutex> lock(m_tasksMutex);
      m_tasks.clear();
    }
    std::unique_lock<std::mutex> lock(cvMutex);
    cv.notify_one();
  });
  cv.wait(lock);
}

HRESULT ABPAtlModule::PreMessageLoop(int showCmd) throw()
{
  const std::wstring className = L"ABPEngineMessageWindow";
  WNDCLASS wc;
  wc.style = 0;
  wc.lpfnWndProc = DefWindowProcW;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = ATL::_AtlBaseModule.GetModuleInstance();
  wc.hIcon = nullptr;
  wc.hCursor = nullptr;
  wc.hbrBackground = nullptr;
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = className.c_str();
  ATOM atom = RegisterClass(&wc);
  if (!atom)
  {
    DebugLastError("Cannot register class for message only window");
    return E_FAIL;
  }
  m_msgHWnd = CreateWindowW(className.c_str(),
    nullptr,      // window name
    0,            // style
    0, 0, 0, 0,   // geometry (x, y, w, h)
    HWND_MESSAGE, // parent
    nullptr,      // menu handle
    wc.hInstance,
    0);           // windows creation data.
  if (!m_msgHWnd)
  {
    DebugLastError("Cannot create message only window");
    return E_FAIL;
  }

  filterEngine->SetShowNotificationCallback([this](const AdblockPlus::NotificationPtr& notification)
  {
    if (!notification)
    {
      return;
    }
    DispatchTask([notification, this]
    {
      onNewNotification(notification);
    });
  });

  HRESULT retValue = __super::PreMessageLoop(showCmd);
  // __super::PreMessageLoop returns S_FALSE because there is nothing to
  // register but S_OK is required to run message loop.
  return FAILED(retValue) ? retValue : S_OK;
}

void ABPAtlModule::RunMessageLoop() throw()
{
  MSG msg = {};
  while (GetMessage(&msg, /*hwnd*/nullptr, /*msgFilterMin*/0, /*msgFilterMax*/0))
  {
    if (msg.hwnd == m_msgHWnd && msg.message == CustomMessages::TASK_POSTED)
    {
      ProcessTasks();
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

void ABPAtlModule::onNewNotification(const AdblockPlus::NotificationPtr& notification)
{
  m_notificationWindow.reset(new NotificationBorderWindow(*notification, GetExeDir() + L"html\\templates\\"));
  m_notificationWindow->SetOnDestroyed([notification, this]
  {
    notification->MarkAsShown();
    m_notificationWindow.reset();
  });
  m_notificationWindow->SetOnLinkClicked([](const std::wstring& url)
  {
    ATL::CComPtr<IWebBrowser2> webBrowser;
    if (SUCCEEDED(webBrowser.CoCreateInstance(__uuidof(InternetExplorer))) && webBrowser)
    {
      ATL::CComVariant emptyVariant;
      webBrowser->Navigate(ATL::CComBSTR(url.c_str()), &emptyVariant, &emptyVariant, &emptyVariant, &emptyVariant);
      webBrowser->put_Visible(VARIANT_TRUE);
    }
  });
  m_notificationWindow->Create(/*parent window*/nullptr);
  if (m_notificationWindow->operator HWND() != nullptr)
  {
    m_notificationWindow->ShowWindow(SW_SHOWNOACTIVATE);
    m_notificationWindow->UpdateWindow();
  }
}

void ABPAtlModule::DispatchTask(std::function<void()>&& task)
{
  {
    std::lock_guard<std::recursive_mutex> lock(m_tasksMutex);
    m_tasks.emplace_back(std::move(task));
  }
  PostMessageW(m_msgHWnd, CustomMessages::TASK_POSTED, 0, 0);
}

void ABPAtlModule::ProcessTasks()
{
  std::lock_guard<std::recursive_mutex> lock(m_tasksMutex);
  while(!m_tasks.empty())
  {
    auto task = *m_tasks.begin();
    m_tasks.pop_front();
    if (task)
      task();
  }
}

} // namespace {

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int cmdShow)
{
  AutoHandle mutex(CreateMutexW(0, false, L"AdblockPlusEngine"));
  if (!mutex)
  {
    DebugLastError("CreateMutex failed");
    return 1;
  }

  if (GetLastError() == ERROR_ALREADY_EXISTS)
  {
    DebugLastError("Named pipe exists, another engine instance appears to be running");
    return 1;
  }

  int argc;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::wstring locale(argc >= 2 ? argv[1] : L"");
  LocalFree(argv);
  Dictionary::Create(locale);
  filterEngine = CreateFilterEngine(locale);
  updater.reset(new Updater(filterEngine->GetJsEngine()));

  std::thread communicationThread([]
  {
    for (;;)
    {
      try
      {
        auto pipe = std::make_shared<Communication::Pipe>(Communication::pipeName, Communication::Pipe::MODE_CREATE);
    
        // TODO: we should wait for the finishing of the thread before exiting from this function.
        // It works now in most cases because the browser waits for the response in the pipe, and the
        // thread has time to finish while this response is being processed and the browser is
        // disposing all its stuff.
        std::thread([pipe]()
        {
          ClientThread(pipe.get());
        }).detach();
      }
      catch(const std::system_error& ex)
      {
        DebugException(ex);
        return 1;
      }
      catch (const std::runtime_error& e)
      {
        DebugException(e);
        return 1;
      }
    }
  });

  int retValue = _AtlModule.WinMain(cmdShow);
  if (communicationThread.joinable())
  {
    communicationThread.join();
  }

  return retValue;
}
