#ifndef _STDAFX_H
#define _STDAFX_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000



//#define STRICT
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#define _ATL_APARTMENT_THREADED

#define _CRT_SECURE_NO_DEPRECATE 1
#include <atlbase.h>
#include <atlstr.h>
#include <atltypes.h>

extern CComModule _Module;
#include <comutil.h>
#include <atlcom.h>
#include <atlhost.h>
#include <stdio.h>
#include <assert.h>
#include <stdexcept>
#include <ExDisp.h>
#include <ExDispID.h>
#include <Mshtml.h>

#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <sstream>
#include <commctrl.h>
#include <mshtmdid.h>
#include <Mlang.h>
#include <initguid.h>

// Win32
#include <shlguid.h>
#include <shlobj.h>
#include <iepmapi.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#include "PluginDebugMacros.h"
#include "PluginErrorCodes.h"

#if (defined PRODUCT_SIMPLEADBLOCK)
 #include "Config.h"
#endif

#if (defined PRODUCT_SIMPLEADBLOCK)
 #include "..\AdBlocker\Version.h"
#endif

#if (defined PRODUCT_SIMPLEADBLOCK)
#if (defined ENTERPRISE)
	#define CONFIG_IN_REGISTRY
#endif
 #include "..\AdBlocker\Resource.h"
#endif

#if (defined PRODUCT_SIMPLEADBLOCK)
 #include "PluginDebug.h"
#endif


#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>


#ifdef _MSC_VER
 #pragma warning(push)
 // warning C4996: function call with parameters that might be unsafe
 #pragma warning(disable : 4996)
#endif


#define _CRTDBG_MAPALLOC 
#endif // not _STDAFX_H

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(*x))
#endif 

#define USE_CONSOLE
#include "Console.h"
