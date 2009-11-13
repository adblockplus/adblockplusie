#ifndef _PLUGIN_INI_FILE_H_
#define _PLUGIN_INI_FILE_H_


class CPluginChecksum;


class CPluginIniFile
{

public:

    typedef std::map<CStringA, CStringA> TSectionData;
    typedef std::set<CStringA> TSectionNames;

	CPluginIniFile(const CString& filename, bool hasChecksum=false);

    CString GetFilePath() const;

    void Clear();
    bool Exists();
    bool Read();
    bool ReadString(const CStringA& memFile);
    bool Write();

	// methods to return the lists of section data and section names
	CPluginIniFile::TSectionData GetSectionData(const CStringA& section) const;

	const CPluginIniFile::TSectionNames& GetSectionNames() const;

	void SetInitialChecksumString(const CString& str);
	bool IsValidChecksum() const;

	// check if the section exists in the file
	bool HasSection(const CStringA& section) const;
    bool HasKey(const CStringA& section, const CStringA& key) const;

	// updates the key value, if key already exists, else creates a key-value pair
	bool SetValue(const CStringA& section, const CStringA& key, const CStringA& value);
    bool SetValue(const CStringA& section, const CStringA& key, int value);

    void UpdateSection(const CStringA& section, const CPluginIniFile::TSectionData& data);

	// give the value for the specified key of a section
	CStringA GetValue(const CStringA& section, const CStringA& key) const;

    unsigned int GetLastError() const;

private:

    typedef std::map<CStringA, CPluginIniFile::TSectionData> TData;

	CString m_filename;
	bool m_isDirty;
	bool m_isValidChecksum;
	bool m_hasChecksum;
	unsigned int m_lastError;

	CString m_checksumInit;
	CPluginIniFile::TData m_data;
    CPluginIniFile::TSectionNames m_sectionNames;
    
    std::auto_ptr<CPluginChecksum> m_checksum;
};

#endif // _PLUGIN_INI_FILE_H_
