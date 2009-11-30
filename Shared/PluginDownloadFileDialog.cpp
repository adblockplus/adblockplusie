#include "AdPluginStdAfx.h"

#include "AdPluginDictionary.h"
#include "AdPluginDownloadFileDialog.h"
#include "AdPluginClient.h"


#define    WM_USER_ENDDOWNLOAD           (WM_USER + 1)
#define    WM_USER_DISPLAYSTATUS         (WM_USER + 2)


// CDownloadDialog
LRESULT CDownloadFileDialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CDownloadFileDialog>::OnInitDialog(uMsg, wParam, lParam, bHandled);

	this->CenterWindow();

	SetTimer(IDT_TIMER, 30, NULL);

	try 
	{	    
	    CAdPluginDictionary* dictionary = CAdPluginDictionary::GetInstance();

		CString text;

		text = dictionary->Lookup("DOWNLOAD_TITLE");
	    this->SetWindowTextW(text);
		
		text = dictionary->Lookup("DOWNLOAD_PROGRESS_TEXT");
		SetDlgItemText(IDC_DOWNLOAD_MSG, text);

		text = dictionary->Lookup("CANCEL");
		SetDlgItemText(IDCANCEL, text);
	}
	catch(std::runtime_error&) 
	{
	}

	CWindow& bar = ATL::CWindow::GetDlgItem(IDC_DOWNLOAD_PROGRESS);

    bar.PostMessage(PBM_SETPOS);

	THREADSTRUCT* param = new THREADSTRUCT;

	param->_this = this;
	param->url = m_url; 
	param->path = m_path;
	param->hEventStop = m_eventStop;
	param->wndBar = ATL::CWindow::GetDlgItem(IDC_DOWNLOAD_PROGRESS);
	param->wndMessage = ATL::CWindow::GetDlgItem(IDC_DOWNLOAD_MSG);

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


LRESULT CDownloadFileDialog::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    DestroyWindow();

	return 0;
}


DWORD WINAPI CDownloadFileDialog::StartThread(LPVOID param)
{
DEBUG("StartThread")
	THREADSTRUCT* ts = (THREADSTRUCT*)param;
	
	DOWNLOADPARAM *const pDownloadParam = static_cast<DOWNLOADPARAM *>(param);

	CAdPluginDownloadFileCallbackImpl bsc(pDownloadParam->hWnd, pDownloadParam->hEventStop, ts->wndBar, ts->wndMessage);

	HRESULT hr = ::URLDownloadToFile(NULL, ts->url, ts->path, 0, &bsc);

//	pDownloadParam->strFileName.ReleaseBuffer(SUCCEEDED(hr) ? -1 : 0);

	//you can also call AfxEndThread() here
	delete ts;

	return TRUE;
}


void CDownloadFileDialog::SetUrlAndPath(CString url, CString path) 
{
	m_url = url;
	m_path = path;

    DEBUG("url:" + url)
    DEBUG("path:" + path)
}


CAdPluginDownloadFileCallbackImpl::CAdPluginDownloadFileCallbackImpl(HWND hWnd, HANDLE eventStop, CWindow wndBar, CWindow wndMessage)
{
	m_hWnd = hWnd;  // the window handle to display status
	m_wndBar = wndBar;
	m_wndMessage = wndMessage;
	m_hEventStop = eventStop;
	m_ulObjRefCount = 1;
}

STDMETHODIMP CAdPluginDownloadFileCallbackImpl::QueryInterface(REFIID riid, void **ppvObject)
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

STDMETHODIMP_(ULONG) CAdPluginDownloadFileCallbackImpl::AddRef()
{
	return ++m_ulObjRefCount;
}

STDMETHODIMP_(ULONG) CAdPluginDownloadFileCallbackImpl::Release()
{
	return --m_ulObjRefCount;
}

STDMETHODIMP CAdPluginDownloadFileCallbackImpl::OnStartBinding(DWORD, IBinding *)
{
	return S_OK;
}

STDMETHODIMP CAdPluginDownloadFileCallbackImpl::GetPriority(LONG *)
{
	return E_NOTIMPL;
}

STDMETHODIMP CAdPluginDownloadFileCallbackImpl::OnLowResource(DWORD)
{
	return S_OK;
}

STDMETHODIMP CAdPluginDownloadFileCallbackImpl::OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText)
{
DEBUG("OnProcess")
	if (ulStatusCode < UF_BINDSTATUS_FIRST || ulStatusCode > UF_BINDSTATUS_LAST)
	{
		ulStatusCode = UF_BINDSTATUS_LAST + 1;
	}	
	
	if (m_hWnd != NULL && ::IsWindow(m_wndBar) && ::IsWindow(m_wndMessage))
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
    		{
		        CStringW x;
		        x.Format(L"Downloaded %d of %d", ulProgress, ulProgressMax);
    		
		    DEBUG(x)

		        m_wndMessage.SetWindowTextW(x.GetBuffer());

			    m_wndBar.PostMessage(PBM_SETPOS);
			}
			break;

		case 5: // progress > 0;

			if (ulProgressMax > 0)
			{
			    bool isMb = (ulProgressMax > 1024000L);

                float fProgress = 0.0;
                float fProgressMax = 0.0;
                			    
			    if (isMb)
			    {
    			    fProgress = float(ulProgress) / 1024000.0f;
			        fProgressMax = float(ulProgressMax) / 1024000.0f;
			    }
			    else
			    {
    			    fProgress = float(ulProgress) / 1024.0f;
			        fProgressMax = float(ulProgressMax) / 1024.0f;
			    }

		        CStringW x;
		        if (isMb)
		        {
    		        x.Format(L"Downloaded %.1f of %.1f MB", fProgress, fProgressMax);
		        }
		        else
		        {
    		        x.Format(L"Downloaded %.1f of %.1f kB", fProgress, fProgressMax);
		        }
DEBUG(x)
		        m_wndMessage.SetWindowTextW(x.GetBuffer());
		
    			m_wndBar.PostMessage(PBM_SETPOS, (LPARAM)(ULONG)((FLOAT)(100*ulProgress) / (FLOAT)ulProgressMax));
			}
			break;

		case 6:
			m_wndBar.PostMessage(PBM_SETPOS, (LPARAM)100);
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

STDMETHODIMP CAdPluginDownloadFileCallbackImpl::OnStopBinding(HRESULT, LPCWSTR)
{
	return S_OK;
}

STDMETHODIMP CAdPluginDownloadFileCallbackImpl::GetBindInfo(DWORD *, BINDINFO *)
{
	return S_OK;
}

STDMETHODIMP CAdPluginDownloadFileCallbackImpl::OnDataAvailable(DWORD, DWORD, FORMATETC *, STGMEDIUM *)
{
	return S_OK;
}

STDMETHODIMP CAdPluginDownloadFileCallbackImpl::OnObjectAvailable(REFIID, IUnknown *)
{
	return S_OK;
}
