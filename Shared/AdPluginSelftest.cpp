#include "AdPluginStdAfx.h"

#include "AdPluginSelftest.h"
#include "AdPluginClient.h"
#include "AdPluginSettings.h"
#include "AdPluginMutex.h"


#if (defined ENABLE_DEBUG_SELFTEST)


class CAdPluginSelftestLock : public CAdPluginMutex
{

private:

    static CComAutoCriticalSection s_criticalSectionSelftestLock;

public:

    CAdPluginSelftestLock() : CAdPluginMutex("SelftestFile", PLUGIN_ERROR_MUTEX_SELFTEST_FILE)
    {
        s_criticalSectionSelftestLock.Lock();
    }

    ~CAdPluginSelftestLock()
    {
        s_criticalSectionSelftestLock.Unlock();
    }
};

CComAutoCriticalSection CAdPluginSelftestLock::s_criticalSectionSelftestLock;


bool CAdPluginSelftest::s_isSupported = false;


CAdPluginSelftest::CAdPluginSelftest()
{
}

CAdPluginSelftest::~CAdPluginSelftest()
{
}


void CAdPluginSelftest::SetSupported(bool isSupported)
{
    s_isSupported = isSupported;
}


void CAdPluginSelftest::AddText(const CStringA& text)
{
    // Prevent circular references
    if (s_isSupported && CAdPluginSettings::HasInstance())
    {
	    CAdPluginSettings* settings = CAdPluginSettings::GetInstance();
    	
        CStringA processor;
        CStringA thread;
        
        if (settings->IsMainProcess())
        {
            processor = "MAIN.";
        }
        else
        {
            processor.Format("%4.4u.", ::GetCurrentProcessId());
        }

        thread.Format("%4.4u - ", ::GetCurrentThreadId());

        SYSTEMTIME st;    
        ::GetSystemTime(&st);

        CStringA sysTime;
        sysTime.Format("%2.2d:%2.2d:%2.2d.%3.3d - ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        CAdPluginSelftestLock lock;;
        if (lock.IsLocked())
        {
            std::ofstream selftestFile;

            selftestFile.open(CAdPluginSettings::GetDataPath("selftest.txt"), std::ios::app);

            int pos = 0;
            CStringA line = text.Tokenize("\n\r", pos);

            while (pos >= 0)
            {
                selftestFile.write(sysTime.GetBuffer(), sysTime.GetLength());
                selftestFile.write(processor.GetBuffer(), processor.GetLength());
                selftestFile.write(thread.GetBuffer(), thread.GetLength());

                selftestFile.write(line.GetBuffer(), line.GetLength());
                selftestFile.write("\n", 1);
            
                line = text.Tokenize("\n\r", pos);
            }

            selftestFile.flush();
        }
    }
}

void CAdPluginSelftest::Clear()
{
    CAdPluginSelftestLock lock;;
    if (lock.IsLocked())
    {
        ::DeleteFileA(CAdPluginSettings::GetDataPath("selftest.txt"));
    }
}

/*
bool CAdPluginSelftest::Send()
{
    bool bResult;
    
    CAdPluginSelftestLock lock;;
    if (lock.IsLocked())
    {
	    CAdPluginSettings* settings = CAdPluginSettings::GetInstance();

        USES_CONVERSION;
        CString outputFile = CString(IEPLUGIN_VERSION) + _T("_") + A2T(settings->GetString(SETTING_PLUGIN_ID));

        CString userId = settings->GetString(SETTING_USER_ID);
        if (!userId.IsEmpty())
        {
            outputFile += _T("_") + userId.Left(4);
        }
        
        outputFile += _T(".txt");

        DEBUG_GENERAL("*** Sending selftest file:" + CStringA(outputFile));

        bResult = LocalClient::SendFtpFile(_T("ftp.ieadblocker.com"), CString(CAdPluginSettings::GetDataPath("selftest.txt")), outputFile);
    }

    return bResult;
}
*/

bool CAdPluginSelftest::Send()
{
    CAdPluginSettings* settings = CAdPluginSettings::GetInstance();

    // Move file to temp file    
    CStringA tempFile = CAdPluginSettings::GetTempFile(TEMP_FILE_PREFIX);

    bool bResult = !tempFile.IsEmpty();
    if (bResult)
    {
        // Move the temporary file to the new text file.
        CAdPluginSelftestLock lock;;
        if (lock.IsLocked())
        {
            if (!::MoveFileExA(CAdPluginSettings::GetDataPath("selftest.txt"), tempFile, MOVEFILE_REPLACE_EXISTING))
            {
                DWORD dwError = ::GetLastError();

                // Not same device? copy/delete instead
                if (dwError == ERROR_NOT_SAME_DEVICE)
                {
                    if (!::CopyFileA(CAdPluginSettings::GetDataPath("selftest.txt"), tempFile, FALSE))
                    {
                        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SELFTEST, PLUGIN_ERROR_SELFTEST_COPY_FILE, "Selftest::Send - CopyFile")

                        bResult = false;
                    }

                    ::DeleteFileA(CAdPluginSettings::GetDataPath("selftest.txt"));
                }
                else
                {
                    DEBUG_ERROR_LOG(dwError, PLUGIN_ERROR_SELFTEST, PLUGIN_ERROR_SELFTEST_MOVE_FILE, "Selftest::Send - MoveFileEx")

                    bResult = false;
                }
            }
        }
        else
        {
            bResult = false;
        }
    }
    
    if (bResult)
    {
        USES_CONVERSION;
        CString outputFile = CString(IEPLUGIN_VERSION) + _T("_") + A2T(settings->GetString(SETTING_PLUGIN_ID));

        CString userId = settings->GetString(SETTING_USER_ID);
        if (!userId.IsEmpty())
        {
            outputFile += _T("_") + userId.Left(4);
        }
        
        outputFile += _T(".txt");

        DEBUG_GENERAL("*** Sending selftest file:" + CStringA(outputFile));

        bResult = LocalClient::SendFtpFile(_T("ftp.ieadblocker.com"), CString(tempFile), outputFile);
    }
    
    return bResult;
}

bool CAdPluginSelftest::IsFileTooLarge()
{
    bool isTooLarge = false;

    CAdPluginSelftestLock lock;
    if (lock.IsLocked())
    {
        HANDLE hFile = ::CreateFileA(CAdPluginSettings::GetDataPath("selftest.txt"), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile)
        {
            LARGE_INTEGER liFileSize;
            if (::GetFileSizeEx(hFile, &liFileSize))
            {
                isTooLarge = liFileSize.LowPart > 10000;
            }
            
            ::CloseHandle(hFile);
        }
    }

    return isTooLarge; 
}


#endif // ENABLE_DEBUG_SELFTEST
