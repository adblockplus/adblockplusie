/**
 * \file ATL_Deprecate.cpp Transient functions used during the ATL removal process.
 */

#include "ATL_Deprecate.h"

std::wstring ToWstring(const ATL::CString& s)
{
  std::wstring result(static_cast<const wchar_t *>(s));
  return result;
}

std::wstring to_wstring(const ATL::CString& s)
{
  return ToWstring(s);
}

ATL::CString ToCString(const std::wstring& s)
{
  return ATL::CString(s.c_str());
}

ATL::CString to_CString(const std::wstring& s)
{
  return ToCString(s);
}
