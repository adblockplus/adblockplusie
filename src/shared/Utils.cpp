#include <memory>
#include <stdexcept>
#include <vector>

#include <Windows.h>
#include <ShlObj.h>

#include "Utils.h"

namespace
{
  // See http://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
  EXTERN_C IMAGE_DOS_HEADER __ImageBase;

  std::wstring appDataPath;

}

bool IsWindowsVistaOrLater()
{
  OSVERSIONINFOEX osvi;
  ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&osvi));
  return osvi.dwMajorVersion >= 6;
}

bool IsWindows8OrLater()
{
  OSVERSIONINFOEX osvi;
  ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&osvi));
  return osvi.dwMajorVersion >= 6 && osvi.dwMinorVersion >= 2;
}

std::string ToUtf8String(const std::wstring& str)
{
  int length = static_cast<int>(str.size());
  if (length == 0)
    return std::string();

  int utf8StringLength = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), length, 0, 0, 0, 0);
  if (utf8StringLength == 0)
    throw std::runtime_error("Failed to determine the required buffer size");

  std::string utf8String(utf8StringLength, '\0');
  WideCharToMultiByte(CP_UTF8, 0, str.c_str(), length, &utf8String[0], utf8StringLength, 0, 0);
  return utf8String;
}

std::wstring ToUtf16String(const std::string& str)
{
  int length = static_cast<int>(str.size());
  if (length == 0)
    return std::wstring();

  int utf16StringLength = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), length, NULL, 0);
  if (utf16StringLength == 0)
    throw std::runtime_error("ToUTF16String failed. Can't determine the length of the buffer needed.");

  std::wstring utf16String(utf16StringLength, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), length, &utf16String[0], utf16StringLength);
  return utf16String;
}

std::wstring GetDllDir()
{
  std::vector<WCHAR> path(MAX_PATH);
  int length = GetModuleFileNameW((HINSTANCE)&__ImageBase, &path[0], static_cast<DWORD>(path.size()));

  while (length == path.size())
  {
    // Buffer too small, double buffer size
    path.resize(path.size() * 2);
    length = GetModuleFileNameW((HINSTANCE)&__ImageBase, &path[0], static_cast<DWORD>(path.size()));
  }

  try
  {
    if (length == 0)
      throw std::runtime_error("Failed determining module path");

    std::vector<WCHAR>::reverse_iterator it = std::find(path.rbegin(), path.rend(), L'\\');
    if (it == path.rend())
      throw std::runtime_error("Unexpected plugin path, no backslash found");

    return std::wstring(path.begin(), it.base());
  }
  catch (const std::exception&)
  {
    return std::wstring();
  }
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
      if (!SHGetSpecialFolderPathW(0, pathBuffer.get(), CSIDL_LOCAL_APPDATA, true))
        throw std::runtime_error("Unable to find app data directory");
      appDataPath.assign(pathBuffer.get());
    }
    appDataPath += L"\\Adblock Plus for IE";

    // Ignore errors here, this isn't a critical operation
    ::CreateDirectoryW(appDataPath.c_str(), NULL);
  }
  return appDataPath;
}

void ReplaceString(std::wstring& input, const std::wstring placeholder, const std::wstring replacement)
{
  size_t replaceStart = input.find(placeholder);
  if (replaceStart != std::string::npos)
  {
    input.replace(replaceStart, placeholder.length(), replacement);
  }
}
