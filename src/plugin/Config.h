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

#ifndef _CONFIG_H
#define _CONFIG_H

#define TIMER_THREAD_SLEEP_TAB_LOOP 10000

// Should we to on debug information
#ifdef _DEBUG
#define ENABLE_DEBUG_INFO
#define ENABLE_DEBUG_GENERAL
#define ENABLE_DEBUG_ERROR

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

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_GENERAL)
#define DEBUG_GENERAL(x) DEBUG_FUNC(x);
#else
#define DEBUG_GENERAL(x)
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_FILTER)
#define DEBUG_FILTER(x) DEBUG_FUNC(x);
#else
#define DEBUG_FILTER(x)
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_SETTINGS)
#define DEBUG_SETTINGS(x) DEBUG_FUNC(x);
#else
#define DEBUG_SETTINGS(x)
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_THREAD)
#define DEBUG_THREAD(x) DEBUG_FUNC(x);
#else
#define DEBUG_THREAD(x)
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_NAVI)
#define DEBUG_NAVI(x) DEBUG_FUNC(x);
#else
#define DEBUG_NAVI(x)
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_MUTEX)
#define DEBUG_MUTEX(x) DEBUG_FUNC(x);
#else
#define DEBUG_MUTEX(x)
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_HIDE_EL)
#define DEBUG_HIDE_EL(x) DEBUG_FUNC(x);
#else
#define DEBUG_HIDE_EL(x)
#endif

#if (defined ENABLE_DEBUG_INFO && defined ENABLE_DEBUG_ERROR)
#define DEBUG_EXCEPTION(x) CPluginDebug::DebugException(x)
#define DEBUG_ERROR_CODE(err, x) CPluginDebug::DebugErrorCode(err, x);
#define DEBUG_ERROR_CODE_EX(err, x, process, thread) CPluginDebug::DebugErrorCode(err, x, process, thread);
#else
#define DEBUG_EXCEPTION(x)
#define DEBUG_ERROR_CODE(err, x)
#define DEBUG_ERROR_CODE_EX(err, x, process, thread)
#endif

#define DEBUG_ERROR_LOG(err, id, subid, description) LogQueue::PostPluginError(err, id, subid, description);
#define DEBUG_SYSTEM_EXCEPTION(ex, id, subid, description) CPluginDebug::DebugSystemException(ex, id, subid, description)
#define DEBUG_SELFTEST(x)

// ----------------------------------------------------------------------------
// Miscellaneous
// ----------------------------------------------------------------------------

// Status bar pane name
#define STATUSBAR_PANE_NAME L"AdblockPlusStatusBarPane"

// Status bar pane number
#define STATUSBAR_PANE_NUMBER 2

#define ENGINE_STARTUP_TIMEOUT 10000



#endif // _CONFIG_H
