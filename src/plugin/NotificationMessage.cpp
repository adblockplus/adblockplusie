/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
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

#include <Windows.h>
#include <CommCtrl.h>

#include "NotificationMessage.h"

NotificationMessage::NotificationMessage(HWND parent): parentWindow(parent)
{
  toolTipWindow = 0;
  InitializeCommonControls();
};

bool NotificationMessage::commonControlsInitialized(false);

void NotificationMessage::InitializeCommonControls()
{
  if (!commonControlsInitialized)
  {
    INITCOMMONCONTROLSEX commControls;
    commControls.dwSize = sizeof(commControls);
    commControls.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&commControls);
    commonControlsInitialized = true;
  }
}

bool NotificationMessage::Show(std::wstring message, std::wstring title, int icon)
{
  if (toolTipWindow != 0)
  {
    Hide();
  }
  toolTipWindow = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
                                  TTS_NOPREFIX | TTS_BALLOON | TTS_CLOSE,
                                  0, 0,
                                  0, 0,
                                  parentWindow, NULL, NULL,
                                  NULL);

  SetWindowPos(toolTipWindow, HWND_TOPMOST,0, 0, 0, 0,
          SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  TOOLINFOW ti = {};
  ti.cbSize = sizeof(ti);
  ti.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_TRANSPARENT;
  ti.hwnd = parentWindow;
  ti.hinst = NULL;
  ti.uId = (UINT_PTR)parentWindow;
  ti.lpszText = const_cast<LPWSTR>(message.c_str());
  GetClientRect(parentWindow, &ti.rect);

  LRESULT res = ::SendMessage(toolTipWindow, TTM_ADDTOOL, 0, (LPARAM)&ti);

  RECT rect;
  GetWindowRect(parentWindow, &rect);
  MoveToCenter(rect);

  SetTextAndIcon(message, title, icon);
  res = ::SendMessage(toolTipWindow, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);

  return true;
}

void NotificationMessage::Hide()
{
  if (toolTipWindow != 0)
  {
    DestroyWindow(toolTipWindow);
    toolTipWindow = 0;
  }
}

void NotificationMessage::Move(short x, short y)
{
  ::SendMessage(toolTipWindow, TTM_TRACKPOSITION, 0, MAKELONG(x, y));
  return;
}

bool NotificationMessage::SetTextAndIcon(std::wstring text, std::wstring title, int icon)
{
  TOOLINFOW ti = {};
  ti.cbSize = sizeof(ti);
  ti.hwnd = parentWindow;
  ti.hinst = NULL;
  ti.uId = (UINT_PTR)parentWindow;
  ti.lpszText = const_cast<LPWSTR>(text.c_str());
  LRESULT res = ::SendMessage(toolTipWindow, TTM_SETTITLE, icon, (LPARAM)title.c_str());
  res = ::SendMessage(toolTipWindow, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
  return res == TRUE;
}

void NotificationMessage::SetParent(HWND parent)
{
  parentWindow = parent;
}

bool NotificationMessage::IsVisible()
{
  if (toolTipWindow == 0)
    return false;
  return IsWindowVisible(toolTipWindow) != FALSE;
}
