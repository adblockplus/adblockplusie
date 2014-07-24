/**
 * \file ATL_Deprecate.h Include package for all the ATL headers, with optional forwarding to detect ATL dependencies.
 *
 * This header is a nexus for refactoring work to remove ATL.
 * The main purpose of this mechanism is to determine the exact scope of the refactoring effort.
 * The lifetime of this header is during the work on #276 Stop using ATL https://issues.adblockplus.org/ticket/276
 *
 * This file implements namespace forwarding that allows detection of ATL symbols.
 * This forwarding is ordinarily disabled, and always disabled in Release configuration for safety.
 * When forwarding is disabled, it simply includes the ATL headers.
 * With forwarding turned on, the ATL headers are included within a namespace,
 *   then particular ATL items used are then manually reintroduced into visibility.
 * This method requires explicitly enumerating each of the symbols needed to get the code to compile.
 *
 * When the ATL namespace is forwarded, the code does not link.
 * Since forwarding the namespace is a code analysis tool, this is OK.
 * The ATL DLL libraries expose symbols in the ATL namespace.
 * The forwarded libraries use a different namespace.
 * Thus the external symbols do not match, and the linker reports unresolved externals.
 */

/*
 * NDEBUG is defined for release but not for debug, so release versions never forward.
 * Because the plugin doesn't link correctly with forwarding turned off, forwarding is disabled by default even for debug.
 */
#define DISABLE_FORWARDING_FOR_DEBUG 1
#define DISABLE_ATL_FORWARDING defined(NDEBUG) || DISABLE_FORWARDING_FOR_DEBUG

#if DISABLE_ATL_FORWARDING

#include <atlbase.h>
#include <atlstr.h>
#include <atltypes.h>
#include <atlcom.h>

#else

/*
 * ATL requires the following includes in the global namespace.
 */
#include <apiset.h>
#include <apisetcconv.h>
#include <rpc.h>
#include <rpcndr.h>
#include <pshpack8.h>
#include <stddef.h>
#include <string.h>
#include <malloc.h>
#include <mbstring.h>
#include <wchar.h>
#include <tchar.h>
#include <stdlib.h>
#include <comutil.h>
#include <OaIdl.h>
#include <ObjIdl.h>
#include <new>
#include <propsys.h>
#include <ShObjIdl.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <MsHTML.h>
#include <crtdbg.h>
#include <ole2.h>
#include <MsHtmHst.h>

/*
 * ATL is used as an explicit global namespace within ATL itself.
 * These declarations forward the wrapped ATL namespace back into the global one.
 * Fortuitously, none of the plugin code uses explicitly global ATL 
 */
namespace OLD_ATL {
  namespace ATL {
  }
}
namespace ATL = OLD_ATL::ATL ;

namespace OLD_ATL {
  /*
   * We need to bring some globals back into this namespace.
   */
  using ::tagVARIANT ;
  using ::IStream ;

  /*
   * The original ATL include files.
   */
  #include <atlbase.h>
  #include <atlstr.h>
  #include <atltypes.h>
  #include <atlcom.h>
}

/*
 * The list of symbols that appear in the code.
 * Each needs to be explicitly incorporated back in the global namespace when forwarding is on.
 */
using OLD_ATL::_AtlBaseModule;
using OLD_ATL::CAtlBaseModule;
using OLD_ATL::CComAggObject;
using OLD_ATL::CComAutoCriticalSection;
using OLD_ATL::CComBSTR;
using OLD_ATL::CComClassFactory;
using OLD_ATL::CComCoClass;
using OLD_ATL::CComCreator;
using OLD_ATL::CComCreator2;
using OLD_ATL::CComModule;
using OLD_ATL::CComMultiThreadModel;
using OLD_ATL::CComObject;
using OLD_ATL::CComObjectNoLock;
using OLD_ATL::CComObjectRootEx;
using OLD_ATL::CComPtr;
using OLD_ATL::CComQIPtr;
using OLD_ATL::CComSingleThreadModel;
using OLD_ATL::CComVariant;
using OLD_ATL::CString;
using OLD_ATL::CW2A;
using OLD_ATL::IDispatchImpl;
using OLD_ATL::IObjectWithSiteImpl;
using OLD_ATL::OLE2T;

#endif

/*
 * Deprecation pragmas use the compiler to identify refactoring targets.
 */
#if DISABLE_ATL_FORWARDING
namespace ATL {
#else 
namespace OLD_ATL {
#endif
#pragma deprecated( CRect )
#pragma deprecated( CSimpleArray )
}

/*
 * Note:
 *   The preprocessor symbols ATLASSERT and ATLTRACE appear in the source.
 *   These resolve to nothing in Release configurations.
 *   In Debug configurations, these are non-trivial.
 *   ATLASSERT resolves to an expression with _CrtDbgReportW and _CrtDbgBreak.
 *   ATLTRACE resolves to a ATL::CTraceFileAndLineInfo.
 *   These will need to be replaced or removed.
 */
