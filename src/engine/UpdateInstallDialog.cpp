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

#include <sstream>
#include <string>
#include <Windows.h>

#include "../shared/Dictionary.h"
#include "../shared/Utils.h"
#include "UpdateInstallDialog.h"

namespace
{
  LRESULT CALLBACK ProcessWindowMessage(HWND window, UINT message,
                                        WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
    case WM_CLOSE:
      PostQuitMessage(1);
      return 0;
    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
      case IDYES:
        PostQuitMessage(0);
        return 0;
      case IDNO:
        PostQuitMessage(1);
        return 0;
      }
      break;
    }
    return DefWindowProc(window, message, wParam, lParam);
  }

  HWND CreateDialogWindow()
  {
    const std::wstring windowClassName = L"ABP_UPDATE_INSTALL_DIALOG";
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ProcessWindowMessage;
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_MENU + 1);
    windowClass.lpszClassName = windowClassName.c_str();
    RegisterClassEx(&windowClass);

    Dictionary* dictionary = Dictionary::GetInstance();
    std::wstring title =
      dictionary->Lookup("updater", "install-question-title");

    return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                           windowClassName.c_str(), title.c_str(), WS_SYSMENU,
                           0, 0, 0, 0, 0, 0, 0, 0);
  }

  int GetAverageCharacterWidth(HDC deviceContext)
  {
    std::wstring alphabet;
    for (wchar_t letter = L'A'; letter != 'Z'; letter++)
      alphabet += letter;
    RECT alphabetRect = {};
    DrawTextW(deviceContext, alphabet.c_str(), -1, &alphabetRect,
              DT_CALCRECT | DT_NOPREFIX);
    return alphabetRect.right / static_cast<int>(alphabet.length());
  }

  HWND CreateStaticText(HWND parent)
  {
    Dictionary* dictionary = Dictionary::GetInstance();
    std::wstring text = dictionary->Lookup("updater", "install-question-text");

    HDC deviceContext = GetDC(parent);
    const int characterWidth = GetAverageCharacterWidth(deviceContext);
    const int lineLength = 40;
    const int textWidth = lineLength * characterWidth;

    RECT textRect = {};
    textRect.right = textWidth;
    DrawTextW(deviceContext, text.c_str(), -1, &textRect,
              DT_CALCRECT | DT_NOPREFIX | DT_WORDBREAK);
    const int textHeight = textRect.bottom;

    return CreateWindowW(L"STATIC", text.c_str(),
                         WS_CHILD | WS_VISIBLE| SS_LEFT, 0, 0,
                         textWidth, textHeight, parent, 0, 0, 0);
  }

  HWND CreateButtons(HWND parent)
  {
    const std::wstring panelClassName = L"ABP_PANEL";
    WNDCLASSW panelClass = {};
    panelClass.lpfnWndProc =
      reinterpret_cast<WNDPROC>(GetWindowLongPtr(parent, GWLP_WNDPROC));
    panelClass.lpszClassName = panelClassName.c_str();
    RegisterClass(&panelClass);

    Dictionary* dictionary = Dictionary::GetInstance();
    const std::wstring yesLabel = dictionary->Lookup("general", "button-yes");
    const std::wstring noLabel = dictionary->Lookup("general", "button-no");

    HDC deviceContext = GetDC(parent);
    RECT yesButtonRect = {};
    DrawTextW(deviceContext, yesLabel.c_str(), -1, &yesButtonRect,
              DT_CALCRECT | DT_NOPREFIX);
    RECT noButtonRect = {};
    DrawTextW(deviceContext, noLabel.c_str(), -1, &noButtonRect,
              DT_CALCRECT | DT_NOPREFIX);

    const int minButtonWidth = 120;
    const int minButtonHeight = 30;
    const int buttonPadding = 5;
    int yesButtonWidth = yesButtonRect.right + buttonPadding * 2;
    yesButtonWidth = max(yesButtonWidth, minButtonWidth);
    int noButtonWidth =  noButtonRect.right + buttonPadding * 2;
    noButtonWidth = max(noButtonWidth, minButtonWidth);
    int buttonHeight = max(yesButtonRect.bottom, noButtonRect.bottom)
      + buttonPadding * 2;
    buttonHeight = max(buttonHeight, minButtonHeight);

    const int gap = 10;
    const int panelWidth = yesButtonWidth + gap + noButtonWidth;
    const int panelHeight = buttonHeight;

    const DWORD flags = WS_CHILD | WS_VISIBLE;
    HWND buttons = CreateWindowW(panelClassName.c_str(), 0, flags, 0, 0,
                                 panelWidth, panelHeight, parent, 0, 0, 0);
    CreateWindowW(L"BUTTON", yesLabel.c_str(), flags,
                  0, 0, yesButtonWidth, buttonHeight,
                  buttons, reinterpret_cast<HMENU>(IDYES), 0, 0);
    CreateWindowW(L"BUTTON", noLabel.c_str(), flags,
                  yesButtonWidth + gap, 0, noButtonWidth, buttonHeight,
                  buttons, reinterpret_cast<HMENU>(IDNO), 0, 0);
    return buttons;
  }

  void UpdateSizes(HWND window, const HWND text, const HWND buttons)
  {
    RECT textRect;
    GetWindowRect(text, &textRect);

    RECT buttonsRect;
    GetWindowRect(buttons, &buttonsRect);

    const int padding = 10;
    const int margin = 10;
    const int gap = 10;
    const int contentWidth = max(textRect.right, buttonsRect.right);
    const int contentHeight = textRect.bottom + buttonsRect.bottom + margin;
    const int windowWidth = contentWidth + padding * 2;
    const int windowHeight = contentHeight + padding * 2;

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    const int windowX = workArea.right - windowWidth - margin;
    const int windowY = workArea.bottom - windowHeight - margin;
    SetWindowPos(window, 0, windowX, windowY, windowWidth, windowHeight, 0);

    const int textX = padding;
    const int textY = padding;
    SetWindowPos(text, 0, padding, padding, 0, 0, SWP_NOSIZE);

    const int buttonsX = windowWidth - buttonsRect.right - padding;
    const int buttonsY = windowHeight - buttonsRect.bottom - padding;
    SetWindowPos(buttons, 0, buttonsX, buttonsY, 0, 0, SWP_NOSIZE);
  }
}

UpdateInstallDialog::UpdateInstallDialog()
{
  window = CreateDialogWindow();
  HWND staticText = CreateStaticText(window);
  HWND buttons = CreateButtons(window);
  UpdateSizes(window, staticText, buttons);
}

UpdateInstallDialog::~UpdateInstallDialog()
{
  DestroyWindow(window);
}

bool UpdateInstallDialog::Show()
{
  ShowWindow(window, SW_SHOW);
  MSG message;
  while(GetMessage(&message, 0, 0, 0) > 0)
  {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }
  ShowWindow(window, SW_HIDE);
  return message.wParam == 0;
}
