#include "AdPluginStdAfx.h"

#include "AdPluginSelftest.h"
#include "AdPluginClient.h"
#include "AdPluginSettings.h"
#include "AdPluginMutex.h"


#if (defined ENABLE_DEBUG_SELFTEST)


class CPluginSelftestLock : public CPluginMutex
{

private:

    static CComAutoCriticalSection s_criticalSectionSelftestLock;

public:

    CPluginSelftestLock() : CPluginMutex("SelftestFile", PLUGIN_ERROR_MUTEX_SELFTEST_FILE)
    {
        s_criticalSectionSelftestLock.Lock();
    }

    ~CPluginSelftestLock()
    {
        s_criticalSectionSelftestLock.Unlock();
    }
};

CComAutoCriticalSection CPluginSelftestLock::s_criticalSectionSelftestLock;


bool CPluginSelftest::s_isSupported = false;


CPluginSelftest::CPluginSelftest()
{
}

CPluginSelftest::~CPluginSelftest()
{
}


void CPluginSelftest::SetSupported(bool isSupported)
{
    s_isSupported = isSupported;
}


void CPluginSelftest::AddText(const CStringA& text)
{
    // Prevent circular references
    if (s_isSupported && CPluginSettings::HasInstance())
    {
	    CPluginSettings* settings = CPluginSettings::GetInstance();
    	
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

        CPluginSelftestLock lock;;
        if (lock.IsLocked())
        {
            std::ofstream selftestFile;

            selftestFile.open(CPluginSettings::GetDataPath("selftest.txt"), std::ios::app);

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

void CPluginSelftest::Clear()
{
    CPluginSelftestLock lock;;
    if (lock.IsLocked())
    {
        ::DeleteFileA(CPluginSettings::GetDataPath("selftest.txt"));
    }
}

/*
bool CPluginSelftest::Send()
{
    bool bResult;
    
    CPluginSelftestLock lock;;
    if (lock.IsLocked())
    {
	    CPluginSettings* settings = CPluginSettings::GetInstance();

        USES_CONVERSION;
        CString outputFile = CString(IEPLUGIN_VERSION) + _T("_") + A2T(settings->GetString(SETTING_PLUGIN_ID));

        CString userId = settings->GetString(SETTING_USER_ID);
        if (!userId.IsEmpty())
        {
            outputFile += _T("_") + userId.Left(4);
        }
        
        outputFile += _T(".txt");

        DEBUG_GENERAL("*** Sending selftest file:" + CStringA(outputFile));

        bResult = LocalClient::SendFtpFile(_T("ftp.ieadblocker.com"), CString(CPluginSettings::GetDataPath("selftest.txt")), outputFile);
    }

    return bResult;
}
*/

bool CPluginSelftest::Send()
{
    CPluginSettings* settings = CPluginSettings::GetInstance();

    // Move file to temp file    
    CStringA tempFile = CPluginSettings::GetTempFile(TEMP_FILE_PREFIX);

    bool bResult = !tempFile.IsEmpty();
    if (bResult)
    {
        // Move the temporary file to the new text file.
        CPluginSelftestLock lock;;
        if (lock.IsLocked())
        {
            if (!::MoveFileExA(CPluginSettings::GetDataPath("selftest.txt"), tempFile, MOVEFILE_REPLACE_EXISTING))
            {
                DWORD dwError = ::GetLastError();

                // Not same device? copy/delete instead
                if (dwError == ERROR_NOT_SAME_DEVICE)
                {
                    if (!::CopyFileA(CPluginSettings::GetDataPath("selftest.txt"), tempFile, FALSE))
                    {
                        DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SELFTEST, PLUGIN_ERROR_SELFTEST_COPY_FILE, "Selftest::Send - CopyFile")

                        bResult = false;
                    }

                    ::DeleteFileA(CPluginSettings::GetDataPath("selftest.txt"));
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

bool CPluginSelftest::IsFileTooLarge()
{
    bool isTooLarge = false;

    CPluginSelftestLock lock;
    if (lock.IsLocked())
    {
        HANDLE hFile = ::CreateFileA(CPluginSettings::GetDataPath("selftest.txt"), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
