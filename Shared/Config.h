#ifndef _CONFIG_H
#define _CONFIG_H

// ----------------------------------------------------------------------------
// Filter configuration
// ----------------------------------------------------------------------------

#ifdef NDEBUG
#undef _DEBUG
#endif

// Define filter configuration
#if (defined PRODUCT_ADBLOCKPLUS)
#define FILTERS_PROTOCOL "https://"
#define FILTERS_HOST "easylist-downloads.adblockplus.org"
#endif

// ----------------------------------------------------------------------------
// Define default protocols, hosts, scripts and pages
// ----------------------------------------------------------------------------

#define USERS_PROTOCOL              L"http://"
#define USERS_PATH                  L""
#define USERS_PORT                  INTERNET_DEFAULT_HTTP_PORT

#define USERS_SCRIPT_SETTINGS       L"/user_manager.php"
#define USERS_SCRIPT_ABOUT          L"/user_about.php"
#define USERS_SCRIPT_FAQ            L"/user_faq.php"
#define USERS_SCRIPT_FEEDBACK       L"/user_feedback.php"
#define USERS_SCRIPT_INFO           L"/user_info.php"
#define USERS_SCRIPT_WELCOME        L"/user_welcome.php"
#define USERS_SCRIPT_USER_SETTINGS  L"/user_mysettings.php"
#define USERS_SCRIPT_INVITATION     L"/user_invitation.php"
#define USERS_SCRIPT_UPGRADE        L"/user_upgrade.php"

// ----------------------------------------------------------------------------
// Define actual configurations
// ----------------------------------------------------------------------------

// AdBlocker configuration


#if (defined PRODUCT_ADBLOCKPLUS)
#define ABPDOMAIN L"adblockplus.org"
#ifdef ADPLUGIN_TEST_MODE
#define USERS_HOST L"127.0.0.1"
#elif (defined ADPLUGIN_PRODUCTION_MODE)
#ifdef _DEBUG
#define USERS_HOST L"127.0.0.1"
#else
#define USERS_HOST L"update.adblockplus.org"
#endif
#else
#error "Undefined mode. Please use configuation Release Production/Test or Debug Production/Test"
#endif
#else
#error "Undefined product. Please specify PRODUCT_ADBLOCKPLUS in configuration"
#endif

// ----------------------------------------------------------------------------
// Timers
// ----------------------------------------------------------------------------

// Time interval between user registration trials
#define TIMER_INTERVAL_USER_REGISTRATION 1000

// How long time should we wait between each try of initializing the server client (ms)
#ifdef _DEBUG
#define TIMER_INTERVAL_SERVER_CLIENT_INIT 10000
#else
#define TIMER_INTERVAL_SERVER_CLIENT_INIT 120000
#endif

// How long time sleep in background thread (ms)
#define TIMER_THREAD_SLEEP_USER_REGISTRATION 10000
#define TIMER_THREAD_SLEEP_MAIN_LOOP 60000
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
#undef  SUPPORT_FILE_DOWNLOAD
#undef  SUPPORT_CONFIG
#define SUPPORT_DOM_TRAVERSER
#define SUPPORT_FRAME_CACHING
#endif

// ----------------------------------------------------------------------------
// Miscellaneous
// ----------------------------------------------------------------------------

//For debugging production build
//#define ENABLE_DEBUG_INFO


// Max elements in white list menus
#define DOMAIN_HISTORY_MAX_COUNT 5

// Max registration attempts
#define REGISTRATION_MAX_ATTEMPTS 5

// If defined, we will surround most of the methods with try catch
#undef CATCHALL

// If defined, we will throw exceptions for errors
// Otherwise we will try to handle it in a silent way, and only report
#undef THROW_ON_ERROR

// Should we report errors to the local filesystem and/or to the ad plugin server
#ifdef ADPLUGIN_PRODUCTION_MODE
#undef REPORT_ERROR_FILE
#undef REPORT_ERROR_SERVER
#else
#define REPORT_ERROR_FILE
#define REPORT_ERROR_SERVER
#endif

// Should we shut down plugin automatically before starting the 
// installer? Remember to change the dictionary POSTDOWNLOADTEXT text
// to correspond to this behaviour.
// ex. AUTOMATIC_SHUTDOWN - "If you choose to install the new plugin, you Explorer will close before installation."
// or  NO_AUTOMATIC_SHUTDOWN" - "If you choose to install the new plugin, you have to restart Explorer for the update to take effect"
#define AUTOMATIC_SHUTDOWN


#if (defined PRODUCT_ADBLOCKPLUS)
//This is used as an agent string for HTTP requests to our servers from the plugin
#define BHO_NAME _T("Adblock Plus BHO/1.0")
#endif

// Name of ini file in Windows directory for uninstall
#if (defined PRODUCT_ADBLOCKPLUS)
#define UNINSTALL_INI_FILE "AdBlockPlus.ini"
#endif

// Prefix on temp dir files
#if (defined PRODUCT_ADBLOCKPLUS)
#define TEMP_FILE_PREFIX "ab_"
#endif

// Dictionary filename
#if (defined PRODUCT_ADBLOCKPLUS)
#define DICTIONARY_INI_FILE "dictionary_w.ini"
#else
#define DICTIONARY_INI_FILE "dictionary.ini"
#endif
#define DICTIONARY_DIR_NAME "Languages\\"
#define DEFAULT_LANGUAGE "en-US"

#define SETTING_PAGE_INI_FILE "settings_page_w.ini"

// Config filename
#ifdef SUPPORT_CONFIG
#define CONFIG_INI_FILE "config.ini"
#endif

// Settings filename
#define SETTINGS_INI_FILE "settings.ini"

// Settings tab filename
#define SETTINGS_INI_FILE_TAB "settings_tab.ini"

// Setting whitelist filename
#ifdef SUPPORT_WHITELIST
#define SETTINGS_INI_FILE_WHITELIST "settings_whitelist.ini"
#endif

// Personal filter filename
#ifdef SUPPORT_FILTER
#define PERSONAL_FILTER_FILE "filter_personal.txt"
#define PERSONAL_FILTER_FILE_OLD "easylist_personal.txt"
#endif

// Install MSI filename
#if (defined PRODUCT_ADBLOCKPLUS)
#define INSTALL_MSI_FILE "adblock.msi"
#define INSTALL_EXE_FILE "Setup.exe"
#endif

// Status bar pane name
#if (defined PRODUCT_ADBLOCKPLUS)
#define STATUSBAR_PANE_NAME "AdblockPlusStatusBarPane"
#endif

// Status bar pane number
#if (defined PRODUCT_ADBLOCKPLUS)
#define STATUSBAR_PANE_NUMBER 2
#endif



#endif // _CONFIG_H
