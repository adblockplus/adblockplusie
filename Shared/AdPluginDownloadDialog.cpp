#include "AdPluginStdAfx.h"

#include "AdPluginDictionary.h"
#include "AdPluginDownloadDialog.h"
#include "AdPluginClient.h"


#define    WM_USER_ENDDOWNLOAD           (WM_USER + 1)
#define    WM_USER_DISPLAYSTATUS         (WM_USER + 2)


//	Just a small helper to guarantee that PostMessage succeeded
inline void _PostMessage(CWindow& wnd, UINT message, WPARAM wParam = 0, LPARAM lParam = 0)
{
	while (!wnd.PostMessage(message, wParam, lParam))
	{
		::Sleep(11);
	}
}


// CDownloadDialog
LRESULT CDownloadDialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CDownloadDialog>::OnInitDialog(uMsg, wParam, lParam, bHandled);

	this->CenterWindow();

	SetTimer(IDT_TIMER, 30, NULL);

	try 
	{	    
	    CAdPluginDictionary* dictionary = CAdPluginDictionary::GetInstance();

		CString text;

		text = dictionary->Lookup("DOWNLOAD_TITLE");
	    this->SetWindowTextW(text);
		
		text = dictionary->Lookup("DOWNLOAD_PROGRESS_TEXT");
		SetDlgItemText(IDC_INSTALLMSG, text);

		text = dictionary->Lookup("DOWNLOAD_UPDATE_BUTTON");
		SetDlgItemText(IDC_INSTALLBTN, text);

		text = dictionary->Lookup("CANCEL");
		SetDlgItemText(IDCANCEL, text);

		m_errorText = dictionary->Lookup("DOWNLOAD_DOWNLOAD_ERROR_TEXT");
		m_postText = dictionary->Lookup("DOWNLOAD_POST_DOWNLOAD_TEXT");
	}
	catch(std::runtime_error&) 
	{
	}

	CWindow& pBar = ATL::CWindow::GetDlgItem(IDC_PROGRESS1);

	_PostMessage(pBar, PBM_SETPOS, 0);

	THREADSTRUCT* param = new THREADSTRUCT;

	param->_this = this;
	param->url = m_url; 
	param->path = m_path;
	param->hEventStop = m_eventStop;
	param->errortext = m_errorText;
	param->postdownloadtext = m_postText;
	param->pBar = ATL::CWindow::GetDlgItem(IDC_PROGRESS1);

	DWORD dwThreadId = 1;
	HANDLE hThread = ::CreateThread(
        NULL,                       // no security attributes
        0,                          // use default stack size 
        StartThread,                // thread function
        param,                     // argument to thread function
        0,                          // use default creation flags
        &dwThreadId);               // returns the thread identifier 

	// Check the return value for success.
	if (hThread)
	{
	    ::CloseHandle(hThread);
	}
	
	bHandled = TRUE;

	return TRUE;  // return TRUE  unless you set the focus to a control
}


DWORD WINAPI CDownloadDialog::StartThread(LPVOID param)
{
	THREADSTRUCT* ts = (THREADSTRUCT*)param;
	
	DOWNLOADPARAM *const pDownloadParam = static_cast<DOWNLOADPARAM *>(param);

	CBSCallbackImpl bsc(pDownloadParam->hWnd, pDownloadParam->hEventStop, ts->pBar);

	HRESULT hr = ::URLDownloadToFile(NULL, ts->url, ts->path, 0, &bsc);

	pDownloadParam->strFileName.ReleaseBuffer(SUCCEEDED(hr) ? -1 : 0);

	HWND hWnd;

	try 
	{
		if (hr != S_OK)
		{
			hWnd = ts->_this->GetDlgItem(IDC_INSTALLMSG);
			::SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)ts->errortext.GetBuffer());
	        
            DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_UPDATER, PLUGIN_ERROR_UPDATER_DOWNLOAD_FILE, "DownloadDialog::StartThead - URLDownloadToFile")
		}
		else
		{
			hWnd = ts->_this->GetDlgItem(IDC_INSTALLBTN);
			::EnableWindow(hWnd, TRUE);
		
			hWnd = ts->_this->GetDlgItem(IDC_INSTALLMSG);
			::SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)ts->postdownloadtext.GetBuffer());

			_PostMessage(ts->pBar, PBM_SETPOS, (LPARAM)100);
		}
	}
	catch(std::runtime_error&)
	{
	}

	//you can also call AfxEndThread() here
	delete ts;

	return TRUE;
}


void CDownloadDialog::SetUrlAndPath(CString url, CString path) 
{
	m_url = url;
	m_path = path;
}

LRESULT CDownloadDialog::OnBnClickedInstallbtn(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	return 0;
}


CBSCallbackImpl::CBSCallbackImpl(HWND hWnd, HANDLE eventStop, CWindow pBar)
{
	m_hWnd = hWnd;  // the window handle to display status
	m_pBar = pBar;
	m_hEventStop = eventStop;
	m_ulObjRefCount = 1;
}

STDMETHODIMP CBSCallbackImpl::QueryInterface(REFIID riid, void **ppvObject)
{
	*ppvObject = NULL;
	
	// IUnknown
	if (::IsEqualIID(riid, __uuidof(IUnknown)))
	{
		*ppvObject = this;
	}
	// IBindStatusCallback
	else if (::IsEqualIID(riid, __uuidof(IBindStatusCallback)))
	{
		*ppvObject = static_cast<IBindStatusCallback *>(this);
	}

	if (*ppvObject)
	{
		(*reinterpret_cast<LPUNKNOWN *>(ppvObject))->AddRef();

		return S_OK;
	}
	
	return E_NOINTERFACE;
}                                             

STDMETHODIMP_(ULONG) CBSCallbackImpl::AddRef()
{
	return ++m_ulObjRefCount;
}

STDMETHODIMP_(ULONG) CBSCallbackImpl::Release()
{
	return --m_ulObjRefCount;
}

STDMETHODIMP CBSCallbackImpl::OnStartBinding(DWORD, IBinding *)
{
	return S_OK;
}

STDMETHODIMP CBSCallbackImpl::GetPriority(LONG *)
{
	return E_NOTIMPL;
}

STDMETHODIMP CBSCallbackImpl::OnLowResource(DWORD)
{
	return S_OK;
}

STDMETHODIMP CBSCallbackImpl::OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText)
{
	if (ulStatusCode < UF_BINDSTATUS_FIRST || ulStatusCode > UF_BINDSTATUS_LAST)
	{
		ulStatusCode = UF_BINDSTATUS_LAST + 1;
	}	
	
	if (m_hWnd != NULL && ::IsWindow(m_pBar))
	{
		// inform the dialog box to display current status, don't use PostMessage
		//CDownloadDialog::DOWNLOADSTATUS downloadStatus = { ulProgress, ulProgressMax, ulStatusCode, szStatusText };
		//::SendMessage(m_hWnd, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(&downloadStatus));
		
		switch (ulStatusCode) 
		{
		case 1: // url
			break;

		case 2: //ip
			break;

		case 4: // progress < 1;
			_PostMessage(m_pBar, PBM_SETPOS, (LPARAM)0);
			break;

		case 5: // progress > 0;
			if (ulProgressMax > 0)
			{
    			_PostMessage(m_pBar, PBM_SETPOS, (LPARAM)(ULONG)((FLOAT)(100*ulProgress) / (FLOAT)ulProgressMax));
			}
			break;

		case 6:
			_PostMessage(m_pBar, PBM_SETPOS, (LPARAM)100);
			break;

		case 11: // not text
			break;

		case 13: // MIME
			break;
		}
	}

	if (m_hEventStop != NULL)
	{
		if (::WaitForSingleObject(m_hEventStop, 0) == WAIT_OBJECT_0)
		{
			//::SendMessage(m_hWnd, WM_SETTEXT, 0, (LPARAM)_T(""));	
			return E_ABORT;  // canceled by the user
		}
	}

	return S_OK;
}

STDMETHODIMP CBSCallbackImpl::OnStopBinding(HRESULT, LPCWSTR)
{
	return S_OK;
}

STDMETHODIMP CBSCallbackImpl::GetBindInfo(DWORD *, BINDINFO *)
{
	return S_OK;
}

STDMETHODIMP CBSCallbackImpl::OnDataAvailable(DWORD, DWORD, FORMATETC *, STGMEDIUM *)
{
	return S_OK;
}

STDMETHODIMP CBSCallbackImpl::OnObjectAvailable(REFIID, IUnknown *)
{
	return S_OK;
}
