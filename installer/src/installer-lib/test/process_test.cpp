#include <gtest/gtest.h>
#include "../process.h"
#include <functional>

// Turn off warnings for string copies
#pragma warning( disable : 4996 )

//-------------------------------------------------------
// Comparison objects
//-------------------------------------------------------

const wchar_t exact_exe_name[] = L"installer-ca-tests.exe";
const std::wstring exact_exe_string(exact_exe_name);
const WstringCaseInsensitive exact_exe_string_ci(exact_exe_name);

const wchar_t mixedcase_exe_name[] = L"Installer-CA-Tests.exe";
const WstringCaseInsensitive mixedcase_exe_string_ci(mixedcase_exe_name);

const wchar_t unknown_name[] = L"non-matching-name";
const wchar_t* multiple_exe_names[] = { mixedcase_exe_name, unknown_name };

/**
 * Compare to our own process name, case-sensitive, no length limit
 */
struct our_process_by_name
  : std::unary_function<PROCESSENTRY32W, bool>
{
  bool operator()(const PROCESSENTRY32W& process)
  {
    return std::wstring(process.szExeFile) == exact_exe_string;
  };
};

/**
 * Compare to our own process name, case-insensitive, no length limit
 */
struct our_process_by_name_CI
  : std::unary_function<PROCESSENTRY32W, bool>
{
  bool operator()(const PROCESSENTRY32W& process)
  {
    return WstringCaseInsensitive(process.szExeFile) == mixedcase_exe_string_ci;
  };
};

//-------------------------------------------------------
//-------------------------------------------------------
/**
 * Filter by process name. Comparison is case-insensitive.
 */
class process_by_any_file_name_CI
  : public std::unary_function<PROCESSENTRY32W, bool>
{
  const FileNameSet& names;
public:
  bool operator()(const PROCESSENTRY32W& process)
  {
    return names.find(process.szExeFile) != names.end();
  }
  process_by_any_file_name_CI(const FileNameSet& names)
    : names(names)
  {}
};

/**
 * Filter by process name. Comparison is case-insensitive.
 */
class process_by_name_CI
  : public std::unary_function<PROCESSENTRY32W, bool>
{
  const WstringCaseInsensitive _name;
public:
  bool operator()(const PROCESSENTRY32W& process)
  {
    return _name == WstringCaseInsensitive(process.szExeFile);
  }

  process_by_name_CI(const wchar_t* name)
    : _name(name)
  {}
};

//-------------------------------------------------------
// TESTS, no snapshots
//-------------------------------------------------------
PROCESSENTRY32 process_with_name(const wchar_t* s)
{
  PROCESSENTRY32W p;
  wcsncpy(p.szExeFile, s, MAX_PATH);
  return p;
}

PROCESSENTRY32 process_empty = process_with_name(L"");
PROCESSENTRY32 process_exact = process_with_name(exact_exe_name);
PROCESSENTRY32 process_mixedcase = process_with_name(mixedcase_exe_name);
PROCESSENTRY32 process_explorer = process_with_name(L"explorer.exe");
PROCESSENTRY32 process_absent = process_with_name(L"no_such_name");

FileNameSet multiple_name_set(multiple_exe_names);
process_by_any_file_name_CI find_in_set(multiple_name_set);
ProcessByAnyExeNotImmersive find_in_set_not_immersive(multiple_name_set);


TEST(file_name_set, validate_setup)
{
  ASSERT_EQ(2u, multiple_name_set.size());
  ASSERT_TRUE(multiple_name_set.find(exact_exe_string_ci) != multiple_name_set.end());
  ASSERT_TRUE(multiple_name_set.find(mixedcase_exe_string_ci) != multiple_name_set.end());
  ASSERT_TRUE(multiple_name_set.find(L"") == multiple_name_set.end());
  ASSERT_TRUE(multiple_name_set.find(L"not-in-list") == multiple_name_set.end());
}

TEST(process_by_any_file_name_CI, empty)
{
  FileNameSet s;
  process_by_any_file_name_CI x(s);

  ASSERT_FALSE(x(process_empty));
  ASSERT_FALSE(x(process_exact));
  ASSERT_FALSE(x(process_mixedcase));
  ASSERT_FALSE(x(process_explorer));
  ASSERT_FALSE(x(process_absent));
}

TEST(process_by_any_file_name_CI, single_element_known)
{
  const wchar_t* elements[1] = { exact_exe_name };
  FileNameSet s(elements);
  process_by_any_file_name_CI x(s);

  ASSERT_FALSE(x(process_empty));
  ASSERT_TRUE(x(process_exact));
  ASSERT_TRUE(x(process_mixedcase));
  ASSERT_FALSE(x(process_explorer));
  ASSERT_FALSE(x(process_absent));
}

TEST(process_by_any_file_name_CI, single_element_unknown)
{
  const wchar_t* elements[1] = { unknown_name };
  FileNameSet s(elements);
  process_by_any_file_name_CI x(s);

  ASSERT_FALSE(x(process_empty));
  ASSERT_FALSE(x(process_exact));
  ASSERT_FALSE(x(process_mixedcase));
  ASSERT_FALSE(x(process_explorer));
  ASSERT_FALSE(x(process_absent));
}

TEST(process_by_any_file_name_CI, two_elements)
{
  FileNameSet s(multiple_exe_names);
  process_by_any_file_name_CI x(s);

  ASSERT_FALSE(find_in_set(process_empty));
  ASSERT_TRUE(find_in_set(process_exact));
  ASSERT_TRUE(find_in_set(process_mixedcase));
  ASSERT_FALSE(find_in_set(process_explorer));
  ASSERT_FALSE(find_in_set(process_absent));
}

//-------------------------------------------------------
// Single-snapshot version of initializers
//-------------------------------------------------------
/**
 * Single-snapshot version of initialize_process_list, for testing.
 */
template<class T, class Admittance, class Extractor>
void initialize_process_list(std::vector<T>& v, Admittance admit = Admittance(), Extractor extract = Extractor())
{
  InitializeProcessList(v, ProcessSnapshot(), admit, extract);
}

/**
 * Single-snapshot version of initialize_process_set, for testing.
 */
template<class T, class Admittance, class Extractor>
void initialize_process_set(std::set<T>& s, Admittance admit = Admittance(), Extractor extract = Extractor())
{
  InitializeProcessSet(s, ProcessSnapshot(), admit, extract);
}

//-------------------------------------------------------
// TESTS with snapshots
//-------------------------------------------------------
/**
 * Construction test ensures that we don't throw and that at least one process shows up.
 */
TEST(Process_List_Test, construct_vector)
{
  std::vector<PROCESSENTRY32W> v;
  initialize_process_list(v, EveryProcess(), CopyAll());
  ASSERT_GE(v.size(), 1u);
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 */
TEST(Process_List_Test, find_our_process)
{
  std::vector<PROCESSENTRY32W> v;
  initialize_process_list(v, our_process_by_name(), CopyAll());
  size_t size(v.size());
  EXPECT_EQ(1u, size);      // Please, don't run multiple test executables simultaneously
  ASSERT_GE(1u, size);
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 * This test uses same one used in Process_Closer
 */
TEST(Process_List_Test, find_our_process_CI_generic)
{
  std::vector<PROCESSENTRY32W> v;
  initialize_process_list(v, process_by_name_CI(mixedcase_exe_name), CopyAll());
  size_t size(v.size());
  EXPECT_EQ(1u, size);      // Please, don't run multiple test executables simultaneously
  ASSERT_GE(1u, size);
}

/**
 * The only process we are really guaranteed to have is this test process itself.
 * This test uses the generic filter function.
 */
TEST(Process_List_Test, find_our_process_CI_as_used)
{
  std::vector<PROCESSENTRY32W> v;
  initialize_process_list(v, process_by_any_file_name_CI(FileNameSet(multiple_exe_names)), CopyAll());
  size_t size(v.size());
  EXPECT_EQ(1u, size);      // Please, don't run multiple test executables simultaneously
  ASSERT_GE(1u, size);
}

/**
 * Locate the PID of our process.
 */
TEST(Process_List_Test, find_our_PID)
{
  std::vector<DWORD> v;
  initialize_process_list(v, our_process_by_name(), CopyPID());
  size_t size(v.size());
  EXPECT_EQ(size, 1u);      // Please, don't run multiple test executables simultaneously
  ASSERT_GE(size, 1u);
}

/**
 * Locate the PID of our process using the
 */
TEST(Process_List_Test, find_our_process_in_set)
{
  std::vector<DWORD> v;
  initialize_process_list(v, find_in_set, CopyPID());
  size_t size(v.size());
  EXPECT_EQ(size, 1u);      // Please, don't run multiple test executables simultaneously
  ASSERT_GE(size, 1u);
}

//-------------------------------------------------------
// TESTS for process ID sets
//-------------------------------------------------------
/*
 * Can't use copy_all without a definition for "less<PROCESSENTRY32W>".
 * Thus all tests only use copy_PID
 */

/**
 * Construction test ensures that we don't throw and that at least one process shows up.
 */
TEST(pid_set, construct_set)
{
  std::set<DWORD> s;
  initialize_process_set(s, EveryProcess(), CopyPID());
  ASSERT_GE(s.size(), 1u);
}

TEST(pid_set, find_our_process_in_set)
{
  std::set<DWORD> s;
  initialize_process_set(s, find_in_set, CopyPID());
  size_t size(s.size());
  EXPECT_EQ(size, 1u);
  ASSERT_GE(size, 1u);
}

TEST(pid_set, find_our_process_in_set_not_immersive)
{
  std::set<DWORD> s;
  initialize_process_set(s, find_in_set_not_immersive, CopyPID());
  size_t size(s.size());
  EXPECT_EQ(size, 1u);
  ASSERT_GE(size, 1u);
}