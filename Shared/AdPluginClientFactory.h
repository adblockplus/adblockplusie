#ifndef _PLUGIN_CLIENT_FACTORY_H_
#define _PLUGIN_CLIENT_FACTORY_H_


class CPluginClient;
class CPluginMimeFilterClient;


class CPluginClientFactory 
{

public:

	static CPluginClient* GetClientInstance();

	static CPluginMimeFilterClient* GetMimeFilterClientInstance();
	static void ReleaseMimeFilterClientInstance();

	// returns a client instance if initialized, otherwise it is null
	static CPluginClient* GetLazyClientInstance();

private:
	
	// the client for the process
	// if this variable is not null, it means that the client has been succesfully initialize
	// otherwise initialization has not happened, or it has failed
	static CPluginClient* s_clientInstance;
	static CPluginMimeFilterClient* s_mimeFilterInstance;
	
	static CComAutoCriticalSection s_criticalSection;
};


#endif // _PLUGIN_CLIENT_FACTORY_H_
