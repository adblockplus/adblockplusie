#include "PluginStdAfx.h"
#include "PluginUserSettings.h"
#include "PluginUtil.h"
#include <map>
#include "PluginClient.h"

using namespace std;

static const TCHAR ADBLOCK_MYSETTINGS[] = L"?user_mysettings&";

typedef map<CString, CString> MAP_KEY_VALUE;

static bool sParseKeyValue(MAP_KEY_VALUE& mKeyValue, const CString& url)
{
  int indx = url.Find(ADBLOCK_MYSETTINGS);

  CString keyValue = (LPCTSTR)url + indx + countof_1(ADBLOCK_MYSETTINGS);
  keyValue += "&";

  TCHAR* pKeyValue = wcsdup(keyValue);
  std::auto_ptr<TCHAR> autoKeyValue(pKeyValue);

  while (TCHAR* pSep = wcschr(pKeyValue, L'&')) {
    *pSep = L'\0';

    TCHAR* pValue = wcschr(pKeyValue, L'=');
    if (!pValue) {
      DEBUG_ERROR_LOG(0, PLUGIN_ERROR_USER_SETTINGS, PLUGIN_ERROR_USER_SETTINGS_PARSE_KEY_VALUE, L"No key value seaprator '='");
      return false;
    }
    *pValue = L'\0';
    pValue++;

    mKeyValue.insert(make_pair(pKeyValue, pValue));

    pKeyValue = pSep + 1;
  };

  return true;
}


bool PluginUserSettings::Process(const CComQIPtr<IWebBrowser2>& browser, const CString& curUrl, const CString& newUrl, VARIANT_BOOL& cancel)
{
  CONSOLE("!!! PluginUserSettings::Process Beforenavigate newUrl %ws", newUrl);
  do {
    // Check current url is local file
    static TCHAR file[] = L"file://";
    if (wcsncmp((LPCTSTR)curUrl, file, countof_1(file)))
      break;

    if (!UrlTailEqual(curUrl, Bk2FwSlash(UserSettingsUrl())))
      break;

    if (-1 == newUrl.Find(Bk2FwSlash(UserSettingsUrl() + ADBLOCK_MYSETTINGS)))
      break;

    MAP_KEY_VALUE mKeyValue;
    if (!sParseKeyValue(mKeyValue, newUrl))
      break;

    cancel = VARIANT_TRUE;

    // Just for testing
    browser->Navigate(CComBSTR(UserSettingsUpdatedUrl()), &CComVariant(), 0, 0, 0);

    return true;
  } while (false);

  return false;
}