#include "PluginStdAfx.h"
#include "Locale.h"
#include "PluginSettings.h"
#include <map>

using namespace std;

static map<CString, CString> s_keyValue;

static bool sReadLocale()
{
    LANGID lcid = ::GetUserDefaultLangID();

    TCHAR language[128] = {0};
	if (!::GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, language, countof(language) - 1))
    {
        return false;
    }

    TCHAR country[128] = {0};
	if (!::GetLocaleInfo(lcid, LOCALE_SISO3166CTRYNAME, country, countof(country) - 1))
    {
        return false;
    }

    CString fLocale = CString(language) + "-" + CString(country) + ".locale.txt";

	std::ifstream is;
	is.open(CPluginSettings::GetDataPath(fLocale), std::ios_base::in);
	if (is.is_open())
	{
        char buf[4096];
        while (is.good()) 
        {
	        is.getline(buf, sizeof buf);

            char* pEQ = strstr(buf, "=");
            if (pEQ)
            {
                *pEQ = '\0';

                CString key = buf;
                CString value = pEQ + 1;

                s_keyValue.insert(make_pair(key, value));
            }
	    }

        return true;
	}
	else
	{
        return false;
	}
}


CString GetLocaleValue(const CString& key)
{
    // Don't need critical section
    static bool s_readLocale = sReadLocale(); 

    map<CString, CString>::const_iterator it = s_keyValue.find(key);
    if (s_keyValue.end() == it)
        return "";

    return it->second;
}