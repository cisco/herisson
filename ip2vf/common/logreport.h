#ifndef _LOGREPORT_H
#define _LOGREPORT_H

#include <string>
#include <stdexcept>

#include "libvMI.h"
#include "tcp_basic.h"
#include "tools.h"

#ifdef _WIN32

#ifdef VMILIBRARY_EXPORTS
#pragma once
#define VMILIBRARY_API_LOG __declspec(dllexport)
#else
#pragma once
#define VMILIBRARY_API_LOG __declspec(dllimport)
#endif

#else
#define VMILIBRARY_API_LOG 
#endif

VMILIBRARY_API_LOG void vMI_report_sendmsg(const char* mesg);

#endif //_LOGREPORT_H
