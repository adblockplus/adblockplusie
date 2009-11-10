#ifndef _ADPLUGIN_INI_FILE_H_
#define _ADPLUGIN_INI_FILE_H_

#if _MSC_VER > 1000
 #pragma once
#endif // _MSC_VER > 1000


class CAdPluginChecksum;


class CAdPluginIniFile
{

public:

    typedef std::map<CStringA, CStringA> TSectionData;
    typedef std::set<CStringA> TSectionNames;

	CAdPluginIniFile(const CStringA& filename, bool hasChecksum=false);

    CStringA GetFilePath() const;

    void Clear();
    bool Exists();
    bool Read();
    bool ReadString(const CStringA& memFile);
    bool Write();

	// methods to return the lists of section data and section names
	CAdPluginIniFile::TSectionData GetSectionData(const CStringA& section) const;

	const CAdPluginIniFile::TSectionNames& GetSectionNames() const;

	void SetInitialChecksumString(const CStringA& str);
	bool IsValidChecksum() const;

	// check if the section exists in the file
	bool HasSection(const CStringA& section) const;
    bool HasKey(const CStringA& section, const CStringA& key) const;

	// updates the key value, if key already exists, else creates a key-value pair
	bool SetValue(const CStringA& section, const CStringA& key, const CStringA& value);
    bool SetValue(const CStringA& section, const CStringA& key, int value);

    void UpdateSection(const CStringA& section, const CAdPluginIniFile::TSectionData& data);

	// give the value for the specified key of a section
	CStringA GetValue(const CStringA& section, const CStringA& key) const;

    unsigned int GetLastError() const;

private:

    typedef std::map<CStringA, CAdPluginIniFile::TSectionData> TData;

	CStringA m_filename;
	bool m_isDirty;
	bool m_isValidChecksum;
	bool m_hasChecksum;
	unsigned int m_lastError;

	CStringA m_checksumInit;
	CAdPluginIniFile::TData m_data;
    CAdPluginIniFile::TSectionNames m_sectionNames;
    
    std::auto_ptr<CAdPluginChecksum> m_checksum;
};

#endif // _ADPLUGIN_INI_FILE_H_
