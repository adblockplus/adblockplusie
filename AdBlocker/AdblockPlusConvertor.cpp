#include "PluginStdAfx.h"
#include "AdblockPlusConvertor.h"
#include "list"


AdblockPlusConvertor::AdblockPlusConvertor()
{

}

bool AdblockPlusConvertor::Convert(CString abpFilterPath, CString outputPath)
{
	CStdioFile input;
	CStdioFile output;


    // Check file existence
    std::ifstream is;
    is.open(outputPath, std::ios_base::in);
    if (is.is_open())
    {
        is.close();
        return false;
    }
	
	input.Open(abpFilterPath, CFile::modeRead | CFile::shareDenyNone);
	CString line = "";
	bool firstLine = true;
	std::list<CString> domains;
	std::list<CString> whiteDomains;
	std::list<CString> domainsCopy;
	while (input.ReadString(line))
	{
		CString origLine = line;
		//is this a hiding rule
		int elemIndex = line.Find(L"##", 0);
		if (elemIndex >= 0)
		{
			if (output.m_hFile == INVALID_HANDLE_VALUE)
			{
				output.Open(outputPath, CFile::modeWrite | CFile::modeCreate | CFile::shareExclusive);
			}

			//First we add all rules without domain exceptions
			if (elemIndex > 0)
			{
				int iStart = 0;
				while (iStart >= 0)
				{
					CString domain = line.Tokenize(L",#", iStart);
					if (domain.Find(L"~") == 0)
					{
						domain = domain.Right(domain.GetLength() - 1);
						domains.push_back(domain);
					}
					else
					{
						whiteDomains.push_back(domain);
						int whyAreWeHereWeShouldNotBeHere = 0;
					}
				}
				line = line.Right(line.GetLength() - elemIndex);
			}
			line.Replace(L"##", L"");
			int r = line.FindOneOf(L"_-!@$^&*()+~1234567890");
			if ((line.FindOneOf(L"_-!@$^&*()+~1234567890") > 1) || (line.FindOneOf(L"_-!@$^&*()+~1234567890") < 0))
			{
				if ((line.Find(L":") < 0) || (line.Find(L"http://")) > 0)
				{
					if (firstLine)
					{
						output.WriteString(line + L", \n");
					}
					else
					{
						output.WriteString(L",\n" + line + L",\n");
					}
				}
			}
		}
	}
	if (output.m_hFile != INVALID_HANDLE_VALUE)
	{
		output.WriteString(L"{ display: none !important }\n");
	}

	input.SeekToBegin();
	//Now let's add exceptions selector
	int nsIndex = 0;
	domainsCopy = domains;
	while (domains.size() > 0)
	{
		CString d = domains.front();
		WCHAR tmp[1024];
		wsprintf(tmp, L"@namespace sa%d url(%s);\n", nsIndex, d);
		output.WriteString(tmp);

		domains.pop_front();
		nsIndex ++;
	}

	nsIndex = 0;
	while (input.ReadString(line))
	{
		int elemIndex = line.Find(L"##", 0);
		//First we add all rules without domain exceptions
		if (elemIndex > 0)
		{
			int iStart = 0;
			while (iStart >= 0)
			{
				CString domain = line.Tokenize(L",#", iStart);
				int hideElPos = line.Find(L"##", 0) + 2;
				CString hideEl = line.Right(line.GetLength() - hideElPos);
				if (domain.Find(L"~") == 0)
				{
					if (hideEl.FindOneOf(L"_-!@#$^&*()+~1234567890") != 0)
					{
						WCHAR tmp[1024];
						wsprintf(tmp, L"sa%d|%s,\n", nsIndex, hideEl);
						output.WriteString(tmp);
						nsIndex ++;
					}
				}				
			}
			line = line.Right(line.GetLength() - elemIndex);
		}
	}
	if (output.m_hFile != INVALID_HANDLE_VALUE)
	{
		output.WriteString(L"{ display: inline !important }");
		output.Close();
	}
	input.Close();
	return true;
}
