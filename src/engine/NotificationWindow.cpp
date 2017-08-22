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

#include <cassert>
#include "NotificationWindow.h"
#include "../shared/Utils.h"
#include <algorithm>
#include <fstream>
#include "../shared/MsHTMLUtils.h"

// it is taken from src/plugin/Resource.h
#define IDI_ICON_ENABLED 301

namespace {
  // DIP = device independant pixels, as DPI is 96
  const uint32_t kWindowWidth = 400 /*DIP*/;
  const uint32_t kWindowHeight = 120 /*DIP*/;
  const uint32_t kIconSize = 64 /*DIP*/;
  const uint32_t kIconPadding = 10 /*DIP*/;

  // offsets from boundaries of working area of screen
  const uint32_t kWindowMarginRight = 50 /*DIP*/;
  const uint32_t kWindowMarginBottom = 20 /*DIP*/;

  std::vector<uint16_t> iconSizes = []()->std::vector<uint16_t>
  {
    std::vector<uint16_t> iconSizes;
    iconSizes.emplace_back(16);
    iconSizes.emplace_back(19);
    iconSizes.emplace_back(48);
    iconSizes.emplace_back(128);
    iconSizes.emplace_back(256);
    return iconSizes;
  }();

  class DCHandle
  {
  public:
    explicit DCHandle(HDC hDC = nullptr) : m_hDC(hDC)
    {
    }

    ~DCHandle()
    {
      if(m_hDC != nullptr)
      {
        ::DeleteDC(m_hDC);
        m_hDC = nullptr;
      }
    }

    operator HDC()
    {
      return m_hDC;
    }

    int GetDeviceCaps(int nIndex) const
    {
      ATLASSERT(m_hDC != nullptr);
      return ::GetDeviceCaps(m_hDC, nIndex);
    }

    HBITMAP SelectBitmap(HBITMAP value)
    {
      return static_cast<HBITMAP>(::SelectObject(m_hDC, value));
    }
  private:
    DCHandle(const DCHandle&);
    DCHandle& operator=(const DCHandle&);
  private:
    HDC m_hDC;
  };

  /// Case insensitive pattern replacing function.
  /// ReplaceMulti("Some TeXt <PlaceHolder>Some link</A> and text", "<placeholder>", ->"<a>")->
  ///              "Some TeXt <a>Some link</A> and text".
  std::wstring ReplaceMulti(const std::wstring& src, std::wstring placeholder, const std::function<std::wstring()>& replacementGenerator)
  {
    std::transform(placeholder.begin(), placeholder.end(), placeholder.begin(), ::towlower);
    std::wstring srcLowerCase = src;
    std::transform(srcLowerCase.begin(), srcLowerCase.end(), srcLowerCase.begin(), ::towlower);
    std::wstring retValue;
    std::wstring::size_type placeHolderOffset = 0;
    std::wstring::size_type nextStringChunkOffset = 0;
    while ((placeHolderOffset = srcLowerCase.find(placeholder, nextStringChunkOffset)) != std::wstring::npos)
    {
      retValue.append(src.substr(nextStringChunkOffset, placeHolderOffset - nextStringChunkOffset));
      retValue.append(replacementGenerator());
      nextStringChunkOffset = placeHolderOffset + placeholder.length();
    }
    retValue.append(src.substr(nextStringChunkOffset));
    return retValue;
  }
  std::wstring ReplaceMulti(const std::wstring& workingString, const std::wstring& templ, const std::wstring& replacement)
  {
    return ReplaceMulti(workingString, templ, [&replacement]()->std::wstring
    {
      return replacement;
    });
  }
}

NotificationWindow::NotificationWindow(const AdblockPlus::Notification& notification, const std::wstring& htmlFileDir)
{
  const std::wstring filePath = htmlFileDir + L"NotificationWindow.html";
  std::wifstream ifs(filePath);
  assert(ifs.good() && "Cannot open NotificationWindow.html file");
  if (!ifs.good())
  {
    throw std::runtime_error("Cannot read NotificationWindow.html");
  }
  m_htmlPage.assign((std::istreambuf_iterator<wchar_t>(ifs)), std::istreambuf_iterator<wchar_t>());

  m_links = ToUtf16Strings(notification.GetLinks());
  auto body = ToUtf16String(notification.GetTexts().message);
  uint32_t linkIDCounter = 0;
  body = ReplaceMulti(body, L"<a>", [this, &linkIDCounter]()->std::wstring
  {
    return L"<a href=\"#\" data-linkID=\"" + std::to_wstring(linkIDCounter++) + L"\">";
  });
  assert(linkIDCounter == m_links.size() && "The amount of links in the text is different from the amount of provided links");
  m_htmlPage = ReplaceMulti(m_htmlPage, L"<!--Title-->", ToUtf16String(notification.GetTexts().title));
  m_htmlPage = ReplaceMulti(m_htmlPage, L"<!--Body-->", body);
}

NotificationWindow::~NotificationWindow()
{
}

LRESULT NotificationWindow::OnCreate(const CREATESTRUCT* /*createStruct*/) {
  {
    DCHandle hdc(GetDC());
    m_dpi = hdc.GetDeviceCaps(LOGPIXELSX);
  }
  m_bgColor.CreateSolidBrush(RGB(255, 255, 255));

  CRect iconRect(CPoint(0, 0), CSize(kIconSize + 2 * kIconPadding, kIconSize + 2 * kIconPadding));
  m_icon.Create(m_hWnd, DPIAware(iconRect), nullptr, WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE);
  LoadABPIcon();

  m_axIE.Create(m_hWnd, DPIAware(CRect(CPoint(iconRect.right, 0), CSize(kWindowWidth - iconRect.right, kWindowHeight))),
    L"", WS_CHILD | WS_VISIBLE, 0, kHTMLDocumentCtrlID);
  m_axIE.CreateControl((L"mshtml:" + m_htmlPage).c_str());
  ATL::CComPtr<IAxWinAmbientDispatch> axWinAmbient;
  if (SUCCEEDED(m_axIE.QueryHost(&axWinAmbient))) {
    // disable web browser context menu
    axWinAmbient->put_AllowContextMenu(VARIANT_FALSE);
    // make web browser DPI aware, so the browser itself sets zoom level and
    // cares about rendering (not zooming) in the proper size.
    DWORD docFlags;
    axWinAmbient->get_DocHostFlags(&docFlags);
    docFlags |= DOCHOSTUIFLAG_DPI_AWARE;
    // remove DOCHOSTUIFLAG_SCROLL_NO, so it's scrollable
    docFlags &= ~DOCHOSTUIFLAG_SCROLL_NO;
    axWinAmbient->put_DocHostFlags(docFlags);
  }
  // kHTMLDocumentCtrlID works here
  AtlAdviseSinkMap(this, true);

  SetMsgHandled(false);
  return 0;
}

LRESULT NotificationWindow::OnCtlColor(UINT /*msg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& handled)
{
  if (reinterpret_cast<HWND>(lParam) != m_icon)
  {
    handled = FALSE;
  }
  return reinterpret_cast<LRESULT>(static_cast<HBRUSH>(m_bgColor));
}

LRESULT NotificationWindow::OnClick(UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*handled*/)
{
  if(m_onClickCallback)
    m_onClickCallback();
  return 0;
}

void NotificationWindow::OnSize(uint32_t wParam, CSize size)
{
  if (m_icon)
  {
    CRect rect(CPoint(0, 0), DPIAware(CSize(kIconSize + 2 * kIconPadding, kIconSize + 2 * kIconPadding)));
    m_icon.SetWindowPos(0, &rect, SWP_NOMOVE);
  }
  if (m_axIE)
  {
    size.cx -= DPIAware(kIconSize + 2 * kIconPadding);
    CRect rect(CPoint(0, 0), size);
    m_axIE.SetWindowPos(0, &rect, SWP_NOMOVE);
  }
  SetMsgHandled(false);
}

void NotificationWindow::OnDestroy()
{
  AtlAdviseSinkMap(this, false);
  // and proceed as usual
  SetMsgHandled(false);
}

void __stdcall NotificationWindow::OnHTMLDocumentClick(IHTMLEventObj* eventObject)
{
  // stop propagating the event since it's handled by us and should not cause any other actions.
  if (!eventObject)
    return;
  eventObject->put_cancelBubble(VARIANT_TRUE);
  eventObject->put_returnValue(ATL::CComVariant(false));
  ATL::CComPtr<IHTMLElement> htmlElement;
  if (FAILED(eventObject->get_srcElement(&htmlElement)) || !htmlElement) {
    return;
  }
  ATL::CComBSTR tag;
  htmlElement->get_tagName(&tag);
  const wchar_t expectedTag[] = { L"a" };
  if (_wcsnicmp(tag, expectedTag, min(sizeof(expectedTag), tag.Length())) != 0) {
    return;
  }
  auto classAttr = GetHtmlElementAttribute(*htmlElement, ATL::CComBSTR(L"class"));
  if (classAttr.attributeValue == L"closeButton")
  {
    if (m_onCloseCallback)
      m_onCloseCallback();
    return;
  }
  if (!m_onLinkClickedCallback)
  {
    return;
  }
  auto linkIDAttr = GetHtmlElementAttribute(*htmlElement, ATL::CComBSTR(L"data-linkID"));
  uint32_t linkID = 0;
  if (!linkIDAttr.attributeValue.empty() && (linkID = std::stoi(linkIDAttr.attributeValue)) < m_links.size())
  {
    m_onLinkClickedCallback(m_links[linkID]);
    if (m_onCloseCallback)
      m_onCloseCallback();
  }
}

void __stdcall NotificationWindow::OnHTMLDocumentSelectStart(IHTMLEventObj* eventObject)
{
  if (!eventObject)
    return;
  // disable selecting
  eventObject->put_cancelBubble(VARIANT_TRUE);
  eventObject->put_returnValue(ATL::CComVariant(false));
}

void NotificationWindow::LoadABPIcon()
{
  ScopedModule m_adblockPlusDLL;
  if (!(m_adblockPlusDLL.Open(L"AdblockPlus32.dll", LOAD_LIBRARY_AS_DATAFILE) ||
    m_adblockPlusDLL.Open(L"AdblockPlus64.dll", LOAD_LIBRARY_AS_DATAFILE) ||
    // for debug
    m_adblockPlusDLL.Open(L"AdblockPlus.dll", LOAD_LIBRARY_AS_DATAFILE)))
  {
    return;
  }
  auto iconSizeIterator = lower_bound(iconSizes.begin(), iconSizes.end(), DPIAware(kIconSize));
  if (iconSizeIterator == iconSizes.end())
  {
    iconSizeIterator = iconSizes.rbegin().base();
  }

  auto hIcon = static_cast<HICON>(::LoadImageW(m_adblockPlusDLL, (L"#" + std::to_wstring(IDI_ICON_ENABLED)).c_str(),
    IMAGE_ICON, *iconSizeIterator, *iconSizeIterator, LR_SHARED));
  if (hIcon == nullptr)
  {
    return;
  }

  HDC screenDC = m_icon.GetDC();
  DCHandle tmpDC(::CreateCompatibleDC(screenDC));
  ScopedObjectHandle<HBITMAP> bitmap;
  bitmap = CreateCompatibleBitmap(screenDC, DPIAware(kIconSize), DPIAware(kIconSize));
  HBITMAP prevBitmap = tmpDC.SelectBitmap(bitmap);
  CRect tmpRect(CPoint(0, 0), DPIAware(CSize(kIconSize, kIconSize)));
  FillRect(tmpDC, &tmpRect, m_bgColor);
  ::DrawIconEx(tmpDC, 0, 0, hIcon, tmpRect.Width(), tmpRect.Height(), 0, nullptr, DI_NORMAL);
  m_iconImg.Attach(bitmap.Detach());
  tmpDC.SelectBitmap(prevBitmap);
  m_icon.SetBitmap(m_iconImg);
}

POINT NotificationBorderWindow::GetWindowCoordinates() {
  HMONITOR primaryMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY);
  {
    DCHandle hdc(GetDC());
    m_dpi = hdc.GetDeviceCaps(LOGPIXELSX);
  }
  MONITORINFO monitorInfo = {};
  monitorInfo.cbSize = sizeof(monitorInfo);
  GetMonitorInfo(primaryMonitor, &monitorInfo);
  int windowX = monitorInfo.rcWork.right - DPIAware(kWindowWidth + kWindowMarginRight);
  int windowY = monitorInfo.rcWork.bottom - DPIAware(kWindowHeight + kWindowMarginBottom);
  POINT coords = {windowX, windowY};
  return coords;
}

NotificationBorderWindow::NotificationBorderWindow(const AdblockPlus::Notification& notification, const std::wstring& htmlFileDir)
  : m_content(notification, htmlFileDir)
{
  m_content.SetOnClick([this]
  {
    PostMessage(WM_CLOSE);
  });
  m_content.SetOnClose([this]
  {
    PostMessage(WM_CLOSE);
  });
}

LRESULT NotificationBorderWindow::OnCreate(const CREATESTRUCT* createStruct)
{
  auto windowCoords = GetWindowCoordinates();
  MoveWindow(windowCoords.x, windowCoords.y, DPIAware(kWindowWidth), DPIAware(kWindowHeight));

  RECT clientRect;
  GetClientRect(&clientRect);
  // make one pixel border
  clientRect.top += 1;
  clientRect.left += 1;
  clientRect.bottom -= 1;
  clientRect.right -= 1;
  m_content.Create(m_hWnd, clientRect, nullptr, WS_CHILD | WS_VISIBLE);
  auto err = GetLastError();
  SetMsgHandled(false);
  return 0;
}

void NotificationBorderWindow::OnSize(uint32_t wParam, CSize size)
{
  if (m_content.IsWindow())
  {
    RECT clientRect;
    GetClientRect(&clientRect);
    clientRect.top += 1;
    clientRect.left += 1;
    clientRect.bottom -= 1;
    clientRect.right -= 1;
    m_content.MoveWindow(&clientRect);
  }
  SetMsgHandled(false);
}

LRESULT NotificationBorderWindow::OnClick(UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*handled*/)
{
  DestroyWindow();
  return 0;
}

void NotificationBorderWindow::OnFinalMessage(HWND) {
  if (!!m_onDestroyedCallback) {
    m_onDestroyedCallback();
  }
}