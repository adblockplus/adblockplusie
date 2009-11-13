#ifndef _PLUGIN_HTTP_REQUEST_H_
#define _PLUGIN_HTTP_REQUEST_H_


class CPluginChecksum;
class CPluginIniFile;

class CPluginHttpRequest
{

public:

    CPluginHttpRequest(const CString& script, bool addChecksum=true);
    ~CPluginHttpRequest();

    bool Send(bool checkResponse=true);

    void AddPluginId();
	void AddOsInfo();

    void Add(const CString& arg, const CString& value, bool addToChecksum=true);
    void Add(const CString& arg, unsigned int value, bool addToChecksum=true);

    CString GetUrl();
    CStringA GetResponseText() const;
	const std::auto_ptr<CPluginIniFile>& GetResponseFile() const;
	bool IsValidResponse() const;

    static CString GetStandardUrl(const CString& script);

    static BOOL GetProxySettings(CString& proxyName, CString& proxyBypass);

    static bool SendHttpRequest(LPCWSTR server, LPCWSTR file, CStringA* response, WORD nServerPort);

protected:

    CString m_url;
    CString m_urlPrefix;
    CString m_script;
    CStringA m_responseText;
    bool m_addChecksum;

	std::auto_ptr<CPluginChecksum> m_checksum;    
	std::auto_ptr<CPluginIniFile> m_responseFile;
};


#endif // _PLUGIN_HTTP_REQUEST_H_
