#include "AdPluginStdAfx.h"

#include "AdPluginConfig.h"
#include "AdPluginClient.h"
#include "AdPluginSettings.h"
#include "AdPluginMutex.h"


class CAdPluginConfigLock : public CAdPluginMutex
{
public:
    CAdPluginConfigLock() : CAdPluginMutex("ConfigFile", PLUGIN_ERROR_MUTEX_CONFIG_FILE) {}
    ~CAdPluginConfigLock() {}
};


CAdPluginConfig* CAdPluginConfig::s_instance = NULL;

CComAutoCriticalSection CAdPluginConfig::s_criticalSection;


CAdPluginConfig::CAdPluginConfig()
{
	DEBUG_GENERAL("*** Initializing config")

    bool isExisting = true;
    {
        CAdPluginConfigLock lock;
        if (lock.IsLocked())
        {
	        std::ifstream is;
	        is.open(CAdPluginSettings::GetDataPath(CONFIG_INI_FILE), std::ios_base::in);
	        if (!is.is_open())
	        {
	            isExisting = false;
	        }
	        else
	        {
		        is.close();
	        }
        }
    }

    if (isExisting)
    {
        Read();
    }
    else
    {
	    Create();
    }
}


CAdPluginConfig::~CAdPluginConfig()
{
    s_criticalSection.Lock();
	{
		s_instance = NULL;
	}
    s_criticalSection.Unlock();
}


CAdPluginConfig* CAdPluginConfig::GetInstance() 
{
	CAdPluginConfig* instance = NULL;

    s_criticalSection.Lock();
	{
		if (!s_instance)
		{
			s_instance = new CAdPluginConfig();
		}

		instance = s_instance;
	}
    s_criticalSection.Unlock();

	return instance;
}


void CAdPluginConfig::Read()
{
	DEBUG_GENERAL("*** Loading config:" + CAdPluginSettings::GetDataPath(CONFIG_INI_FILE))

    CAdPluginConfigLock lock;
    if (lock.IsLocked())
    {
        CAdPluginIniFile iniFile(CAdPluginSettings::GetDataPath(CONFIG_INI_FILE), true);

        if (!iniFile.Read())
        {
            DEBUG_ERROR_LOG(iniFile.GetLastError(), PLUGIN_ERROR_CONFIG, PLUGIN_ERROR_CONFIG_READ_FILE, "Dictionary::SetLanguage - Read")
            return;
        }

        s_criticalSection.Lock();
	    {
            m_downloadFileProperties.clear();

			const CAdPluginIniFile::TSectionNames& names = iniFile.GetSectionNames();

			for (CAdPluginIniFile::TSectionNames::const_iterator it = names.begin(); it != names.end(); ++it)
			{
				if (it->Left(6) == L"format")
				{
					CAdPluginIniFile::TSectionData data = iniFile.GetSectionData(*it);

                    SDownloadFileProperties properties;
                    
                    properties.content = data["type"];
                    properties.extension = data["extension"];
                    properties.description = data["descriptor"];

                    m_downloadFileProperties[properties.content] = properties;
				}
			}
	    }
        s_criticalSection.Unlock();
    }
}


void CAdPluginConfig::Create()
{
	DEBUG_GENERAL("*** Creating config:" + CAdPluginSettings::GetDataPath(CONFIG_INI_FILE));

    CAdPluginConfigLock lock;
    if (lock.IsLocked())
    {
        CAdPluginIniFile iniFile(CAdPluginSettings::GetDataPath(CONFIG_INI_FILE), true);

        CAdPluginSettings* settings = CAdPluginSettings::GetInstance();

        if (iniFile.Exists() || !settings->IsMainProcess())
        {
            return;
        }

        s_criticalSection.Lock();
	    {
    	    CAdPluginIniFile::TSectionData formatFlv;

		    formatFlv["type"] = "video/x-flv";
		    formatFlv["extension"]  = "flv";
		    formatFlv["descriptor"]  = "Flash Video";

	        iniFile.UpdateSection("formatFlv", formatFlv);

    	    CAdPluginIniFile::TSectionData formatWmv;

		    formatWmv["type"] = "video/x-ms-wmv";
		    formatWmv["extension"] = "wmv";
		    formatWmv["descriptor"] = "Windows Media Video";

	        iniFile.UpdateSection("formatWmv", formatWmv);

    	    CAdPluginIniFile::TSectionData formatAsf;

		    formatAsf["type"] = "video/x-ms-asf";
		    formatAsf["extension"] = "asf";
		    formatAsf["descriptor"] = "Advanced Systems Format";

	        iniFile.UpdateSection("formatAsf", formatAsf);

// Windows Media Audio - WMA
	    }
        s_criticalSection.Unlock();

        if (iniFile.Write())
        {
            CAdPluginSettings* settings = CAdPluginSettings::GetInstance();
            
            settings->SetValue(SETTING_CONFIG_VERSION, 1);
            settings->Write();
        }
        else
        {
            DEBUG_ERROR_LOG(iniFile.GetLastError(), PLUGIN_ERROR_CONFIG, PLUGIN_ERROR_CONFIG_CREATE_FILE, "Config::Create - Write")
        }
    }
}

bool CAdPluginConfig::Download(const CStringA& url, const CStringA& filename)
{
    CStringA tempFile = CAdPluginSettings::GetTempFile(TEMP_FILE_PREFIX);

    DEBUG_GENERAL("*** Downloading config:" + filename +  " (to " + tempFile + ")");

    bool bResult = !tempFile.IsEmpty();
    if (bResult)
    {
	    // if new filter urls are found download them and update the persistent data
	    HRESULT hr = ::URLDownloadToFileA(NULL, url, tempFile, 0, NULL);
        if (SUCCEEDED(hr))
        {
            CAdPluginConfigLock lock;
            if (lock.IsLocked())
            {
                // Move the temporary file to the new text file.
                if (!::MoveFileExA(tempFile, CAdPluginSettings::GetDataPath(CONFIG_INI_FILE), MOVEFILE_REPLACE_EXISTING))
                {
                    DWORD dwError = ::GetLastError();

                    // Not same device? copy/delete instead
                    if (dwError == ERROR_NOT_SAME_DEVICE)
                    {
                        if (!::CopyFileA(tempFile, CAdPluginSettings::GetDataPath(CONFIG_INI_FILE), FALSE))
                        {
                            DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_CONFIG, PLUGIN_ERROR_CONFIG_COPY_FILE, "Config::Download - CopyFile(" + filename + ")")

                            bResult = false;
                        }

                        ::DeleteFileA(tempFile);
                    }
                    else
                    {
                        DEBUG_ERROR_LOG(dwError, PLUGIN_ERROR_CONFIG, PLUGIN_ERROR_CONFIG_MOVE_FILE, "Config::Download - MoveFileEx(" + filename + ")")

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
            DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_CONFIG, PLUGIN_ERROR_CONFIG_DOWNLOAD_FILE, "Config::Download - URLDownloadToFile(" + CStringA(CONFIG_INI_FILE) + ")");

            bResult = false;
        }
    }

    return bResult;
}


bool CAdPluginConfig::GetDownloadProperties(const CStringA& contentType, SDownloadFileProperties& properties) const
{
    bool isValid = false;

    s_criticalSection.Lock();
    {
        TDownloadFileProperties::const_iterator it = m_downloadFileProperties.find(contentType);
        if (it != m_downloadFileProperties.end())
        {
            properties = it->second;
            isValid = true;
		}
    }
    s_criticalSection.Unlock();

    return isValid;
}
