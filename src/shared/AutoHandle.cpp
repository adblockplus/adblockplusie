#include PRECOMPILED_HEADER_FILE

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

HANDLE AutoHandle::get()
{
  return handle;
}
