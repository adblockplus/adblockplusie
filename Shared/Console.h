#pragma once

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(*x))
#endif 

#ifndef countof_1
#define countof_1(x) (sizeof(x)/sizeof(*x) - 1)
#endif 

#if defined(USE_CONSOLE)

  static volatile bool g_consoleCalled = false;
  static long g_consoleCount = -1;
	static bool s_hasError = false;

	inline void Console(int code, const char* moduleName, int count, const char* format, va_list args)
	{	
		DWORD lastError = GetLastError();

		if (1 == code) {
			s_hasError = true;
		}

		static bool s_log = false;
		if (!s_log)
		{
			s_log = true;

      char log[] = "Log \"";

      const int lenLog = sizeof(log) - 1;

      char title[lenLog + MAX_PATH + 2];
      strcpy_s(title, sizeof(title), log);

      char* pFileName = title + lenLog;

      GetModuleFileNameA(0, pFileName, MAX_PATH);
      char* pDelim = strrchr(pFileName, '\\') + 1;

      strcpy_s(pFileName, sizeof(title) - lenLog, pDelim); 

      int lenFileName = strlen(pFileName);
      *(pFileName + lenFileName) = '"';
      *(pFileName + lenFileName + 1) = 0;

      AllocConsole();
			SetConsoleTitleA(title);
		}
  
		SYSTEMTIME systemTime;
		GetLocalTime(&systemTime);

		char buf[256];
		if (-1 == count) {
		  wsprintfA(buf, "[%.2d:%.2d:%.2d] {%d} %s\n", systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds, GetCurrentThreadId(), format);
		} else {
		  wsprintfA(buf, "[%.2d:%.2d:%.2d] {%d} [%s %d] %s\n", systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds, GetCurrentThreadId(), moduleName, count, format);
		}

		HANDLE hError = GetStdHandle(STD_ERROR_HANDLE);

		WORD color;
		if (0 == code) {
			color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		} else if (1 == code) {
			color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		} else if (2 == code) {
			color = FOREGROUND_RED | FOREGROUND_INTENSITY;
    }

		SetConsoleTextAttribute(hError, color);

		char out[2048];
		size_t size = wvsprintfA(out, buf, args);
		out[1024] = 0;
		int len = min(size, 1024);

		DWORD dwOutput;
		WriteConsoleA(hError, out, len, &dwOutput, 0);
		FlushConsoleInputBuffer(hError);

		SetLastError(lastError);
	}

  inline void Console(int code, const char** pFormat)
  {
		g_consoleCalled = true;

  #ifdef CONSOLE_NAME
    #define CONSOLE_MODULE_NAME CONSOLE_NAME
    InterlockedIncrement(&g_consoleCount);
  #else
    #define CONSOLE_MODULE_NAME ""
  #endif

    va_list args;
    va_start(args, *pFormat);    
    Console(code, CONSOLE_MODULE_NAME, g_consoleCount, *pFormat, args);
    va_end(args);
  }

  inline void CONSOLE(const char* format, ...)
  {
    Console(0, &format);
  }

	inline void CONSOLE_WAIT(const char* format = "", ...)
	{
		if (format  &&  0 != format[0]) {
			Console(0, &format);
		}
		CONSOLE("HIT RETURN TO CONTINUE");
		TCHAR buf[128];
		DWORD nRead = 0;
		ReadConsole(GetStdHandle(STD_INPUT_HANDLE), buf, countof(buf), &nRead, 0);
	}

  inline void CONSOLE_WARN(const char* format, ...)
  {
    Console(1, &format);
  }

  inline void CONSOLE_ERROR(const char* format, ...)
  {
    Console(2, &format);
//		CONSOLE_WAIT("");
  }

	class ConsoleWaitReturn
	{
		int m_time;
	public:
		ConsoleWaitReturn(int time): m_time(time) {}
		~ConsoleWaitReturn()
		{
			if (s_hasError) {
				s_hasError = false;

				HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE); 

				TCHAR buf[128];
				DWORD nRead = 0;
				ReadConsole(hInput, buf, 128, &nRead, 0);
			} else {
				CONSOLE("!");
				if (m_time > 0) { 
					Sleep(m_time);
				}
			}
		}
	};

	#define CONSOLE_WAIT_RETURN(time) ConsoleWaitReturn g_consoleWaitReturn(time)

#else
  int RemoveConsole(...);

  #define CONSOLE sizeof RemoveConsole
  #define CONSOLE_ERROR sizeof RemoveConsole
	#define CONSOLE_WAIT_RETURN sizeof RemoveConsole
	#define CONSOLE_WAIT sizeof RemoveConsole
#endif

#define BREAK_IF_TRUE(x)					    if (x) break;
#define BREAK_IF_FALSE(x)							if (!(x)) break;
#define BREAK_IF_0(x)									BREAK_IF_FALSE(x)
#define BREAK_IF_FAILED(x)						if (FAILED(x)) break;
#define BREAK_IF_FAILED_OR_0(x, y)		if (FAILED(x)  ||  !(y)) break;
#define CONTINUE_IF_0(x)							if (!(x)) continue;
#define CONTINUE_IF_FAILED(x)					if (FAILED(x)) continue;
#define RETURN_IF_TRUE(x)							if (x) return;
#define RETURN_IF_FALSE(x)						if (!(x)) return;
#define RETURN_IF_0(x)								RETURN_IF_FALSE(x)
#define RETURN_IF_FAILED(x)						if (FAILED(x)) return;
#define RETURN_0_IF_0(x)							if (!(x)) return 0;
#define RETURN_0_IF_FAILED(x)					if (FAILED(x)) return 0;
#define RETURN_S_FALSE_IF_TRUE(x)			if (x) return S_FALSE;
#define RETURN_S_FALSE_IF_0(x)				if (!(x)) return S_FALSE;
#define RETURN_S_FALSE_IF_FAILED(x)		if (FAILED(x)) return S_FALSE;
#define RETURN_E_FAIL_IF_TRUE(x)			if (x) return E_FAIL;
#define RETURN_E_FAIL_IF_0(x)				  if (!(x)) return E_FAIL;
#define RETURN_E_FAIL_IF_FAILED(x)		if (FAILED(x)) return E_FAIL;
#define RETURN_R_IF_TRUE(r, x)				if (x) return r;
#define RETURN_R_IF_FALSE(r, x)				if (!(x)) return r;
#define RETURN_R_IF_0(r, x)						RETURN_R_IF_FALSE(r, x)
#define RETURN_R_IF_FAILED(r, x)			if (FAILED(x)) return r; 

#define WN_BREAK_IF_TRUE(x)					    if (x) { CONSOLE_WARN(#x); break; }
#define WN_BREAK_IF_FALSE(x)						if (!(x)) { CONSOLE_WARN("!"#x); break; }
#define WN_BREAK_IF_0(x)								WN_BREAK_IF_FALSE(x)
#define WN_BREAK_IF_FAILED(x)						if (FAILED(x)) { CONSOLE_WARN("!"#x); break; }
#define WN_BREAK_IF_FAILED_OR_0(x, y)		if (FAILED(x)  ||  !(y)) { CONSOLE_WARN("!"#y); break; }
#define WN_CONTINUE_IF_0(x)							if (!(x)) { CONSOLE_WARN("!"#x); continue; }
#define WN_CONTINUE_IF_FAILED(x)				if (FAILED(x)) { CONSOLE_WARN("!"#x); continue; }
#define WN_RETURN_IF_TRUE(x)						if (x) { CONSOLE_WARN(#x); return; }
#define WN_RETURN_IF_FALSE(x)						if (!(x)) { CONSOLE_WARN("!"#x); return; }
#define WN_RETURN_IF_0(x)								WN_RETURN_IF_FALSE(x)
#define WN_RETURN_IF_FAILED(x)					if (FAILED(x)) { CONSOLE_WARN("!"#x); return; }
#define WN_RETURN_0_IF_0(x)							if (!(x)) { CONSOLE_WARN("!"#x); return 0; }
#define WN_RETURN_0_IF_FAILED(x)				if (FAILED(x)) { CONSOLE_WARN("!"#x); return 0; }
#define WN_RETURN_S_FALSE_IF_TRUE(x)		if (x) { CONSOLE_WARN(#x); return S_FALSE; }
#define WN_RETURN_S_FALSE_IF_0(x)				if (!(x)) { CONSOLE_ERROR("!"#x); return S_FALSE; }
#define WN_RETURN_S_FALSE_IF_FAILED(x)	if (FAILED(x)) { CONSOLE_WARN("!"#x); return S_FALSE; }
#define WN_RETURN_E_FAIL_IF_TRUE(x)			if (x) { CONSOLE_WARN(#x); return E_FAIL; }
#define WN_RETURN_E_FAIL_IF_0(x)				if (!(x)) { CONSOLE_WARN("!"#x); return E_FAIL; }
#define WN_RETURN_E_FAIL_IF_FAILED(x)		if (FAILED(x)) { CONSOLE_WARN("!"#x); return E_FAIL; }
#define WN_RETURN_R_IF_TRUE(r, x)				if (x) { CONSOLE_WARN(#x); return r; }
#define WN_RETURN_R_IF_FALSE(r, x)			if (!(x)) { CONSOLE_WARN("!"#x); return r; }
#define WN_RETURN_R_IF_0(r, x)					WN_RETURN_R_IF_FALSE(r, x)
#define WN_RETURN_R_IF_FAILED(r, x)			if (FAILED(x)) { CONSOLE_WARN("!"#x); return r; }

#define ER_BREAK_IF_TRUE(x)					    if (x) { CONSOLE_ERROR(#x); break; }
#define ER_BREAK_IF_FALSE(x)						if (!(x)) { CONSOLE_ERROR("!"#x); break; }
#define ER_BREAK_IF_0(x)								ER_BREAK_IF_FALSE(x)
#define ER_BREAK_IF_FAILED(x)						if (FAILED(x)) { CONSOLE_ERROR("!"#x); break; }
#define ER_BREAK_IF_FAILED_OR_0(x, y)		if (FAILED(x)  ||  !(y)) { CONSOLE_ERROR("!"#y); break; }
#define ER_CONTINUE_IF_0(x)							if (!(x)) { CONSOLE_ERROR("!"#x); continue; }
#define ER_CONTINUE_IF_FAILED(x)				if (FAILED(x)) { CONSOLE_ERROR("!"#x); continue; }
#define ER_RETURN_IF_TRUE(x)						if (x) { CONSOLE_ERROR(#x); return; }
#define ER_RETURN_IF_FALSE(x)						if (!(x)) { CONSOLE_ERROR("!"#x); return; }
#define ER_RETURN_IF_0(x)								ER_RETURN_IF_FALSE(x)
#define ER_RETURN_IF_FAILED(x)					if (FAILED(x)) { CONSOLE_ERROR("!"#x); return; }
#define ER_RETURN_0_IF_0(x)							if (!(x)) { CONSOLE_ERROR("!"#x); return 0; }
#define ER_RETURN_0_IF_FAILED(x)				if (FAILED(x)) { CONSOLE_ERROR("!"#x); return 0; }
#define ER_RETURN_S_FALSE_IF_TRUE(x)		if (x) { CONSOLE_ERROR(#x); return S_FALSE; }
#define ER_RETURN_S_FALSE_IF_0(x)				if (!(x)) { CONSOLE_ERROR("!"#x); return S_FALSE; }
#define ER_RETURN_S_FALSE_IF_FAILED(x)	if (FAILED(x)) { CONSOLE_ERROR("!"#x); return S_FALSE; }
#define ER_RETURN_E_FAIL_IF_TRUE(x)			if (x) { CONSOLE_ERROR(#x); return E_FAIL; }
#define ER_RETURN_E_FAIL_IF_0(x)				if (!(x)) { CONSOLE_ERROR("!"#x); return E_FAIL; }
#define ER_RETURN_E_FAIL_IF_FAILED(x)		if (FAILED(x)) { CONSOLE_ERROR("!"#x); return E_FAIL; }
#define ER_RETURN_R_IF_TRUE(r, x)				if (x) { CONSOLE_ERROR(#x); return r; }
#define ER_RETURN_R_IF_FALSE(r, x)			if (!(x)) { CONSOLE_ERROR("!"#x); return r; }
#define ER_RETURN_R_IF_0(r, x)					ER_RETURN_R_IF_FALSE(r, x)
#define ER_RETURN_R_IF_FAILED(r, x)			if (FAILED(x)) { CONSOLE_ERROR("!"#x); return r; }

