#include "AdPluginStdAfx.h"

#include "AdPluginClientFactory.h"
#include "AdPluginMimeFilterClient.h"
#include "AdPluginClient.h"


LocalClient* CAdPluginClientFactory::s_localInstance = NULL;
MimeFilterClient* CAdPluginClientFactory::s_mimeFilterInstance = NULL;

CComAutoCriticalSection CAdPluginClientFactory::s_criticalSection;


LocalClient* CAdPluginClientFactory::GetLazyClientInstance()
{
    LocalClient* client;

    s_criticalSection.Lock();
    {
        client = s_localInstance;
    }
    s_criticalSection.Unlock();

	return client;	
}

LocalClient* CAdPluginClientFactory::GetClientInstance() 
{
    LocalClient* client;

    s_criticalSection.Lock();
    {
	    if (!s_localInstance)
	    {
		    // We cannot copy the client directly into the instance variable
		    // If the constructor throws we do not want to alter instance
		    LocalClient* localInstance = new LocalClient();

		    s_localInstance = localInstance;
	    }
	    
	    client = s_localInstance;
    }
    s_criticalSection.Unlock();

	return client;
}


MimeFilterClient* CAdPluginClientFactory ::GetMimeFilterClientInstance() 
{
    MimeFilterClient* localInstance = NULL;

	s_criticalSection.Lock();
	{
	    if (!s_mimeFilterInstance)
	    {
		    //we cannot copy the client directly into the instance variable
		    //if the constructor throws we do not want to alter instance
		    localInstance = new MimeFilterClient();

		    s_mimeFilterInstance = localInstance;
	    }
	    else
	    {
	        localInstance = s_mimeFilterInstance;
	    }
    }
    s_criticalSection.Unlock();

	return localInstance;
}
