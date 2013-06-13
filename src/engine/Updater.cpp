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

    std::wstring params = L"/i " + EscapeCommandLineArg(path)
        + L" ACTION=INSTALL INSTALLUILEVEL=2 REINSTALL=ALL"
          L" REINSTALLMODE=vomus MSIENFORCEUPGRADECOMPONENTRULES=1";

    HINSTANCE instance = ShellExecuteW(NULL, L"runas", msiexec.c_str(), params.c_str(), NULL, SW_HIDE);
    if (reinterpret_cast<int>(instance) <= 32)
      return false;

    // As far as we are concerned everything is fine - MSI service will handle
    // further errors.
    return true;
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

    if (!InstallUpdate(tempFile))
    {
      DebugLastError("Running updater failed");

      Dictionary* dict = Dictionary::GetInstance();
      MessageBoxW(NULL,
          dict->Lookup("updater", "download-error-runerror").c_str(),
          dict->Lookup("updater", "download-error-title").c_str(),
          0);
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
