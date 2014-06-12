#include <AdblockPlus.h>
#include <functional>
#include <vector>
#include <Windows.h>

#include "../shared/AutoHandle.h"
#include "../shared/Communication.h"
#include "../shared/Dictionary.h"
#include "../shared/Utils.h"
#include "../shared/Version.h"
#include "../shared/CriticalSection.h"
#include "Debug.h"
#include "Updater.h"

namespace
{
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine;
  std::auto_ptr<Updater> updater;
  int activeConnections = 0;
  CriticalSection activeConnectionsLock;
  HWND callbackWindow;

  void WriteStrings(Communication::OutputBuffer& response,
      const std::vector<std::string>& strings)
  {
    int32_t count = static_cast<int32_t>(strings.size());
    response << count;
    for (int32_t i = 0; i < count; i++)
      response << strings[i];
  }

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
        std::string type;
        std::string documentUrl;
        request >> url >> type >> documentUrl;
        AdblockPlus::FilterPtr filter = filterEngine->Matches(url, type, documentUrl);
        response << (filter && filter->GetType() != AdblockPlus::Filter::TYPE_EXCEPTION);
        break;
      }
      case Communication::PROC_GET_ELEMHIDE_SELECTORS:
      {
        std::string domain;
        request >> domain;
        WriteStrings(response, filterEngine->GetElementHidingSelectors(domain));
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

        std::vector<AdblockPlus::SubscriptionPtr> subscriptions = filterEngine->GetListedSubscriptions();
        for (size_t i = 0, count = subscriptions.size(); i < count; i++)
          subscriptions[i]->RemoveFromList();

        filterEngine->GetSubscription(url)->AddToList();
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

        WriteStrings(response, domains);
        break;
      }
      case Communication::PROC_IS_WHITELISTED_URL:
      {
        std::string url;
        request >> url;
        AdblockPlus::FilterPtr match = filterEngine->Matches(url, "DOCUMENT", url);
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
      case Communication::PROC_GET_DOCUMENTATION_LINK:
      {
        response << ToUtf16String(filterEngine->GetPref("documentation_link")->AsString());
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

  DWORD WINAPI ClientThread(LPVOID param)
  {
    std::auto_ptr<Communication::Pipe> pipe(static_cast<Communication::Pipe*>(param));

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
        exit(0);
      }
    }

    return 0;
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
  // TODO: Set applicationVersion parameter
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
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine(new AdblockPlus::FilterEngine(jsEngine));
  return filterEngine;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
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

  for (;;)
  {
    try
    {
      Communication::Pipe* pipe = new Communication::Pipe(Communication::pipeName,
            Communication::Pipe::MODE_CREATE);

      AutoHandle thread(CreateThread(0, 0, ClientThread, static_cast<LPVOID>(pipe), 0, 0));
      if (!thread)
      {
        delete pipe;
        DebugLastError("CreateThread failed");
        return 1;
      }
    }
    catch (std::runtime_error e)
    {
      DebugException(e);
      return 1;
    }
  }

  return 0;
}
