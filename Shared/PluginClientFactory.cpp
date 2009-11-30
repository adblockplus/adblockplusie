#include "PluginStdAfx.h"

#include "PluginClientFactory.h"
#include "PluginMimeFilterClient.h"


CPluginMimeFilterClient* CPluginClientFactory::s_mimeFilterInstance = NULL;

CComAutoCriticalSection CPluginClientFactory::s_criticalSection;

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
	