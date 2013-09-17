#include <Windows.h>
#include <CommCtrl.h>

#include "NotificationMessage.h"

NotificationMessage::NotificationMessage()
{
  CommonControlsInitialize();
}

NotificationMessage::NotificationMessage(HWND parent)
{ 
  parentWindow = parent;
  CommonControlsInitialize();
};

bool NotificationMessage::commonControlsInitialized(false);

void NotificationMessage::CommonControlsInitialize()
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
  toolTipWindow = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
                                  TTS_NOPREFIX | TTS_BALLOON | TTS_CLOSE,
                                  0, 0,
                                  0, 0,
                                  parentWindow, NULL, NULL,
                                  NULL);

  SetWindowPos(toolTipWindow, HWND_TOPMOST,0, 0, 0, 0,
          SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  TOOLINFO ti;
	ti.cbSize = sizeof(TOOLINFO);
  ti.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_TRANSPARENT;
	ti.hwnd = toolTipWindow;
	ti.hinst = NULL;
	ti.uId = (UINT_PTR)parentWindow;
	ti.lpszText = (LPWSTR)message.c_str();		
  GetClientRect(parentWindow, &ti.rect);

	LRESULT res = ::SendMessage(toolTipWindow, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

  RECT rect;
  GetWindowRect(parentWindow, &rect);
  Move(rect.left + (rect.right - rect.left) / 2, rect.top + (rect.bottom - rect.top) / 2); 

	res = ::SendMessage(toolTipWindow, TTM_SETTITLE, icon, (LPARAM)title.c_str());
	res = ::SendMessage(toolTipWindow, TTM_TRACKACTIVATE, TRUE, (LPARAM)(LPTOOLINFO) &ti);

  return true;
}

bool NotificationMessage::Hide()
{
  DestroyWindow(toolTipWindow);
  toolTipWindow = 0;
  return true;
}

void NotificationMessage::Move(short x, short y)
{
	::SendMessage(toolTipWindow, TTM_TRACKPOSITION, 0, (LPARAM)(LPTOOLINFO)MAKELONG(x, y));
  return;
}

bool NotificationMessage::SetTextAndIcon(std::wstring text, std::wstring title, int icon)
{
  TOOLINFO ti;
	ti.cbSize = sizeof(TOOLINFO);
  ti.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_TRANSPARENT;
	ti.hwnd = toolTipWindow;
	ti.hinst = NULL;
	ti.uId = (UINT_PTR)parentWindow;
	ti.lpszText = (LPWSTR)text.c_str();		
  GetClientRect(parentWindow, &ti.rect);
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