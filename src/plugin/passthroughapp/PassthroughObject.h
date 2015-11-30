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

#ifndef PASSTHROUGHAPP_PASSTHROUGHOBJECT_H
#define PASSTHROUGHAPP_PASSTHROUGHOBJECT_H

// {C38D254C-4C40-4192-A746-AC6FE519831E}
extern "C" const __declspec(selectany) IID IID_IPassthroughObject = 
{0xc38d254c, 0x4c40, 0x4192,
{0xa7, 0x46, 0xac, 0x6f, 0xe5, 0x19, 0x83, 0x1e}};

struct
  __declspec(uuid("{C38D254C-4C40-4192-A746-AC6FE519831E}"))
  __declspec(novtable)
IPassthroughObject : public IUnknown
{
  STDMETHOD(SetTargetUnknown)(IUnknown* punkTarget) = 0;
};

#if _ATL_VER < 0x700
#define InlineIsEqualGUID ::ATL::InlineIsEqualGUID
#else
#define InlineIsEqualGUID ::InlineIsEqualGUID
#endif

#endif // PASSTHROUGHAPP_PASSTHROUGHOBJECT_H
