#ifndef UTILS_H
#define UTILS_H

#include <string>

std::string ToUtf8String(std::wstring str);
std::wstring GetAppDataPath();

#endif // UTILS_H
