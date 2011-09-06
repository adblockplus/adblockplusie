#include "PluginStdAfx.h"

class AdblockPlusConvertor
{
public:
	AdblockPlusConvertor();

	static bool Convert(CString abpFilterPath, CString outputPath);
};