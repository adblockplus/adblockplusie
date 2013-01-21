#pragma once

#include <vector>
#include <utility>

/*
Class is used to call methods Get,Set,Update of Settings object from JavaScript.
When url is local page "user_mysettings.html", after document is loaded, C++ creates Settings object in page's JavaScript.
Then var value = window.Settings.Get(par), window.Settings.Set(par, value), window.Settings.Update() can be called from JavaScript
*/
class CPluginUserSettings: public IDispatch
{
public:
  CPluginUserSettings();

  // IUnknown
  STDMETHOD(QueryInterface)(REFIID riid, void **ppvObj);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

  // IDispatch
	STDMETHOD(GetTypeInfoCount)(UINT* pctinfo);
	STDMETHOD(GetTypeInfo)(UINT itinfo, LCID lcid, ITypeInfo** pptinfo);
	STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid);
	STDMETHOD(Invoke)(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispparams, VARIANT* pVarResult,
		EXCEPINFO* pExcepinfo, UINT* pArgErr);
};
