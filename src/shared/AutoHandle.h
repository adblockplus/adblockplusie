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

  AutoHandle(const AutoHandle&);
  AutoHandle& operator=(const AutoHandle&);
};

#endif
