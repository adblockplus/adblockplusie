#include "PluginStdAfx.h"

// Internet / FTP
#include <wininet.h>

// IP adapter
#include <iphlpapi.h>

#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginMutex.h"
#include "PluginClass.h"

#include "PluginClientBase.h"

// IP adapter
#pragma comment(lib, "IPHLPAPI.lib")

// IE functions
#pragma comment(lib, "iepmapi.lib")

// Internet / FTP
#pragma comment(lib, "wininet.lib")


CComAutoCriticalSection CPluginClientBase::s_criticalSectionLocal;

std::vector<CPluginError> CPluginClientBase::s_pluginErrors;

bool CPluginClientBase::s_isErrorLogging = false;


CPluginClientBase::CPluginClientBase()
{
}


CPluginClientBase::~CPluginClientBase()
{
}

void UnescapeUrl(std::wstring& url)
{
  try
  {
    DWORD result_length = INTERNET_MAX_URL_LENGTH;
    std::unique_ptr<wchar_t[]> result(new wchar_t[result_length]);
    HRESULT hr = UrlUnescapeW(const_cast<wchar_t*>(url.c_str()), result.get(), &result_length, 0);
    if (hr == S_OK)
    {
      url = std::wstring(result.get(), result_length);
    }
    /*
     * Do nothing. This masks error return values from UrlUnescape without logging the error.
     */
  }
  catch(std::bad_alloc e)
  {
    /*
     * When the code has a systematic way of handling bad_alloc, we'll rethrow (probably).
     * Until then, we mask the exception and make no modification.
     */
  }
  catch(...)
  {
    // no modification if any other exception
  }
}

void CPluginClientBase::LogPluginError(DWORD errorCode, int errorId, int errorSubid, const CString& description, bool isAsync, DWORD dwProcessId, DWORD dwThreadId)
{
  // Prevent circular references
  if (CPluginSettings::HasInstance() && isAsync)
  {
    DEBUG_ERROR_CODE_EX(errorCode, description, dwProcessId, dwThreadId);

    CString pluginError;
    pluginError.Format(L"%2.2d%2.2d", errorId, errorSubid);

    CString pluginErrorCode;
    pluginErrorCode.Format(L"%u", errorCode);

    CPluginSettings* settings = CPluginSettings::GetInstance();

    settings->AddError(pluginError, pluginErrorCode);
  }

  // Post error to client for later submittal
  if (!isAsync)
  {
    CPluginClientBase::PostPluginError(errorId, errorSubid, errorCode, description);
  }
}


void CPluginClientBase::PostPluginError(int errorId, int errorSubid, DWORD errorCode, const CString& errorDescription)
{
  s_criticalSectionLocal.Lock();
  {
    CPluginError pluginError(errorId, errorSubid, errorCode, errorDescription);

    s_pluginErrors.push_back(pluginError);
  }
  s_criticalSectionLocal.Unlock();
}


bool CPluginClientBase::PopFirstPluginError(CPluginError& pluginError)
{
  bool hasError = false;

  s_criticalSectionLocal.Lock();
  {
    std::vector<CPluginError>::iterator it = s_pluginErrors.begin();
    if (it != s_pluginErrors.end())
    {
      pluginError = *it;

      hasError = true;

      s_pluginErrors.erase(it);
    }
  }
  s_criticalSectionLocal.Unlock();

  return hasError;
}
