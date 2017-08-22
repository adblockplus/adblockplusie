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

#ifndef NOTIFICATION_WINDOW_H
#define NOTIFICATION_WINDOW_H

#include <atlbase.h>
#include <atlwin.h>
#include <atlctl.h>
#include <atlimage.h>
#include <AdblockPlus/JsValue.h>
#include <AdblockPlus/Notification.h>
#include <functional>
#include <MsHtmdid.h>

class IconStaticControl : public ATL::CWindow
{
public:
  explicit IconStaticControl(HWND hWnd = nullptr) : ATL::CWindow(hWnd)
  { }

  IconStaticControl& operator=(HWND hWnd)
  {
    m_hWnd = hWnd;
    return *this;
  }

  HWND Create(HWND hWndParent, ATL::_U_RECT rect = nullptr, LPCTSTR szWindowName = nullptr,
      DWORD dwStyle = 0, DWORD dwExStyle = 0,
      ATL::_U_MENUorID MenuOrID = nullptr, LPVOID lpCreateParam = nullptr)
  {
    return ATL::CWindow::Create(GetWndClassName(), hWndParent, rect.m_lpRect, szWindowName, dwStyle, dwExStyle, MenuOrID.m_hMenu, lpCreateParam);
  }

  static wchar_t* GetWndClassName()
  {
    return L"STATIC";
  }

  void SetBitmap(HBITMAP hBitmap)
  {
    ATLASSERT(::IsWindow(m_hWnd));
    ::SendMessage(m_hWnd, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(hBitmap));
  }
};

template<typename T>
class ScopedObjectHandle
{
public:
  explicit ScopedObjectHandle(T handle = nullptr) : m_handle(handle)
  { }

  ~ScopedObjectHandle()
  {
    if(m_handle != nullptr)
    {
      ::DeleteObject(m_handle);
      m_handle = nullptr;
    }
  }

  ScopedObjectHandle& operator=(T handle)
  {
    if(m_handle != nullptr && m_handle != handle)
      ::DeleteObject(m_handle);
    m_handle = handle;
    return *this;
  }

  T Detach()
  {
    T retValue = m_handle;
    m_handle = nullptr;
    return retValue;
  }

  operator T()
  {
    return m_handle;
  }

  operator bool() const
  {
    return m_handle == nullptr;
  }
protected:
  T m_handle;
private:
  ScopedObjectHandle(const ScopedObjectHandle&);
  ScopedObjectHandle& operator=(const ScopedObjectHandle&);
};

class CBrush : public ScopedObjectHandle<HBRUSH>
{
public:
  explicit CBrush(HBRUSH brush = nullptr) : ScopedObjectHandle(brush)
  {
  }

  void CreateSolidBrush(COLORREF crColor)
  {
    ATLASSERT(m_handle == nullptr);
    m_handle = ::CreateSolidBrush(crColor);
  }
};

class ScopedModule
{
public:
  ScopedModule()
    : m_hModule(nullptr)
  {
  }

  bool Open(const wchar_t* fileName, int flags)
  {
    m_hModule = LoadLibraryEx(fileName, nullptr, flags);
    return m_hModule != nullptr;
  }
  
  ~ScopedModule()
  {
    if (m_hModule != nullptr)
    {
      FreeLibrary(m_hModule);
      m_hModule = nullptr;
    }
  }
  operator HMODULE()
  {
    return m_hModule;
  }
private:
  ScopedModule(const ScopedModule&);
  ScopedModule& operator=(const ScopedModule&);
private:
  HMODULE m_hModule;
};

class DpiAwareness {
public:
  DpiAwareness() : m_dpi(96)
  {
  }
protected:
  uint32_t DPIAware(uint32_t value) const {
    return MulDiv(value, m_dpi, 96);
  }
  SIZE DPIAware(SIZE value) const {
    return CSize(DPIAware(value.cx), DPIAware(value.cy));
  }
  RECT DPIAware(RECT value) const {
    return CRect(DPIAware(value.left), DPIAware(value.top), DPIAware(value.right), DPIAware(value.bottom));
  }
  uint32_t m_dpi;
};

enum
{
  // ID of HTMLDocument ActiveX control, it's used for event binding.
  kHTMLDocumentCtrlID = 101
};

class NotificationWindow : public ATL::CWindowImpl<NotificationWindow>
  , ATL::IDispEventImpl<kHTMLDocumentCtrlID, NotificationWindow, &DIID_HTMLDocumentEvents2, &LIBID_MSHTML, 4, 0>
  , protected DpiAwareness
{
public:
  explicit NotificationWindow(const AdblockPlus::Notification& notification, const std::wstring& htmlFileDir);
  ~NotificationWindow();
  BEGIN_MSG_MAP(NotificationWindow)
    if (uMsg == WM_CREATE)
    {
      SetMsgHandled(TRUE);
      lResult = OnCreate(reinterpret_cast<CREATESTRUCT*>(lParam));
      if(IsMsgHandled())
        return TRUE;
    }
    MESSAGE_HANDLER(WM_LBUTTONUP, OnClick)
    MESSAGE_HANDLER(WM_RBUTTONUP, OnClick)
    MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColor)
    if (uMsg == WM_SIZE)
    {
      SetMsgHandled(TRUE);
      OnSize(static_cast<uint32_t>(wParam), CSize(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
      lResult = 0;
      if(IsMsgHandled())
        return TRUE;
    }
    if (uMsg == WM_DESTROY)
    {
      SetMsgHandled(TRUE);
      OnDestroy();
      lResult = 0;
      if(IsMsgHandled())
        return TRUE;
    }
  END_MSG_MAP()

  BEGIN_SINK_MAP(NotificationWindow)
    SINK_ENTRY_EX(kHTMLDocumentCtrlID, DIID_HTMLDocumentEvents2, DISPID_HTMLDOCUMENTEVENTS2_ONCLICK, OnHTMLDocumentClick)
    SINK_ENTRY_EX(kHTMLDocumentCtrlID, DIID_HTMLDocumentEvents2, DISPID_HTMLDOCUMENTEVENTS2_ONSELECTSTART, OnHTMLDocumentSelectStart)
  END_SINK_MAP()

  void SetOnClick(const std::function<void()>& callback)
  {
    m_onClickCallback = callback;
  }
  void SetOnLinkClicked(const std::function<void(const std::wstring& url)>& callback)
  {
    m_onLinkClickedCallback = callback;
  }
  void SetOnClose(const std::function<void()>& callback)
  {
    m_onCloseCallback = callback;
  }
private:
  LRESULT OnCreate(const CREATESTRUCT* createStruct);
  LRESULT OnCtlColor(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& handled);
  LRESULT OnClick(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& handled);
  void OnSize(uint32_t wParam, CSize size);
  void OnDestroy();

  void __stdcall OnHTMLDocumentClick(IHTMLEventObj* pEvtObj);
  void __stdcall OnHTMLDocumentSelectStart(IHTMLEventObj* pEvtObj);

  void LoadABPIcon();
private:
  std::wstring m_htmlPage;
  CBrush m_bgColor;
  ATL::CAxWindow m_axIE;
  ATL::CImage m_iconImg;
  IconStaticControl m_icon;
  std::vector<std::wstring> m_links;
  std::function<void()> m_onClickCallback;
  std::function<void()> m_onCloseCallback;
  std::function<void(const std::wstring& url)> m_onLinkClickedCallback;
};

typedef ATL::CWinTraits<WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, WS_EX_TOOLWINDOW | WS_EX_TOPMOST> NotificationBorderWindowStyles;
class NotificationBorderWindow : public ATL::CWindowImpl<NotificationBorderWindow, ATL::CWindow, NotificationBorderWindowStyles>
  , protected DpiAwareness
{
public:
  DECLARE_WND_CLASS_EX(/*generate class name*/nullptr, CS_DROPSHADOW, WHITE_BRUSH);
  NotificationBorderWindow(const AdblockPlus::Notification& notification, const std::wstring& htmlFileDir);
    BEGIN_MSG_MAP(NotificationWindow)
    if (uMsg == WM_CREATE)
    {
      SetMsgHandled(TRUE);
      lResult = OnCreate(reinterpret_cast<CREATESTRUCT*>(lParam));
      if(IsMsgHandled())
        return TRUE;
    }
    MESSAGE_HANDLER(WM_LBUTTONUP, OnClick)
    MESSAGE_HANDLER(WM_RBUTTONUP, OnClick)
    if (uMsg == WM_SIZE)
    {
      SetMsgHandled(TRUE);
      OnSize(static_cast<uint32_t>(wParam), CSize(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
      lResult = 0;
      if(IsMsgHandled())
        return TRUE;
    }
  END_MSG_MAP()
  void SetOnDestroyed(const std::function<void()>& callback)
  {
    m_onDestroyedCallback = callback;
  }
  void SetOnLinkClicked(const std::function<void(const std::wstring& url)>& callback)
  {
    m_content.SetOnLinkClicked(callback);
  }
private:
  LRESULT OnCreate(const CREATESTRUCT* createStruct);
  void OnSize(uint32_t wParam, CSize size);
  LRESULT OnClick(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& handled);

  void OnFinalMessage(HWND) override;

  // returns {windowX, windowY} of top left corner on the monitor
  POINT GetWindowCoordinates();
private:
  // m_content is used as a holder of all children and we need it to have a border.
  // It seems the most correct way to have a border to set WS_POPUPWINDOW style
  // and paint the border in WM_NCPAINT but it simply does not work here.
  NotificationWindow m_content;
  std::function<void()> m_onDestroyedCallback;
};
#endif
