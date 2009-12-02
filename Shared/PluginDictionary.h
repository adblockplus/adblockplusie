#ifndef _DICTIONARY_H
#define _DICTIONARY_H


#include "PluginIniFileW.h"
#include "PluginChecksum.h"


class CPluginDictionary
{

private:

	static CPluginDictionary* s_instance;

    static CComAutoCriticalSection s_criticalSectionDictionary;

	CPluginIniFileW::TSectionData m_dictionary;
	CString m_dictionaryLanguage;
	std::map<CString,CString> m_dictionaryConversions;

	// private constructor used by the singleton pattern
	CPluginDictionary(bool forceCreate=false);
	
	void Create(bool forceCreate=false);

public:
	
	~CPluginDictionary();
	
	// Returns an instance of the Dictionary
	static CPluginDictionary* GetInstance(bool forceCreate=false); 

	// Initializes the Dictionary. Should be called before any thing else
	void SetLanguage(const CString& lang);
	bool IsLanguageSupported(const CString& lang);

    static bool Download(const CString& url, const CString& filename);

	CString Lookup(const CString& key);
};

#endif
