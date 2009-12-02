#include "PluginStdAfx.h"

#include "PluginIniFile.h"
#include "PluginChecksum.h"
#include "PluginHttpRequest.h"
#include "PluginClient.h"
#include "PluginSettings.h"
#include "PluginSystem.h"

#include <winhttp.h>


//class to ensure that HInternet always is closed
class HINTERNETHandle 
{

private:

	HINTERNET m_handle;

public:

	HINTERNETHandle() : m_handle(NULL) {}

	HINTERNETHandle(HINTERNET hInternet) : m_handle(hInternet) {}

	HINTERNETHandle(HINTERNETHandle& handle)
	{
		m_handle = handle.m_handle;
		handle.m_handle = NULL; 
	}

	HINTERNETHandle& operator=(HINTERNET hInternet)
	{
		assert(!m_handle);
		m_handle = hInternet;
		return *this;
	}

	HINTERNET operator*()
	{
		return m_handle;
	}

	~HINTERNETHandle()
	{
		// close the handle
		if (m_handle)
		{
			if (!::WinHttpCloseHandle(m_handle))
			{
			    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP, PLUGIN_ERROR_HTTP_CLOSE_HANDLE, "Http::~HINTERNETHandle - WinHttpCloseHandle")
			}
		}
	}
};


CPluginHttpRequest::CPluginHttpRequest(const CString& script, bool addChecksum) :
	m_script(script), m_urlPrefix("?"), m_addChecksum(addChecksum)
{
    m_checksum = std::auto_ptr<CPluginChecksum>(new CPluginChecksum());
    m_checksum->Add(script);

	m_responseFile = std::auto_ptr<CPluginIniFile>(new CPluginIniFile("", true));

    m_url = CString(USERS_PATH) + script;
}


CPluginHttpRequest::~CPluginHttpRequest()
{
}


void CPluginHttpRequest::AddPluginId()
{
	CPluginSettings* settings = CPluginSettings::GetInstance();

    CPluginSystem* system = CPluginSystem::GetInstance();

    if (settings->Has(SETTING_PLUGIN_ID))
    {
        Add("plugin", settings->GetString(SETTING_PLUGIN_ID));
    }
    else
    {
	    Add("plugin", system->GetPluginId());
    }
    Add("user", settings->GetString(SETTING_USER_ID));
    Add("password", settings->GetString(SETTING_PLUGIN_PASSWORD));

    Add("version", IEPLUGIN_VERSION);
}

void CPluginHttpRequest::AddOsInfo()
{
	DWORD osVersion = ::GetVersion();

    Add("os1", (LOBYTE(LOWORD(osVersion))));
    Add("os2", (HIBYTE(LOWORD(osVersion))));
}

bool CPluginHttpRequest::Send(bool checkResponse)
{
    if (m_addChecksum)
    {
        m_url += m_urlPrefix + "checksum=" + m_checksum->GetAsString();

		m_urlPrefix = "&";
    }

    DEBUG_GENERAL("*** Sending HTTP request:" + m_url)

    bool isOk = SendHttpRequest(USERS_HOST, m_url, &m_responseText, USERS_PORT) ? true:false;
	if (isOk && checkResponse)
	{
		isOk = IsValidResponse();
	}

	return isOk;
}

void CPluginHttpRequest::Add(const CString& arg, const CString& value, bool addToChecksum)
{
    if (!arg.IsEmpty() && !value.IsEmpty())
    {
        CString valueEncoded;
        DWORD cb = 2048;
        
        HRESULT hr = ::UrlEscape(value, valueEncoded.GetBufferSetLength(cb), &cb, URL_ESCAPE_SEGMENT_ONLY);
		if (SUCCEEDED(hr))
		{
	        valueEncoded.Truncate(cb);
		}
		else
		{
	        valueEncoded = value;

			DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_URL_ESCAPE, "HttpRequest::Add - UrlEscape failed on " + value)
		}

        m_url += m_urlPrefix + arg + "=" + valueEncoded;
        
        if (addToChecksum)
        {
            m_checksum->Add(arg);
            m_checksum->Add(value);
        }
        
        m_urlPrefix = "&";
    }
}

void CPluginHttpRequest::Add(const CString& arg, unsigned int value, bool addToChecksum)
{
    CString valueStr;
    valueStr.Format(L"%u", value);

    Add(arg, valueStr, addToChecksum);    
}

CString CPluginHttpRequest::GetUrl()
{
    if (m_addChecksum)
    {
        m_url += m_urlPrefix + "checksum=" + m_checksum->GetAsString();
        m_urlPrefix = "&";
    }

    return CString(USERS_HOST) + m_url;
}


CString CPluginHttpRequest::GetStandardUrl(const CString& script)
{
    CPluginHttpRequest httpRequest(script);

    httpRequest.AddPluginId();

	return httpRequest.GetUrl();
}


CStringA CPluginHttpRequest::GetResponseText() const
{
    return m_responseText;
}

const std::auto_ptr<CPluginIniFile>& CPluginHttpRequest::GetResponseFile() const
{
	return m_responseFile;
}

bool CPluginHttpRequest::IsValidResponse() const
{
	m_responseFile->Clear();
	m_responseFile->SetInitialChecksumString(m_script);

	bool isValidResponse = m_responseFile->ReadString(m_responseText);
    if (isValidResponse && m_responseFile->IsValidChecksum())
    {
        CPluginIniFile::TSectionData status = m_responseFile->GetSectionData(_T("Status"));
        CPluginIniFile::TSectionData::iterator it;
        
        it = status.find(_T("status"));
        if (it != status.end())
        {
            isValidResponse = (it->second == "OK");
        }
		else
		{
			isValidResponse = false;
		}
	}

	return isValidResponse;
}

BOOL CPluginHttpRequest::GetProxySettings(CString& proxyName, CString& proxyBypass)
{
    BOOL bResult = TRUE;

    proxyName.Empty();
    proxyBypass.Empty();
     
	// Get Proxy config info.
	WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig;

	::ZeroMemory(&proxyConfig, sizeof(proxyConfig));

    if (::WinHttpGetIEProxyConfigForCurrentUser(&proxyConfig))
    {
        proxyName   = proxyConfig.lpszProxy;
        proxyBypass = proxyConfig.lpszProxyBypass;
    }
    else
    {
		DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP, PLUGIN_ERROR_HTTP_PROXY_SETTINGS, "Http::GetProxySettings - WinHttpGetIEProxyConfigForCurrentUser")		
        bResult = FALSE;
    }

	// The strings need to be freed.
	if (proxyConfig.lpszProxy != NULL)
	{
		::GlobalFree(proxyConfig.lpszProxy);
	}
	if (proxyConfig.lpszAutoConfigUrl != NULL)
	{
		::GlobalFree(proxyConfig.lpszAutoConfigUrl);
	}
	if (proxyConfig.lpszProxyBypass!= NULL)
	{
		::GlobalFree(proxyConfig.lpszProxyBypass);
	}
	
	return bResult;
}

bool CPluginHttpRequest::SendHttpRequest(LPCWSTR server, LPCWSTR file, CStringA* response, WORD nServerPort)
{
    // Prepare url
	DWORD cb = 2049;
	CString url;
	HRESULT hr = ::UrlCanonicalize(file, url.GetBufferSetLength(cb), &cb, URL_ESCAPE_UNSAFE);
	if (FAILED(hr))
	{
		DEBUG_ERROR_CODE(hr, "HttpRequest::SendHttpRequest::UrlCanonicalize failed on " + CString(file))
		return false;
	}

	DWORD dwSize = 0;
	DWORD dwDownloaded = 0;
	bool  bResult = false;
	HINTERNETHandle hSession, hConnect, hRequest;

	// Get Proxy config info.
	CString proxyName;
	CString proxyBypass;
	
	CPluginHttpRequest::GetProxySettings(proxyName, proxyBypass);
		
	// If there is is proxy setting, use it.
	if (proxyName.IsEmpty())
	{
		hSession = ::WinHttpOpen(BHO_NAME, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	}
	// Use WinHttpOpen to obtain a session handle.
	else
	{
		hSession = ::WinHttpOpen(BHO_NAME, WINHTTP_ACCESS_TYPE_NAMED_PROXY, proxyName, proxyBypass, 0);
	}

	// Specify an HTTP server.
	if (*hSession)
	{
		hConnect = ::WinHttpConnect(*hSession, server, nServerPort, 0);
	}
	else
	{
	    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_OPEN, "HttpRequest::SendHttpRequest - WinHttpOpen")
	}

	// Create an HTTP request handle.
	if (*hConnect) 
	{
		DWORD dwFlags = 0;
	    if (nServerPort == INTERNET_DEFAULT_HTTPS_PORT)
		{
			dwFlags = WINHTTP_FLAG_SECURE;
		}

		hRequest = ::WinHttpOpenRequest(*hConnect, L"GET", url, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
	}
	else
	{
	    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_CONNECT, "HttpRequest::SendHttpRequest - WinHttpConnect")
	}

	// close the url, wont be needed anymore
	url.ReleaseBuffer();

	// Send a request.
    if (*hRequest)
    {
	    bResult = ::WinHttpSendRequest(*hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ? true : false;
	    if (!bResult)
	    {
	        DWORD dwError = ::GetLastError();

	        if (dwError == 12007L) // ERROR_INTERNET_NAME_NOT_RESOLVED
	        {
	            DEBUG_GENERAL("*** Trying to detect proxy for URL")

                // Set up the autoproxy call
                WINHTTP_AUTOPROXY_OPTIONS  autoProxyOptions;
                WINHTTP_PROXY_INFO         proxyInfo;
                DWORD                      cbProxyInfoSize = sizeof(proxyInfo);

                ::ZeroMemory(&autoProxyOptions, sizeof(autoProxyOptions));
                ::ZeroMemory(&proxyInfo, sizeof(proxyInfo));

                // Use auto-detection because the Proxy 
                // Auto-Config URL is not known.
                autoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
                
                // Use DHCP and DNS-based auto-detection.
                autoProxyOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;

                // If obtaining the PAC script requires NTLM/Negotiate
                // authentication, then automatically supply the client
                // domain credentials.
                autoProxyOptions.fAutoLogonIfChallenged = TRUE;

                CString completeUrl = (nServerPort == INTERNET_DEFAULT_HTTPS_PORT ? "https://" : "http://");
                completeUrl += server;
                completeUrl += file;

                bResult = ::WinHttpGetProxyForUrl(*hSession, completeUrl, &autoProxyOptions, &proxyInfo) ? true : false;
                if (bResult)
                {
                    bResult = ::WinHttpSetOption(*hRequest, WINHTTP_OPTION_PROXY, &proxyInfo, cbProxyInfoSize) ? true : false;
                    if (!bResult)
                    {
                        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_SET_OPTION, "HttpRequest::SendHttpRequest - WinHttpSetOption")
                    }
                }
                else
                {
                    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_GET_URL_PROXY, "HttpRequest::SendHttpRequest WinHttpGetProxyForUrl")
                }

                if (bResult)
                {
                    bResult = ::WinHttpSendRequest(*hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ? true : false;
                    if (!bResult)
                    {
	                    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_SEND_REQUEST, "HttpRequest::SendHttpRequest - WinHttpSendRequest")
                    }
                }
	        }
	        else
	        {
	            DEBUG_ERROR_LOG(dwError, PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_SEND_REQUEST, "HttpRequest::SendHttpRequest - WinHttpSendRequest")
            }
	    }
    }
    else
    {
        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_OPEN_REQUEST, "HttpRequest::SendHttpRequest - WinHttpOpenRequest")
    }
 
	// End the request.
	if (bResult)
	{
		bResult = ::WinHttpReceiveResponse(*hRequest, NULL) ? true : false;
		if (!bResult)
		{
    	    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_RECEIVE_RESPONSE, "HttpRequest::SendHttpRequest - WinHttpReceiveResponse")
	    }
	}

	// Keep checking for data until there is nothing left.
	if (bResult)
	{
		//check the header - if we do not receive an ad-aid tag in the header
		//then the answer cannot come from our server, maybe it is from an hotspot or something similar 
		//that demands validation before access to the internet is granted
		//see http://msdn.microsoft.com/en-us/library/aa384102(VS.85).aspx for documentation of queryheaders
		//we look for X-AIDPING: aidonline
		LPCWSTR headerName = L"X-AIDPING";
		wchar_t lpOutBuffer[50];
		DWORD dwSize = 50;

		if (nServerPort != 80 && ::WinHttpQueryHeaders(*hRequest, WINHTTP_QUERY_CUSTOM, headerName, lpOutBuffer, &dwSize, WINHTTP_NO_HEADER_INDEX))
		{
			if (CStringW(lpOutBuffer,dwSize) != CStringW(L"aidonline"))
			{
				// Unknown server - we return error
        	    DEBUG_ERROR_LOG(0, PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_INVALID_RESPONSE_SERVER, "HttpRequest::SendHttpRequest - Reponse not from correct server")
				return false;
			}
		}
		else if (nServerPort != 80)
		{
    	    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_QUERY_HEADERS, "HttpRequest::SendHttpRequest - WinHttpQueryHeaders")
		}

		do 
		{
			// Check for available data.
			dwSize = 0;
			if (!::WinHttpQueryDataAvailable(*hRequest, &dwSize))
			{
        	    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_QUERY_DATA_AVAILABLE, "HttpRequest::SendHttpRequest - WinHttpQueryDataAvailable")
				return false;
			}

			// Allocate space for the buffer.
			CStringA outBuffer;
			LPVOID pOutBuffer = outBuffer.GetBufferSetLength(dwSize+1);
			if (!pOutBuffer)
			{
				dwSize = 0;
				return false;
			}
			else
			{
				// Read the data.
				::ZeroMemory(pOutBuffer, dwSize+1);

				if (!::WinHttpReadData(*hRequest, pOutBuffer, dwSize, &dwDownloaded))
				{
            	    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_HTTP_REQUEST, PLUGIN_ERROR_HTTP_REQUEST_READ_DATA, "HttpRequest::SendHttpRequest - WinHttpReadData")
					return false;
				}
				else
				{
					if (response)
					{
					    *response += outBuffer;
                    }
				}

				// Free the memory allocated to the buffer.
			}

			outBuffer.ReleaseBuffer();

		} while (dwSize > 0);
	}

	return bResult;
}
