#ifndef DEBUG_H
#define DEBUG_H

#include <string>

#ifdef _DEBUG
void Debug(const std::string& text);
void DebugLastError(const std::string& message);
void DebugException(const std::exception& exception);
#else
void Debug(const std::string& text) {}
void DebugLastError(const std::string& message) {}
void DebugException(const std::exception& exception) {}
#endif

#endif // DEBUG_H
