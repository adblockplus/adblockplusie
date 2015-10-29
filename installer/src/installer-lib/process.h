/**
 * \file process.h
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "installer-lib.h"
#include "handle.h"
#include "session.h"

#include <string>
#include <cctype>
#include <vector>
#include <set>
#include <algorithm>
#include <memory>

#include <Windows.h>
#include <TlHelp32.h>

//-------------------------------------------------------
// WstringCaseInsensitive: case-insensitive wide string
//-------------------------------------------------------

/**
 * Traits class for case-insensitive strings.
 */
template<class T>
struct CaseInsensitiveTraits: std::char_traits<T>
{
  static bool eq(T c1, T c2)
  {
    return std::tolower(c1) == std::tolower(c2);
  }

  static bool lt(T c1, T c2)
  {
    return std::tolower(c1) < std::tolower(c2);
  }

  /**
   * Trait comparison function.
   *
   * Note that this is not a comparison of C-style strings.
   * In particular, there's no concern over null characters '\0'.
   * The argument 'n' is the minimum length of the two strings being compared.
   * We may assume that the intervals p1[0..n) and p2[0..n) are both valid substrings.
   */
  static int compare(const T* p1, const T* p2, size_t n)
  {
    while (n-- > 0)
    {
      T l1 = std::tolower(* p1 ++);
      T l2 = std::tolower(* p2 ++);
      if (l1 == l2)
      {
        continue;
      }
      return (l1 < l2) ? -1 : +1;
    }
    return 0;
  }
};

typedef std::basic_string<wchar_t, CaseInsensitiveTraits<wchar_t>> WstringCaseInsensitive;

//-------------------------------------------------------
// FileNameSet: case-insensitive wide-string set
//-------------------------------------------------------
struct FileNameSet
  : public std::set<WstringCaseInsensitive>
{
  /**
   * Empty set constructor.
   */
  FileNameSet()
  {}

  /**
   * Constructor initialization from an array.
   */
  template<size_t nFileNames>
  FileNameSet(const wchar_t* (& fileNameList)[nFileNames])
  {
    for (unsigned int j = 0 ; j < nFileNames ; ++ j)
    {
      insert(WstringCaseInsensitive(fileNameList[j]));
    }
  }
};

//-------------------------------------------------------
//-------------------------------------------------------
/**
 * Filter by process name. Comparison is case-insensitive. Windows Store app processes excluded
 */
class ProcessByAnyExeNotImmersive
  : public std::unary_function<PROCESSENTRY32W, bool>
{
  /**
   * Set of file names from which to match candidate process names.
   *
   * This is a reference to, not a copy of, the set.
   * The lifetime of this object must be subordinate to that of its referent.
   * The set used to instantiate this class is a member of ProcessCloser,
   *   and so also is this class.
   * Hence the lifetimes are coterminous, and the reference is not problematic.
   */
  const FileNameSet& processNames;
public:
  bool operator()(const PROCESSENTRY32W&);
  ProcessByAnyExeNotImmersive(const FileNameSet& names) : processNames(names) {}
};


//-------------------------------------------------------
// Process utility functions.
//-------------------------------------------------------
/**
 * A promiscuous filter admits everything.
 */
struct EveryProcess
  : public std::unary_function<PROCESSENTRY32W, bool>
{
  bool operator()(const PROCESSENTRY32W&)
  {
    return true ;
  };
};

/**
 * Extractor that copies the entire process structure.
 */
struct CopyAll
  : public std::unary_function<PROCESSENTRY32W, PROCESSENTRY32W>
{
  PROCESSENTRY32W operator()(const PROCESSENTRY32W& process)
  {
    return process ;
  }
};

/**
 * Extractor that copies only the PID.
 */
struct CopyPid
  : public std::unary_function<PROCESSENTRY32W, DWORD>
{
  inline DWORD operator()(const PROCESSENTRY32W& process)
  {
    return process.th32ProcessID ;
  }
};

/**
 * Retrieve the process ID that created a window.
 *
 * Wrapper around GetWindowThreadProcessId.
 * Converts an error return from the system call into an exception.
 * The system call can also retrieve the creating thread; we ignore it.
 *
 * \param window
 *   Handle of the window
 * \return
 *   ID of the process that created the argument window
 *
 * \sa
 *   MSDN [GetWindowThreadProcessId function](http://msdn.microsoft.com/en-us/library/windows/desktop/ms633522%28v=vs.85%29.aspx)
 */
DWORD CreatorProcess(HWND window);

//-------------------------------------------------------
// Snapshot
//-------------------------------------------------------
/**
 * Traits class for snapshots of all processes on the system.
 */
struct ProcessSnapshotTraits
{
  /**
   * The type of the data resulting from CreateToolhelp32Snapshot.
   */
  typedef PROCESSENTRY32W ResultType;

  /**
   * Flags used to call CreateToolhelp32Snapshot.
   */
  const static DWORD SnapshotFlags = TH32CS_SNAPPROCESS;

  /**
   * Wrapper for 'first' function for processes
   */
  static BOOL First(HANDLE arg1, LPPROCESSENTRY32 arg2)
  {
    return ::Process32First(arg1, arg2);
  }

  /**
   * Wrapper for 'next' function for processes
   */
  static BOOL Next(HANDLE arg1, LPPROCESSENTRY32 arg2)
  {
    return ::Process32Next(arg1, arg2);
  }
};

/**
 * Traits class for snapshots of all modules loaded by a process.
 */
struct ModuleSnapshotTraits
{
  /**
   * The type of the data resulting from CreateToolhelp32Snapshot.
   */
  typedef MODULEENTRY32W ResultType;

  /**
   * Flags used to call CreateToolhelp32Snapshot.
   */
  const static DWORD SnapshotFlags = TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32;

  /**
   * Wrapper for 'first' function for modules
   */
  static BOOL First(HANDLE arg1, LPMODULEENTRY32 arg2)
  {
    return ::Module32First(arg1, arg2);
  }

  /**
   * Wrapper for 'next' function for modules
   */
  static BOOL Next(HANDLE arg1, LPMODULEENTRY32 arg2)
  {
    return ::Module32Next(arg1, arg2);
  }
};

/**
 * A snapshot wrapping the results of CreateToolhelp32Snapshot system call.
 *
 * Unfortunately, we cannot provide standard iterator for this class.
 * Standard iterators must be copy-constructible, which entails the possibility of multiple, coexisting iteration states.
 * The iteration behavior provided by Process32First and Process32Next relies upon state held within the snapshot itself.
 * Thus, there can be only one iterator at a time for the snapshot.
 * The two requirements are not simultaneously satisfiable.
 *
 * Instead of a standard iterator, we provide a First() and Next() functions wrapping the corresponding system calls.
 *
 * \par Implementation
 *
 * - MSDN [CreateToolhelp32Snapshot function](http://msdn.microsoft.com/en-us/library/windows/desktop/ms682489%28v=vs.85%29.aspx)
 * - MSDN [Process32First function](http://msdn.microsoft.com/en-us/library/windows/desktop/ms684834%28v=vs.85%29.aspx)
 * - MSDN [Process32Next function](http://msdn.microsoft.com/en-us/library/windows/desktop/ms684836%28v=vs.85%29.aspx)
 * - MSDN [PROCESSENTRY32 structure](http://msdn.microsoft.com/en-us/library/windows/desktop/ms684839%28v=vs.85%29.aspx)
 *
 * \par Design Note
 *   The traits class defines First() and Next() functions instead of using function pointers.
 *   This arises from a limitation in the compiler.
 *   The system calls are declared 'WINAPI', which is a compiler-specific extension.
 *   That extension, however, does not go far enough to be able to declare a pointer with the same modifier.
 *   Hence the system calls must be called directly; they are wrapped in the trait functions.
 */
template<class Traits>
class Snapshot
{
public:
  /**
   * Expose the result type from the traits class as our own.
   */
  typedef typename Traits::ResultType ResultType;

private:
  /**
   * Process ID argument for CreateToolhelp32Snapshot.
   */
  DWORD id;

  /**
   * Handle to the underlying snapshot.
   */
  WindowsHandle handle;

  /**
   * Buffer for reading a single process entry out of the snapshot.
   *
   * This buffer is constant insofar as the code outside this class is concerned.
   * The accessor functions First() and Next() return pointers to constant ResultType.
   */
  ResultType buffer;

  /**
   * Copy constructor declared private and not defined.
   *
   * \par Implementation
   *   Add "= delete" for C++11.
   */
  Snapshot(const Snapshot&);

  /**
   * Copy assignment declared private and not defined.
   *
   * \par Implementation
   *   Add "= delete" for C++11.
   */
  Snapshot operator=(const Snapshot&);

  /**
   * Create a new snapshot and return its handle.
   */
  WindowsHandle::HandleType MakeHandle()
  {
    WindowsHandle::HandleType h = ::CreateToolhelp32Snapshot(Traits::SnapshotFlags, id);
    if (h == INVALID_HANDLE_VALUE)
    {
      throw WindowsApiError("CreateToolhelp32Snapshot", "INVALID_HANDLE_VALUE");
    }
    return h;
  }

protected:
  /**
   * Constructor takes a snapshot.
   */
  Snapshot(DWORD id)
    : id(id), handle(MakeHandle())
  {
    // The various result types all define 'dwSize' with the same semantics.
    buffer.dwSize = sizeof(ResultType);
  }

public:
  /**
   * Reconstruct the current instance with a new system snapshot.
   *
   * This function uses reinitialization assignment in the WindowsHandle class,
   *   which takes care of closing the old handle.
   */
  void Refresh()
  {
    handle = MakeHandle();
  }

  /**
   * Retrieve the first snapshot item into our member buffer.
   *
   * \return
   *   Pointer to our member buffer if there was a first item
   *   0 otherwise
   *
   * \par Design Note
   *   There's no error handling in the present version of this function.
   *   In part that's because the underlying system call returns either true or false, both of which are ordinarily valid answers.
   *   The trouble is that a false return is overloaded.
   *   It can mean either that (ordinary) there are no more items or (exceptional) the snapshot did not contain the right kind of item.
   *   GetLastError is no help here; it doesn't distinguish between these cases.
   *   The upshot is that we rely that our implementation calls the right functions on the snapshot,
   *     and so we ignore the case where we've passed bad arguments to the system call.
   */
  const ResultType* First()
  {
    return Traits::First(handle, &buffer) ? &buffer : 0;
  }

  /**
   * Retrieve the next snapshot item into our member buffer and return a pointer to it.
   * begin() must have been called first.
   *
   * \return
   *   Pointer to our member buffer if there was a first item
   *   0 otherwise
   *
   * \par Design Note
   *   See the Design Note for First(); the same considerations apply here.
   */
  const ResultType* Next()
  {
    return Traits::Next(handle, &buffer) ? &buffer : 0;
  }
};

/**
 * A snapshot of all processes running on the system.
 */
struct ProcessSnapshot
  : public Snapshot<ProcessSnapshotTraits>
{
  ProcessSnapshot()
    : Snapshot(0)
  {}
};

/**
 * A snapshot of all modules loaded for a given process.
 */
struct ModuleSnapshot
  : public Snapshot<ModuleSnapshotTraits>
{
  ModuleSnapshot(DWORD processId)
    : Snapshot(processId)
  {}
};

//-------------------------------------------------------
// InitializeProcessList
//-------------------------------------------------------
/**
 * \tparam T The type into which a PROCESSENTRY32W struture is extracted.
 * \tparam Admittance Function type for argument 'admit'
 * \tparam Extractor Function type for argument 'extract'
 * \param admit A unary predicate function class that determines what's included
 *   A process appears in the list only if the predicate returns true.
 *   The use of this predicate is analogous to that in std::copy_if.
 * \param convert A conversion function that takes a PROCESSENTRY32W as input argument and returns an element of type T.
 */
template<class T, class Admittance, class Extractor>
void InitializeProcessList(std::vector<T>& v, ProcessSnapshot& snap, Admittance admit = Admittance(), Extractor extract = Extractor())
{
  const PROCESSENTRY32W* p = snap.First();
  while (p != 0)
  {
    if (admit(*p))
    {
      /*
       * We don't have C++11 emplace_back, which can construct the element in place.
       * Instead, we copy the return value of the converter.
       */
      v.push_back(extract(*p));
    }
    p = snap.Next();
  }
};

//-------------------------------------------------------
// InitializeProcessSet
//-------------------------------------------------------
/**
 * \tparam T The type into which a PROCESSENTRY32W struture is extracted.
 * \tparam Admittance Function type for argument 'admit'
 * \tparam Extractor Function type for argument 'extract'
 * \param admit A unary predicate function class that determines what's included
 *   A process appears in the list only if the predicate returns true.
 *   The use of this predicate is analogous to that in std::copy_if.
 * \param convert A conversion function that takes a PROCESSENTRY32W as input argument and returns an element of type T.
 */
template<class T, class Admittance, class Extractor>
void InitializeProcessSet(std::set<T>& set, ProcessSnapshot& snap, Admittance admit = Admittance(), Extractor extract = Extractor())
{
  const PROCESSENTRY32W* p = snap.First();
  while (p != 0)
  {
    if (admit(*p))
    {
      set.insert(extract(*p));
    }
    p = snap.Next();
  }
};

//-------------------------------------------------------
// EnumerateWindows
//-------------------------------------------------------

/**
 * States of a window enumeration.
 */
typedef enum
{
  started,    ///< The iteration is currently running
  normal,     ///< Iteration terminated without error.
  early,      ///< Callback returned false and terminated iteration early.
  exception,  ///< Callback threw an exception and thereby terminated iteration.
  error       ///< Callback always return true but EnumWindows failed.
} EnumerateWindowsState;

/**
 * Data to perform a window enumeration, shared between the main function and the callback function.
 */
template<class F>
struct EWData
{
  /**
   * Function to be applied to each enumerated window.
   */
  F& f;

  /**
   * Completion status of the enumeration.
   */
  EnumerateWindowsState status;

  /**
   * An exception to be transported across the callback.
   *
   * The enumerator and the callback are not guaranteed to share a call stack,
   *   nor need they even share compatible exception conventions,
   *   and might not even be in the same thread.
   * Thus, if the applied function throws an exception,
   *   we catch it in the callback and re-throw it in the enumerator.
   * This member holds such an exception.
   *
   * This member holds an exception only if 'status' has the value 'exception'.
   * Otherwise it's a null pointer.
   */
  std::unique_ptr<std::exception> ee;

  /**
   * Ordinary constructor.
   */
  EWData(F& f)
    : f(f), status(started)
  {}
};

/**
 * Callback function for EnumWindows.
 *
 * This function provides two standard behaviors.
 * It records early termination of the enumeration, should that happen by the applied function returning false.
 * It captures any exception thrown for transport back to the enumerator.
 */
template<class F>
BOOL CALLBACK EnumerationCallback(HWND window, LPARAM x)
{
  // LPARAM is always the same size as a pointer
  EWData<F>* data = reinterpret_cast<EWData<F> *>(x);
  /*
   * Top-level try statement prevents exception from propagating back to system.
   */
  try
  {
    bool r = data -> f(window);
    if (! r)
    {
      data -> status = early;
    }
    return r;
  }
  catch (std::exception e)
  {
    data -> ee = std::unique_ptr<std::exception>(new(std::nothrow) std::exception(e));
    data -> status = exception;
    return FALSE;
  }
  catch (...)
  {
    data -> ee = std::unique_ptr<std::exception>();
    data -> status = exception;
    return FALSE;
  }
}

/**
 * Enumerate windows, applying a function to each one.
 */
template<class F>
bool EnumerateWindows(F f)
{
  EWData<F> data(f);
  BOOL x(::EnumWindows(EnumerationCallback<F>, reinterpret_cast<LPARAM>(& data)));
  bool r;
  if (data.status != started)
  {
    // Assert status was changed within the callback
    if (data.status == exception)
    {
      /*
       * The callback threw an exception of some sort.
       * We forward it to the extent we are able.
       */
      if (data.ee)
      {
        throw* data.ee;
      }
      else
      {
        throw std::runtime_error("Unknown exception thrown in callback function.");
      }
    }
    r = false;
  }
  else
  {
    if (x)
    {
      data.status = normal;
      r = true;
    }
    else
    {
      // Assert EnumWindows failed
      data.status = error;
      r = false;
    }
  }
  return r;
}

//-------------------------------------------------------
// ProcessCloser
//-------------------------------------------------------
class ProcessCloser
{
  /**
   * Set of process identifiers matching one of the executable names.
   */
  std::set<DWORD> pidSet;

  /**
   * Set of executable names by which to filter.
   *
   * The argument of the filter constructor is a set by reference.
   * Since it does not make a copy for itself, we define it as a class member to provide its allocation.
   */
  FileNameSet processNames;

  ProcessByAnyExeNotImmersive filter;

  /**
   * Copy function object copies just the process ID.
   */
  CopyPid copy;

  /**
   * Snapshot of running processes.
   */
  ProcessSnapshot& snapshot;

  void update()
  {
    InitializeProcessSet(pidSet, snapshot, filter, copy);
  };

  template<class F>
  class OnlyOurProcesses
  {
    ProcessCloser& self;

    F f;

  public:
    OnlyOurProcesses(ProcessCloser& self, F f)
      : f(f), self(self)
    {}

    bool operator()(HWND window)
    {
      bool b;
      try
      {
        b = self.Contains(CreatorProcess(window));
      }
      catch (...)
      {
        // ignore window handles that are no longer valid
        return true;
      }
      if (! b)
      {
        // Assert the process that created the window is not in our pidSet
        return true;
      }
      return f(window);
    }
  };

public:
  template <size_t nFileNames>
  ProcessCloser(ProcessSnapshot& snapshot, const wchar_t* (&fileNameList)[nFileNames])
    : snapshot(snapshot), processNames(fileNameList), filter(processNames)
  {
    update();
  }

  /**
   * Refresh our state to match the snapshot state.
   */
  void Refresh()
  {
    pidSet.clear();
    update();
  }

  bool IsRunning()
  {
    return ! pidSet.empty() ;
  };

  bool Contains(DWORD pid) const
  {
    return pidSet.find(pid) != pidSet.end() ;
  };

  template<class F>
  bool IterateOurWindows(F f)
  {
    OnlyOurProcesses<F> g(* this, f);
    return EnumerateWindows(g);
  }

  /*
   * Shut down every process in the pidSet.
   */
  bool ShutDown(ImmediateSession& session);

};

#endif
