#ifndef _ADPLUGIN_CONFIG_H_
#define _ADPLUGIN_CONFIG_H_


#include "AdPluginTypedef.h"
#include "AdPluginIniFile.h"
#include "AdPluginChecksum.h"


class CAdPluginConfig
{

private:

	static CAdPluginConfig* s_instance;

    static CComAutoCriticalSection s_criticalSection;

	TDownloadFileProperties m_downloadFileProperties;

	// private constructor used by the singleton pattern
	CAdPluginConfig();
	
	void Create();

public:
	
	~CAdPluginConfig();
	
	// Returns an instance of the Dictionary
	static CAdPluginConfig* GetInstance(); 

    static bool Download(const CStringA& url, const CStringA& filename);
    bool GetDownloadProperties(const CStringA& headers, SDownloadFileProperties& properties) const;
    
    void Read();
};


#endif // _ADPLUGIN_CONFIG_H_
