#include "PluginStdAfx.h"

#include "PluginIniFileW.h"
#include "PluginChecksum.h"

#if (defined PRODUCT_SIMPLEADBLOCK)
 #include "PluginClient.h"
#endif


CPluginIniFileW::CPluginIniFileW(const CString& filename, bool hasChecksum) : 
    m_isValidChecksum(false), m_isDirty(false), m_filename(filename), m_hasChecksum(hasChecksum), m_lastError(0)
{
    m_checksum = std::auto_ptr<CPluginChecksum>(new CPluginChecksum());
}

void CPluginIniFileW::SetInitialChecksumString(const CString& str)
{
	m_checksumInit = str;
}

CString CPluginIniFileW::GetFilePath() const
{
    return m_filename;
}


void CPluginIniFileW::Clear()
{
    m_data.clear();
    m_sectionNames.clear();
	m_checksumInit.Empty();
    m_checksum->Clear();
}

bool CPluginIniFileW::Exists()
{
    bool isExisting = false;
    
    HANDLE hFile = ::CreateFile(m_filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);  
    if (hFile != INVALID_HANDLE_VALUE)
    {
        isExisting = true;
        ::CloseHandle(hFile);
    }

    return isExisting;
}

bool CPluginIniFileW::ReadString(const CStringW& memFile)
{
    bool isOk = true;

    DWORD checksumValue = 0;

    m_checksum->Clear();
	m_checksum->Add(m_checksumInit);

    CPluginIniFileW::TSectionData sectionData;
    CStringW sectionName;

    bool bHasSection = false;
        
    // Parse file string
    int pos = 0;
    CStringW line = memFile.Tokenize(L"\r\n", pos);

    // Remove initial unicode-chars
    if (line.GetAt(0) == 0xfeff)
    {
        line = line.Right(line.GetLength() - 1);
    }
	else
	{
	}

    while (pos >= 0)
    {
	    line.Trim();

	    if (!line.IsEmpty())
	    {
	        // Comment
	        if (line.GetAt(0) == L'#')
	        {
	        }
	        // Section start
	        else if (line.GetAt(0) == L'[' && line.GetAt(line.GetLength() - 1) == L']')
	        {
	            if (bHasSection)
	            {
	                m_data[sectionName] = sectionData;
	                sectionData.clear();
	            }
		        
	            // Add section name to list
	            sectionName = line.Mid(1, line.GetLength() - 2);

                DEBUG_INI("Ini::Read section:" + sectionName)

                // Add to checksum
                if (m_hasChecksum && sectionName != "Checksum")
                {
                    m_checksum->Add(sectionName);
                }

                m_sectionNames.insert(sectionName);
		        
	            bHasSection = true;
	        }
	        // Section data
	        else if (bHasSection)
	        {
	            int pos = 0;
	            if ((pos = line.Find(L'=')) > 0)
	            {
	                CStringW key = line.Left(pos).Trim();
	                CStringW value = line.Right(line.GetLength() - pos - 1).Trim();

                    if (!key.IsEmpty())
                    {
		                sectionData[key] = value;
                    }

                    if (m_hasChecksum && sectionName != "Checksum")
                    {
                        m_checksum->Add(key);
                        m_checksum->Add(value);
                    }
	            }
                else if (m_hasChecksum && sectionName == "Checksum")
                {
                    checksumValue = (DWORD)_wtol(line);
                }
	        }
	        else
	        {
                DEBUG_INI("Ini::Read ignoring line::" + CString(line))
            }
	    }

        line = memFile.Tokenize(L"\r\n", pos);
    }

    // Add final section
    if (bHasSection)
    {
        m_data[sectionName] = sectionData;
    }

    m_isValidChecksum = (checksumValue == m_checksum->Get());

    return isOk;
}


bool CPluginIniFileW::Read()
{
    bool isRead = true;
    
    m_lastError = 0;

    // Read file
    HANDLE hFile = ::CreateFile(m_filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);  
    if (hFile == INVALID_HANDLE_VALUE)
    {
        m_lastError = ::GetLastError();
        isRead = false;
    }
    else
    {
        DEBUG_INI("Ini::Reading file " + m_filename);

        // Read file
        WCHAR buffer[1026];
        LPVOID pBuffer = buffer;
        LPBYTE pByteBuffer = (LPBYTE)pBuffer;
        DWORD dwBytesRead = 0;
        CStringW fileContent;
        BOOL bRead = TRUE;       
        while ((bRead = ::ReadFile(hFile, pBuffer, 1024, &dwBytesRead, NULL)) == TRUE && dwBytesRead > 0)
        {
            pByteBuffer[dwBytesRead] = 0;
            pByteBuffer[dwBytesRead+1] = 0;

            fileContent += buffer;
        }

        // Read error        
        if (!bRead)
        {
            m_lastError = ::GetLastError();
            isRead = false;
        }

        // Close file
        ::CloseHandle(hFile);
        
        // Parse file
        if (isRead)
        {
            isRead = ReadString(fileContent);
        }
    }

	return isRead;
}


bool CPluginIniFileW::Write()
{
    bool isWritten = true;
    
    m_lastError = 0;

    DEBUG_INI("Ini::Write " + m_filename);

    // Save file
    if (m_isDirty)
    {
        DEBUG_INI("Ini::Writing file " + m_filename);

        m_checksum->Clear();

        // Create file
        HANDLE hFile = ::CreateFile(m_filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);  
        if (hFile == INVALID_HANDLE_VALUE)
        {
            m_lastError = ::GetLastError();
            isWritten = false;
        }
        else
        {
            CStringW line;

            // Write warning
            if (m_hasChecksum)
            {
                line += "# Please do not edit this file!\r\n\r\n";;
            }

            for (CPluginIniFileW::TData::iterator it = m_data.begin(); it != m_data.end(); ++it)
            {
                line += L"[" + it->first + L"]\r\n";
                
                m_checksum->Add(it->first);

                for (CPluginIniFileW::TSectionData::iterator itValues = it->second.begin(); itValues != it->second.end(); ++itValues)
                {
                    line += itValues->first + L"=" + itValues->second + L"\r\n";

                    m_checksum->Add(itValues->first);
                    m_checksum->Add(itValues->second);
                }

                line += L"\r\n";
            }
            
            // Add checksum
            if (m_hasChecksum)
            {
                line += L"[Checksum]\r\n";

                CStringW checksum;
                checksum.Format(L"%lu\r\n", m_checksum->Get());
                
                line += checksum;
            }

            // Write file            
            DWORD dwBytesWritten = 0;
            if (::WriteFile(hFile, line.GetBuffer(), line.GetLength()*2, &dwBytesWritten, NULL) && dwBytesWritten == line.GetLength()*2)
            {
                m_isDirty = false;

                Clear();
            }
            else
            {
                m_lastError = ::GetLastError();
                isWritten = false;
            }

            // Close file
            ::CloseHandle(hFile);
        }
    }

    return isWritten;
}


// Used to retrieve a value give the section and key
CStringW CPluginIniFileW::GetValue(const CStringW& section, const CStringW& key) const
{
    CStringW value;

    TSectionData sectionData = GetSectionData(section);
    
    TSectionData::iterator it = sectionData.find(key);
    if (it != sectionData.end())
    {
        value = it->second;
    }

	return value;
}


// Used to add or set a key value pair to a section
bool CPluginIniFileW::SetValue(const CStringW& section, const CStringW& key, const CStringW& value)
{
    bool isSet = false;
    
    CPluginIniFileW::TData::iterator it = m_data.find(section);
    if (it != m_data.end())
    {
        CPluginIniFileW::TSectionData::iterator itValue = it->second.find(key);
        if (itValue != it->second.end())
        {
            if (itValue->second != value)
            {
                m_isDirty = true;
            }
        }
        
        it->second[key] = value;
        isSet = true;
    }
    
    return isSet;
}


// Used to add or set a key value pair to a section
bool CPluginIniFileW::SetValue(const CStringW& section, const CStringW& key, int value)
{
	CStringW valueStr;
	valueStr.Format(L"%d", value);
	
	return SetValue(section, key, valueStr);
}


// Used to find out if a given section exists
bool CPluginIniFileW::HasSection(const CStringW& section) const
{    
	return m_sectionNames.find(section) != m_sectionNames.end();
}


// Used to find out if a given key exists
bool CPluginIniFileW::HasKey(const CStringW& section, const CStringW& key) const
{
    return !GetValue(section, key).IsEmpty();
}


void CPluginIniFileW::UpdateSection(const CStringW& section, const CPluginIniFileW::TSectionData& data)
{
    m_data[section] = data;
    m_isDirty = true;
}


// Used to retrieve all of the  section names in the ini file
const CPluginIniFileW::TSectionNames& CPluginIniFileW::GetSectionNames() const
{
    return m_sectionNames;
}


// Used to retrieve all key/value pairs of a given section.  
CPluginIniFileW::TSectionData CPluginIniFileW::GetSectionData(const CStringW& section) const
{
    CPluginIniFileW::TSectionData sectionData;
    
    CPluginIniFileW::TData::const_iterator it = m_data.find(section);
    if (it != m_data.end())
    {
        sectionData = it->second;
    }

	return sectionData;
}

bool CPluginIniFileW::IsValidChecksum() const
{
    return m_isValidChecksum && m_hasChecksum || !m_hasChecksum;
}

unsigned int CPluginIniFileW::GetLastError() const
{
    return m_lastError;
}