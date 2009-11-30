#include "AdPluginStdAfx.h"

#include "AdPluginListener.h"


STDMETHODIMP CAdPluginListener::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, 
								 WORD wFlags, DISPPARAMS* pDispParams, 
								 VARIANT* pvarResult, EXCEPINFO*  pExcepInfo,
								 UINT* puArgErr)
{
	if (!pDispParams)
	{
		return E_INVALIDARG;
	}

	switch(dispidMember)
	{
	case DISPID_AMBIENT_DLCONTROL:
		// respond to this ambient to indicate that we only want to
		// download the page, but we don't want to run scripts,
		// Java applets, or ActiveX controls
		V_VT(pvarResult) = VT_I4;
		V_I4(pvarResult) =  DLCTL_DOWNLOADONLY |
							DLCTL_NO_SCRIPTS |
							DLCTL_NO_JAVA |
							DLCTL_NO_DLACTIVEXCTLS |
							DLCTL_NO_RUNACTIVEXCTLS | 
							DLCTL_NO_FRAMEDOWNLOAD | 
							DLCTL_NO_BEHAVIORS |
							DLCTL_NO_CLIENTPULL |
							DLCTL_URL_ENCODING_ENABLE_UTF8;
		break;
	case DISPID_AMBIENT_USERMODE:
		// put MSHTML into design mode
		V_VT(pvarResult) = VT_BOOL;
		V_BOOL(pvarResult) = VARIANT_TRUE;
		break;
	case DISPID_AMBIENT_CODEPAGE:
		V_VT(pvarResult) = VT_I4;
		V_I4(pvarResult) = CP_UTF8;
		return S_OK;
		break;
	//case DISPID_AMBIENT_CHARSET:
	//	V_VT(pvarResult) = VT_I4;
	//	V_I4(pvarResult) = CP_UTF8;
	//	return S_OK;
	//	break;
	default:
		return DISP_E_MEMBERNOTFOUND;
	}
	return S_OK;
}


// CAdPluginListener

STDMETHODIMP CAdPluginListener::SaveObject()
{
/*	LPPERSISTSTORAGE lpPS;
	SCODE sc = E_FAIL;

	OutputDebugString("In IOCS::SaveObject\r\n");

	// get a pointer to IPersistStorage
	HRESULT hErr = m_pSite->m_lpOleObject->QueryInterface(IID_IPersistStorage, (LPVOID FAR *)&lpPS);

	// save the object
	if (hErr == NOERROR)
		{
		sc =  OleSave(lpPS, m_pSite->m_lpObjStorage, TRUE) ;
		lpPS->SaveCompleted(NULL);
		lpPS->Release();
		}

	return sc;
	*/
	return E_FAIL;
}


STDMETHODIMP CAdPluginListener::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, LPMONIKER FAR* ppmk)
{
	// need to null the out pointer
	*ppmk = NULL;

	return E_NOTIMPL;
}


STDMETHODIMP CAdPluginListener::GetContainer(LPOLECONTAINER FAR* ppContainer)
{
	// NULL the out pointer
	*ppContainer = NULL;

	return E_NOTIMPL;
}


STDMETHODIMP CAdPluginListener::ShowObject()
{
	return NOERROR;
}

STDMETHODIMP CAdPluginListener::OnShowWindow(BOOL fShow)
{
/*	OutputDebugString("In IOCS::OnShowWindow\r\n");
	m_pSite->m_fObjectOpen = fShow;
	InvalidateRect(m_pSite->m_lpDoc->m_hDocWnd, NULL, TRUE);

	// if object window is closing, then bring container window to top
	if (! fShow) {
		BringWindowToTop(m_pSite->m_lpDoc->m_hDocWnd);
		SetFocus(m_pSite->m_lpDoc->m_hDocWnd);
	}
	*/
	return S_OK;
}


STDMETHODIMP CAdPluginListener::RequestNewObjectLayout()
{
	return E_NOTIMPL;
}
