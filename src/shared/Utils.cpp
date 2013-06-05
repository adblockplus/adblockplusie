#include <memory>
#include <stdexcept>

#include <Windows.h>
#include <ShlObj.h>

#include "Utils.h"

namespace
{
  std::wstring appDataPath;

  bool IsWindowsVistaOrLater()
  {
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&osvi));
    return osvi.dwMajorVersion >= 6;
  }
}

std::string ToUtf8String(std::wstring str)
{
  size_t length = str.size();
  if (length == 0)
    return std::string();

  DWORD utf8StringLength = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), length, 0, 0, 0, 0);
  if (utf8StringLength == 0)
    throw std::runtime_error("Failed to determine the required buffer size");

  std::string utf8String(utf8StringLength, '\0');
  WideCharToMultiByte(CP_UTF8, 0, str.c_str(), length, &utf8String[0], utf8StringLength, 0, 0);
  return utf8String;
}

std::wstring GetAppDataPath()
{
  if (appDataPath.empty())
  {
    if (IsWindowsVistaOrLater())
    {
      WCHAR* pathBuffer;
      if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, 0, &pathBuffer)))
        throw std::runtime_error("Unable to find app data directory");
      appDataPath.assign(pathBuffer);
      CoTaskMemFree(pathBuffer);
    }
    else
    {
      std::auto_ptr<wchar_t> pathBuffer(new wchar_t[MAX_PATH]);
      if (!SHGetSpecialFolderPath(0, pathBuffer.get(), CSIDL_LOCAL_APPDATA, true))
        throw std::runtime_error("Unable to find app data directory");
      appDataPath.assign(pathBuffer.get());
    }
    appDataPath += L"\\Adblock Plus for IE";

    // Ignore errors here, this isn't a critical operation
    ::CreateDirectoryW(appDataPath.c_str(), NULL);
  }
  return appDataPath;
}
