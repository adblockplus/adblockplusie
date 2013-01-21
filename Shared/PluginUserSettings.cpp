#include "PluginStdAfx.h"
#include "PluginUserSettings.h"
#include <algorithm>
#include "PluginSettings.h"

#define SET_LANGUAGE                L"SetLanguage"
#define GET_LANGUAGE                L"GetLanguage"
#define GET_WHITELIST_DOMAINS       L"GetWhitelistDomains"
#define ADD_WHITELIST_DOMAIN        L"AddWhitelistDomain"
#define REMOVE_WHITELIST_DOMAIN     L"RemoveWhitelistDomain"

static const CString s_Methods[] = {SET_LANGUAGE, GET_LANGUAGE, GET_WHITELIST_DOMAINS, ADD_WHITELIST_DOMAIN, REMOVE_WHITELIST_DOMAIN};

CPluginUserSettings::CPluginUserSettings()
{
}


STDMETHODIMP CPluginUserSettings::QueryInterface(REFIID riid, void **ppvObj)
{
    if (IID_IUnknown == riid  ||  IID_IDispatch == riid) 
    {
        *ppvObj = (LPVOID)this;
        return NOERROR;
    }

    return E_NOINTERFACE;
}


ULONG __stdcall CPluginUserSettings::AddRef()
{
    return 1;
}


ULONG __stdcall CPluginUserSettings::Release()
{
    return 1;
}


STDMETHODIMP CPluginUserSettings::GetTypeInfoCount(UINT* pctinfo)
{
	return E_NOTIMPL;
}


STDMETHODIMP CPluginUserSettings::GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo)
{
	return E_NOTIMPL;
}


STDMETHODIMP CPluginUserSettings::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid)
{
    if (!rgszNames)
        return E_POINTER;

    if (!rgdispid)
        return E_POINTER;

    if (1 != cNames)
        return E_FAIL;

    size_t indxMethod = 0;
    for (; indxMethod < countof(s_Methods); indxMethod++)
    {
        if (*rgszNames == s_Methods[indxMethod])
            break;
    }

    if (indxMethod == countof(s_Methods))
        return DISP_E_MEMBERNOTFOUND;

    *rgdispid = indxMethod;

    return S_OK;
}


STDMETHODIMP CPluginUserSettings::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispparams, VARIANT* pVarResult,
	EXCEPINFO* pExcepinfo, UINT* pArgErr)
{
    if (!pDispparams)
        return E_POINTER;

    if (!pExcepinfo)
        return E_POINTER; 

    if (pDispparams->cNamedArgs)
        return DISP_E_NONAMEDARGS;

    CPluginSettings* settings = CPluginSettings::GetInstance();

    if (dispidMember  < 0  ||  dispidMember >= countof(s_Methods))
        return DISP_E_BADINDEX;

    const CString& method = s_Methods[dispidMember];

    if (SET_LANGUAGE == method)
    {
        if (1 != pDispparams->cArgs)
            return DISP_E_BADPARAMCOUNT;

        if (VT_BSTR != pDispparams->rgvarg[0].vt)
            return DISP_E_TYPEMISMATCH;
    }
    else if (GET_LANGUAGE == method)
    {
        if (pDispparams->cArgs)
            return DISP_E_BADPARAMCOUNT;

        if (pVarResult)
        {
            CString val = settings->GetString(SETTING_LANGUAGE);

            pVarResult->vt = VT_BSTR; 
            pVarResult->bstrVal = SysAllocString(val);
        }
    }
    else if (GET_WHITELIST_DOMAINS == method)
    {
        if (pDispparams->cArgs)
            return DISP_E_BADPARAMCOUNT;

        if (pVarResult)
        {
            TDomainList whiteList = settings->GetWhiteListedDomainList(true);
            CString sWhiteList;
            for (TDomainList::const_iterator it = whiteList.begin(); it != whiteList.end(); ++it)
            {            
                if (!sWhiteList.IsEmpty())
                {
                    sWhiteList += ',';
                }
                sWhiteList += it->first;
            }

            pVarResult->vt = VT_BSTR; 
            pVarResult->bstrVal = SysAllocString(sWhiteList);
        }
    }
    else if (ADD_WHITELIST_DOMAIN == method)
    {
        if (1 != pDispparams->cArgs)
            return DISP_E_BADPARAMCOUNT;

        if (VT_BSTR != pDispparams->rgvarg[0].vt)
            return DISP_E_TYPEMISMATCH;

        CComBSTR domain = pDispparams->rgvarg[0].bstrVal;
        if (domain.Length())
        {
            if (!settings->IsWhiteListedDomain((BSTR)domain)) 
			{
                settings->AddWhiteListedDomain((BSTR)domain, 1, true);
            }
        }
    }
    else if (REMOVE_WHITELIST_DOMAIN == method)
    {
        if (1 != pDispparams->cArgs)
            return DISP_E_BADPARAMCOUNT;

        if (VT_BSTR != pDispparams->rgvarg[0].vt)
            return DISP_E_TYPEMISMATCH;

        CComBSTR domain = pDispparams->rgvarg[0].bstrVal;
        if (settings->IsWhiteListedDomain((BSTR)domain)) 
		{
            settings->AddWhiteListedDomain((BSTR)domain, 3, true);
        }
    }
    else 
        return DISP_E_MEMBERNOTFOUND;

    return S_OK;
}

 