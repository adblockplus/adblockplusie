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

std::wstring HtmlFolderPath();
std::wstring UserSettingsFileUrl();
std::wstring FirstRunPageFileUrl();
std::wstring FileUrl(const std::wstring& url);
void ReplaceString(std::wstring& input, const std::wstring placeholder, const std::wstring replacement);
