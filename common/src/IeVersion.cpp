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
#include "IeVersion.h"
#include "Registry.h"
#include <cstdlib>

using namespace AdblockPlus;

/**
 * Internal implementation of the IE version string.
 *
 * This version throws exceptions for its errors, relying on its caller to handle them.
 *
 * Quoting http://support.microsoft.com/kb/969393:
 *   "Note The version string value for Internet Explorer 10 is 9.10.9200.16384, and the svcVersion string value is 10.0.9200.16384."
 * [EH 2014-06-20] My current version of IE 11 is reporting these values:
 *   Version 9.11.9600.17041
 *   svcVersion 11.0.9600.17041
 *
 * Warning: IE version 12 and later might not behave the same, so this function should be reviewed with each new release.
 *
 * \par postcondition
 *   Return value matches regex `^[0-9]{1,2}\.$`; this is a sanity check on the format.
 */
std::wstring IeVersionString()
{
  RegistryKey ieKey(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Internet Explorer");
  std::wstring version(ieKey.value_wstring(L"Version"));
  /*
   * We're expecting a version string that matches the regex "^[1-9]\.".
   * Since IE versions 10 and 11 use a version string that begins with "9.", this simplistic parsing method works adequately.
   */
  if (version.length() < 2 || version[1] != '.' || version[0] < L'0' || version[0] > L'9')
  {
    throw std::runtime_error("IE version string has unexpected format");
  }
  if (version[0] != L'9')
  {
    return version;
  }
  // Assert the major version in the "Version" value is 9.
  /*
   * Version 9 might either be an actual version 9 or it might represent a version >= 10
   * If the value named "svcVersion" exists, we'll report that instead.
   */
  try
  {
    version = ieKey.value_wstring(L"svcVersion"); // throws if value not found
  }
  catch (...)
  {
    // Assert svcVersion value not found
    // Thus the major version is 9
    return version;
  } 
  // Assert major version is >= 10
  if (version.length() < 3 || version[0] < L'0' || version[0] > L'9' || version[1] < L'0' || version[1] > L'9' || version[2] != L'.')
  {
    throw std::runtime_error("IE version string has unexpected format");
  }
  return version;
}

std::wstring AdblockPlus::IE::InstalledVersionString()
{
  try
  {
    return IeVersionString();
  }
  catch (...)
  {
    return L"";
  }
}

int AdblockPlus::IE::InstalledMajorVersion()
{
  try
  {
    std::wstring version = IeVersionString();
    /*
     * The version number is either one or two digits,
     *   and thus either the second or third character is a period,
     */
    if (version[1] == L'.')
    {
      return version[0] - L'0';
    } 
    else if (version[2] == L'.')
    {
      return 10 * (version[0] - L'0') + (version[1] - L'0');
    }
  }
  catch (...)
  {
  }
  return 0;
}
