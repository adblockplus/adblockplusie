#pragma once
#include <string>

class BString
{
public:
  BString(const std::wstring& value);
  ~BString();
  operator BSTR();
private:
  BSTR value;
  BString(const BString&);
  BString& operator=(const BString&);
};

std::wstring DllDir();
std::wstring UserSettingsFileUrl();
std::wstring FileUrl(const std::wstring& url);
