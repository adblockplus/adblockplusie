#include "AdPluginStdAfx.h"

#include "AdPluginConfig.h"
#include "AdPluginClient.h"
#include "AdPluginSettings.h"
#include "AdPluginMutex.h"


class CPluginConfigLock : public CPluginMutex
{
public:
    CPluginConfigLock() : CPluginMutex("ConfigFile", PLUGIN_ERROR_MUTEX_CONFIG_FILE) {}
    ~CPluginConfigLock() {}
};


CPluginConfig* CPluginConfig::s_instance = NULL;

CComAutoCriticalSection CPluginConfig::s_criticalSection;


CPluginConfig::CPluginConfig()
{
	DEBUG_GENERAL("*** Initializing config")

    bool isExisting = true;
    {
        CPluginConfigLock lock;
        if (lock.IsLocked())
        {
	        std::ifstream is;
	        is.open(CPluginSettings::GetDataPath(CONFIG_INI_FILE), std::ios_base::in);
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

    if (!isExisting)
    {
	    Create();
    }
	Read();
}


CPluginConfig::~CPluginConfig()
{
    s_criticalSection.Lock();
	{
		s_instance = NULL;
	}
    s_criticalSection.Unlock();
}


CPluginConfig* CPluginConfig::GetInstance() 
{
	CPluginConfig* instance = NULL;

    s_criticalSection.Lock();
	{
		if (!s_instance)
		{
			s_instance = new CPluginConfig();
		}

		instance = s_instance;
	}
    s_criticalSection.Unlock();

	return instance;
}


void CPluginConfig::Read()
{
	DEBUG_GENERAL("*** Loading config:" + CPluginSettings::GetDataPath(CONFIG_INI_FILE))

    CPluginConfigLock lock;
    if (lock.IsLocked())
    {
        CPluginIniFile iniFile(CPluginSettings::GetDataPath(CONFIG_INI_FILE), true);

        if (!iniFile.Read())
        {
            DEBUG_ERROR_LOG(iniFile.GetLastError(), PLUGIN_ERROR_CONFIG, PLUGIN_ERROR_CONFIG_READ_FILE, "Dictionary::SetLanguage - Read")
            return;
        }

        s_criticalSection.Lock();
	    {
            m_downloadFileProperties.clear();

			const CPluginIniFile::TSectionNames& names = iniFile.GetSectionNames();

			for (CPluginIniFile::TSectionNames::const_iterator it = names.begin(); it != names.end(); ++it)
			{
				if (it->Left(6) == L"format")
				{
					CPluginIniFile::TSectionData data = iniFile.GetSectionData(*it);

					CString mimeTypes = data["type"];

					int pos = 0;

					CString mimeType = mimeTypes.Tokenize(L";", pos);
					while (pos >= 0)
					{
						SDownloadFileProperties properties;

						properties.type = mimeType;
						properties.extension = data["extension"];
						properties.description = data["descriptor"];

						m_downloadFileProperties[mimeType] = properties;

						mimeType = mimeTypes.Tokenize(L";", pos);
					}
				}
			}
	    }
        s_criticalSection.Unlock();
    }
}


void CPluginConfig::Create()
{
	DEBUG_GENERAL("*** Creating config:" + CPluginSettings::GetDataPath(CONFIG_INI_FILE));

    CPluginConfigLock lock;
    if (lock.IsLocked())
    {
        CPluginIniFile iniFile(CPluginSettings::GetDataPath(CONFIG_INI_FILE), true);

        CPluginSettings* settings = CPluginSettings::GetInstance();

        if (iniFile.Exists() || !settings->IsMainProcess())
        {
            return;
        }

        s_criticalSection.Lock();
	    {
			// .asf
			{
    			CPluginIniFile::TSectionData format;

				format["type"] = "video/x-ms-asf";
				format["extension"] = "asf";
				format["descriptor"] = "Advanced Systems Format";

				iniFile.UpdateSection("formatAsf", format);
			}
			// .avi
			{
    			CPluginIniFile::TSectionData format;

				format["type"] = "video/avi;video/msvideo;video/x-msvideo";
				format["extension"] = "avi";
				format["descriptor"] = "Audio Video Interleave";

				iniFile.UpdateSection("formatAvi", format);
			}
			// .flv
			{
    			CPluginIniFile::TSectionData format;

				format["type"] = "video/x-flv";
				format["extension"] = "flv";
				format["descriptor"] = "Flash Video";

				iniFile.UpdateSection("formatFlv", format);
			}
			// .mov
			{
    			CPluginIniFile::TSectionData format;

				format["type"] = "video/quicktime";
				format["extension"] = "mov";
				format["descriptor"] = "QuickTime";

				iniFile.UpdateSection("formatMov", format);
			}
			// .mp3
			{
    			CPluginIniFile::TSectionData format;

				format["type"] = "audio/mpeg";
				format["extension"] = "mp3";
				format["descriptor"] = "MPEG-1 Audio Layer 3 (MP3)";

				iniFile.UpdateSection("formatMp3", format);
			}
			// .mp4
			{
    			CPluginIniFile::TSectionData format;

				format["type"] = "video/mp4;audio/mp4;application/mp4";
				format["extension"] = "mp4";
				format["descriptor"] = "MPEG-4 Part 14";

				iniFile.UpdateSection("formatMp4", format);
			}
			// .wav
			{
    			CPluginIniFile::TSectionData format;

				format["type"] = "audio/x-wav;audio/wav;audio/wave";
				format["extension"] = "wav";
				format["descriptor"] = "Waveform Audio";

				iniFile.UpdateSection("formatWav", format);
			}
			// .wmv
			{
    			CPluginIniFile::TSectionData format;

				format["type"] = "video/x-ms-wmv";
				format["extension"] = "wmv";
				format["descriptor"] = "Windows Media Video";

				iniFile.UpdateSection("formatWmv", format);
			}
		}
        s_criticalSection.Unlock();

        if (iniFile.Write())
        {
            CPluginSettings* settings = CPluginSettings::GetInstance();
            
            settings->SetValue(SETTING_CONFIG_VERSION, 3);
            settings->Write();
        }
        else
        {
            DEBUG_ERROR_LOG(iniFile.GetLastError(), PLUGIN_ERROR_CONFIG, PLUGIN_ERROR_CONFIG_CREATE_FILE, "Config::Create - Write")
        }
    }
}

bool CPluginConfig::Download(const CString& url, const CString& filename)
{
    CString tempFile = CPluginSettings::GetTempFile(TEMP_FILE_PREFIX);

    DEBUG_GENERAL("*** Downloading config:" + filename +  " (to " + tempFile + ")");

    bool bResult = !tempFile.IsEmpty();
    if (bResult)
    {
	    // if new filter urls are found download them and update the persistent data
	    HRESULT hr = ::URLDownloadToFile(NULL, url, tempFile, 0, NULL);
        if (SUCCEEDED(hr))
        {
            CPluginConfigLock lock;
            if (lock.IsLocked())
            {
                // Move the temporary file to the new text file.
                if (!::MoveFileEx(tempFile, CPluginSettings::GetDataPath(CONFIG_INI_FILE), MOVEFILE_REPLACE_EXISTING))
                {
                    DWORD dwError = ::GetLastError();

                    // Not same device? copy/delete instead
                    if (dwError == ERROR_NOT_SAME_DEVICE)
                    {
                        if (!::CopyFile(tempFile, CPluginSettings::GetDataPath(CONFIG_INI_FILE), FALSE))
                        {
                            DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_CONFIG, PLUGIN_ERROR_CONFIG_COPY_FILE, "Config::Download - CopyFile(" + filename + ")")

                            bResult = false;
                        }

                        ::DeleteFile(tempFile);
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
            DEBUG_ERROR_LOG(hr, PLUGIN_ERROR_CONFIG, PLUGIN_ERROR_CONFIG_DOWNLOAD_FILE, "Config::Download - URLDownloadToFile(" + CString(CONFIG_INI_FILE) + ")");

            bResult = false;
        }
    }

    return bResult;
}


bool CPluginConfig::GetDownloadProperties(const CString& contentType, SDownloadFileProperties& properties) const
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
