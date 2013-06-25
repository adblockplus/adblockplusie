#ifndef CRITICAL_SECTION_H
#define CRITICAL_SECTION_H

class CriticalSection
{
public:
  CriticalSection()
  {
    InitializeCriticalSection(&section);
  }

  ~CriticalSection()
  {
    DeleteCriticalSection(&section);
  }

  class Lock
  {
  public:
    Lock(CriticalSection& cs)
        : section(&cs.section)
    {
      EnterCriticalSection(section);
    }

    ~Lock()
    {
      LeaveCriticalSection(section);
    }
  private:
    LPCRITICAL_SECTION section;
    Lock(const Lock&);
    Lock& operator=(const Lock&);
  };
private:
  CRITICAL_SECTION section;
  CriticalSection(const CriticalSection&);
  CriticalSection& operator=(const CriticalSection&);
};
#endif