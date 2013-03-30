#include "PluginStdAfx.h"

#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginClass.h"
#include "PluginConfiguration.h"
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

  int r = url.Find(L".simple-adblock.com");
  if ((r > 0) && (r < 15))
  {
    if (url.Find(L"?update") > 0)
    {
      CPluginConfiguration pluginConfig;
      pluginConfig.Download();
      DWORD id;
      HANDLE handle = ::CreateThread(NULL, 0, CPluginClass::MainThreadProc, (LPVOID)this, NULL, &id);
      CPluginSettings* settings = CPluginSettings::GetInstance();

      //Also register a mime filter if it's not registered yet
      if (CPluginClass::s_mimeFilter == NULL)
      {
        CPluginClass::s_mimeFilter = CPluginClientFactory::GetMimeFilterClientInstance();
      }

      settings->Write();
      this->OnUpdateConfig();
      this->OnUpdateSettings(true);
    }
  }
}
