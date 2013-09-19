#ifndef AUTO_HANDLE_H
#define AUTO_HANDLE_H

#include <Windows.h>

class AutoHandle
{
public:
  explicit AutoHandle(HANDLE handle = 0);
  ~AutoHandle();
  operator HANDLE();
  operator PHANDLE();
  operator bool();

private:
  HANDLE handle;

  AutoHandle(const AutoHandle&);
  AutoHandle& operator=(const AutoHandle&);
};

#endif
