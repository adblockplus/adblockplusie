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

std::unique_ptr<OSVERSIONINFOEX> GetWindowsVersion()
{
  std::unique_ptr<OSVERSIONINFOEX> osvi(new OSVERSIONINFOEX());
  osvi->dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(osvi.get()));
  return osvi;
}

bool IsWindowsVistaOrLater()
{
  std::unique_ptr<OSVERSIONINFOEX> osvi = GetWindowsVersion();
  return osvi->dwMajorVersion >= 6;
}

bool IsWindows8OrLater()
{
  std::unique_ptr<OSVERSIONINFOEX> osvi = GetWindowsVersion();
  return (osvi->dwMajorVersion == 6 && osvi->dwMinorVersion >= 2) || osvi->dwMajorVersion > 6;
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

std::vector<std::wstring> ToUtf16Strings(const std::vector<std::string>& values)
{
  std::vector<std::wstring> result;
  result.reserve(values.size());
  transform(values.begin(), values.end(), back_inserter(result), ToUtf16String);
  return result;
}

namespace
{
  std::wstring GetModulePath(HINSTANCE hInstance)
  {
    std::vector<WCHAR> path(MAX_PATH);
    int length = GetModuleFileNameW(hInstance, &path[0], static_cast<DWORD>(path.size()));

    while (length == path.size())
    {
      // Buffer too small, double buffer size
      path.resize(path.size() * 2);
      length = GetModuleFileNameW(hInstance, &path[0], static_cast<DWORD>(path.size()));
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
}

std::wstring GetDllDir()
{
  return GetModulePath((HINSTANCE)&__ImageBase);
}

std::wstring GetExeDir()
{
  return GetModulePath(nullptr);
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

void ReplaceString(std::wstring& input, const std::wstring& placeholder, const std::wstring& replacement)
{
  size_t replaceStart = input.find(placeholder);
  if (replaceStart != std::string::npos)
  {
    input.replace(replaceStart, placeholder.length(), replacement);
  }
}

std::wstring GetSchemeAndHierarchicalPart(const std::wstring& url)
{
  auto schemeAndHierarchicalPartEndsAt = url.find(L'?');
  if (schemeAndHierarchicalPartEndsAt == std::wstring::npos)
  {
    schemeAndHierarchicalPartEndsAt = url.find(L'#');
  }
  return url.substr(0, schemeAndHierarchicalPartEndsAt);
}

std::wstring GetQueryString(const std::wstring& url)
{
  auto questionSignPos = url.find(L'?');
  if (questionSignPos == std::wstring::npos)
  {
    return L"";
  }
  auto queryStringBeginsAt = questionSignPos + 1;
  auto endQueryStringPos = url.find(L'#', queryStringBeginsAt);
  if (endQueryStringPos == std::wstring::npos)
  {
    endQueryStringPos = url.length();
  }
  return url.substr(queryStringBeginsAt, endQueryStringPos - queryStringBeginsAt);
}
