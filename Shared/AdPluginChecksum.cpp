#include "AdPluginStdAfx.h"

#include "AdPluginChecksum.h"
#include "AdPluginClient.h"


CAdPluginChecksum::CAdPluginChecksum()
{
    Clear();
}


CAdPluginChecksum::~CAdPluginChecksum()
{
}


void CAdPluginChecksum::Clear() 
{ 
    m_sum = 0; 
    m_r = 55665; 
    m_c1 = 52845; 
    m_c2 = 22719;
}


DWORD CAdPluginChecksum::Get() const
{ 
    return m_sum; 
}


CStringA CAdPluginChecksum::GetAsString() const
{ 
	CStringA checksum;

	checksum.Format("%lu", Get());

	return checksum;
}


void CAdPluginChecksum::Add(BYTE value)
{
    if (value)
    {
        BYTE cipher = (value ^ (m_r >> 8));
        m_r = (cipher + m_r) * m_c1 + m_c2;
        m_sum += cipher;
    }

/*
    CString r;
    r.Format(L"%d", m_r);

    CString cip;
    cip.Format(L"%d", cipher);

    CString v;
    v.Format(L"%d", value);
    
    DEBUG_THREAD("val:" + v + " r:" + r + " cipher:" + cip)
*/
}


void CAdPluginChecksum::Add(const CStringA& s)
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

void CAdPluginChecksum::Add(const CStringW& s)
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

void CAdPluginChecksum::Add(const CStringA& s1, const CStringA& s2)
{
    if (!s1.IsEmpty() && !s2.IsEmpty())
    {
	    Add(s1);
	    Add(s2);
    }
}

void CAdPluginChecksum::Add(const CStringW& s1, const CStringW& s2)
{
    if (!s1.IsEmpty() && !s2.IsEmpty())
    {
	    Add(s1);
	    Add(s2);
    }
}
