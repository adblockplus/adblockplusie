/*
* DownloadDialog.h 2009-01-11 14:18:18 Jakob Holck
*/

#pragma once

enum
{
  UF_BINDSTATUS_FIRST = BINDSTATUS_FINDINGRESOURCE,
  UF_BINDSTATUS_LAST = BINDSTATUS_ACCEPTRANGES
};

#include <atlhost.h>
#include <atlstr.h>

// CPluginDownloadDialog

class CPluginDownloadDialog : public CAxDialogImpl<CPluginDownloadDialog>
{

public:

  CPluginDownloadDialog(){};
  ~CPluginDownloadDialog(){};

  static DWORD WINAPI StartThread (LPVOID param);	//controlling function header

  struct DOWNLOADSTATUS
  {
    ULONG ulProgress;
    ULONG ulProgressMax;
    ULONG ulStatusCode;
    LPCWSTR szStatusText;
  };

  typedef struct THREADSTRUCT				//structure for passing to the controlling function
  {
    CPluginDownloadDialog* _this;
    HANDLE hEventStop;
    CWindow pBar;
    CString url;
    CString path;
    CString errortext;
    CString postdownloadtext;
  } THREADSTRUCT;

  void SetUrlAndPath(CString url_, CString path_);

  enum { IDD = IDD_DOWNLOADDIALOG };

  BEGIN_MSG_MAP(CPluginDownloadDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
    COMMAND_HANDLER(IDC_INSTALLBTN, BN_CLICKED, OnClickedInstall)
    COMMAND_HANDLER(IDC_INSTALLBTN, BN_CLICKED, OnBnClickedInstallbtn)
    CHAIN_MSG_MAP(CAxDialogImpl<CPluginDownloadDialog>)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnClickedInstall(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
  {
    EndDialog(wID);
    return 1;
  }

  LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
  {
    EndDialog(wID);
    return 0;
  }

  struct DOWNLOADPARAM
  {
    HWND hWnd;
    HANDLE hEventStop;
    CString strURL;
    CString strFileName;
  };

private:
  HANDLE m_eventStop;

protected:

  CString m_url;
  CString m_path;
  CString m_errorText;
  CString m_postText;

public:
  LRESULT OnBnClickedInstallbtn(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

};


// CBSCallbackImpl
class CBSCallbackImpl : public IBindStatusCallback
{

public:

  CBSCallbackImpl(HWND hWnd, HANDLE eventStop, CWindow pBar);

  // IUnknown methods
  STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();

  // IBindStatusCallback methods
  STDMETHOD(OnStartBinding)(DWORD, IBinding *);
  STDMETHOD(GetPriority)(LONG *);
  STDMETHOD(OnLowResource)(DWORD);
  STDMETHOD(OnProgress)(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText);
  STDMETHOD(OnStopBinding)(HRESULT, LPCWSTR);
  STDMETHOD(GetBindInfo)(DWORD *, BINDINFO *);
  STDMETHOD(OnDataAvailable)(DWORD, DWORD, FORMATETC *, STGMEDIUM *);
  STDMETHOD(OnObjectAvailable)(REFIID, IUnknown *);

protected:

  ULONG m_ulObjRefCount;

private:

  HWND m_hWnd;
  CWindow m_pBar;
  HANDLE m_hEventStop;
};
