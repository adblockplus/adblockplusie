#include <functional>
#include <memory>
#include <sstream>

#include <Windows.h>
#include <Msi.h>

#include <AdblockPlus/FileSystem.h>
#include <AdblockPlus/WebRequest.h>

#include "../shared/Dictionary.h"
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
        Dictionary* dict = Dictionary::GetInstance();
        SetWindowTextW(hWnd, dict->Lookup("updater", "update-title").c_str());
        SetDlgItemTextW(hWnd, IDC_UPDATETEXT, dict->Lookup("updater", "update-text").c_str());
        SetDlgItemTextW(hWnd, IDC_DOYOU, dict->Lookup("updater", "update-question").c_str());
        SetDlgItemTextW(hWnd, IDOK, dict->Lookup("general", "button-yes").c_str());
        SetDlgItemTextW(hWnd, IDCANCEL, dict->Lookup("general", "button-no").c_str());
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
        Dictionary* dict = Dictionary::GetInstance();
        SetWindowTextW(hWnd, dict->Lookup("updater", "download-title").c_str());
        SetDlgItemTextW(hWnd, IDC_INSTALLMSG, dict->Lookup("updater", "download-progress-text").c_str());
        SetDlgItemTextW(hWnd, IDCANCEL, dict->Lookup("general", "button-cancel").c_str());

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
    : jsEngine(jsEngine), url(url), tempFile(GetAppDataPath() + L"\\update.msi")
{
}

void Updater::Update()
{
  Debug("Update available: " + url);

  if (DialogBox(NULL, MAKEINTRESOURCE(IDD_UPDATEDIALOG), GetDesktopWindow(),
      reinterpret_cast<DLGPROC>(&UpdateDlgProc)) == IDOK)
  {
    Debug("User accepted update");

    {
      DialogCallbackType* callback = new DialogCallbackType(std::bind(&Updater::StartDownload,
          this, std::placeholders::_1));
      int result = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DOWNLOADDIALOG), GetDesktopWindow(),
          reinterpret_cast<DLGPROC>(&DownloadDlgProc),
          reinterpret_cast<LPARAM>(callback));
      if (result == DOWNLOAD_FAILED)
      {
        Dictionary* dict = Dictionary::GetInstance();
        MessageBoxW(NULL,
            dict->Lookup("updater", "download-error-neterror").c_str(),
            dict->Lookup("updater", "download-error-title").c_str(),
            0);
      }
      if (result != IDOK)
        return;
    }

    {
      UINT result = ::MsiInstallProductW(tempFile.c_str(), L"ACTION=INSTALL INSTALLUILEVEL=2");
      if (result != ERROR_SUCCESS)
      {
        Dictionary* dict = Dictionary::GetInstance();
        std::wstringstream message;
        message << dict->Lookup("updater", "download-error-runerror");
        message << std::endl << L"(error " << result << L")";
        MessageBoxW(NULL,
            message.str().c_str(),
            dict->Lookup("updater", "download-error-title").c_str(),
            0);

        std::stringstream error;
        error << "Installing update failed (error " << result << ")";
        Debug(error.str());
        return;
      }
    }
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
