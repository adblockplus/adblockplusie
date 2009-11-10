#ifndef _ADPLUGIN_CLIENT_FACTORY_H_
#define _ADPLUGIN_CLIENT_FACTORY_H_


class LocalClient;
class MimeFilterClient;


class CAdPluginClientFactory 
{

public:

	static LocalClient* GetClientInstance();
	static MimeFilterClient* GetMimeFilterClientInstance();
	
	// returns a client instance if initialized, otherwise it is null
	static LocalClient* GetLazyClientInstance();

private:
	
	// the client for the process
	// if this variable is not null, it means that the client has been succesfully initialize
	// otherwise initialization has not happened, or it has failed
	static LocalClient* s_localInstance;
	static MimeFilterClient* s_mimeFilterInstance;
	
	static CComAutoCriticalSection s_criticalSection;
};


#endif // _ADPLUGIN_CLIENT_FACTORY_H_
