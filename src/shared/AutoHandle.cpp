#include "AutoHandle.h"

AutoHandle::AutoHandle()
{
}

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
