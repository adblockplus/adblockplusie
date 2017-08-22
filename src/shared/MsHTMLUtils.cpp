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

#include "MsHTMLUtils.h"

GetHtmlElementAttributeResult GetHtmlElementAttribute(IHTMLElement& htmlElement,
  const ATL::CComBSTR& attributeName)
{
  GetHtmlElementAttributeResult retValue;
  ATL::CComVariant vAttr;
  ATL::CComPtr<IHTMLElement4> htmlElement4;
  if (FAILED(htmlElement.QueryInterface(&htmlElement4)) || !htmlElement4)
  {
    return retValue;
  }
  ATL::CComPtr<IHTMLDOMAttribute> attributeNode;
  if (FAILED(htmlElement4->getAttributeNode(attributeName, &attributeNode)) || !attributeNode)
  {
    return retValue;
  }
  // we set that attribute found but it's not necessary that we can retrieve its value
  retValue.isAttributeFound = true;
  if (FAILED(attributeNode->get_nodeValue(&vAttr)))
  {
    return retValue;
  }
  if (vAttr.vt == VT_BSTR && vAttr.bstrVal)
  {
    retValue.attributeValue = vAttr.bstrVal;
  }
  else if (vAttr.vt == VT_I4)
  {
    retValue.attributeValue = std::to_wstring(vAttr.iVal);
  }
  return retValue;
}