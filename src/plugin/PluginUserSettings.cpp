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
#include "AdblockPlusClient.h"
#include "PluginSettings.h"
#include "../shared/Dictionary.h"
#include <unordered_map>

namespace
{
  enum UserSettingsMethods
  {
    dispatchID_GetMessage = 0,
    dispatchID_GetLanguageCount,
    dispatchID_GetLanguageByIndex,
    dispatchID_GetLanguageTitleByIndex,
    dispatchID_SetLanguage,
    dispatchID_GetLanguage,
    dispatchID_GetWhitelistDomains,
    dispatchID_AddWhitelistDomain,
    dispatchID_RemoveWhitelistDomain,
    dispatchID_GetAppLocale,
    dispatchID_GetDocumentationLink,
    dispatchID_IsAcceptableAdsEnabled,
    dispatchID_SetAcceptableAdsEnabled,
    dispatchID_IsUpdate,
  };

  /**
   * Auxiliary for static initialization
   */
  std::unordered_map<std::wstring, DISPID> InitMethodIndex()
  {
    std::unordered_map<std::wstring, DISPID> m;
    // try-block for safety during static initialization
    try
    {
      m.emplace(L"GetMessage", dispatchID_GetMessage);
      m.emplace(L"GetLanguageCount", dispatchID_GetLanguageCount);
      m.emplace(L"GetLanguageByIndex", dispatchID_GetLanguageByIndex);
      m.emplace(L"GetLanguageTitleByIndex", dispatchID_GetLanguageTitleByIndex);
      m.emplace(L"SetLanguage", dispatchID_SetLanguage);
      m.emplace(L"GetLanguage", dispatchID_GetLanguage);
      m.emplace(L"GetWhitelistDomains", dispatchID_GetWhitelistDomains);
      m.emplace(L"AddWhitelistDomain", dispatchID_AddWhitelistDomain);
      m.emplace(L"RemoveWhitelistDomain", dispatchID_RemoveWhitelistDomain);
      m.emplace(L"GetAppLocale", dispatchID_GetAppLocale);
      m.emplace(L"GetDocumentationLink", dispatchID_GetDocumentationLink);
      m.emplace(L"IsAcceptableAdsEnabled", dispatchID_IsAcceptableAdsEnabled);
      m.emplace(L"SetAcceptableAdsEnabled", dispatchID_SetAcceptableAdsEnabled);
      m.emplace(L"IsUpdate", dispatchID_IsUpdate);
    }
    catch(...)
    {
    }
    return m;
  }

  /**
   * Static map from method names to dispatch identifiers.
   */
  std::unordered_map<std::wstring, DISPID> methodIndex = InitMethodIndex();
}

// ENTRY POINT
STDMETHODIMP CPluginUserSettings::QueryInterface(REFIID riid, void **ppvObj)
{
  if (!ppvObj)
  {
    return E_POINTER;
  }
  if (riid == IID_IUnknown || riid == IID_IDispatch) // GUID comparison does not throw
  {
    *ppvObj = static_cast<void*>(this);
    return S_OK;
  }
  return E_NOINTERFACE;
}

/**
 * \par Limitation
 *   CPluginUserSettings is not allocated on the heap.
 *   It appears only as a member variable in CPluginTabBase.
 *   'AddRef' and 'Release' don't need reference counting because they don't present COM factories.
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

/**
 * \par Limitation
 *   The specification for this method in IDispatch maps an array of names to an array of identifiers.
 *   This version only supports single-element arrays, which is enough for IE's JavaScript interpreter.
 */
STDMETHODIMP CPluginUserSettings::GetIDsOfNames(REFIID, LPOLESTR* name, UINT count, LCID, DISPID* id)
{
  try
  {
    if (!name || !id)
    {
      return E_POINTER;
    }
    if (count != 1)
    {
      return E_FAIL;
    }
    auto item = methodIndex.find(*name); // unordered_map::find is not declared noexcept
    if (item == methodIndex.end())
    {
      return DISP_E_UNKNOWNNAME;
    }
    *id = item->second;
  }
  catch (...)
  {
    return E_FAIL;
  }
  return S_OK;
}

CStringW sGetMessage(const CString& section, const CString& key)
{
  Dictionary* dictionary = Dictionary::GetInstance();
  return CStringW(dictionary->Lookup(std::string(CW2A(section)), std::string(CW2A(key))).c_str());
}

STDMETHODIMP CPluginUserSettings::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispparams, VARIANT* pVarResult,
                                         EXCEPINFO* pExcepinfo, UINT* pArgErr)
{
  try
  {
    if (!pDispparams)
    {
      return E_POINTER;
    }
    if (pDispparams->cNamedArgs != 0)
    {
      return DISP_E_NONAMEDARGS;
    }
    CPluginSettings* settings = CPluginSettings::GetInstance();
    switch (dispidMember)
    {
    case dispatchID_GetMessage:
      {
        if (pDispparams->cArgs != 2)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pDispparams->rgvarg[0].vt != VT_BSTR || pDispparams->rgvarg[1].vt != VT_BSTR)
        {
          return DISP_E_TYPEMISMATCH;
        }
        if (pVarResult)
        {
          CComBSTR key = pDispparams->rgvarg[0].bstrVal;
          CComBSTR section = pDispparams->rgvarg[1].bstrVal;
          CStringW message = sGetMessage((BSTR)section, (BSTR)key);

          pVarResult->vt = VT_BSTR;
          pVarResult->bstrVal = SysAllocString(message);
        }
      }
      break;
    case dispatchID_GetLanguageCount:
      {
        if (pDispparams->cArgs != 0)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pVarResult)
        {
          std::map<CString, CString> languageList = settings->GetFilterLanguageTitleList();

          pVarResult->vt = VT_I4;
          pVarResult->lVal = static_cast<LONG>(languageList.size());
        }
      }
      break;
    case dispatchID_GetLanguageByIndex:
      {
        if (pDispparams->cArgs != 1)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pDispparams->rgvarg[0].vt != VT_I4)
        {
          return DISP_E_TYPEMISMATCH;
        }
        if (pVarResult)
        {
          int index = pDispparams->rgvarg[0].lVal;

          std::map<CString, CString> languageTitleList = settings->GetFilterLanguageTitleList();

          if (index < 0  ||  index >= static_cast<int>(languageTitleList.size()))
            return DISP_E_EXCEPTION;

          CString language;

          int loopIndex = 0;
          for (std::map<CString, CString>::const_iterator it = languageTitleList.begin(); it != languageTitleList.end(); ++it)
          {
            if (loopIndex == index)
            {
              language = it->first;
              break;
            }
            ++loopIndex;
          }

          pVarResult->vt = VT_BSTR;
          pVarResult->bstrVal = SysAllocString(language);
        }
      }
      break;
    case dispatchID_GetLanguageTitleByIndex:
      {
        if (pDispparams->cArgs != 1)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pDispparams->rgvarg[0].vt != VT_I4)
        {
          return DISP_E_TYPEMISMATCH;
        }
        if (pVarResult)
        {
          int index = pDispparams->rgvarg[0].lVal;

          std::map<CString, CString> languageTitleList = settings->GetFilterLanguageTitleList();

          if (index < 0  ||  index >= static_cast<int>(languageTitleList.size()))
            return DISP_E_EXCEPTION;

          CString languageTitle;

          int loopIndex = 0;
          for (std::map<CString, CString>::const_iterator it = languageTitleList.begin(); it != languageTitleList.end(); ++it)
          {
            if (loopIndex == index)
            {
              languageTitle = it->second;
              break;
            }
            loopIndex++;
          }

          pVarResult->vt = VT_BSTR;
          pVarResult->bstrVal = SysAllocString(languageTitle);
        }
      }
      break;
    case dispatchID_SetLanguage:
      {
        if (pDispparams->cArgs != 1)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pDispparams->rgvarg[0].vt != VT_BSTR)
        {
          return DISP_E_TYPEMISMATCH;
        }
        CComBSTR url = pDispparams->rgvarg[0].bstrVal;
        settings->SetSubscription((BSTR)url);
      }
      break;
    case dispatchID_GetLanguage:
      {
        if (pDispparams->cArgs != 0)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pVarResult)
        {
          CString url = settings->GetSubscription();
          pVarResult->vt = VT_BSTR;
          pVarResult->bstrVal = SysAllocString(url);
        }
      }
      break;
    case dispatchID_GetWhitelistDomains:
      {
        if (pDispparams->cArgs != 0)
        {
          return DISP_E_BADPARAMCOUNT;
        }
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
      break;
    case dispatchID_AddWhitelistDomain:
      {
        if (pDispparams->cArgs != 1)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pDispparams->rgvarg[0].vt != VT_BSTR)
        {
          return DISP_E_TYPEMISMATCH;
        }
        CComBSTR domain = pDispparams->rgvarg[0].bstrVal;
        if (domain.Length())
        {
          settings->AddWhiteListedDomain((BSTR)domain);
        }
      }
      break;
    case dispatchID_RemoveWhitelistDomain:
      {
        if (pDispparams->cArgs != 1)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pDispparams->rgvarg[0].vt != VT_BSTR)
        {
          return DISP_E_TYPEMISMATCH;
        }
        CComBSTR domain = pDispparams->rgvarg[0].bstrVal;
        if (domain.Length())
        {
          settings->RemoveWhiteListedDomain((BSTR)domain);
        }
      }
      break;
    case dispatchID_GetAppLocale:
      {
        if (pDispparams->cArgs != 0)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pVarResult)
        {
          pVarResult->vt = VT_BSTR;
          pVarResult->bstrVal = SysAllocString(settings->GetAppLocale());
        }
      }
      break;
    case dispatchID_GetDocumentationLink:
      {
        if (pDispparams->cArgs != 0)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pVarResult)
        {
          pVarResult->vt = VT_BSTR;
          pVarResult->bstrVal = SysAllocString(settings->GetDocumentationLink());
        }
      }
      break;
    case dispatchID_IsAcceptableAdsEnabled:
      {
        if (pDispparams->cArgs != 0)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pVarResult)
        {
          pVarResult->vt = VT_BOOL;
          pVarResult->boolVal = CPluginClient::GetInstance()->IsAcceptableAdsEnabled() ? VARIANT_TRUE : VARIANT_FALSE;
        }
      }
      break;
    case dispatchID_SetAcceptableAdsEnabled:
      {
        if (pDispparams->cArgs != 1)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pDispparams->rgvarg[0].vt != VT_BOOL)
        {
          return DISP_E_TYPEMISMATCH;
        }
        if (pDispparams->rgvarg[0].boolVal != VARIANT_FALSE)
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
      break;
    case dispatchID_IsUpdate:
      {
        if (pDispparams->cArgs != 0)
        {
          return DISP_E_BADPARAMCOUNT;
        }
        if (pVarResult)
        {
          pVarResult->vt = VT_BOOL;
          pVarResult->boolVal = CPluginClient::GetInstance()->GetPref(L"displayUpdatePage", false) ? VARIANT_TRUE : VARIANT_FALSE;
        }
      }
      break;
    default:
      return DISP_E_MEMBERNOTFOUND;
      break;
    }
  }
  catch (...)
  {
    return E_FAIL;
  }
  return S_OK;
}
