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

#ifndef _PLUGIN_MUTEX_H_
#define _PLUGIN_MUTEX_H_


class CPluginMutex
{

private:

  HANDLE m_hMutex;
  bool m_isLocked;
  int m_errorSubidBase;
  std::wstring system_name;

public:

  CPluginMutex(const std::wstring& name, int errorSubidBase);
  ~CPluginMutex();

  bool IsLocked() const;
};


#endif // _PLUGIN_MUTEX_H_
