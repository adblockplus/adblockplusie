#ifndef _PLUGIN_CLIENT_H_
#define _PLUGIN_CLIENT_H_


#if (defined PRODUCT_ADBLOCKPLUS)
 #include "../AdBlocker/SimpleAdblockClient.h"
 typedef CSimpleAdblockClient CPluginClient;
#endif


#endif // _PLUGIN_CLIENT_H_
