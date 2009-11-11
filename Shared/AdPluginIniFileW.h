#ifndef _PLUGIN_INI_FILE_W_H_
#define _PLUGIN_INI_FILE_W_H_

#if _MSC_VER > 1000
 #pragma once
#endif // _MSC_VER > 1000


class CPluginChecksum;


class CPluginIniFileW
{

public:

    typedef std::map<CStringW, CStringW> TSectionData;
    typedef std::set<CStringW> TSectionNames;

	CPluginIniFileW(const CStringA& filename, bool hasChecksum=false);

    CStringA GetFilePath() const;

    void Clear();
    bool Read();
    bool ReadString(const CStringW& memFile);
    bool Write();
    bool Exists();

	// methods to return the lists of section data and section names
	CPluginIniFileW::TSectionData GetSectionData(const CStringW& section) const;

	const CPluginIniFileW::TSectionNames& GetSectionNames() const;

	void SetInitialChecksumString(const CStringA& str);
	bool IsValidChecksum() const;

	// check if the section exists in the file
	bool HasSection(const CStringW& section) const;
    bool HasKey(const CStringW& section, const CStringW& key) const;

	// updates the key value, if key already exists, else creates a key-value pair
	bool SetValue(const CStringW& section, const CStringW& key, const CStringW& value);
    bool SetValue(const CStringW& section, const CStringW& key, int value);

    void UpdateSection(const CStringW& section, const CPluginIniFileW::TSectionData& data);

	// give the value for the specified key of a section
	CStringW GetValue(const CStringW& section, const CStringW& key) const;

    unsigned int GetLastError() const;

private:

    typedef std::map<CStringW, CPluginIniFileW::TSectionData> TData;

	CStringA m_filename;
	bool m_isDirty;
	bool m_isValidChecksum;
	bool m_hasChecksum;
	unsigned int m_lastError;

	CStringA m_checksumInit;
	CPluginIniFileW::TData m_data;
    CPluginIniFileW::TSectionNames m_sectionNames;
    
    std::auto_ptr<CPluginChecksum> m_checksum;
};


#endif // _PLUGIN_INI_FILE_W_H_
