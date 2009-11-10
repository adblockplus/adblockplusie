#include "AdPluginStdAfx.h"

#include "AdPluginDictionary.h"
#include "AdPluginClient.h"
#include "AdPluginSettings.h"
#include "AdPluginMutex.h"


class CAdPluginDictionaryLock : public CAdPluginMutex
{
public:
    CAdPluginDictionaryLock() : CAdPluginMutex("DictionaryFile", PLUGIN_ERROR_MUTEX_DICTIONARY_FILE) {}
    ~CAdPluginDictionaryLock() {}
};


CAdPluginDictionary* CAdPluginDictionary::s_instance = NULL;

CComAutoCriticalSection CAdPluginDictionary::s_criticalSectionDictionary;


CAdPluginDictionary::CAdPluginDictionary() : m_dictionaryLanguage("en")
{
	DEBUG_GENERAL("*** Initializing dictionary")

    bool isExisting = true;
    {
        CAdPluginDictionaryLock lock;
        if (lock.IsLocked())
        {
	        std::ifstream is;
	        is.open(CAdPluginSettings::GetDataPath(DICTIONARY_INI_FILE), std::ios_base::in);
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

    if (!isExisting)
    {
	    Create();
    }
}


CAdPluginDictionary::~CAdPluginDictionary()
{
    s_criticalSectionDictionary.Lock();
	{
		s_instance = NULL;
	}
    s_criticalSectionDictionary.Unlock();
}


CAdPluginDictionary* CAdPluginDictionary::GetInstance() 
{
	CAdPluginDictionary* instance = NULL;

    s_criticalSectionDictionary.Lock();
	{
		if (!s_instance)
		{
	        DEBUG_DICTIONARY("Dictionary::GetInstance - creating")
			s_instance = new CAdPluginDictionary();
		}

		instance = s_instance;
	}
    s_criticalSectionDictionary.Unlock();

	return instance;
}


bool CAdPluginDictionary::IsLanguageSupported(const CStringA& lang) 
{
    bool hasLanguage = false;

    CAdPluginDictionaryLock lock;
    if (lock.IsLocked())
    {
        CAdPluginIniFileW iniFile(CAdPluginSettings::GetDataPath(DICTIONARY_INI_FILE));

        iniFile.Read();
            
        USES_CONVERSION;
        hasLanguage = iniFile.HasSection(A2W(lang));
    }
        
	return hasLanguage;
}


void CAdPluginDictionary::SetLanguage(const CStringA& lang)
{
	DEBUG_GENERAL("*** Loading dictionary:" + CAdPluginSettings::GetDataPath(DICTIONARY_INI_FILE))

    CAdPluginDictionaryLock lock;
    if (lock.IsLocked())
    {
        CAdPluginIniFileW iniFile(CAdPluginSettings::GetDataPath(DICTIONARY_INI_FILE));

        if (!iniFile.Read())
        {
            DEBUG_ERROR_LOG(iniFile.GetLastError(), PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_READ_FILE, "Dictionary::SetLanguage - Read")
            return;
        }

        s_criticalSectionDictionary.Lock();
	    {
            USES_CONVERSION;
            CStringW langW = lang;

		    if (iniFile.HasSection(langW))
		    {
			    m_dictionaryLanguage = lang;

			    m_dictionary = iniFile.GetSectionData(langW);

		        CStringA dicEl;
		        dicEl.Format("%d", m_dictionary.size());
		        
    		    DEBUG_GENERAL("*** Using dictionary section [" + lang + "] - " + dicEl + " elements")
		    }
		    else
		    {
			    m_dictionary = iniFile.GetSectionData(L"en");
			    m_dictionaryLanguage = "en";

		        CStringA dicEl;
		        dicEl.Format("%d", m_dictionary.size());

    		    DEBUG_GENERAL("*** Using dictionary section [en] instead of [" + lang + "] - " + dicEl + " elements")
		    }

#ifdef ENABLE_DEBUG_DICTIONARY
            for (CAdPluginIniFileW::TSectionData::iterator it = m_dictionary.begin(); it != m_dictionary.end(); ++it)
            {
    		    DEBUG_DICTIONARY("- " + it->first + " -> " + it->second)
            }
#endif
	    }
        s_criticalSectionDictionary.Unlock();
    }
}


CString CAdPluginDictionary::Lookup(const CString& key) 
{
    CString value = key;

    s_criticalSectionDictionary.Lock();
	{
		CAdPluginIniFileW::TSectionData::iterator it = m_dictionary.find(key);
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
		    
		    DEBUG_ERROR_LOG(dwErrorCode, PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_LOOKUP, "Dictionary::Lookup - " + CStringA(key))
		}
#endif
	}
    s_criticalSectionDictionary.Unlock();

    DEBUG_DICTIONARY("Dictionary::Lookup key:" + key + " value:" + value)

    return value;
}


void CAdPluginDictionary::Create()
{
	DEBUG_GENERAL("*** Creating dictionary:" + CAdPluginSettings::GetDataPath(DICTIONARY_INI_FILE))

    CAdPluginDictionaryLock lock;
    if (lock.IsLocked())
    {
        CAdPluginIniFileW iniFile(CAdPluginSettings::GetDataPath(DICTIONARY_INI_FILE));

        CAdPluginSettings* settings = CAdPluginSettings::GetInstance();

        if (iniFile.Exists() || !settings->IsMainProcess())
        {
            return;
        }

        s_criticalSectionDictionary.Lock();
	    {
		    m_dictionary.clear();

#if (defined PRODUCT_ADBLOCKER)

            // Popup menu
		    m_dictionary["UPDATE"] = "Update Simple Adblock to newest version";
		    m_dictionary["ABOUT"] = "About Simple Adblock";
		    m_dictionary["ACTIVATE"] = "Activate Simple Adblock";
		    m_dictionary["FAQ"] = "Frequently Asked Questions";
		    m_dictionary["FEEDBACK"] = "Feedback";
		    m_dictionary["INVITE_FRIENDS"] = "Invite friends";
		    m_dictionary["SETTINGS"] = "Settings";
		    m_dictionary["ENABLE"] = "Enable Simple Adblock";
		    m_dictionary["DISABLE"] = "Disable Simple Adblock";
		    m_dictionary["DISABLE_ON"] = "Disable Simple Adblock on...";

            // Update dialog
		    m_dictionary["UPDATE_TITLE"] = "Update Simple Adblock";
		    m_dictionary["UPDATE_NEW_VERSION_EXISTS"] = "A new version of Simple Adblock is available";
		    m_dictionary["UPDATE_DO_YOU_WISH_TO_DOWNLOAD"] = "Do you wish to download it now?";

            // Download update dialog
		    m_dictionary["DOWNLOAD_TITLE"] = "Download Simple Adblock";
		    m_dictionary["DOWNLOAD_UPDATE_BUTTON"] = "Update";
		    m_dictionary["DOWNLOAD_PROGRESS_TEXT"] = "Please wait...";
		    m_dictionary["DOWNLOAD_DOWNLOAD_ERROR_TEXT"] = "Error downloading installer";
		    m_dictionary["DOWNLOAD_POST_DOWNLOAD_TEXT"] = "If you choose to update Simple Adblock, your Internet Explorer will close before installation";

#elif (defined PRODUCT_DOWNLOADHELPER)

            // Popup menu
		    m_dictionary["UPDATE"] = "Update IE Download Helper to newest version";
		    m_dictionary["ABOUT"] = "About IE Download Helper";
		    m_dictionary["ACTIVATE"] = "Activate IE Download Helper";
		    m_dictionary["FAQ"] = "Frequently Asked Questions";
		    m_dictionary["FEEDBACK"] = "Feedback";
		    m_dictionary["INVITE_FRIENDS"] = "Invite friends";
		    m_dictionary["SETTINGS"] = "Settings";
		    m_dictionary["ENABLE"] = "Enable IE Download Helper";
		    m_dictionary["DISABLE"] = "Disable IE Download Helper";
		    m_dictionary["DISABLE_ON"] = "Disable IE Download Helper on...";

            // Update dialog
		    m_dictionary["UPDATE_TITLE"] = "Update IE Download Helper";
		    m_dictionary["UPDATE_NEW_VERSION_EXISTS"] = "A new version of IE Download Helper is available";
		    m_dictionary["UPDATE_DO_YOU_WISH_TO_DOWNLOAD"] = "Do you wish to download it now?";

            // Download update dialog
		    m_dictionary["DOWNLOAD_TITLE"] = "Download IE Download Helper";
		    m_dictionary["DOWNLOAD_UPDATE_BUTTON"] = "Update";
		    m_dictionary["DOWNLOAD_PROGRESS_TEXT"] = "Please wait...";
		    m_dictionary["DOWNLOAD_DOWNLOAD_ERROR_TEXT"] = "Error downloading installer";
		    m_dictionary["DOWNLOAD_POST_DOWNLOAD_TEXT"] = "If you choose to update IE Download Helper, your Internet Explorer will close before installation";

#endif

            // General texts
		    m_dictionary["YES"] = "Yes";
		    m_dictionary["NO"] = "No";
		    m_dictionary["CANCEL"] = "Cancel";

	        iniFile.UpdateSection("en", m_dictionary);
	    }
        s_criticalSectionDictionary.Unlock();

        if (iniFile.Write())
        {
            CAdPluginSettings* settings = CAdPluginSettings::GetInstance();
            
            settings->SetValue(SETTING_DICTIONARY_VERSION, 1);
            settings->Write();
        }
        else
        {
            DEBUG_ERROR_LOG(iniFile.GetLastError(), PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_CREATE_FILE, "Dictionary::Create - Write")
        }

        // Delete old
        ::DeleteFileA(CAdPluginSettings::GetDataPath("dictionary.ini"));
    }
}

bool CAdPluginDictionary::Download(const CStringA& url, const CStringA& filename)
{
    CStringA tempFile = CAdPluginSettings::GetTempFile(TEMP_FILE_PREFIX);

    DEBUG_GENERAL("*** Downloading dictionary:" + filename +  " (to " + tempFile + ")");

    bool bResult = !tempFile.IsEmpty();
    if (bResult)
    {
	    // if new filter urls are found download them and update the persistent data
	    HRESULT hr = ::URLDownloadToFileA(NULL, url, tempFile, 0, NULL);
        if (SUCCEEDED(hr))
        {
            CAdPluginDictionaryLock lock;
            if (lock.IsLocked())
            {
                // Move the temporary file to the new text file.
                if (!::MoveFileExA(tempFile, CAdPluginSettings::GetDataPath(DICTIONARY_INI_FILE), MOVEFILE_REPLACE_EXISTING))
                {
                    DWORD dwError = ::GetLastError();

                    // Not same device? copy/delete instead
                    if (dwError == ERROR_NOT_SAME_DEVICE)
                    {
                        if (!::CopyFileA(tempFile, CAdPluginSettings::GetDataPath(DICTIONARY_INI_FILE), FALSE))
                        {
                            DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_COPY_FILE, "Dictionary::Download - CopyFile(" + filename + ")")

                            bResult = false;
                        }

                        ::DeleteFileA(tempFile);
                    }
                    else
                    {
                        DEBUG_ERROR_LOG(dwError, PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_MOVE_FILE, "Dictionary::Download - MoveFileEx(" + filename + ")")

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
            DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_DICTIONARY, PLUGIN_ERROR_DICTIONARY_DOWNLOAD_FILE, "Dictionary::Download - URLDownloadToFile(" + CStringA(DICTIONARY_INI_FILE) + ")")

            bResult = false;
        }
    }

    return bResult;
}
