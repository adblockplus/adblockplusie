#include "PluginStdAfx.h"

#include "PluginDictionary.h"
#include "PluginClient.h"
#include "PluginSettings.h"
#include "PluginMutex.h"


class CPluginDictionaryLock : public CPluginMutex
{
public:
    CPluginDictionaryLock() : CPluginMutex("DictionaryFile", PLUGIN_ERROR_MUTEX_DICTIONARY_FILE) {}
    ~CPluginDictionaryLock() {}
};


CPluginDictionary* CPluginDictionary::s_instance = NULL;

CComAutoCriticalSection CPluginDictionary::s_criticalSectionDictionary;


CPluginDictionary::CPluginDictionary(bool forceCreate) : m_dictionaryLanguage("en")
{
	DEBUG_GENERAL("*** Initializing dictionary")

	m_dictionaryConversions[L"UPDATE"]			= L"MENU_UPDATE";
	m_dictionaryConversions[L"ABOUT"]			= L"MENU_ABOUT";
	m_dictionaryConversions[L"ACTIVATE"]		= L"MENU_ACTIVATE";
	m_dictionaryConversions[L"FAQ"]				= L"MENU_FAQ";
	m_dictionaryConversions[L"FEEDBACK"]		= L"MENU_FEEDBACK";
	m_dictionaryConversions[L"INVITE_FRIENDS"]	= L"MENU_INVITE_FRIENDS";
	m_dictionaryConversions[L"UPGRADE"]			= L"MENU_UPGRADE";
	m_dictionaryConversions[L"ENTERLICENSE"]		= L"MENU_ENTERLICENSE";
	m_dictionaryConversions[L"SETTINGS"]		= L"MENU_SETTINGS";
	m_dictionaryConversions[L"ENABLE_CONVERSSION"]	= L"MENU_ENABLE_CONVERSION";
	m_dictionaryConversions[L"ENABLE"]			= L"MENU_ENABLE";
	m_dictionaryConversions[L"DISABLE"]			= L"MENU_DISABLE";
	m_dictionaryConversions[L"DISABLE_ON"]		= L"MENU_DISABLE_ON";

	m_dictionaryConversions[L"DOWNLOADLIMIT_MESSAGE"]		= L"DOWNLOAD_LIMIT_MESSAGE";
	m_dictionaryConversions[L"DOWNLOADLIMIT_TITLE"]			= L"DOWNLOAD_LIMIT_TITLE";

	m_dictionaryConversions[L"DOWNLOAD_TITLE"]				= L"DOWNLOAD_UPDATE_TITLE";
	m_dictionaryConversions[L"DOWNLOAD_PROGRESS_TEXT"]		= L"DOWNLOAD_PLEASE_WAIT";
	m_dictionaryConversions[L"DOWNLOAD_DOWNLOAD_ERROR_TEXT"]= L"DOWNLOAD_UPDATE_ERROR_TEXT";
	m_dictionaryConversions[L"DOWNLOAD_POST_DOWNLOAD_TEXT"]	= L"DOWNLOAD_UPDATE_SUCCESS_TEXT";

	m_dictionaryConversions[L"YES"]		= L"GENERAL_YES";
	m_dictionaryConversions[L"NO"]		= L"GENERAL_NO";
	m_dictionaryConversions[L"CANCEL"]	= L"GENERAL_CANCEL";

    bool isExisting = true;
    {
        CPluginDictionaryLock lock;
        if (lock.IsLocked())
        {
	        std::ifstream is;
	        is.open(CPluginSettings::GetDataPath(DICTIONARY_INI_FILE), std::ios_base::in);
	        if (!is.is_open())
	        {
	            DEBUG_DICTIONARY("Dictionary::Constructor - Exists:no")
	            isExisting = false;
	        }
	        else
	        {
	            DEBUG_DICTIONARY("Dictionary::Constructor - Exists:yes")
		        is.close();
	        }
        }
    }

    if (!isExisting || forceCreate)
    {
	    Create(forceCreate);
    }
}


CPluginDictionary::~CPluginDictionary()
{
    s_criticalSectionDictionary.Lock();
	{
		s_instance = NULL;
	}
    s_criticalSectionDictionary.Unlock();
}


CPluginDictionary* CPluginDictionary::GetInstance(bool forceCreate) 
{
	CPluginDictionary* instance = NULL;

    s_criticalSectionDictionary.Lock();
	{
		if (!s_instance)
		{
	        DEBUG_DICTIONARY("Dictionary::GetInstance - creating")
			s_instance = new CPluginDictionary(forceCreate);
		}

		instance = s_instance;
	}
    s_criticalSectionDictionary.Unlock();

	return instance;
}


bool CPluginDictionary::IsLanguageSupported(const CString& lang) 
{
    bool hasLanguage = false;

    CPluginDictionaryLock lock;
    if (lock.IsLocked())
    {
        CPluginIniFileW iniFile(CPluginSettings::GetDataPath(DICTIONARY_INI_FILE));

        iniFile.Read();
            
        hasLanguage = iniFile.HasSection(lang);
    }
        
	return hasLanguage;
}


void CPluginDictionary::SetLanguage(const CString& lang)
{
	DEBUG_GENERAL(L"*** Loading dictionary:" + CPluginSettings::GetDataPath(DICTIONARY_INI_FILE))

    CPluginDictionaryLock lock;
    if (lock.IsLocked())
    {
        CPluginIniFileW iniFile(CPluginSettings::GetDataPath(DICTIONARY_INI_FILE));

        if (!iniFile.Read())
        {
            DEBUG_ERROR_LOG(iniFile.GetLastError(), PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_READ_FILE, "Dictionary::SetLanguage - Read")
            return;
        }

        s_criticalSectionDictionary.Lock();
	    {
		    if (iniFile.HasSection(lang))
		    {
			    m_dictionaryLanguage = lang;

			    m_dictionary = iniFile.GetSectionData(lang);

		        CString dicEl;
		        dicEl.Format(L"%d", m_dictionary.size());
		        
    		    DEBUG_GENERAL(L"*** Using dictionary section [" + lang + "] - " + dicEl + L" elements")
		    }
		    else
		    {
			    m_dictionary = iniFile.GetSectionData(L"en");
			    m_dictionaryLanguage = L"en";

		        CString dicEl;
		        dicEl.Format(L"%d", m_dictionary.size());

    		    DEBUG_GENERAL(L"*** Using dictionary section [en] instead of [" + lang + "] - " + dicEl + L" elements")
		    }

			// Dictionary conversions
			for (std::map<CString,CString>::iterator it = m_dictionaryConversions.begin(); it != m_dictionaryConversions.end(); ++it)
			{
				CPluginIniFileW::TSectionData::iterator itDic = m_dictionary.find(it->first);
				if (itDic != m_dictionary.end())
				{
					m_dictionary[it->second] = itDic->second;
				}
			}

#ifdef ENABLE_DEBUG_DICTIONARY
            for (CPluginIniFileW::TSectionData::iterator it = m_dictionary.begin(); it != m_dictionary.end(); ++it)
            {
    		    DEBUG_DICTIONARY("- " + it->first + " -> " + it->second)
            }
#endif
	    }
        s_criticalSectionDictionary.Unlock();
    }
}


CString CPluginDictionary::Lookup(const CString& key) 
{
    CString value = key;

    s_criticalSectionDictionary.Lock();
	{
		CPluginIniFileW::TSectionData::iterator it = m_dictionary.find(key);
		if (it != m_dictionary.end())
		{
			value = it->second;
		}
#ifdef DEBUG_ERROR_LOG
		else
		{
		    DWORD dwErrorCode = 0;

            dwErrorCode |= m_dictionaryLanguage.GetAt(0) << 24;
            dwErrorCode |= m_dictionaryLanguage.GetAt(1) << 16;
            dwErrorCode |= key.GetAt(0) << 8;
            dwErrorCode |= key.GetAt(1);
		    
		    DEBUG_ERROR_LOG(dwErrorCode, PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_LOOKUP, L"Dictionary::Lookup - " + key)
		}
#endif
	}
    s_criticalSectionDictionary.Unlock();

    DEBUG_DICTIONARY("Dictionary::Lookup key:" + key + " value:" + value)

    return value;
}


void CPluginDictionary::Create(bool forceCreate)
{
	DEBUG_GENERAL(L"*** Creating dictionary:" + CPluginSettings::GetDataPath(DICTIONARY_INI_FILE))

    CPluginDictionaryLock lock;
    if (lock.IsLocked())
    {
        CPluginIniFileW iniFile(CPluginSettings::GetDataPath(DICTIONARY_INI_FILE));

        CPluginSettings* settings = CPluginSettings::GetInstance();

		if (forceCreate)
		{
		}
        else if (iniFile.Exists() || !settings->IsMainProcess())
        {
            return;
        }

		int dictionaryVersion = 1;

        s_criticalSectionDictionary.Lock();
	    {
			m_dictionary.clear();

#if (defined PRODUCT_SIMPLEADBLOCK)

            // Popup menu
		    m_dictionary["MENU_UPDATE"] = "Update Simple Adblock to newest version";
		    m_dictionary["MENU_ABOUT"] = "About Simple Adblock";
		    m_dictionary["MENU_ACTIVATE"] = "Activate Simple Adblock";
		    m_dictionary["MENU_FAQ"] = "Frequently Asked Questions";
		    m_dictionary["MENU_FEEDBACK"] = "Feedback";
		    m_dictionary["MENU_INVITE_FRIENDS"] = "Invite friends";
		    m_dictionary["MENU_SETTINGS"] = "Settings";
		    m_dictionary["MENU_ENABLE"] = "Enable Simple Adblock";
		    m_dictionary["MENU_DISABLE"] = "Disable Simple Adblock";
		    m_dictionary["MENU_DISABLE_ON"] = "Disable Simple Adblock on...";

            // Update dialog
		    m_dictionary["UPDATE_TITLE"] = "Update Simple Adblock";
		    m_dictionary["UPDATE_NEW_VERSION_EXISTS"] = "A new version of Simple Adblock is available";
		    m_dictionary["UPDATE_DO_YOU_WISH_TO_DOWNLOAD"] = "Do you wish to download it now?";

            // Download update dialog
		    m_dictionary["DOWNLOAD_UPDATE_TITLE"] = "Download Simple Adblock";
		    m_dictionary["DOWNLOAD_UPDATE_BUTTON"] = "Update";
		    m_dictionary["DOWNLOAD_PLEASE_WAIT"] = "Please wait...";
		    m_dictionary["DOWNLOAD_UPDATE_ERROR_TEXT"] = "Error downloading installer";
		    m_dictionary["DOWNLOAD_UPDATE_SUCCESS_TEXT"] = "If you choose to update Simple Adblock, your Internet Explorer will close before installation";

#endif
            // General texts
		    m_dictionary["GENERAL_YES"] = "Yes";
		    m_dictionary["GENERAL_NO"] = "No";
		    m_dictionary["GENERAL_CANCEL"] = "Cancel";
		    m_dictionary["GENERAL_CLOSE"] = "Close";

	        iniFile.UpdateSection("en", m_dictionary);
	    }
        s_criticalSectionDictionary.Unlock();

        if (iniFile.Write())
        {
            CPluginSettings* settings = CPluginSettings::GetInstance();
            
            settings->SetValue(SETTING_DICTIONARY_VERSION, dictionaryVersion);
            settings->Write();
        }
        else
        {
            DEBUG_ERROR_LOG(iniFile.GetLastError(), PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_CREATE_FILE, L"Dictionary::Create - Write")
        }
#ifdef PRODUCT_SIMPLEADBLOCK
        // Delete old
        ::DeleteFile(CPluginSettings::GetDataPath(L"dictionary.ini"));
#endif
	}
}

bool CPluginDictionary::Download(const CString& url, const CString& filename)
{
    CString tempFile = CPluginSettings::GetTempFile(TEMP_FILE_PREFIX);

    DEBUG_GENERAL(L"*** Downloading dictionary:" + filename +  " (to " + tempFile + ")");

    bool bResult = !tempFile.IsEmpty();
    if (bResult)
    {
	    // if new filter urls are found download them and update the persistent data
	    HRESULT hr = ::URLDownloadToFile(NULL, url, tempFile, 0, NULL);
        if (SUCCEEDED(hr))
        {
            CPluginDictionaryLock lock;
            if (lock.IsLocked())
            {
                // Move the temporary file to the new text file.
                if (!::MoveFileEx(tempFile, CPluginSettings::GetDataPath(DICTIONARY_INI_FILE), MOVEFILE_REPLACE_EXISTING))
                {
                    DWORD dwError = ::GetLastError();

                    // Not same device? copy/delete instead
                    if (dwError == ERROR_NOT_SAME_DEVICE)
                    {
                        if (!::CopyFile(tempFile, CPluginSettings::GetDataPath(DICTIONARY_INI_FILE), FALSE))
                        {
                            DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_COPY_FILE, L"Dictionary::Download - CopyFile(" + filename + L")")

                            bResult = false;
                        }

                        ::DeleteFile(tempFile);
                    }
                    else
                    {
                        DEBUG_ERROR_LOG(dwError, PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_MOVE_FILE, L"Dictionary::Download - MoveFileEx(" + filename + L")")

                        bResult = false;
                    }
                }
            }
            else
            {
                bResult = false;
            }
        }
        else
        {
            DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_DOWNLOAD_FILE, L"Dictionary::Download - URLDownloadToFile(" + CString(DICTIONARY_INI_FILE) + L")")

            bResult = false;
        }
    }

    return bResult;
}
