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

#ifndef NOTIFICATION_MESSAGE_H
#define NOTIFICATION_MESSAGE_H

#include <string>

class NotificationMessage
{
public:
  NotificationMessage(HWND parent = 0);
  ~NotificationMessage(){};
  bool Show(std::wstring message, std::wstring title, int icon);
  void Hide();
  void Move(short x, short y);
  void MoveToCenter(const RECT& r)
  {
    Move(static_cast<short>((r.left + r.right) / 2), static_cast<short>((r.top + r.bottom) / 2));
  }
  bool SetTextAndIcon(std::wstring text, std::wstring title, int icon);
  void SetParent(HWND parent);
  bool IsVisible();
private:
  HWND parentWindow;
  HWND toolTipWindow;
  static bool commonControlsInitialized;
  static void InitializeCommonControls();
};

#endif
