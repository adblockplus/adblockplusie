#ifndef _SIMPLE_ADBLOCK_TAB_H_
#define _SIMPLE_ADBLOCK_TAB_H_


#include "PluginTabBase.h"


class CPluginTab : public CPluginTabBase
{

public:
	void OnNavigate(const CString& url);
	CPluginTab(CPluginClass* plugin);
	~CPluginTab();
};


#endif // _SIMPLE_ADBLOCK_TAB_H_
