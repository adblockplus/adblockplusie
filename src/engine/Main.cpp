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

  void WriteStrings(Communication::OutputBuffer& response,
      const std::vector<std::string>& strings)
  {
    int32_t count = strings.size();
    response << count;
    for (int32_t i = 0; i < count; i++)
      response << strings[i];
  }

  void WriteSubscriptions(Communication::OutputBuffer& response,
      const std::vector<AdblockPlus::SubscriptionPtr>& subscriptions)
  {
    int32_t count = subscriptions.size();
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
  void UpdateCallback(const std::string res)
  {
    if (updateAvailable)
      return;
    Dictionary* dictionary = Dictionary::GetInstance();
    if (res.length() == 0)
    {
      std::wstring upToDateText = dictionary->Lookup("updater", "update-already-up-to-date-text");
      std::wstring upToDateTitle = dictionary->Lookup("updater", "update-already-up-to-date-title");
      MessageBox(NULL, upToDateText.c_str(), upToDateTitle.c_str(), MB_OK);
    }
    else
    {
      std::wstring errorText = dictionary->Lookup("updater", "update-error-text");
      std::wstring errorTitle = dictionary->Lookup("updater", "update-error-title");
      ReplaceString(errorText, L"?1?", ToUtf16String(res));
      MessageBox(NULL, errorText.c_str(), errorTitle.c_str(), MB_OK);
    }
    return;
  }


  CriticalSection firstRunLock;
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
            const int prefixLen = strlen(prefix);
            const int suffixLen = strlen(suffix);
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
        updateAvailable = false;
        filterEngine->ForceUpdateCheck(UpdateCallback);
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

    }
    return response;
  }

  DWORD WINAPI ClientThread(LPVOID param)
  {
    std::auto_ptr<Communication::Pipe> pipe(static_cast<Communication::Pipe*>(param));

    try
    {
      Communication::InputBuffer message = pipe->ReadMessage();
      Communication::OutputBuffer response = HandleRequest(message);
      pipe->WriteMessage(response);
    }
    catch (const std::exception& e)
    {
      DebugException(e);
    }

    // TODO: Keep the pipe open until the client disconnects

    return 0;
  }

  void OnUpdateAvailable(AdblockPlus::JsEnginePtr jsEngine, AdblockPlus::JsValueList& params)
  {
    updateAvailable = true;
    if (params.size() < 1)
    {
      Debug("updateAvailable event missing URL");
      return;
    }

    Updater updater(jsEngine, params[0]->AsString());
    updater.Update();
  }
}

std::auto_ptr<AdblockPlus::FilterEngine> CreateFilterEngine(const std::wstring& locale)
{
  AdblockPlus::AppInfo appInfo;
  appInfo.version = ToUtf8String(IEPLUGIN_VERSION);
  appInfo.name = "adblockplusie";
#ifdef _WIN64
  appInfo.platform = "msie64";
#else
  appInfo.platform = "msie32";
#endif
  appInfo.locale = ToUtf8String(locale);
#ifdef ADBLOCK_PLUS_TEST_MODE
  appInfo.developmentBuild = true;
#else
  appInfo.developmentBuild = false;
#endif

  AdblockPlus::JsEnginePtr jsEngine = AdblockPlus::JsEngine::New(appInfo);
  jsEngine->SetEventCallback("updateAvailable",
      std::bind(&OnUpdateAvailable, jsEngine, std::placeholders::_1));

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

  for (;;)
  {
    try
    {
      Communication::Pipe* pipe = new Communication::Pipe(Communication::pipeName,
            Communication::Pipe::MODE_CREATE);

      // TODO: Count established connections, kill the engine when none are left
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
