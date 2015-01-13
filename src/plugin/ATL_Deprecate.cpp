/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
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

/**
 * \file ATL_Deprecate.cpp Transient functions used during the ATL removal process.
 */

#include "ATL_Deprecate.h"

std::wstring ToWstring(const ATL::CString& s)
{
  std::wstring result(static_cast<const wchar_t*>(s));
  return result;
}

ATL::CString ToCString(const std::wstring& s)
{
  return ATL::CString(s.c_str());
}
