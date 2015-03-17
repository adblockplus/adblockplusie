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

#include "PluginStdAfx.h"

#include "PluginMutex.h"
#include "PluginClientBase.h"


CPluginMutex::CPluginMutex(const std::wstring& name, int errorSubidBase) 
  : m_isLocked(false), m_errorSubidBase(errorSubidBase), system_name(L"Global\\AdblockPlus" + name)
{
  if (m_errorSubidBase != PLUGIN_ERROR_MUTEX_DEBUG_FILE)
  {
    DEBUG_MUTEX(L"Mutex::Create name:" + name)
  }
  m_hMutex = CreateMutexW(NULL, FALSE, system_name.c_str());

  if (m_hMutex == NULL)
  {
    DWORD error = GetLastError();
    m_hMutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, system_name.c_str());
    if (m_hMutex == NULL)
    {
      system_name = L"Local\\AdblockPlus" + name;
      m_hMutex = CreateMutexW(NULL, FALSE, system_name.c_str());
      if (m_hMutex == NULL)
      {
        m_hMutex = OpenMutexW(NULL, FALSE, system_name.c_str());
        if (m_hMutex == NULL)
        {
          DWORD error = GetLastError();
          DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_CREATE + m_errorSubidBase, "Mutex::CreateMutex");
        }
      }
      else
      // TODO: Combine this block with identical one below.
      {
        switch (::WaitForSingleObject(m_hMutex, 3000))
        {
          // The thread got ownership of the mutex
        case WAIT_OBJECT_0: 
          m_isLocked = true;
          break;

        case WAIT_TIMEOUT:
          DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_WAIT_TIMEOUT + m_errorSubidBase, "Mutex::CreateMutex - Timeout");
          m_hMutex = NULL;
          break;

        case WAIT_FAILED:
          DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_WAIT + m_errorSubidBase, "Mutex::CreateMutex - Wait error");
          break;
        }
      }

    }
  }
  else
  // TODO: Combine this block with identical one above.
  {
    switch (::WaitForSingleObject(m_hMutex, 3000))
    {
      // The thread got ownership of the mutex
    case WAIT_OBJECT_0: 
      m_isLocked = true;
      break;

    case WAIT_TIMEOUT:
      DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_WAIT_TIMEOUT + m_errorSubidBase, "Mutex::CreateMutex - Timeout");
      m_hMutex = NULL;
      break;

    case WAIT_FAILED:
      DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_WAIT + m_errorSubidBase, "Mutex::CreateMutex - Wait error");
      break;
    }
  }
}

CPluginMutex::~CPluginMutex()
{
  if (m_errorSubidBase != PLUGIN_ERROR_MUTEX_DEBUG_FILE)
  {
    DEBUG_MUTEX(L"Mutex::Release name:" + system_name)
  }

  if (m_isLocked)
  {
    m_isLocked = false;
  }

  if (m_hMutex)
  {
    if (!::ReleaseMutex(m_hMutex))
    {
      DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_RELEASE + m_errorSubidBase, "Mutex::ReleaseMutex");
    }
  }

  m_hMutex = NULL;
}

bool CPluginMutex::IsLocked() const
{
  return m_isLocked;
}
