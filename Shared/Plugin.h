#ifndef _PLUGIN_H_
#define _PLUGIN_H_


#if (defined PRODUCT_SIMPLEADBLOCK)
 #include "../AdBlocker/AdBlocker.h"
#elif (defined PRODUCT_DOWNLOADHELPER)
 #include "../DownloadHelper/DownloadHelper.h"
#endif


#endif // _PLUGIN_H_
