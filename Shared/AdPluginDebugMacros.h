#ifndef _ADPLUGIN_DEBUG_MACROS_H_
#define _ADPLUGIN_DEBUG_MACROS_H_


#undef  DEBUG_INFO
#undef  DEBUG_GENERAL
#undef  DEBUG_BLOCKER
#undef  DEBUG_PARSER
#undef  DEBUG_FILTER
#undef  DEBUG_SETTINGS
#undef  DEBUG_THREAD
#undef  DEBUG_NAVI
#undef  DEBUG_CHECKSUM
#undef  DEBUG_DICTIONARY
#undef  DEBUG_ERROR
#undef  DEBUG_ERROR_CODE
#undef  DEBUG_ERROR_CODE_EX
#undef  DEBUG_ERROR_LOG
#undef  DEBUG_SELFTEST
#undef  DEBUG_INI
#undef  DEBUG_MUTEX
#undef  DEBUG_HIDE_EL
#undef  DEBUG_WHITELIST
#undef  DEBUG

#define DEBUG_GENERAL(x)
#define DEBUG_BLOCKER(x)
#define DEBUG_PARSER(x)
#define DEBUG_FILTER(x)
#define DEBUG_SETTINGS(x)
#define DEBUG_THREAD(x)
#define DEBUG_NAVI(x)
#define DEBUG_CHECKSUM(x)
#define DEBUG_DICTIONARY(x)
#define DEBUG_ERROR(x)
#define DEBUG_ERROR_CODE(err, x)
#define DEBUG_ERROR_CODE_EX(err, x, process, thread)
#define DEBUG_ERROR_LOG(err, id, subid, description)
#define DEBUG_SELFTEST(x)
#define DEBUG_INI(x)
#define DEBUG_MUTEX(x)
#define DEBUG_HIDE_EL(x)
#define DEBUG_WHITELIST(x)
#define DEBUG(x)


#endif // _ADPLUGIN_DEBUG_MACROS_H_
