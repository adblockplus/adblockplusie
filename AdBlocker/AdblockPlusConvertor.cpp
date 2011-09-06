#include "PluginStdAfx.h"
#include "AdblockPlusConvertor.h"


AdblockPlusConvertor::AdblockPlusConvertor()
{

}

bool AdblockPlusConvertor::Convert(CString abpFilterPath, CString outputPath)
{
	CStdioFile input;
	CStdioFile output;
	input.Open(abpFilterPath, CFile::modeRead | CFile::shareDenyNone);
	CString line = "";
	bool firstLine = true;
	while (input.ReadString(line))
	{
		CString origLine = line;
		//is this a hiding rule
		int elemIndex = line.Find(L"###", 0);
		if (elemIndex >= 0)
		{
			if ((line.Find(L"~") < 0) && (elemIndex == 0))
			{
				if (output.m_hFile == INVALID_HANDLE_VALUE)
				{
					output.Open(outputPath, CFile::modeWrite | CFile::modeCreate | CFile::shareExclusive);
				}

				line.Replace(L"###", L".");
				origLine.Replace(L"###", L"#");
				if (firstLine)
				{
					output.WriteString(line + L", \n");
				}
				else
				{
					output.WriteString(L",\n" + line + L",\n");
				}
				output.WriteString(origLine + L"\n");
			}
		}
	}
	if (output.m_hFile != INVALID_HANDLE_VALUE)
	{
		output.WriteString(L"{ display: none !important }");
		output.Close();
	}
	input.Close();
	return true;
}
