#ifndef _CONFIG_H
#define _CONFIG_H

#define TIMER_THREAD_SLEEP_TAB_LOOP 10000

// Should we to on debug information
#ifdef _DEBUG
#define ENABLE_DEBUG_INFO
#define ENABLE_DEBUG_GENERAL
#define ENABLE_DEBUG_ERROR
#undef  ENABLE_DEBUG_BLOCKER
#undef  ENABLE_DEBUG_FILTER
#undef  ENABLE_DEBUG_SETTINGS
#undef  ENABLE_DEBUG_THREAD
#undef  ENABLE_DEBUG_NAVI
#undef  ENABLE_DEBUG_DICTIONARY
#undef  ENABLE_DEBUG_CHECKSUM
#undef  ENABLE_DEBUG_INI
#undef  ENABLE_DEBUG_MUTEX
#undef  ENABLE_DEBUG_HIDE_EL
#undef  ENABLE_DEBUG_WHITELIST

#define ENABLE_DEBUG_RESULT
#define ENABLE_DEBUG_RESULT_IGNORED
#define ENABLE_DEBUG_SPLIT_FILE
#else
#undef ENABLE_DEBUG_INFO
#endif

#ifdef NDEBUG
#undef ENABLE_DEBUG_INFO
#endif

#undef ENABLE_DEBUG_SELFTEST

#define DEBUG_FUNC CPluginDebug::Debug
#define DEBUG_ERROR_FUNC CPluginDebug::DebugError
#define DEBUG_ERROR_CODE_FUNC CPluginDebug::DebugErrorCode

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_GENERAL)
#undef  DEBUG_GENERAL
#define DEBUG_GENERAL(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO)
#undef  DEBUG
#define DEBUG(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_BLOCKER)
#undef  DEBUG_BLOCKER
#define DEBUG_BLOCKER(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_FILTER)
#undef  DEBUG_FILTER
#define DEBUG_FILTER(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_SETTINGS)
#undef  DEBUG_SETTINGS
#define DEBUG_SETTINGS(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_THREAD)
#undef  DEBUG_THREAD
#define DEBUG_THREAD(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_NAVI)
#undef  DEBUG_NAVI
#define DEBUG_NAVI(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_CHECKSUM)
#undef  DEBUG_CHECKSUM
#define DEBUG_CHECKSUM(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_DICTIONARY)
#undef  DEBUG_DICTIONARY
#define DEBUG_DICTIONARY(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_INI)
#undef  DEBUG_INI
#define DEBUG_INI(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_MUTEX)
#undef  DEBUG_MUTEX
#define DEBUG_MUTEX(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_HIDE_EL)
#undef  DEBUG_HIDE_EL
#define DEBUG_HIDE_EL(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_WHITELIST)
#undef  DEBUG_WHITELIST
#define DEBUG_WHITELIST(x) DEBUG_FUNC(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_ERROR)
#undef  DEBUG_ERROR
#define DEBUG_ERROR(x) DEBUG_ERROR_FUNC("!!! Error:" + CString(x));
#undef  DEBUG_ERROR_CODE
#define DEBUG_ERROR_CODE(err, x) DEBUG_ERROR_CODE_FUNC(err, "!!! Error:" + CString(x));
#undef  DEBUG_ERROR_CODE_EX
#define DEBUG_ERROR_CODE_EX(err, x, process, thread) DEBUG_ERROR_CODE_FUNC(err, "!!! Error:" + CString(x), process, thread);
#endif

#undef  DEBUG_ERROR_LOG
#define DEBUG_ERROR_LOG(err, id, subid, description) CPluginClient::LogPluginError(err, id, subid, description);

// ----------------------------------------------------------------------------
// Features
// ----------------------------------------------------------------------------

#if (defined PRODUCT_ADBLOCKPLUS)
#define SUPPORT_FILTER
#define SUPPORT_WHITELIST
#define SUPPORT_DOM_TRAVERSER
#define SUPPORT_FRAME_CACHING
#endif

// ----------------------------------------------------------------------------
// Miscellaneous
// ----------------------------------------------------------------------------

//For debugging production build
//#define ENABLE_DEBUG_INFO

// If defined, we will surround most of the methods with try catch
#undef CATCHALL

// If defined, we will throw exceptions for errors
// Otherwise we will try to handle it in a silent way, and only report
#undef THROW_ON_ERROR

// Status bar pane name
#if (defined PRODUCT_ADBLOCKPLUS)
#define STATUSBAR_PANE_NAME L"AdblockPlusStatusBarPane"
#endif

// Status bar pane number
#if (defined PRODUCT_ADBLOCKPLUS)
#define STATUSBAR_PANE_NUMBER 2
#endif

#define ENGINE_STARTUP_TIMEOUT 10000



#endif // _CONFIG_H
