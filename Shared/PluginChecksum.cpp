#include "PluginStdAfx.h"

#include "PluginChecksum.h"
#include "PluginClient.h"


CPluginChecksum::CPluginChecksum()
{
    Clear();
}


CPluginChecksum::~CPluginChecksum()
{
}


void CPluginChecksum::Clear() 
{ 
    m_sum = 0; 
    m_r = 55665; 
    m_c1 = 52845; 
    m_c2 = 22719;
}


DWORD CPluginChecksum::Get() const
{ 
    return m_sum; 
}


CString CPluginChecksum::GetAsString() const
{ 
	CString checksum;

	checksum.Format(L"%lu", Get());

	return checksum;
}


void CPluginChecksum::Add(BYTE value)
{
    if (value)
    {
        BYTE cipher = (value ^ (m_r >> 8));
        m_r = (cipher + m_r) * m_c1 + m_c2;
        m_sum += cipher;
    }
}


void CPluginChecksum::Add(const CStringA& s)
{
    for (int i = 0; i < s.GetLength(); i++)
    {
        Add((BYTE)s.GetAt(i));
    }

#ifdef ENABLE_DEBUG_CHECKSUM
    CStringA sum;
    sum.Format("%d", m_sum);

	DEBUG_CHECKSUM("Checksum::AddString " + s + " sum:" + sum)
#endif
}

void CPluginChecksum::Add(const CStringW& s)
{
    for (int i = 0; i < s.GetLength(); i++)
    {
        WORD value = (WORD)s.GetAt(i);
        
        Add(LOBYTE(value));
        Add(HIBYTE(value));
    }

#ifdef ENABLE_DEBUG_CHECKSUM
    CStringA sum;
    sum.Format("%d", m_sum);

	DEBUG_CHECKSUM("Checksum::AddString " + s + " sum:" + sum)
#endif
}

void CPluginChecksum::Add(const CStringA& s1, const CStringA& s2)
{
    if (!s1.IsEmpty() && !s2.IsEmpty())
    {
	    Add(s1);
	    Add(s2);
    }
}

void CPluginChecksum::Add(const CStringW& s1, const CStringW& s2)
{
    if (!s1.IsEmpty() && !s2.IsEmpty())
    {
	    Add(s1);
	    Add(s2);
    }
}
