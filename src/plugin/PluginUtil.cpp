#include "PluginStdAfx.h"
#include <algorithm>
#include <stdexcept>
#include <vector>

#include "../shared/Utils.h"
#include "PluginUtil.h"
#include "PluginSettings.h"

std::wstring HtmlFolderPath()
{
  return GetDllDir() + L"html\\templates\\";
}

std::wstring UserSettingsFileUrl()
{
  return FileUrl(HtmlFolderPath() + L"index.html");
}

std::wstring FirstRunPageFileUrl()
{
  return FileUrl(HtmlFolderPath() + L"firstRun.html");
}

std::wstring FileUrl(const std::wstring& path)
{
  std::wstring url = path;
  std::replace(url.begin(), url.end(), L'\\', L'/');
  return L"file:///" + url;
}
