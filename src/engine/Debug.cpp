/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2017 eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <fstream>
#include <stdio.h>
#include <sstream>
#include <Windows.h>

#include "../shared/Utils.h"
#include "../shared/CriticalSection.h"

#include "Debug.h"

#ifdef _DEBUG

namespace
{
  CriticalSection debugLock;
};
void Debug(const std::string& text)
{
  SYSTEMTIME st;
  ::GetSystemTime(&st);

  char timeBuf[14];
  _snprintf_s(timeBuf, _TRUNCATE, "%02i:%02i:%02i.%03i", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

  std::wstring filePath = GetAppDataPath() + L"\\debug_engine.txt";

  CriticalSection::Lock lock(debugLock);
  std::ofstream out(filePath, std::ios::app);
  out << timeBuf << " - " << text << std::endl;
  out.flush();
}

void DebugLastError(const std::string& message)
{
  std::stringstream stream;
  stream << message << " (Error code: " << GetLastError() << ")";
  Debug(stream.str());
}

void DebugException(const std::exception& exception)
{
  Debug(std::string("An exception occurred: ") + exception.what());
}
#else
void Debug(const std::string& text) {}
void DebugLastError(const std::string& message) {}
void DebugException(const std::exception& exception) {}
#endif // _DEBUG
