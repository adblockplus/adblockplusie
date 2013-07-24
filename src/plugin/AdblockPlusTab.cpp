#include "PluginStdAfx.h"

#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginClass.h"
#include "PluginClientFactory.h"

#include "AdblockPlusTab.h"


CPluginTab::CPluginTab(CPluginClass* plugin) : CPluginTabBase(plugin)
{
}

CPluginTab::~CPluginTab()
{
}


void CPluginTab::OnNavigate(const CString& url)
{
  CPluginTabBase::OnNavigate(url);
}
