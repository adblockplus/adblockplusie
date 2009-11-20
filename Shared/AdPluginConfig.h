#ifndef _PLUGIN_CONFIG_H_
#define _PLUGIN_CONFIG_H_


#include "AdPluginTypedef.h"
#include "AdPluginIniFile.h"
#include "AdPluginChecksum.h"


class CPluginConfig
{

private:

	static CPluginConfig* s_instance;

    static CComAutoCriticalSection s_criticalSection;

	TDownloadFileProperties m_downloadFileProperties;

	// private constructor used by the singleton pattern
	CPluginConfig();
	
	void Create();

public:
	
	~CPluginConfig();
	
	// Returns an instance of the Dictionary
	static CPluginConfig* GetInstance(); 

    static bool Download(const CString& url, const CString& filename);
    bool GetDownloadProperties(const CString& headers, SDownloadFileProperties& properties) const;
    
    void Read();
	int GenerateFilterString(TCHAR* pBuffer, const CString& extension, bool allowConversion) const;
};


#endif // _PLUGIN_CONFIG_H_
