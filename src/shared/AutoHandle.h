#ifndef AUTO_HANDLE_H
#define AUTO_HANDLE_H

#include <Windows.h>

class AutoHandle
{
public:
  AutoHandle();
  explicit AutoHandle(HANDLE handle);
  ~AutoHandle();
  operator HANDLE();

private:
  HANDLE handle;

  AutoHandle(const AutoHandle& autoHandle);
  AutoHandle& operator=(const AutoHandle& autoHandle);
};

#endif
