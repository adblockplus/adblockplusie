/*
  Macros CONSOLE, CONSOLE_WARN, CONSOLE_ERROR are used for Console output
  CONSOLE_WAIT waits for user to hit Enter
  In order to see Console output macro USE_CONSOLE should be defined before Console.h is included.
  Since Console.h is added in PluginStdAfx.h, then 2 ways to log in Console ouput:
  1. Uncomment "#define USE_CONSOLE" in PluginStdAfx.h - Console ouput will be available in all files.
  2. Add "#define USE_CONSOLE" as very first line of a cpp file - Console ouput will be available in this file
*/

#pragma once

#if defined(USE_CONSOLE)

#include <Strsafe.h>

enum {eLog = 0, eWarn = 1, eError = 2};

static long g_consoleCount = -1;
static bool s_hasError = false;

void CONSOLE(const char* format, ...);

inline void WritelnToConsole(int code, int count, const char* format, va_list args)
{	
    // In an exe application, if there is an error and exe exists, Console disappears and error or warning are not visible
    // On exit destructor will be called and Console waits for keyboard input.
	struct ConsoleWaitReturn
	{
		ConsoleWaitReturn(){}
		~ConsoleWaitReturn()
		{
			if (s_hasError) 
            {
                CONSOLE("Hit 'ENTER' to exit");

			    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE); 

			    TCHAR buf[128];
			    DWORD nRead = 0;
			    ReadConsole(hInput, buf, countof(buf), &nRead, 0);
			}
		}
	} static s_ConsoleWaitReturn;

	DWORD lastError = GetLastError();

	if (eError == code) 
    {
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

        if (GetModuleFileNameA(0, pFileName, MAX_PATH)) 
        {
            char* pDelim = strrchr(pFileName, '\\') + 1;
            if (pDelim) 
            {
                strcpy_s(pFileName, sizeof(title) - lenLog, pDelim); 
            }

            int lenFileName = strlen(pFileName);
            *(pFileName + lenFileName) = '"';
            *(pFileName + lenFileName + 1) = 0;
        }

        AllocConsole();
	    SetConsoleTitleA(title);
	}
  
	SYSTEMTIME systemTime;
	GetLocalTime(&systemTime);

	char buf[256];
    sprintf_s(buf, countof(buf), "%d [%.2d:%.2d:%.2d] {%d} %s\n", count, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds, GetCurrentThreadId(), format);

	HANDLE hError = GetStdHandle(STD_ERROR_HANDLE);

	WORD color;
	if (eLog == code) 
    {
		color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
	} 
    else if (eWarn == code) 
    {
		color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	} else if (eError == code) 
    {
		color = FOREGROUND_RED | FOREGROUND_INTENSITY;
    }

	SetConsoleTextAttribute(hError, color);

	char out[1024];
    StringCbVPrintfA(out, sizeof(out), buf, args);

	DWORD dwOutput;
	WriteConsoleA(hError, out, strlen(out), &dwOutput, 0);
	FlushConsoleInputBuffer(hError);

	SetLastError(lastError);
}


inline void CONSOLE(const char* format, ...)
{
    InterlockedIncrement(&g_consoleCount);

    va_list args;
    va_start(args, format);    
    WritelnToConsole(eLog, g_consoleCount, format, args);
    va_end(args);
}

inline void CONSOLE_WARN(const char* format, ...)
{
    va_list args;
    va_start(args, format);    
    WritelnToConsole(eWarn, g_consoleCount, format, args);
    va_end(args);
}

inline void CONSOLE_ERROR(const char* format, ...)
{
    va_list args;
    va_start(args, format);    
    WritelnToConsole(eError, g_consoleCount, format, args);
    va_end(args);
}

inline void CONSOLE_WAIT(const char* format = "", ...)
{
	if (format  &&  0 != format[0]) 
    {
        InterlockedIncrement(&g_consoleCount);

        va_list args;
        va_start(args, format);    
        WritelnToConsole(eLog, g_consoleCount, format, args);
        va_end(args);
	}

	CONSOLE("Hit 'ENTER' to continue");
	TCHAR buf[128];
	DWORD nRead = 0;
	ReadConsole(GetStdHandle(STD_INPUT_HANDLE), buf, countof(buf), &nRead, 0);
}


#else
    int RemoveConsole(...);

    #define CONSOLE sizeof RemoveConsole
    #define CONSOLE_WARN sizeof RemoveConsole
    #define CONSOLE_ERROR sizeof RemoveConsole
    #define CONSOLE_WAIT sizeof RemoveConsole
#endif

