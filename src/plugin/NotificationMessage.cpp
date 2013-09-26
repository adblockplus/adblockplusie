#include <Windows.h>
#include <CommCtrl.h>

#include "NotificationMessage.h"

NotificationMessage::NotificationMessage(HWND parent)
{ 
  parentWindow = parent;
  toolTipWindow = 0;
  InitializeCommonControls();
};

bool NotificationMessage::commonControlsInitialized(false);

void NotificationMessage::InitializeCommonControls()
{
  if (!commonControlsInitialized)
  {
    INITCOMMONCONTROLSEX commControls;
    commControls.dwSize = sizeof(INITCOMMONCONTROLSEX);
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
  TOOLINFOW ti;
  ti.cbSize = sizeof(TOOLINFOW);
  ti.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_TRANSPARENT;
  ti.hwnd = toolTipWindow;
  ti.hinst = NULL;
  ti.uId = (UINT_PTR)parentWindow;
  ti.lpszText = const_cast<LPWSTR>(message.c_str());    
  GetClientRect(parentWindow, &ti.rect);

  LRESULT res = ::SendMessage(toolTipWindow, TTM_ADDTOOL, 0, (LPARAM)&ti);

  RECT rect;
  GetWindowRect(parentWindow, &rect);
  Move(rect.left + (rect.right - rect.left) / 2, rect.top + (rect.bottom - rect.top) / 2); 

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
  TOOLINFOW ti;
  memset(&ti, 0, sizeof(TOOLINFOW));
  ti.cbSize = sizeof(TOOLINFOW);
  ti.hwnd = toolTipWindow; 
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
  return IsWindowVisible(toolTipWindow);
}