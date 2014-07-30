#include "PluginStdAfx.h"

#include "PluginSystem.h"
#include "PluginClient.h"

std::wstring GetBrowserLanguage()
{
  LANGID lcid = GetUserDefaultLangID();
  TCHAR language[128];
  memset(language, 0, sizeof(language));
  TCHAR country[128];
  memset(language, 0, sizeof(country));

  std::wstring lang;
  int res = GetLocaleInfoW(lcid, LOCALE_SISO639LANGNAME, language, 127);
  if (res == 0)
  {
    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_BROWSER_LANGUAGE, "System::GetBrowserLang - Failed");
  }
  else
  {
    lang += language;
  }
  lang += L"-";
  res = GetLocaleInfoW(lcid, LOCALE_SISO3166CTRYNAME, country, 127);
  if (res == 0)
  {
    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_BROWSER_LANGUAGE, "System::GetBrowserLang - failed to retrieve country");
  }
  else
  {
    lang += country;
  }
  return lang;
}
