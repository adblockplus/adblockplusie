/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2016 Eyeo GmbH
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
#ifndef PLUGIN_USER_SETTINGS_H
#define PLUGIN_USER_SETTINGS_H

#include <OAIdl.h>

/*
Class is used to call methods Get,Set,Update of Settings object from JavaScript.
When url is local page "user_mysettings.html", after document is loaded, C++ creates Settings object in page's JavaScript.
Then var value = window.Settings.Get(par), window.Settings.Set(par, value), window.Settings.Update() can be called from JavaScript
*/
class CPluginUserSettings: public IDispatch
{
public:
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

#endif