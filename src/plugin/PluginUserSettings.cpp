/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
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

#include "PluginStdAfx.h"
#include "PluginUserSettings.h"
#include <algorithm>
#include "PluginSettings.h"
#include "PluginClient.h"
#include "../shared/Dictionary.h"

static const CString s_GetMessage = L"GetMessage";
static const CString s_GetLanguageCount = L"GetLanguageCount";
static const CString s_GetLanguageByIndex = L"GetLanguageByIndex";
static const CString s_GetLanguageTitleByIndex = L"GetLanguageTitleByIndex";
static const CString s_SetLanguage = L"SetLanguage";
static const CString s_GetLanguage = L"GetLanguage";
static const CString s_GetWhitelistDomains = L"GetWhitelistDomains";
static const CString s_AddWhitelistDomain = L"AddWhitelistDomain";
static const CString s_RemoveWhitelistDomain = L"RemoveWhitelistDomain";
static const CString s_GetAppLocale = L"GetAppLocale";
static const CString s_GetDocumentationLink = L"GetDocumentationLink";
static const CString s_IsAcceptableAdsEnabled = L"IsAcceptableAdsEnabled";
static const CString s_SetAcceptableAdsEnabled = L"SetAcceptableAdsEnabled";
static const CString s_IsUpdate = L"IsUpdate";
static const CString s_Methods[] = {s_GetMessage, s_GetLanguageCount, s_GetLanguageByIndex, s_GetLanguageTitleByIndex, s_SetLanguage, s_GetLanguage, s_GetWhitelistDomains, s_AddWhitelistDomain, s_RemoveWhitelistDomain, s_GetAppLocale, s_GetDocumentationLink, s_IsAcceptableAdsEnabled, s_SetAcceptableAdsEnabled, s_IsUpdate};

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


/*
Since CPluginUserSettings is not allocated on the heap, 'AddRef' and 'Release' don't need reference counting,
because CPluginUserSettings won't be deleted when reference counter == 0
*/

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

  if (cNames != 1)
    return E_FAIL;

  size_t indxMethod = 0;
  for (; indxMethod < countof(s_Methods); indxMethod++)
  {
    if (*rgszNames == s_Methods[indxMethod])
      break;
  }

  if (indxMethod == countof(s_Methods))
    return DISP_E_MEMBERNOTFOUND;

  *rgdispid = static_cast<DISPID>(indxMethod);

  return S_OK;
}


static CString sGetLanguage()
{
  CPluginSettings* settings = CPluginSettings::GetInstance();
  return settings->GetSubscription();
}


CStringW sGetMessage(const CString& section, const CString& key)
{
  Dictionary* dictionary = Dictionary::GetInstance();
  return CStringW(dictionary->Lookup(std::string(CW2A(section)), std::string(CW2A(key))).c_str());
}

std::wstring sGetMessage(const std::string& section, const std::string& key)
{
  Dictionary* dictionary = Dictionary::GetInstance();
  return dictionary->Lookup(section, key);
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

  if (method == s_GetMessage)
  {
    if (pDispparams->cArgs != 2)
      return DISP_E_BADPARAMCOUNT;

    if (pDispparams->rgvarg[0].vt != VT_BSTR)
      return DISP_E_TYPEMISMATCH;

    if (pVarResult)
    {
      CComBSTR key = pDispparams->rgvarg[0].bstrVal;
      CComBSTR section = pDispparams->rgvarg[1].bstrVal;
      CStringW message = sGetMessage((BSTR)section, (BSTR)key);

      pVarResult->vt = VT_BSTR;
      pVarResult->bstrVal = SysAllocString(message);
    }
  }
  else if (method == s_GetLanguageCount)
  {
    if (pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    if (pVarResult)
    {
      std::map<CString, CString> languageList = settings->GetFilterLanguageTitleList();

      pVarResult->vt = VT_I4;
      pVarResult->lVal = static_cast<LONG>(languageList.size());
    }
  }
  else if (method == s_GetLanguageByIndex)
  {
    if (pDispparams->cArgs != 1)
      return DISP_E_BADPARAMCOUNT;

    if (pDispparams->rgvarg[0].vt != VT_I4)
      return DISP_E_TYPEMISMATCH;

    if (pVarResult)
    {
      int indx = pDispparams->rgvarg[0].lVal;

      std::map<CString, CString> languageTitleList = settings->GetFilterLanguageTitleList();

      if (indx < 0  ||  indx >= (int)languageTitleList.size())
        return DISP_E_EXCEPTION;

      CString language;

      int curIndx = 0;
      for(std::map<CString, CString>::const_iterator it = languageTitleList.begin(); it != languageTitleList.end(); ++it)
      {
        if (curIndx == indx)
        {
          language = it->first;
          break;
        }

        curIndx++;
      }

      pVarResult->vt = VT_BSTR;
      pVarResult->bstrVal = SysAllocString(language);
    }
  }
  else if (method == s_GetLanguageTitleByIndex)
  {
    if (pDispparams->cArgs != 1)
      return DISP_E_BADPARAMCOUNT;

    if (pDispparams->rgvarg[0].vt != VT_I4)
      return DISP_E_TYPEMISMATCH;

    if (pVarResult)
    {
      int indx = pDispparams->rgvarg[0].lVal;

      std::map<CString, CString> languageTitleList = settings->GetFilterLanguageTitleList();

      if (indx < 0  ||  indx >= (int)languageTitleList.size())
        return DISP_E_EXCEPTION;

      CString languageTitle;

      int curIndx = 0;
      for(std::map<CString, CString>::const_iterator it = languageTitleList.begin(); it != languageTitleList.end(); ++it)
      {
        if (curIndx == indx)
        {
          languageTitle = it->second;
          break;
        }

        curIndx++;
      }

      pVarResult->vt = VT_BSTR;
      pVarResult->bstrVal = SysAllocString(languageTitle);
    }
  }
  else if (method == s_SetLanguage)
  {
    if (pDispparams->cArgs != 1)
      return DISP_E_BADPARAMCOUNT;

    if (pDispparams->rgvarg[0].vt != VT_BSTR)
      return DISP_E_TYPEMISMATCH;

    CComBSTR url = pDispparams->rgvarg[0].bstrVal;

    settings->SetSubscription((BSTR)url);
  }
  else if (method == s_GetLanguage)
  {
    if (pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    if (pVarResult)
    {
      CString url = settings->GetSubscription();

      pVarResult->vt = VT_BSTR;
      pVarResult->bstrVal = SysAllocString(url);
    }
  }
  else if (method == s_GetWhitelistDomains)
  {
    if (pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    if (pVarResult)
    {
      std::vector<std::wstring> whiteList = settings->GetWhiteListedDomainList();
      CString sWhiteList;
      for (size_t i = 0; i < whiteList.size(); i++)
      {
        if (!sWhiteList.IsEmpty())
        {
          sWhiteList += ',';
        }
        sWhiteList += CString(whiteList[i].c_str());
      }

      pVarResult->vt = VT_BSTR;
      pVarResult->bstrVal = SysAllocString(sWhiteList);
    }
  }
  else if (method == s_AddWhitelistDomain)
  {
    if (pDispparams->cArgs != 1)
      return DISP_E_BADPARAMCOUNT;

    if (pDispparams->rgvarg[0].vt != VT_BSTR)
      return DISP_E_TYPEMISMATCH;

    CComBSTR domain = pDispparams->rgvarg[0].bstrVal;
    if (domain.Length())
    {
      settings->AddWhiteListedDomain((BSTR)domain);
    }
  }
  else if (method == s_RemoveWhitelistDomain)
  {
    if (pDispparams->cArgs != 1)
      return DISP_E_BADPARAMCOUNT;

    if (pDispparams->rgvarg[0].vt != VT_BSTR)
      return DISP_E_TYPEMISMATCH;

    CComBSTR domain = pDispparams->rgvarg[0].bstrVal;
    if (domain.Length())
    {
      settings->RemoveWhiteListedDomain((BSTR)domain);
    }
  }
  else if (method == s_GetAppLocale)
  {
    if (pDispparams->cArgs != 0)
      return DISP_E_BADPARAMCOUNT;

    pVarResult->vt = VT_BSTR;
    pVarResult->bstrVal = SysAllocString(settings->GetAppLocale());
  }
  else if (method == s_GetDocumentationLink)
  {
    if (pDispparams->cArgs != 0)
      return DISP_E_BADPARAMCOUNT;

    pVarResult->vt = VT_BSTR;
    pVarResult->bstrVal = SysAllocString(settings->GetDocumentationLink());
  }
  else if (s_IsAcceptableAdsEnabled == method)
  {
    if (pDispparams->cArgs != 0)
      return DISP_E_BADPARAMCOUNT;

    pVarResult->vt = VT_BOOL;
    pVarResult->boolVal = CPluginClient::GetInstance()->IsAcceptableAdsEnabled() ? VARIANT_TRUE : VARIANT_FALSE;
  }
  else if (method == s_SetAcceptableAdsEnabled)
  {
    if (pDispparams->cArgs != 1)
      return DISP_E_BADPARAMCOUNT;

    if (pDispparams->rgvarg[0].vt != VT_BOOL)
      return DISP_E_TYPEMISMATCH;

    bool enable = pDispparams->rgvarg[0].boolVal != VARIANT_FALSE;

    if (enable)
    {
      CPluginClient* client = CPluginClient::GetInstance();
      client->AddSubscription(client->GetPref(L"subscriptions_exceptionsurl", L""));
    }
    else
    {
      CPluginClient* client = CPluginClient::GetInstance();
      client->RemoveSubscription(client->GetPref(L"subscriptions_exceptionsurl", L""));
    }
  }
  else if (method == s_IsUpdate)
  {
    if (pDispparams->cArgs != 0)
      return DISP_E_BADPARAMCOUNT;

    pVarResult->vt = VT_BOOL;
    pVarResult->boolVal = CPluginClient::GetInstance()->GetPref(L"displayUpdatePage", false);
  }
  else
    return DISP_E_MEMBERNOTFOUND;

  return S_OK;
}

