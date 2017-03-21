/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2017 eyeo GmbH
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

#include "PluginClientFactory.h"
#include "PluginMimeFilterClient.h"


CPluginMimeFilterClient* CPluginClientFactory::s_mimeFilterInstance = NULL;

CComAutoCriticalSection CPluginClientFactory::s_criticalSection;

CPluginMimeFilterClient* CPluginClientFactory ::GetMimeFilterClientInstance() 
{
  CPluginMimeFilterClient* localInstance = NULL;

  s_criticalSection.Lock();
  {
    if (!s_mimeFilterInstance)
    {
      //we cannot copy the client directly into the instance variable
      //if the constructor throws we do not want to alter instance
      localInstance = new CPluginMimeFilterClient();

      s_mimeFilterInstance = localInstance;
    }
    else
    {
      localInstance = s_mimeFilterInstance;
    }
  }
  s_criticalSection.Unlock();

  return localInstance;
}

void CPluginClientFactory::ReleaseMimeFilterClientInstance() 
{
  s_criticalSection.Lock();
  {
    delete s_mimeFilterInstance;

    s_mimeFilterInstance = NULL;
  }
  s_criticalSection.Unlock();
}
