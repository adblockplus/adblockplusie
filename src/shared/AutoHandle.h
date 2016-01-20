/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2016 Eyeo GmbH
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

#ifndef AUTO_HANDLE_H
#define AUTO_HANDLE_H

#include <Windows.h>

class AutoHandle
{
public:
  explicit AutoHandle(HANDLE handle = 0);
  ~AutoHandle();
  operator HANDLE();
  operator PHANDLE();
  operator bool();

private:
  HANDLE handle;

  AutoHandle(const AutoHandle&);
  AutoHandle& operator=(const AutoHandle&);
};

#endif
