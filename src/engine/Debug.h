#ifndef DEBUG_H
#define DEBUG_H

#include <string>

void Debug(const std::string& text);
void DebugLastError(const std::string& message);
void DebugException(const std::exception& exception);

#endif // DEBUG_H
