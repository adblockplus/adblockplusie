#include "PluginStdAfx.h"
#include "PluginUtil.h"

static CString sDllDir()
{
  TCHAR filePath[MAX_PATH + 1];
  GetModuleFileName(_AtlBaseModule.GetModuleInstance(), filePath, countof_1(filePath));
  TCHAR* pLastBackslash = wcsrchr(filePath, L'\\');
  *(pLastBackslash + 1) = L'\0';

  return filePath;
}

CString DllDir()
{
  static CString s_dllDir = sDllDir();
  return s_dllDir;
}

CString HtmlDir()
{
  static CString s_htmlDir = DllDir() + L"html\\";
  return s_htmlDir;
}

CString UserSettingsUrl()
{
  static CString s_userSettingsUrl = HtmlDir() + USERS_LOCAL_USER_SETTINGS_HTML;
  return s_userSettingsUrl;
}


CString UserSettingsUpdatedUrl()
{
  return HtmlDir() + USERS_LOCAL_USER_SETTINGS_UPDATED_HTML;
}


CString Bk2FwSlash(const CString str)
{
  CString b2f = str;
  b2f.Replace(L'\\', L'/');
  return b2f;
}

bool UrlTailEqual(const CString& url, const CString& str)
{
  int indx = url.GetLength() - str.GetLength();
  if (indx < 0)
    return false;

  return -1 != url.Find(str, indx);
}
