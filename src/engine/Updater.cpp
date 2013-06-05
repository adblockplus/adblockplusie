#include <functional>
#include <memory>
#include <sstream>
#include <AdblockPlus/FileSystem.h>
#include <AdblockPlus/WebRequest.h>

#include "../shared/Utils.h"
#include "Debug.h"
#include "Resource.h"
#include "Updater.h"

namespace
{
  typedef std::function<void()> ThreadCallbackType;
  typedef std::function<void(HWND)> DialogCallbackType;

  const int DOWNLOAD_FAILED = 101;

  LRESULT CALLBACK UpdateDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
  {
    switch (msg)
    {
      case WM_INITDIALOG:
      {
        // TODO: Localize dialog strings
        return TRUE;
      }
      case WM_COMMAND:
      {
        if (wParam == IDOK || wParam == IDCANCEL)
        {
          EndDialog(hWnd, wParam);
          return TRUE;
        }
        break;
      }
    }

    return FALSE;
  }

  LRESULT CALLBACK DownloadDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
  {
    // TODO: Indicate progress

    switch (msg)
    {
      case WM_INITDIALOG:
      {
        // TODO: Localize dialog strings
        std::auto_ptr<DialogCallbackType> callback(reinterpret_cast<DialogCallbackType*>(lParam));
        (*callback)(hWnd);
        return TRUE;
      }
      case WM_COMMAND:
      {
        if (wParam == IDCANCEL)
        {
          EndDialog(hWnd, wParam);
          return TRUE;
        }
        break;
      }
    }
    return FALSE;
  }

  DWORD RunThread(LPVOID param)
  {
    std::auto_ptr<ThreadCallbackType> callback(reinterpret_cast<ThreadCallbackType*>(param));
    (*callback)();
    return 0;
  }
}

Updater::Updater(AdblockPlus::JsEnginePtr jsEngine, const std::string& url)
    : jsEngine(jsEngine), url(url), tempFile(GetAppDataPath() + L"\\update.exe")
{
}

void Updater::Update()
{
  Debug("Update available: " + url);

  if (DialogBox(NULL, MAKEINTRESOURCE(IDD_UPDATEDIALOG), GetDesktopWindow(),
      reinterpret_cast<DLGPROC>(&UpdateDlgProc)) == IDOK)
  {
    Debug("User accepted update");

    DialogCallbackType* callback = new DialogCallbackType(std::bind(&Updater::StartDownload,
        this, std::placeholders::_1));
    int result = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DOWNLOADDIALOG), GetDesktopWindow(),
        reinterpret_cast<DLGPROC>(&DownloadDlgProc),
        reinterpret_cast<LPARAM>(callback));
    if (result == DOWNLOAD_FAILED)
    {
      // TODO: Localize
      MessageBoxW(NULL, L"Download failed", L"Downloading update", 0);
    }
    if (result != IDOK)
      return;

    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    ::ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.wShowWindow = FALSE;

    if (!::CreateProcessW(tempFile.c_str(), NULL, NULL, NULL, FALSE, CREATE_BREAKAWAY_FROM_JOB, NULL, NULL, &si, &pi))
    {
      // TODO: Localize
      MessageBoxW(NULL, L"Failed to run updater", L"Downloading update", 0);
      DebugLastError("Creating updater process failed");
      return;
    }
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
  }
}

void Updater::StartDownload(HWND dialog)
{
  this->dialog = dialog;
  ThreadCallbackType* callback = new ThreadCallbackType(std::bind(&Updater::RunDownload, this));
  ::CreateThread(NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(&RunThread),
      callback, 0, NULL);
}

void Updater::RunDownload()
{
  AdblockPlus::ServerResponse response = jsEngine->GetWebRequest()->GET(url, AdblockPlus::HeaderList());
  if (response.status != AdblockPlus::WebRequest::NS_OK ||
      response.responseStatus != 200)
  {
    std::stringstream ss;
    ss << "Update download failed (status: " << response.status << ", ";
    ss << "response: " << response.responseStatus << ")";
    Debug(ss.str());

    EndDialog(dialog, DOWNLOAD_FAILED);
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
    EndDialog(dialog, DOWNLOAD_FAILED);
    return;
  }

  EndDialog(dialog, IDOK);
}
