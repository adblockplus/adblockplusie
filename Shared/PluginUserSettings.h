#pragma once

class PluginUserSettings
{
public:
  static bool Process(const CComQIPtr<IWebBrowser2>& pBrowser, const CString& curUrl, const CString& newUrl, VARIANT_BOOL& cancel);
};