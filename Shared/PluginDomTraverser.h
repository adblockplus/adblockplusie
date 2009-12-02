#ifndef _PLUGIN_DOM_TRAVERSER_H_
#define _PLUGIN_DOM_TRAVERSER_H_


#if (defined PRODUCT_SIMPLEADBLOCK)
 #include "../AdBlocker/SimpleAdblockDomTraverser.h"
#elif (defined PRODUCT_DOWNLOADHELPER)
 #include "../DownloadHelper/DownloadHelperDomTraverser.h"
#endif


#endif // _PLUGIN_DOM_TRAVERSER_H_
