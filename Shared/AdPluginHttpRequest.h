#ifndef _ADPLUGIN_HTTP_REQUEST_H_
#define _ADPLUGIN_HTTP_REQUEST_H_


class CAdPluginChecksum;
class CAdPluginIniFile;

class CAdPluginHttpRequest
{

public:

    CAdPluginHttpRequest(const CStringA& script, bool addChecksum=true);
    ~CAdPluginHttpRequest();

    bool Send(bool checkResponse=true);

    void AddPluginId();
	void AddOsInfo();

    void Add(const CStringA& arg, const CStringA& value, bool addToChecksum=true);
    void Add(const CStringA& arg, unsigned int value, bool addToChecksum=true);

    CStringA GetUrl();
    CStringA GetResponseText() const;
	const std::auto_ptr<CAdPluginIniFile>& GetResponseFile() const;
	bool IsValidResponse() const;

    static CStringA GetStandardUrl(const CStringA& script);

    static BOOL GetProxySettings(CString& proxyName, CString& proxyBypass);

    static bool SendHttpRequest(LPCWSTR server, LPCSTR file, CStringA* response, WORD nServerPort);

protected:

    CStringA m_url;
    CStringA m_urlPrefix;
    CStringA m_script;
    CStringA m_responseText;
    bool m_addChecksum;

	std::auto_ptr<CAdPluginChecksum> m_checksum;    
	std::auto_ptr<CAdPluginIniFile> m_responseFile;
};


#endif // _ADPLUGIN_HTTP_REQUEST_H_
