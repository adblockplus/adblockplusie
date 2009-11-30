#ifndef _PLUGIN_CLIENT_H_
#define _PLUGIN_CLIENT_H_


#if (defined PRODUCT_SIMPLEADBLOCK)
 #include "../AdBlocker/SimpleAdblockClient.h"
 typedef CSimpleAdblockClient CPluginClient;
#elif (defined PRODUCT_DOWNLOADHELPER)
 #include "../DownloadHelper/DownloadHelperClient.h"
 typedef CDownloadHelperClient CPluginClient;
#endif


#endif // _PLUGIN_CLIENT_H_
