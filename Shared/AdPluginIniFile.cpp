#include "AdPluginStdAfx.h"

#include "AdPluginIniFile.h"
#include "AdPluginChecksum.h"

#if (defined PRODUCT_ADBLOCKER)
 #include "AdPluginClient.h"
#endif


CPluginIniFile::CPluginIniFile(const CString& filename, bool hasChecksum) : 
    m_isValidChecksum(false), m_isDirty(false), m_filename(filename), m_hasChecksum(hasChecksum), m_lastError(0)
{
    m_checksum = std::auto_ptr<CPluginChecksum>(new CPluginChecksum());
}

void CPluginIniFile::SetInitialChecksumString(const CString& str)
{
	m_checksumInit = str;
}

CString CPluginIniFile::GetFilePath() const
{
    return m_filename;
}


void CPluginIniFile::Clear()
{
    m_data.clear();
    m_sectionNames.clear();
	m_checksumInit.Empty();
    m_checksum->Clear();
}


bool CPluginIniFile::Exists()
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


bool CPluginIniFile::ReadString(const CStringA& memFile)
{
    bool isOk = true;

    DWORD checksumValue = 0;

    m_checksum->Clear();
	m_checksum->Add(m_checksumInit);

    // Read file
    std::string buffer(memFile);

    std::istringstream is;
    is.str(buffer);

    CPluginIniFile::TSectionData sectionData;
    CStringA sectionName;

    bool bHasSection = false;

    while (isOk && !is.eof())
    {
	    char szLine[256];
	    is.getline(szLine, 255);
	    
	    if (is.fail() && !is.eof())
	    {
	        is.clear();
	        isOk = false;
	        break;
	    }

	    CStringA line(szLine);
	    line.Trim();

	    if (!line.IsEmpty())
	    {
	        // Comment
	        if (line.GetAt(0) == '#')
	        {
	        }
	        // Section start
	        else if (line.GetAt(0) == '[' && line.GetAt(line.GetLength() - 1) == ']')
	        {
	            if (bHasSection)
	            {
	                m_data[sectionName] = sectionData;
	                sectionData.clear();
	            }
		        
	            // Add section name to list
	            sectionName = line.Mid(1, line.GetLength() - 2);

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
	            if ((pos = line.Find('=')) > 0)
	            {
	                CStringA key = line.Left(pos).Trim();
	                CStringA value = line.Right(line.GetLength() - pos - 1).Trim();

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
                    checksumValue = (DWORD)atol(line);
                }
	        }
	    }
    }

    // Add final section
    if (bHasSection)
    {
        m_data[sectionName] = sectionData;
    }

    m_isValidChecksum = checksumValue == m_checksum->Get();

    return isOk;
}


bool CPluginIniFile::Read()
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

        DWORD checksumValue = 0;        
        m_checksum->Clear();

        CPluginIniFile::TSectionData sectionData;
        CStringA sectionName;

        bool bHasSection = false;
        
        // Read file
        char buffer[8194];
        LPVOID pBuffer = buffer;
        LPBYTE pByteBuffer = (LPBYTE)pBuffer;
        DWORD dwBytesRead = 0;
        CStringA fileContent;
        BOOL bRead = TRUE;
        while ((bRead = ::ReadFile(hFile, pBuffer, 8192, &dwBytesRead, NULL)) == TRUE && dwBytesRead > 0)
        {
            pByteBuffer[dwBytesRead] = 0;

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
        
        // Parse the file
        isRead = ReadString(fileContent);
    }

	return isRead;
}


bool CPluginIniFile::Write()
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
            CStringA line;

            // Write warning
            if (m_hasChecksum)
            {
                line += "# Please do not edit this file!\r\n\r\n";;
            }

            for (CPluginIniFile::TData::iterator it = m_data.begin(); it != m_data.end(); ++it)
            {
                line += "[" + it->first + "]\r\n";

                m_checksum->Add(it->first);

                for (CPluginIniFile::TSectionData::iterator itValues = it->second.begin(); itValues != it->second.end(); ++itValues)
                {
                    line += itValues->first + "=" + itValues->second + "\r\n";

                    m_checksum->Add(itValues->first);
                    m_checksum->Add(itValues->second);
                }

                line += "\r\n";
            }
            
            // Add checksum
            if (m_hasChecksum)
            {
                line += "[Checksum]\r\n";

                CStringA checksum;
                checksum.Format("%lu\r\n", m_checksum->Get());
                
                line += checksum;
            }
            
            // Write file            
            DWORD dwBytesWritten = 0;
            if (::WriteFile(hFile, line.GetBuffer(), line.GetLength(), &dwBytesWritten, NULL) && dwBytesWritten == line.GetLength())
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
CStringA CPluginIniFile::GetValue(const CStringA& section, const CStringA& key) const
{
    CStringA value;

    TSectionData sectionData = GetSectionData(section);
    
    TSectionData::iterator it = sectionData.find(key);
    if (it != sectionData.end())
    {
        value = it->second;
    }

	return value;
}


// Used to add or set a key value pair to a section
bool CPluginIniFile::SetValue(const CStringA& section, const CStringA& key, const CStringA& value)
{
    bool isSet = false;
    
    CPluginIniFile::TData::iterator it = m_data.find(section);
    if (it != m_data.end())
    {
        CPluginIniFile::TSectionData::iterator itValue = it->second.find(key);
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
bool CPluginIniFile::SetValue(const CStringA& section, const CStringA& key, int value)
{
	CStringA valueStr;
	valueStr.Format("%d", value);
	
	return SetValue(section, key, valueStr);
}


// Used to find out if a given section exists
bool CPluginIniFile::HasSection(const CStringA& section) const
{
	return m_sectionNames.find(section) != m_sectionNames.end();
}


// Used to find out if a given key exists
bool CPluginIniFile::HasKey(const CStringA& section, const CStringA& key) const
{
    return !GetValue(section, key).IsEmpty();
}


void CPluginIniFile::UpdateSection(const CStringA& section, const CPluginIniFile::TSectionData& data)
{
    m_data[section] = data;
    m_isDirty = true;
}


// Used to retrieve all of the  section names in the ini file
const CPluginIniFile::TSectionNames& CPluginIniFile::GetSectionNames() const
{
    return m_sectionNames;
}


// Used to retrieve all key/value pairs of a given section.  
CPluginIniFile::TSectionData CPluginIniFile::GetSectionData(const CStringA& section) const
{
    CPluginIniFile::TSectionData sectionData;
    
    CPluginIniFile::TData::const_iterator it = m_data.find(section);
    if (it != m_data.end())
    {
        sectionData = it->second;
    }

	return sectionData;
}

bool CPluginIniFile::IsValidChecksum() const
{
    return m_isValidChecksum && m_hasChecksum || !m_hasChecksum;
}

unsigned int CPluginIniFile::GetLastError() const
{
    return m_lastError;
}