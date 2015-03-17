/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "ATL_Deprecate.h"

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

#include "PluginErrorCodes.h"
#include "Config.h"
#include "Resource.h"
#include "PluginDebug.h"

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#endif // not _STDAFX_H

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(*x))
#endif

#ifdef USE_CONSOLE
#include "Console.h"
#endif
