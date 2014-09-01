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

  if (s_GetMessage == method)
  {
    if (2 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    if (VT_BSTR != pDispparams->rgvarg[0].vt)
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
  else if (s_GetLanguageCount == method)
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
  else if (s_GetLanguageByIndex == method)
  {
    if (1 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    if (VT_I4 != pDispparams->rgvarg[0].vt)
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
  else if (s_GetLanguageTitleByIndex == method)
  {
    if (1 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    if (VT_I4 != pDispparams->rgvarg[0].vt)
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
  else if (s_SetLanguage == method)
  {
    if (1 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    if (VT_BSTR != pDispparams->rgvarg[0].vt)
      return DISP_E_TYPEMISMATCH;

    CComBSTR url = pDispparams->rgvarg[0].bstrVal;

    settings->SetSubscription((BSTR)url);
  }
  else if (s_GetLanguage == method)
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
  else if (s_GetWhitelistDomains == method)
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
  else if (s_AddWhitelistDomain == method)
  {
    if (1 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    if (VT_BSTR != pDispparams->rgvarg[0].vt)
      return DISP_E_TYPEMISMATCH;

    CComBSTR domain = pDispparams->rgvarg[0].bstrVal;
    if (domain.Length())
    {
      settings->AddWhiteListedDomain((BSTR)domain);
    }
  }
  else if (s_RemoveWhitelistDomain == method)
  {
    if (1 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    if (VT_BSTR != pDispparams->rgvarg[0].vt)
      return DISP_E_TYPEMISMATCH;

    CComBSTR domain = pDispparams->rgvarg[0].bstrVal;
    if (domain.Length())
    {
      settings->RemoveWhiteListedDomain((BSTR)domain);
    }
  }
  else if (s_GetAppLocale == method)
  {
    if (0 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    pVarResult->vt = VT_BSTR;
    pVarResult->bstrVal = SysAllocString(settings->GetAppLocale());
  }
  else if (s_GetDocumentationLink == method)
  {
    if (0 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    pVarResult->vt = VT_BSTR;
    pVarResult->bstrVal = SysAllocString(settings->GetDocumentationLink());
  }
  else if (s_IsAcceptableAdsEnabled == method)
  {
    if (0 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    pVarResult->vt = VT_BOOL;
    pVarResult->boolVal = CPluginClient::GetInstance()->IsAcceptableAdsEnabled() ? VARIANT_TRUE : VARIANT_FALSE;
  }
  else if (s_SetAcceptableAdsEnabled == method)
  {
    if (1 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    if (VT_BOOL != pDispparams->rgvarg[0].vt)
      return DISP_E_TYPEMISMATCH;

    bool enable = VARIANT_FALSE != pDispparams->rgvarg[0].boolVal;

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
  else if (s_IsUpdate == method)
  {
    if (0 != pDispparams->cArgs)
      return DISP_E_BADPARAMCOUNT;

    pVarResult->vt = VT_BOOL;
    pVarResult->boolVal = CPluginClient::GetInstance()->GetPref(L"displayUpdatePage", false);
  }
  else
    return DISP_E_MEMBERNOTFOUND;

  return S_OK;
}

