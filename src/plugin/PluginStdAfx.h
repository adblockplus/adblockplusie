#ifndef _STDAFX_H
#define _STDAFX_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// Embed manifest as a resource, to enable common controls
// see http://msdn.microsoft.com/en-us/library/windows/desktop/bb773175(v=vs.85).aspx#using_manifests
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


//#define STRICT
#define WINVER 0x0501

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#define _ATL_APARTMENT_THREADED

//#define _CRT_SECURE_NO_DEPRECATE 1
#include <atlbase.h>
#include <atlstr.h>
#include <atltypes.h>

extern CComModule _Module;
#include <comutil.h>
#include <atlcom.h>
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
#include <Windows.h>
#include <Sddl.h>


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#include "PluginDebugMacros.h"
#include "PluginErrorCodes.h"


#if (defined PRODUCT_ADBLOCKPLUS)
#include "Config.h"
#endif

#if (defined PRODUCT_ADBLOCKPLUS)
#if (defined ENTERPRISE)
#define CONFIG_IN_REGISTRY
#endif
#include "Resource.h"
#endif

#if (defined PRODUCT_ADBLOCKPLUS)
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

#endif // not _STDAFX_H

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(*x))
#endif

#ifdef USE_CONSOLE
#include "Console.h"
#endif

#include "BuildVariant.h"
