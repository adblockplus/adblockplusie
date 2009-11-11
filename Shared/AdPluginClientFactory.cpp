#include "AdPluginStdAfx.h"

#include "AdPluginClientFactory.h"
#include "AdPluginMimeFilterClient.h"
#include "AdPluginClient.h"


CPluginClient* CPluginClientFactory::s_clientInstance = NULL;
CPluginMimeFilterClient* CPluginClientFactory::s_mimeFilterInstance = NULL;

CComAutoCriticalSection CPluginClientFactory::s_criticalSection;


CPluginClient* CPluginClientFactory::GetLazyClientInstance()
{
    CPluginClient* client;

    s_criticalSection.Lock();
    {
        client = s_clientInstance;
    }
    s_criticalSection.Unlock();

	return client;	
}

CPluginClient* CPluginClientFactory::GetClientInstance() 
{
    CPluginClient* client;

    s_criticalSection.Lock();
    {
	    if (!s_clientInstance)
	    {
		    // We cannot copy the client directly into the instance variable
		    // If the constructor throws we do not want to alter instance
		    CPluginClient* localInstance = new CPluginClient();

		    s_clientInstance = localInstance;
	    }
	    
	    client = s_clientInstance;
    }
    s_criticalSection.Unlock();

	return client;
}


CPluginMimeFilterClient* CPluginClientFactory ::GetMimeFilterClientInstance() 
{
    CPluginMimeFilterClient* localInstance = NULL;

	s_criticalSection.Lock();
	{
	    if (!s_mimeFilterInstance)
	    {
		    //we cannot copy the client directly into the instance variable
		    //if the constructor throws we do not want to alter instance
		    localInstance = new CPluginMimeFilterClient();

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

void CPluginClientFactory::ReleaseMimeFilterClientInstance() 
{
	s_criticalSection.Lock();
	{
	    delete s_mimeFilterInstance;

		s_mimeFilterInstance = NULL;
    }
    s_criticalSection.Unlock();
}
	