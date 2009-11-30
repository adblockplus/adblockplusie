#ifndef _SERVER_CLIENT_H
#define _SERVER_CLIENT_H


class LocalClient;
class CAdPluginConfiguration;


/*
	manages the current configuration from the server
	from this client we should get updates from aidonline servers etc
	can only be instantiated when we have a valid configuration from the server
*/
class CAdPluginServerClient 
{

public:

	CAdPluginServerClient(LocalClient* localClient);

	// refreshes the status from the server
	// all errors are caught, so the state after a call to this method is that either a new status from the server was 
	// fetched or nothing has happened
	void RefreshStatus();

	// get a status briefing. The status response from the server
	// its typically a text that tells how much the user has been repsonsible to collect
	// this is the latest status that is available
	// this method cannot throw
	CString GetStatus() const;

	CString GetInstallerDownLoadUrl() const;
	CString GetInstallerLocalPath() const;

	// Compare persistent data and server downloaded filterlist to create a
	// new set of filter list.
	void UpdateFilterUrlList();

	// Get new dictionary file from server
	void UpdateDictionary();

private:

	// Configuration
	std::auto_ptr<CAdPluginConfiguration> m_pluginConfiguration;
	
	LocalClient* m_localClient;
};

#endif
