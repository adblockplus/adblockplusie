#include "PluginStdAfx.h"

// Internet / FTP
#include <wininet.h>

// IP adapter
#include <iphlpapi.h>

#include "PluginSystem.h"
#include "PluginClient.h"
#include "PluginSettings.h"


// IP adapter
#pragma comment(lib, "IPHLPAPI.lib")

// IE functions
#pragma comment(lib, "iepmapi.lib")

// Internet / FTP
#pragma comment(lib, "wininet.lib")

CPluginSystem* CPluginSystem::s_instance = NULL;
CComAutoCriticalSection CPluginSystem::s_criticalSection;

CPluginSystem::CPluginSystem()
{
  s_instance = NULL;
}


CPluginSystem::~CPluginSystem()
{
  s_instance = NULL;
}



CPluginSystem* CPluginSystem::GetInstance()
{
  CPluginSystem* system;

  s_criticalSection.Lock();
  {
    if (!s_instance)
    {
      // We cannot copy the client directly into the instance variable
      // If the constructor throws we do not want to alter instance
      CPluginSystem* systemInstance = new CPluginSystem();

      s_instance = systemInstance;
    }

    system = s_instance;
  }
  s_criticalSection.Unlock();

  return system;
}

CString CPluginSystem::GetBrowserLanguage() const
{
  LANGID lcid = ::GetUserDefaultLangID();
  TCHAR language[128];
  memset(language, 0, sizeof(language));

  TCHAR country[128];
  memset(language, 0, sizeof(country));

  CString lang;

  int res = ::GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, language, 127);
  if (res == 0)
  {
    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_BROWSER_LANGUAGE, "System::GetBrowserLang - Failed");
  }
  else
  {
    lang.Append(language);
  }

  lang.Append(L"-");


  res = ::GetLocaleInfo(lcid, LOCALE_SISO3166CTRYNAME, country, 127);
  if (res == 0)
  {
    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_BROWSER_LANGUAGE, "System::GetBrowserLang - failed to retrieve country");
  }
  else
  {
    lang.Append(country);
  }

  return lang;
}


CString CPluginSystem::GetBrowserVersion() const
{
  CString browserVersion;

  HKEY hKey;
  DWORD res;

  // Open the handler
  if ((res = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Internet Explorer", 0, KEY_QUERY_VALUE, &hKey)) == ERROR_SUCCESS)
  {
    TCHAR buf[255];
    DWORD dwBufSize = sizeof(buf);
    DWORD dwType = REG_SZ;

    // Do the processing, find the version
    if ((res = ::RegQueryValueEx(hKey, L"Version", 0, &dwType, (BYTE*)buf, &dwBufSize)) == ERROR_SUCCESS)
    {
      browserVersion = buf;
      int pos = 0;
      if ((pos = browserVersion.Find('.')) >= 0)
      {
        browserVersion = browserVersion.Left(pos);
      }
    }
    else
    {
      DEBUG_ERROR_LOG(res, PLUGIN_ERROR_OS_VERSION, PLUGIN_ERROR_OS_VERSION_REG_QUERY_VALUE, L"Client::GetBrowserVer - Failed reg query value");
    }

    // Close the handler
    ::RegCloseKey(hKey);
  }
  else
  {
    DEBUG_ERROR_LOG(res, PLUGIN_ERROR_OS_VERSION, PLUGIN_ERROR_OS_VERSION_REG_OPEN_KEY, L"Client::GetBrowserVer - Failed reg open");
  }

  return browserVersion;
}
