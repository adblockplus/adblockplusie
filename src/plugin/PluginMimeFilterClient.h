/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
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

#ifndef _PLUGIN_MIME_FILTER_CLIENT_H
#define _PLUGIN_MIME_FILTER_CLIENT_H


class CPluginMimeFilterClient
{

public:

  CPluginMimeFilterClient();
  ~CPluginMimeFilterClient();

  void Unregister();

  CComPtr<IClassFactory> m_classFactory;
  CComPtr<IClassFactory> m_spCFHTTP;
  CComPtr<IClassFactory> m_spCFHTTPS;
};


#endif // _PLUGIN_MIME_FILTER_CLIENT_H
