#include "PluginStdAfx.h"
#include "PluginSystem.h"
#include "PluginClient.h"
#include <array>

std::wstring GetBrowserLanguage()
{
  LANGID lcid = GetUserDefaultLangID();
  std::wstring lang;
  // According to http://msdn.microsoft.com/en-us/library/windows/desktop/dd373848(v=vs.85).aspx
  // The maximum number of characters allowed for this string is nine, including a terminating null character.
  {
    std::array<wchar_t, 9> localeLanguage;
    int res = GetLocaleInfoW(lcid, LOCALE_SISO639LANGNAME, localeLanguage.data(), localeLanguage.size());
    if (res == 0)
    {
      DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_BROWSER_LANGUAGE, "System::GetBrowserLang - Failed");
    }
    else
    {
      lang += localeLanguage.data();
    }
  }
  lang += L"-";
  {
    std::array<wchar_t, 9> localeCountry;
    int res = GetLocaleInfoW(lcid, LOCALE_SISO3166CTRYNAME, localeCountry.data(), localeCountry.size());
    if (res == 0)
    {
      DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_BROWSER_LANGUAGE, "System::GetBrowserLang - failed to retrieve country");
    }
    else
    {
      lang += localeCountry.data();
    }
  }
  return lang;
}
