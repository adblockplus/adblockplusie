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

#include "Registry.h"
#include <memory>

using namespace AdblockPlus;

RegistryKey::RegistryKey(HKEY parent, const std::wstring& key_name)
{
  if (key_name.empty())
  {
    throw std::runtime_error("key_name may not be empty");
  }
  HRESULT hr = RegOpenKeyExW(parent, key_name.c_str(), 0, KEY_QUERY_VALUE, &key);
  if (hr != ERROR_SUCCESS || !key)
  {
    throw std::runtime_error("Failure in RegOpenKeyExW");
  }
}

RegistryKey::~RegistryKey()
{
  RegCloseKey(key);
}

std::wstring RegistryKey::value_wstring(const std::wstring& name) const
{
  /*
   * Step one is to determine the presence of the value, along with its type and byte size.
   */
  DWORD type;
  DWORD size = 0;
  HRESULT hr = ::RegQueryValueExW(static_cast<HKEY>(key), name.c_str(), 0, &type, 0, &size);
  if (hr != ERROR_SUCCESS)
  {
    throw std::runtime_error("Failure in RegQueryValueEx to query name");
  }
  if (type != REG_SZ)
  {
    throw std::runtime_error("Value is not string type");
  }

  /*
   * Step two is to allocate a buffer for the string and query for its value.
   * Note that 'size' is the size in bytes, which we need for the system call,
   *   but that 'psize' is the size in words, which we need to manipulate the wchar_t array 'p'.
   */
  size_t psize = (size + 1)/2;
  // Round the byte size up to the nearest multiple of two.
  size = 2 * psize;
  // We need to allocate a temporary buffer to receive the value, because there's no interface to write directly into the buffer of std::basic_string.
  std::unique_ptr<wchar_t[]> p(new wchar_t[psize]);
  hr = RegQueryValueExW(key, name.c_str(), 0, 0, reinterpret_cast<BYTE*>(p.get()), &size);
  if (hr != ERROR_SUCCESS)
  {
    throw std::runtime_error("Failure in RegQueryValueExW to retrieve value");
  }

  /*
   * Step three is to construct a return string.
   *
   * There's the possibility of an extra terminating null character in the return value of the query.
   * If so, we have to decrement the length of the return value to eliminate it.
   * If it persists, it will interfere with later string operations such as concatenation.
   */
  if (p[psize - 1] == L'\0')
  {
    --psize;
  }
  return std::wstring(p.get(), psize);
}

