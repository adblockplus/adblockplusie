#pragma once

enum
{
	UF_BINDSTATUS_FIRST = BINDSTATUS_FINDINGRESOURCE,
	UF_BINDSTATUS_LAST = BINDSTATUS_ACCEPTRANGES
};

#include "resource.h"       // main symbols

#include <atlhost.h>
#include <atlstr.h>

// CDownloadFileDialog

class CDownloadFileDialog : public CAxDialogImpl<CDownloadFileDialog>
{

public:

	CDownloadFileDialog(){};
	~CDownloadFileDialog(){};

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
		CDownloadFileDialog* _this;
		HANDLE hEventStop;
		CWindow wndBar;
		CWindow wndMessage;
		CString url;
		CString path;
	} THREADSTRUCT;
	
	void SetUrlAndPath(CString url_, CString path_);

	enum { IDD = IDD_DOWNLOAD_FILE_DIALOG };

    BEGIN_MSG_MAP(CDownloadFileDialog)
	    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog) 
	    COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
	    CHAIN_MSG_MAP(CAxDialogImpl<CDownloadFileDialog>)
    END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

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
	CProgressCtrl m_progress;
};


// CBSCallbackImpl
class CAdPluginDownloadFileCallbackImpl  : public IBindStatusCallback
{

public:

	CAdPluginDownloadFileCallbackImpl(HWND hWnd, HANDLE eventStop, CWindow wndBar, CWindow wndMessage);

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
	CWindow m_wndBar;
	CWindow m_wndMessage;
	HANDLE m_hEventStop;
};
