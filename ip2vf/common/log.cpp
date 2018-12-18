#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>              // isalnum

#define _EXTERNAL_LOG_SUPPORT
#ifdef _EXTERNAL_LOG_SUPPORT
#include "logreport.h"
const char* g_module_not_defined = "<not_defined>";
#endif // _EXTERNAL_LOG_SUPPORT

#define _SUPPORT_COLOR

#ifdef _WIN32

#include <wchar.h>
#include <windows.h>
#include <process.h>
#define DISPFORMAT_START    "\x1b[%dm"
#define DISPFORMAT_RESET    "\x1b[0m"
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING  0x0004
#define DISABLE_NEWLINE_AUTO_RETURN         0x0008
#define ENABLE_VIRTUAL_TERMINAL_INPUT       0x0200
#define VSNPRINTF(a,...)   vsnprintf(a, (sizeof(a)/sizeof(char)), __VA_ARGS__)
#define SNPRINTF(a,...)    _snprintf_s(a, (sizeof(a)/sizeof(char)), _TRUNCATE, __VA_ARGS__)
#define STRCPY(a,b)         strcpy_s(a, (sizeof(a)/sizeof(char)), b)
#define STRCAT(a,b)         strcat_s(a, (sizeof(a)/sizeof(char)), b)
#define GETPID              _getpid()
#define GETTID              0           //  pthread_self()

DWORD dwOriginalOutMode = 0;
DWORD dwOriginalInMode  = 0;
DWORD bVirtTermProcessingFlag = false;
int enableVirtualTerminalProcessing()
{
	// Set output mode to handle virtual terminal sequences
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hIn == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD dwOriginalOutMode = 0;
	DWORD dwOriginalInMode = 0;
	if (!GetConsoleMode(hOut, &dwOriginalOutMode))
	{
		return false;
	}
	if (!GetConsoleMode(hIn, &dwOriginalInMode))
	{
		return false;
	}

	DWORD dwRequestedOutModes = ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
	DWORD dwRequestedInModes = ENABLE_VIRTUAL_TERMINAL_INPUT;

	DWORD dwOutMode = dwOriginalOutMode | dwRequestedOutModes;
	if (!SetConsoleMode(hOut, dwOutMode))
	{
		// we failed to set both modes, try to step down mode gracefully.
		dwRequestedOutModes = ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		dwOutMode = dwOriginalOutMode | dwRequestedOutModes;
		if (!SetConsoleMode(hOut, dwOutMode))
		{
			// Failed to set any VT mode, can't do anything here.
			return -1;
		}
	}

	DWORD dwInMode = dwOriginalInMode | ENABLE_VIRTUAL_TERMINAL_INPUT;
	if (!SetConsoleMode(hIn, dwInMode))
	{
		// Failed to set VT input mode, can't do anything here.
		return -1;
	}
	bVirtTermProcessingFlag = true;
	return 0;
}
int revertToDefaultConsoleMode() {
	if (!bVirtTermProcessingFlag)
		return 0;
	// Set output mode to handle virtual terminal sequences
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hIn == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	if (!SetConsoleMode(hOut, dwOriginalOutMode))
	{
		// Failed to set any VT mode, can't do anything here.
		return -1;
	}
	if (!SetConsoleMode(hIn, dwOriginalInMode))
	{
		// Failed to set VT input mode, can't do anything here.
		return -1;
	}
	return 0;
}

#else   // _WIN32

#define DISPFORMAT_START    "\033[1;%dm"
#define DISPFORMAT_RESET    "\033[0m"
#define VSNPRINTF(a,...)    vsnprintf(a, (sizeof(a)/sizeof(char)), __VA_ARGS__)
#define SNPRINTF(a,...)     snprintf(a, (sizeof(a)/sizeof(char)), __VA_ARGS__)
#define STRCPY              strcpy
#define STRCAT              strcat
#define GETPID              getpid()
#define GETTID              syscall(SYS_gettid)
#include <unistd.h>         // getpid & sys_call
#include <sys/syscall.h>    // sys_call

#endif  // _WIN32

#include "log.h"

#ifndef MIN
#define MIN(a, b)       (a<b?a:b)
#endif


LogLevel g_logLevel = LOG_LEVEL_VERBOSE;
void setLogLevel(LogLevel level)
{
#ifdef _WIN32
	enableVirtualTerminalProcessing();
#endif	//_WIN32
	g_logLevel = level;
}
LogLevel getLogLevel() {
    return g_logLevel;
}

void closeLog() {
#ifdef _WIN32
	revertToDefaultConsoleMode();
#endif	//_WIN32
}

void platform_log(const char* message, ...)
{
    char dest[4096];
    va_list argptr;
    va_start(argptr, message);
    VSNPRINTF(dest, message, argptr);
    va_end(argptr);
    int size = (int)strlen(dest);
    if( size > 0)
    {
        if ( (dest[size - 1] != '\n') && (size < sizeof(dest)/sizeof(char)) )
            STRCAT(dest, "\n");
        fprintf(stderr, "%s", dest);
    }
}




void internal_LOG(std::string method, const char* message, ...)
{
    if( g_logLevel<LOG_LEVEL_VERBOSE )
        return;
    char dest[4096];
    va_list argptr;
    va_start(argptr, message);
    VSNPRINTF(dest, message, argptr);
    va_end(argptr);
    platform_log("%5x:%5x: -v- %s: %s", GETPID, (unsigned long)GETTID, method.c_str(), dest);
}
void internal_LOG_INFO(std::string method, const char* message, ...)
{
    if( g_logLevel<LOG_LEVEL_INFO )
        return;
    char dest[4096];
    va_list argptr;
    va_start(argptr, message);
    VSNPRINTF(dest, message, argptr);
    va_end(argptr);
    platform_log("%5x:%5x: -i- %s: %s", GETPID, (unsigned int)GETTID, method.c_str(), dest);

/*#ifdef _EXTERNAL_LOG_SUPPORT
    char dest2[1500];
    const char* module_name = (libvMI_get_module_name(0) == NULL ? g_module_not_defined : libvMI_get_module_name(0));
    SNPRINTF(dest2, "%s|%lld|%5x:%5x: -i- %s: %s", module_name, tools::getUTCEpochTimeInMs(), GETPID, (unsigned int)GETTID, method.c_str(), dest);
    vMI_report_sendmsg(dest2);
#endif // _EXTERNAL_LOG_SUPPORT*/
    //WIN32_HOTFIX_FLUSH_OUTPUT();
}
void internal_LOG_COLOR(std::string method, LogColor color, const char* message, ...)
{
    if (g_logLevel<LOG_LEVEL_INFO)
        return;
    char dest[4096];
    va_list argptr;
    va_start(argptr, message);
    VSNPRINTF(dest, message, argptr);
    va_end(argptr);
#ifdef _SUPPORT_COLOR
    platform_log(DISPFORMAT_START "%5x:%5x: -i- %s: %s" DISPFORMAT_RESET, color, GETPID, (unsigned int)GETTID, method.c_str(), dest);
#else   //_SUPPORT_COLOR
    platform_log("%5x:%5x: *E* %s: %s", GETPID, (unsigned int)GETTID, method.c_str(), dest);
#endif  //_SUPPORT_COLOR
    WIN32_HOTFIX_FLUSH_OUTPUT();
}
void internal_LOG_WARNING(std::string method, const char* message, ...)
{
    if( g_logLevel<LOG_LEVEL_WARNING )
        return;
    char dest[4096];
    va_list argptr;
    va_start(argptr, message);
    VSNPRINTF(dest, message, argptr);
    va_end(argptr);
    platform_log("%5x:%5x: +w+ %s: %s", GETPID, (unsigned int)GETTID, method.c_str(), dest);
    WIN32_HOTFIX_FLUSH_OUTPUT();
}

void internal_LOG_ERROR(std::string method, const char* message, ...)
{
    if (g_logLevel < LOG_LEVEL_ERROR)
        return;
    char dest[4096];
    va_list argptr;
    va_start(argptr, message);
    VSNPRINTF(dest, message, argptr);
    va_end(argptr);

#ifdef _SUPPORT_COLOR
    platform_log(DISPFORMAT_START "%5x:%5x: *E* %s: %s" DISPFORMAT_RESET, LOG_COLOR_RED, GETPID, (unsigned int)GETTID, method.c_str(), dest);
#else   //_SUPPORT_COLOR
    platform_log("%5x:%5x: *E* %s: %s", GETPID, (unsigned int)GETTID, method.c_str(), dest);
#endif  //_SUPPORT_COLOR

/*#ifdef _EXTERNAL_LOG_SUPPORT
    char dest2[1500];
    const char* module_name = (libvMI_get_module_name(0) == NULL ? g_module_not_defined : libvMI_get_module_name(0));
    SNPRINTF(dest2, "%s|%lld|%5x:%5x: *E* %s: %s", module_name, tools::getUTCEpochTimeInMs(), GETPID, (unsigned int)GETTID, method.c_str(), dest);
    vMI_report_sendmsg(dest2);
#endif // _EXTERNAL_LOG_SUPPORT*/    

	WIN32_HOTFIX_FLUSH_OUTPUT();
}
#define NB_ELEMENTS_BY_LINE	16
void internal_LOG_DUMP(std::string method, const char* buffer, int size)
{
    if( g_logLevel<LOG_LEVEL_ERROR)
        return;
    char dest[1024];	// A line of log to display (sent to platform_log)
    char token[16];		// Used to convert each token to display
    char alpha[132];	// Used to display alphanum char, wil be added to the final string (dest) just before platform_log

    SNPRINTF(dest, "%5x:%5x: -v- DUMP memory %p, size=%d", GETPID, (unsigned int)GETTID, buffer, size);
    platform_log(dest);

    SNPRINTF(dest, "%5x:%5x: -v- ", GETPID, (unsigned int)GETTID);
    STRCPY(alpha, "| ");
    for( int i=0, j=1; i<size; i++, j++ )
    {
        SNPRINTF(token, "%02x ", (unsigned char)buffer[i]);
        STRCAT(dest, token);
        char c = (char)buffer[i];
        if( isalnum(c) || c==' ' /*|| ispunct(c)*/ )
            SNPRINTF(token, "%c", c);
        else
            SNPRINTF(token, ".");
        STRCAT(alpha, token);
        if( j>=NB_ELEMENTS_BY_LINE )
        {
            STRCAT(dest, alpha);
            platform_log(dest);
            j=0;
            SNPRINTF(dest, "%5x:%5x: -v- ", GETPID, (unsigned int)GETTID);
            STRCPY(alpha, "| ");
        }
        else if( i==size-1 )
        {
            for( int k=j; k<NB_ELEMENTS_BY_LINE; k++ )
                STRCAT(dest, "   ");
            STRCAT(dest, alpha);
            platform_log(dest);
        }
    }
}
void internal_LOG_DUMP10BITS(std::string method, const char* buffer, int size)
{
    if (g_logLevel<LOG_LEVEL_INFO)
        return;
    int pos = 0;
    unsigned char* p = (unsigned char*)buffer;
    char line[1024];
    char token[16];
    int tokens = 0;
    SNPRINTF(line, "%5x:%5x: -v- ", GETPID, (unsigned int)GETTID);
    int w[4];
    while (pos+4 <= size) {
        // 4 word of 10 bits = 40 bits = 5 bytes
        w[0] = ((p[0]) << 2) + ((p[1] & 0b11000000) >> 6);
        w[1] = ((p[1] & 0b00111111) << 4) + ((p[2] & 0b11110000) >> 4);
        w[2] = ((p[2] & 0b00001111) << 6) + ((p[3] & 0b11111100) >> 2);
        w[3] = ((p[3] & 0b00000011) << 8) + ((p[4]));
        int nb = MIN(4, size - pos);
        tokens += nb;
        for (int i = 0; i < nb; i++) {
            SNPRINTF(token, "%03x ", w[i]);
            STRCAT(line, token);
        }
        pos += 4; p += 5;
        if (tokens >= 16 || pos == size) {
            platform_log(line);
            tokens = 0;
            SNPRINTF(line, "%5x:%5x: -v- ", GETPID, (unsigned int)GETTID);
        }
    }
}


void hotfixWindowsPipeFlushes() {

    fflush(stderr);
    fflush(stdout);
}
