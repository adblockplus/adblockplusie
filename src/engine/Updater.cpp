/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
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

#include <functional>
#include <memory>
#include <sstream>

#include <Windows.h>

#include <AdblockPlus/FileSystem.h>
#include <AdblockPlus/WebRequest.h>

#include "../shared/AutoHandle.h"
#include "../shared/Dictionary.h"
#include "../shared/Utils.h"
#include "Debug.h"
#include "Resource.h"
#include "UpdateInstallDialog.h"
#include "Updater.h"

namespace
{
  typedef std::function<void()> ThreadCallbackType;

  const int DOWNLOAD_FAILED = 101;

  DWORD RunThread(LPVOID param)
  {
    std::auto_ptr<ThreadCallbackType> callback(reinterpret_cast<ThreadCallbackType*>(param));
    (*callback)();
    return 0;
  }

  std::wstring EscapeCommandLineArg(const std::wstring& arg)
  {
    // This does the inverse of CommandLineToArgvW(). See
    // http://blogs.msdn.com/b/oldnewthing/archive/2010/09/17/10063629.aspx for
    // a description of the rules - the backslash rules are very non-obvious.
    std::wstring result = arg;
    size_t pos = arg.find(L'"');
    while (pos != std::wstring::npos)
    {
      // Protect the quotation mark
      result.insert(pos, 1, L'\\');
      pos++;

      // Protect any of the preceding backslashes
      for (int offset = -2; pos + offset >= 0 && result[pos + offset] == L'\\'; offset -= 2)
      {
        result.insert(pos + offset, 1, L'\\');
        pos++;
      }

      // Find next quotation mark
      pos = arg.find(L'"', pos);
    }
    return L'"' + result + L'"';
  }

  BOOL InstallUpdate(const std::wstring& path)
  {
    WCHAR sysDir[MAX_PATH];
    UINT sysDirLen = GetSystemDirectoryW(sysDir, sizeof(sysDir) / sizeof(sysDir[0]));
    if (sysDirLen == 0)
      return false;

    std::wstring msiexec = std::wstring(sysDir, sysDirLen) + L"\\msiexec.exe";

    std::wstring params = L"/i " + EscapeCommandLineArg(path) + L" /qb";

    LPCWSTR operation = IsWindowsVistaOrLater() ? L"runas" : 0;
    HINSTANCE instance = ShellExecuteW(NULL, operation, msiexec.c_str(), params.c_str(), NULL, SW_HIDE);
    if (reinterpret_cast<int>(instance) <= 32)
      return false;

    // As far as we are concerned everything is fine - MSI service will handle
    // further errors.
    return true;
  }
}

Updater::Updater(AdblockPlus::JsEnginePtr jsEngine)
    : jsEngine(jsEngine), tempFile(GetAppDataPath() + L"\\update.msi")
{
}

void Updater::Update(const std::string& url)
{
  this->url = url;
  Debug("Downloading update: " + url);
  ThreadCallbackType* callback = new ThreadCallbackType(std::bind(&Updater::Download, this));
  ::CreateThread(NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(&RunThread), callback, 0, NULL);
}

void Updater::Download()
{
  AdblockPlus::ServerResponse response = jsEngine->GetWebRequest()->GET(url, AdblockPlus::HeaderList());
  if (response.status != AdblockPlus::WebRequest::NS_OK ||
      response.responseStatus != 200)
  {
    std::stringstream ss;
    ss << "Update download failed (status: " << response.status << ", ";
    ss << "response: " << response.responseStatus << ")";
    Debug(ss.str());
    return;
  }

  AdblockPlus::FileSystemPtr fileSystem = jsEngine->GetFileSystem();
  std::string utfTempFile = ToUtf8String(tempFile);
  try
  {
    // Remove left-overs from previous update attempts
    fileSystem->Remove(utfTempFile);
  }
  catch (const std::exception&)
  {
  }

  try
  {
    std::tr1::shared_ptr<std::istream> fileData(new std::istringstream(response.responseText));
    fileSystem->Write(utfTempFile, fileData);
  }
  catch (const std::exception& e)
  {
    DebugException(e);
    return;
  }

  OnDownloadSuccess();
}

void Updater::OnDownloadSuccess()
{
  UpdateInstallDialog dialog;
  bool shouldInstall = dialog.Show();
  if (shouldInstall)
  {
    if (!InstallUpdate(tempFile))
    {
      DebugLastError("Failed to install update");
      Dictionary* dictionary = Dictionary::GetInstance();
      MessageBoxW(
        0, dictionary->Lookup("updater", "install-error-text").c_str(),
        dictionary->Lookup("updater", "install-error-title").c_str(),
        MB_OK | MB_ICONEXCLAMATION);
    }
  }
}
