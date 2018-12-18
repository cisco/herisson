#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "libvMI.h"
#include "tcp_basic.h"
#include "logreport.h"
#include "tools.h"

#ifdef _WIN32

#define DISPFORMAT_START    ""
#define DISPFORMAT_RESET    ""
#define VSNPRINTF(a,...)   vsnprintf(a, (sizeof(a)/sizeof(char)), __VA_ARGS__)
#define SNPRINTF(a,...)    _snprintf_s(a, (sizeof(a)/sizeof(char)), _TRUNCATE, __VA_ARGS__)
#define STRCPY(a,b)         strcpy_s(a, (sizeof(a)/sizeof(char)), b)
#define STRCAT(a,b)         strcat_s(a, (sizeof(a)/sizeof(char)), b)
#define GETPID              _getpid()
#define GETTID              0           //  pthread_self()
#include <process.h>

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

UDP         g_logreport_sock;
bool        g_logreport_ini = false;
const char* g_logreport_ip = "10.60.26.92";
int         g_logreport_port = 1234;

void vMI_report_init(const char* ip, int port) {

    g_logreport_ini = true;
    g_logreport_sock.openSocket((char*)ip, NULL, port);
}

void vMI_report_sendmsg(const char* mesg) {

    if (!g_logreport_ini) {
        vMI_report_init(g_logreport_ip, g_logreport_port);
    }
    if (g_logreport_ini && g_logreport_sock.isValid()) {
        int len = strlen(mesg);
        int result = g_logreport_sock.writeSocket((char*)mesg, &len);
    }
}
