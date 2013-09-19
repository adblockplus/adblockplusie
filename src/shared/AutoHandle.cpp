#include "AutoHandle.h"

AutoHandle::AutoHandle(HANDLE handle) : handle(handle)
{
}

AutoHandle::~AutoHandle()
{
  CloseHandle(handle);
}

AutoHandle::operator HANDLE()
{
  return handle;
}

AutoHandle::operator PHANDLE()
{
  return &handle;
}

AutoHandle::operator bool()
{
  return handle != 0;
}
