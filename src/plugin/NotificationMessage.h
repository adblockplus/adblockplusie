#ifndef NOTIFICATION_MESSAGE_H
#define NOTIFICATION_MESSAGE_H

#include <string>

class NotificationMessage
{
public:
  NotificationMessage();
  NotificationMessage(HWND parent);
  ~NotificationMessage(){};
  bool Show(std::wstring message, std::wstring title, int icon);
  bool Hide();
  void Move(short x, short y);
  bool SetTextAndIcon(std::wstring text, std::wstring title, int icon);
  void SetParent(HWND parent);
  bool IsVisible();
private:
  HWND parentWindow;
  HWND toolTipWindow;
  static bool commonControlsInitialized;
  static void CommonControlsInitialize();
};

#endif
