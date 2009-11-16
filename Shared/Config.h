#ifndef _CONFIG_H
#define _CONFIG_H

// ----------------------------------------------------------------------------
// Filter configuration
// ----------------------------------------------------------------------------

// Define filter configuration
#if (defined PRODUCT_ADBLOCKER)
 #define FILTERS_PROTOCOL "http://"
 #define FILTERS_HOST "simple-adblock.com/download"
#elif (defined PRODUCT_DOWNLOADHELPER)
 #define FILTERS_PROTOCOL "http://"
 #define FILTERS_HOST "ie-downloadhelper.com/download"
#endif

// ----------------------------------------------------------------------------
// Define default protocols, hosts, scripts and pages
// ----------------------------------------------------------------------------

#define USERS_PROTOCOL              "http://"
#define USERS_PATH                  ""
#define USERS_PORT                  INTERNET_DEFAULT_HTTP_PORT

#define USERS_SCRIPT_ACTIVATE       "/user_activate.php"
#define USERS_SCRIPT_SETTINGS       "/user_manager.php"
#define USERS_SCRIPT_ABOUT          "/user_about.php"
#define USERS_SCRIPT_FAQ            "/user_faq.php"
#define USERS_SCRIPT_FEEDBACK       "/user_feedback.php"
#define USERS_SCRIPT_INFO           "/user_info.php"
#define USERS_SCRIPT_WELCOME        "/user_welcome.php"
#define USERS_SCRIPT_USER_SETTINGS  "/user_mysettings.php"
#define USERS_SCRIPT_INVITATION     "/user_invitation.php"

// ----------------------------------------------------------------------------
// Define actual configurations
// ----------------------------------------------------------------------------

// AdBlocker configuration

#if (defined PRODUCT_ADBLOCKER)
 #ifdef ADPLUGIN_TEST_MODE
  #define USERS_HOST L"mytest.simple-adblock.com"
 #elif (defined ADPLUGIN_PRODUCTION_MODE)
//  #undef  USERS_PORT
//  #define USERS_PORT INTERNET_DEFAULT_HTTPS_PORT
//  #undef  USERS_PROTOCOL
//  #define USERS_PROTOCOL "https://"
  #define USERS_HOST L"my.simple-adblock.com"
 #else
  #error "Undefined mode. Please use configuation Release Production/Test or Debug Production/Test"
 #endif

#elif (defined PRODUCT_DOWNLOADHELPER)
 #ifdef ADPLUGIN_TEST_MODE
  #define USERS_HOST L"mytest.ie-downloadhelper.com"
 #elif (defined ADPLUGIN_PRODUCTION_MODE)
//  #undef  USERS_PORT
//  #define USERS_PORT INTERNET_DEFAULT_HTTPS_PORT
//  #undef  USERS_PROTOCOL
//  #define USERS_PROTOCOL "https://"
  #define USERS_HOST L"my.ie-downloadhelper.com"
 #else
  #error "Undefined mode. Please use configuation Release Production/Test or Debug Production/Test"
 #endif

#elif (defined PRODUCT_DOWNLOADHELPER_APP)

// No product defined

#else
 #error "Undefined product. Please specify PRODUCT_ADBLOCKER or PRODUCT_DOWNLOADHELPER in configuration"
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
#define TIMER_THREAD_SLEEP_TAB_LOOP 30000

// ----------------------------------------------------------------------------
// Debug
// ----------------------------------------------------------------------------

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
#define ENABLE_DEBUG_INI
 #undef  ENABLE_DEBUG_MUTEX
 #undef  ENABLE_DEBUG_HIDE_EL
 #undef  ENABLE_DEBUG_WHITELIST

 #define ENABLE_DEBUG_RESULT
 #define ENABLE_DEBUG_RESULT_IGNORED
#else
 #undef ENABLE_DEBUG_INFO
#endif

#undef ENABLE_DEBUG_SELFTEST

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_GENERAL)
 #undef  DEBUG_GENERAL
 #if (defined ENABLE_DEBUG_SELFTEST)
  #define DEBUG_GENERAL(x) CPluginDebug::Debug(x);CPluginSelftest::AddText(x);
 #else
  #define DEBUG_GENERAL(x) CPluginDebug::Debug(x);
 #endif
#endif

#if (defined ENABLE_DEBUG_INFO)
 #undef  DEBUG
 #define DEBUG(x) CPluginDebug::Debug((CString)(x));
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_BLOCKER)
 #undef  DEBUG_BLOCKER
 #define DEBUG_BLOCKER(x) CPluginDebug::Debug((CString)(x));
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_FILTER)
 #undef  DEBUG_FILTER
 #define DEBUG_FILTER(x) CPluginDebug::Debug((CString)(x));
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_SETTINGS)
 #undef  DEBUG_SETTINGS
 #define DEBUG_SETTINGS(x) CPluginDebug::Debug((CString)(x));
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_THREAD)
 #undef  DEBUG_THREAD
 #define DEBUG_THREAD(x) CPluginDebug::Debug((CString)(x));
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_NAVI)
 #undef  DEBUG_NAVI
 #define DEBUG_NAVI(x) CPluginDebug::Debug(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_CHECKSUM)
 #undef  DEBUG_CHECKSUM
 #define DEBUG_CHECKSUM(x) CPluginDebug::Debug((CString)(x));
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_DICTIONARY)
 #undef  DEBUG_DICTIONARY
 #define DEBUG_DICTIONARY(x) CPluginDebug::Debug((CString)(x));
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_INI)
 #undef  DEBUG_INI
 #define DEBUG_INI(x) CPluginDebug::Debug((CString)(x));
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_MUTEX)
 #undef  DEBUG_MUTEX
 #define DEBUG_MUTEX(x) CPluginDebug::Debug((CString)(x));
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_HIDE_EL)
 #undef  DEBUG_HIDE_EL
 #define DEBUG_HIDE_EL(x) CPluginDebug::Debug(x);
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_WHITELIST)
 #undef  DEBUG_WHITELIST
 #define DEBUG_WHITELIST(x) CPluginDebug::Debug(x);
#endif

#if (defined ENABLE_DEBUG_SELFTEST)
 #undef  DEBUG_SELFTEST
 #define DEBUG_SELFTEST(x) CPluginSelftest::AddText(x);

 #if (!defined ENABLE_DEBUG_INFO || !defined ENABLE_DEBUG_GENERAL)
  #undef  DEBUG_GENERAL
  #define DEBUG_GENERAL(x) CPluginSelftest::AddText(x);
 #endif
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_ERROR || defined ENABLE_DEBUG_SELFTEST)
 #undef  DEBUG_ERROR
 #define DEBUG_ERROR(x) CPluginDebug::DebugError("!!! Error:" + CString(x));
 #undef  DEBUG_ERROR_CODE
 #define DEBUG_ERROR_CODE(err, x) CPluginDebug::DebugErrorCode(err, "!!! Error:" + CString(x));
 #undef  DEBUG_ERROR_CODE_EX
 #define DEBUG_ERROR_CODE_EX(err, x, process, thread) CPluginDebug::DebugErrorCode(err, "!!! Error:" + CString(x), process, thread);
#endif

#undef  DEBUG_ERROR_LOG
#if (defined PRODUCT_DOWNLOADHELPER_APP)
 #define DEBUG_ERROR_LOG(err, id, subid, description)
#else
 #define DEBUG_ERROR_LOG(err, id, subid, description) CPluginClient::LogPluginError(err, id, subid, description);
#endif

// ----------------------------------------------------------------------------
// Features
// ----------------------------------------------------------------------------

#if (defined PRODUCT_ADBLOCKER)
 #define SUPPORT_FILTER
 #define SUPPORT_WHITELIST
 #undef  SUPPORT_FILE_DOWNLOAD
 #undef  SUPPORT_CONFIG
#elif (defined PRODUCT_DOWNLOADHELPER)
 #undef  SUPPORT_FILTER
 #undef  SUPPORT_WHITELIST
 #define SUPPORT_FILE_DOWNLOAD
 #define SUPPORT_CONFIG
#elif (defined PRODUCT_DOWNLOADHELPER_APP)
 #undef  SUPPORT_FILTER
 #undef  SUPPORT_WHITELIST
 #define SUPPORT_FILE_DOWNLOAD
 #define SUPPORT_CONFIG
#endif

// ----------------------------------------------------------------------------
// Miscellaneous
// ----------------------------------------------------------------------------

// Max elements in white list menus
#define DOMAIN_HISTORY_MAX_COUNT 5

// Max elements in download file menu
#define DOWNLOAD_FILE_MAX_COUNT 10

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

#if (defined PRODUCT_ADBLOCKER)
 #define SIMPLE_ADBLOCK_NAME "Simple Adblock"
#endif

#if (defined PRODUCT_ADBLOCKER)
 #define BHO_NAME _T("Simple Adblock BHO/1.0")
#elif (defined PRODUCT_DOWNLOADHELPER)
 #define BHO_NAME _T("Download Helper BHO/1.0")
#endif

// Name of ini file in Windows directory for uninstall
#if (defined PRODUCT_ADBLOCKER)
 #define UNINSTALL_INI_FILE "SimpleAdblock.ini"
#elif (defined PRODUCT_DOWNLOADHELPER || defined PRODUCT_DOWNLOADHELPER_APP)
 #define UNINSTALL_INI_FILE "DownloadHelper.ini"
#endif

// Name of user dir
#if (defined PRODUCT_ADBLOCKER)
 #define USER_DIR "Simple Adblock\\"
#elif (defined PRODUCT_DOWNLOADHELPER || defined PRODUCT_DOWNLOADHELPER_APP)
 #define USER_DIR "Download Helper\\"
#endif

// Prefix on temp dir files
#if (defined PRODUCT_ADBLOCKER)
 #define TEMP_FILE_PREFIX "ab_"
#elif (defined PRODUCT_DOWNLOADHELPER || defined PRODUCT_DOWNLOADHELPER_APP)
 #define TEMP_FILE_PREFIX "dh_"
#endif

// Dictionary filename
#if (defined PRODUCT_ADBLOCKER)
 #define DICTIONARY_INI_FILE "dictionary_w.ini"
#else
 #define DICTIONARY_INI_FILE "dictionary.ini"
#endif

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
 #define PERSONAL_FILTER_FILE "easylist_personal.txt"
#endif

// Install MSI filename
#if (defined PRODUCT_ADBLOCKER)
 #define INSTALL_MSI_FILE "simpleadblock.msi"
#elif (defined PRODUCT_DOWNLOADHELPER)
 #define INSTALL_MSI_FILE "downloadhelper.msi"
#endif

// Status bar pane name
#if (defined PRODUCT_ADBLOCKER)
 #define STATUSBAR_PANE_NAME "SimpleAdblockStatusBarPane"
#elif (defined PRODUCT_DOWNLOADHELPER)
 #define STATUSBAR_PANE_NAME "DownloadHelperStatusBarPane"
#endif

// Status bar pane number
#if (defined PRODUCT_ADBLOCKER)
 #define STATUSBAR_PANE_NUMBER 2
#elif (defined PRODUCT_DOWNLOADHELPER)
 #define STATUSBAR_PANE_NUMBER 3
#endif



#endif // _CONFIG_H
