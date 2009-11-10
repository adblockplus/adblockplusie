#ifndef _DICTIONARY_H
#define _DICTIONARY_H


#include "AdPluginIniFileW.h"
#include "AdPluginChecksum.h"


class CAdPluginDictionary
{

private:

	static CAdPluginDictionary* s_instance;

    static CComAutoCriticalSection s_criticalSectionDictionary;

	CAdPluginIniFileW::TSectionData m_dictionary;
	CStringA m_dictionaryLanguage;

	// private constructor used by the singleton pattern
	CAdPluginDictionary();
	
	void Create();

public:
	
	~CAdPluginDictionary();
	
	// Returns an instance of the Dictionary
	static CAdPluginDictionary* GetInstance(); 

	// Initializes the Dictionary. Should be called before any thing else
	void SetLanguage(const CStringA& lang);
	bool IsLanguageSupported(const CStringA& lang);

    static bool Download(const CStringA& url, const CStringA& filename);

	CString Lookup(const CString& key);
};

#endif
