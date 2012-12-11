#pragma once

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(*x))
#endif 

#ifndef countof_1
#define countof_1(x) (sizeof(x)/sizeof(*x) - 1)
#endif 

CString DllDir();
CString HtmlDir();
CString UserSettingsUrl();
CString UserSettingsUpdatedUrl();
CString Bk2FwSlash(const CString str);
bool UrlTailEqual(const CString& url, const CString& str);
