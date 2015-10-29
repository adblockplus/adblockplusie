#include <gtest/gtest.h>
#include "../process.h"
#include <functional>

// Turn off warnings for string copies
#pragma warning( disable : 4996 )

//-------------------------------------------------------
// Comparison objects
//-------------------------------------------------------

const wchar_t exactExeName[] = L"installer-ca-tests.exe";
const std::wstring exactExeString(exactExeName);
const WstringCaseInsensitive exactExeStringCi(exactExeName);

const wchar_t mixedcaseExeName[] = L"Installer-CA-Tests.exe";
const WstringCaseInsensitive mixedcaseExeStringCi(mixedcaseExeName);

const wchar_t unknownName[] = L"non-matching-name";
const wchar_t* multipleExeNames[] = {mixedcaseExeName, unknownName};

/**
 * Compare to our own process name, case-sensitive, no length limit
 */
struct OurProcessByName
  : std::unary_function<PROCESSENTRY32W, bool>
{
  bool operator()(const PROCESSENTRY32W& process)
  {
    return std::wstring(process.szExeFile) == exactExeString;
  };
};

/**
 * Compare to our own process name, case-insensitive, no length limit
 */
struct OurProcessByNameCi
  : std::unary_function<PROCESSENTRY32W, bool>
{
  bool operator()(const PROCESSENTRY32W& process)
  {
    return WstringCaseInsensitive(process.szExeFile) == mixedcaseExeStringCi;
  };
};

//-------------------------------------------------------
//-------------------------------------------------------
/**
 * Filter by process name. Comparison is case-insensitive.
 */
class ProcessByAnyFileNameCi
  : public std::unary_function<PROCESSENTRY32W, bool>
{
  const FileNameSet& names;
public:
  bool operator()(const PROCESSENTRY32W& process)
  {
    return names.find(process.szExeFile) != names.end();
  }
  ProcessByAnyFileNameCi(const FileNameSet& names)
    : names(names)
  {}
};

/**
 * Filter by process name. Comparison is case-insensitive.
 */
class ProcessByNameCi
  : public std::unary_function<PROCESSENTRY32W, bool>
{
  const WstringCaseInsensitive _name;
public:
  bool operator()(const PROCESSENTRY32W& process)
  {
    return _name == WstringCaseInsensitive(process.szExeFile);
  }

  ProcessByNameCi(const wchar_t* name)
    : _name(name)
  {}
};

//-------------------------------------------------------
// TESTS, no snapshots
//-------------------------------------------------------
PROCESSENTRY32 ProcessWithName(const wchar_t* s)
{
  PROCESSENTRY32W p;
  wcsncpy(p.szExeFile, s, MAX_PATH);
  return p;
}

PROCESSENTRY32 processEmpty = ProcessWithName(L"");
PROCESSENTRY32 processExact = ProcessWithName(exactExeName);
PROCESSENTRY32 processMixedcase = ProcessWithName(mixedcaseExeName);
PROCESSENTRY32 processExplorer = ProcessWithName(L"explorer.exe");
PROCESSENTRY32 processAbsent = ProcessWithName(L"no_such_name");

FileNameSet multipleNameSet(multipleExeNames);
ProcessByAnyFileNameCi findInSet(multipleNameSet);
ProcessByAnyExeNotImmersive findInSetNotImmersive(multipleNameSet);


TEST(FileNameSet, ValidateSetup)
{
  ASSERT_EQ(2u, multipleNameSet.size());
  ASSERT_TRUE(multipleNameSet.find(exactExeStringCi) != multipleNameSet.end());
  ASSERT_TRUE(multipleNameSet.find(mixedcaseExeStringCi) != multipleNameSet.end());
  ASSERT_TRUE(multipleNameSet.find(L"") == multipleNameSet.end());
  ASSERT_TRUE(multipleNameSet.find(L"not-in-list") == multipleNameSet.end());
}

TEST(ProcessByAnyFileNameCi, Empty)
{
  FileNameSet s;
  ProcessByAnyFileNameCi x(s);

  ASSERT_FALSE(x(processEmpty));
  ASSERT_FALSE(x(processExact));
  ASSERT_FALSE(x(processMixedcase));
  ASSERT_FALSE(x(processExplorer));
  ASSERT_FALSE(x(processAbsent));
}

TEST(ProcessByAnyFileNameCi, SingleElementKnown)
{
  const wchar_t* elements[1] = {exactExeName};
  FileNameSet s(elements);
  ProcessByAnyFileNameCi x(s);

  ASSERT_FALSE(x(processEmpty));
  ASSERT_TRUE(x(processExact));
  ASSERT_TRUE(x(processMixedcase));
  ASSERT_FALSE(x(processExplorer));
  ASSERT_FALSE(x(processAbsent));
}

TEST(ProcessByAnyFileNameCi, SingleElementUnknown)
{
  const wchar_t* elements[1] = {unknownName};
  FileNameSet s(elements);
  ProcessByAnyFileNameCi x(s);

  ASSERT_FALSE(x(processEmpty));
  ASSERT_FALSE(x(processExact));
  ASSERT_FALSE(x(processMixedcase));
  ASSERT_FALSE(x(processExplorer));
  ASSERT_FALSE(x(processAbsent));
}

TEST(ProcessByAnyFileNameCi, TwoElements)
{
  FileNameSet s(multipleExeNames);
  ProcessByAnyFileNameCi x(s);

  ASSERT_FALSE(findInSet(processEmpty));
  ASSERT_TRUE(findInSet(processExact));
  ASSERT_TRUE(findInSet(processMixedcase));
  ASSERT_FALSE(findInSet(processExplorer));
  ASSERT_FALSE(findInSet(processAbsent));
}

//-------------------------------------------------------
// Single-snapshot version of initializers
//-------------------------------------------------------
/**
 * Single-snapshot version of InitializeProcessList, for testing.
 */
template<class T, class Admittance, class Extractor>
void InitializeProcessList(std::vector<T>& v, Admittance admit = Admittance(), Extractor extract = Extractor())
{
  InitializeProcessList(v, ProcessSnapshot(), admit, extract);
}

/**
 * Single-snapshot version of InitializeProcessSet, for testing.
 */
template<class T, class Admittance, class Extractor>
void InitializeProcessSet(std::set<T>& s, Admittance admit = Admittance(), Extractor extract = Extractor())
{
  InitializeProcessSet(s, ProcessSnapshot(), admit, extract);
}

//-------------------------------------------------------
// TESTS with snapshots
//-------------------------------------------------------
/**
 * Construction test ensures that we don't throw and that at least one process shows up.
 */
TEST(ProcessListTest, ConstructVector)
{
  std::vector<PROCESSENTRY32W> v;
  InitializeProcessList(v, EveryProcess(), CopyAll());
  ASSERT_GE(v.size(), 1u);
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 */
TEST(ProcessListTest, FindOurProcess)
{
  std::vector<PROCESSENTRY32W> v;
  InitializeProcessList(v, OurProcessByName(), CopyAll());
  size_t size(v.size());
  EXPECT_EQ(1u, size);      // Please, don't run multiple test executables simultaneously
  ASSERT_GE(1u, size);
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 * This test uses same one used in Process_Closer
 */
TEST(ProcessListTest, FindOurProcessCiGeneric)
{
  std::vector<PROCESSENTRY32W> v;
  InitializeProcessList(v, ProcessByNameCi(mixedcaseExeName), CopyAll());
  size_t size(v.size());
  EXPECT_EQ(1u, size);      // Please, don't run multiple test executables simultaneously
  ASSERT_GE(1u, size);
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 * This test uses the generic filter function.
 */
TEST(ProcessListTest, FindOurProcessCiAsUsed)
{
  std::vector<PROCESSENTRY32W> v;
  InitializeProcessList(v, ProcessByAnyFileNameCi(FileNameSet(multipleExeNames)), CopyAll());
  size_t size(v.size());
  EXPECT_EQ(1u, size);      // Please, don't run multiple test executables simultaneously
  ASSERT_GE(1u, size);
}

/**
 * Locate the PID of our process.
 */
TEST(ProcessListTest, FindOurPid)
{
  std::vector<DWORD> v;
  InitializeProcessList(v, OurProcessByName(), CopyPid());
  size_t size(v.size());
  EXPECT_EQ(size, 1u);      // Please, don't run multiple test executables simultaneously
  ASSERT_GE(size, 1u);
}

/**
 * Locate the PID of our process using the
 */
TEST(ProcessListTest, FindOurProcessInSet)
{
  std::vector<DWORD> v;
  InitializeProcessList(v, findInSet, CopyPid());
  size_t size(v.size());
  EXPECT_EQ(size, 1u);      // Please, don't run multiple test executables simultaneously
  ASSERT_GE(size, 1u);
}

//-------------------------------------------------------
// TESTS for process ID sets
//-------------------------------------------------------
/*
 * Can't use copy_all without a definition for "less<PROCESSENTRY32W>".
 * Thus all tests only use CopyPid
 */

/**
 * Construction test ensures that we don't throw and that at least one process shows up.
 */
TEST(PidSet, ConstructSet)
{
  std::set<DWORD> s;
  InitializeProcessSet(s, EveryProcess(), CopyPid());
  ASSERT_GE(s.size(), 1u);
}

TEST(PidSet, FindOurProcessInSet)
{
  std::set<DWORD> s;
  InitializeProcessSet(s, findInSet, CopyPid());
  size_t size(s.size());
  EXPECT_EQ(size, 1u);
  ASSERT_GE(size, 1u);
}

TEST(PidSet, FindOurProcessInSetNotImmersive)
{
  std::set<DWORD> s;
  InitializeProcessSet(s, findInSetNotImmersive, CopyPid());
  size_t size(s.size());
  EXPECT_EQ(size, 1u);
  ASSERT_GE(size, 1u);
}