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

#include <AdblockPlus.h>
#include <functional>
#include <vector>
#include <thread>
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

namespace
{
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

  return 0;
}
