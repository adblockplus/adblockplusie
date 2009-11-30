#ifndef _PLUGIN_CLIENT_FACTORY_H_
#define _PLUGIN_CLIENT_FACTORY_H_


class CPluginMimeFilterClient;


class CPluginClientFactory 
{

public:

	static CPluginMimeFilterClient* GetMimeFilterClientInstance();
	static void ReleaseMimeFilterClientInstance();

private:

	static CPluginMimeFilterClient* s_mimeFilterInstance;
	
	static CComAutoCriticalSection s_criticalSection;
};


#endif // _PLUGIN_CLIENT_FACTORY_H_
