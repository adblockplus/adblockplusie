#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <locale>
#include <functional>
#include <string>

bool IsWindowsVistaOrLater();

std::string ToUtf8String(const std::wstring& str);
std::wstring ToUtf16String(const std::string& str);
std::wstring GetDllDir();
std::wstring GetAppDataPath();
void ReplaceString(std::wstring& input, const std::wstring placeholder, const std::wstring replacement);

template<class T>
T TrimString(T text)
{
  // Via http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
  T trimmed(text);
  std::function<bool(T::value_type)> isspace = std::bind(&std::isspace<T::value_type>, std::placeholders::_1, std::locale::classic());
  trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), std::not1(isspace)));
  trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), std::not1(isspace)).base(), trimmed.end());
  return trimmed;
}

#endif // UTILS_H
