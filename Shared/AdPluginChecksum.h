#ifndef _PLUGIN_CHECKSUM_H_
#define _PLUGIN_CHECKSUM_H_


class CPluginChecksum 
{

public:

    CPluginChecksum();
    ~CPluginChecksum();

    void Clear();

    DWORD Get() const;
	CString GetAsString() const;

    void Add(const CStringA& s);
    void Add(const CStringW& s);
    void Add(const CStringA& s1, const CStringA& s2);
    void Add(const CStringW& s1, const CStringW& s2);
    void Add(BYTE b);

protected:

    WORD m_r;
    WORD m_c1;
    WORD m_c2;
    DWORD m_sum;
};


#endif // _PLUGIN_CHECKSUM_H_
