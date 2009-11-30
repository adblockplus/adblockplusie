#ifndef _PLUGIN_TAB_H_
#define _PLUGIN_TAB_H_


#include "PluginTabBase.h"

#if (defined PRODUCT_SIMPLEADBLOCK)
 #include "../AdBlocker/SimpleAdblockTab.h"
#elif (defined PRODUCT_DOWNLOADHELPER)
 #include "../DownloadHelper/DownloadHelperTab.h"
#endif


#endif // _PLUGIN_TAB_H_
