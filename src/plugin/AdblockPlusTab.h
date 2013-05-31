#ifndef _ADBLOCK_PLUS_TAB_H_
#define _ADBLOCK_PLUS_TAB_H_


#include "PluginTabBase.h"


class CPluginTab : public CPluginTabBase
{

public:
  void OnNavigate(const CString& url);
  CPluginTab(CPluginClass* plugin);
  ~CPluginTab();
};


#endif // _ADBLOCK_PLUS_TAB_H_
