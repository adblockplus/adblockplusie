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

#ifndef _PLUGIN_DEBUG_MACROS_H_
#define _PLUGIN_DEBUG_MACROS_H_


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


#endif // _PLUGIN_DEBUG_MACROS_H_
