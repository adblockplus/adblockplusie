#pragma once

#ifdef PRODUCT_AIDONLINE
 #include "../AidOnline/AidOnline.h"
#else
 #include "../AdBlocker/AdBlocker.h"
#endif

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
 #error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif


// CAdPluginListener

class ATL_NO_VTABLE CAdPluginListener :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CAdPluginListener, &CLSID_AdPluginListener>,
	public IDispatchImpl<IAdPluginListener, &IID_IAdPluginListener, &LIBID_AdPluginLib, /*wMajor =*/ 1, /*wMinor =*/ 0>,
	public IOleClientSite
{

public:

	CAdPluginListener()
	{
	}

DECLARE_REGISTRY_RESOURCEID(IDR_ADPLUGIN_LISTENER)


BEGIN_COM_MAP(CAdPluginListener)
	COM_INTERFACE_ENTRY(IAdPluginListener)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IOleClientSite)
END_COM_MAP()

    HRESULT STDMETHODCALLTYPE SaveObject(void) ;
        
    HRESULT STDMETHODCALLTYPE GetMoniker(
            /* [in] */ DWORD dwAssign,
            /* [in] */ DWORD dwWhichMoniker,
            /* [out] */ __RPC__deref_out_opt IMoniker **ppmk);
        
    HRESULT STDMETHODCALLTYPE GetContainer(
            /* [out] */ __RPC__deref_out_opt IOleContainer **ppContainer);
        
    HRESULT STDMETHODCALLTYPE ShowObject(void);
        
    HRESULT STDMETHODCALLTYPE OnShowWindow(
            /* [in] */ BOOL fShow);
        
    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout(void);

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:

	STDMETHOD(Invoke)(DISPID dispidMember,REFIID riid, LCID lcid, WORD wFlags,
		DISPPARAMS * pdispparams, VARIANT * pvarResult,EXCEPINFO * pexcepinfo, UINT * puArgErr);
};

OBJECT_ENTRY_AUTO(__uuidof(AdPluginListener), CAdPluginListener)
