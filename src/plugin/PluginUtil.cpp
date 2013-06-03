#include "PluginStdAfx.h"
#include <algorithm>
#include <stdexcept>
#include <vector>

#include "PluginUtil.h"
#include "PluginSettings.h"

BString::BString(const std::wstring& value)
    : value(::SysAllocString(value.c_str()))
{
}

BString::~BString()
{
  ::SysFreeString(value);
}

BString::operator BSTR()
{
  return value;
}

std::wstring DllDir()
{
  std::vector<WCHAR> path(MAX_PATH);
  DWORD length = GetModuleFileNameW((HINSTANCE)&__ImageBase, &path[0], path.size());

  while (length == path.size())
  {
    // Buffer too small, double buffer size
    path.resize(path.size() * 2);
    length = GetModuleFileNameW((HINSTANCE)&__ImageBase, &path[0], path.size());
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
  catch (const std::exception& e)
  {
    DEBUG_GENERAL(e.what());
    return std::wstring();
  }
}

std::wstring UserSettingsFileUrl()
{
  return FileUrl(DllDir() + L"html\\templates\\index.html");
}

std::wstring FileUrl(const std::wstring& path)
{
  std::wstring url = path;
  std::replace(url.begin(), url.end(), L'\\', L'/');
  return L"file:///" + url;
}

