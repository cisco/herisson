/*-
*   BSD LICENSE
*
*   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
*   All rights reserved.
*
*   Redistribution and use in source and binary forms, with or without
*   modification, are permitted provided that the following conditions
*   are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in
*       the documentation and/or other materials provided with the
*       distribution.
*     * Neither the name of Intel Corporation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
*   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*   OF THIS SOFTWARE, EVEN I ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#ifndef _RTE_WINDOWS_H_
#define _RTE_WINDOWS_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MSC_VER
#error
#endif

#ifndef _WINDOWS
#define _WINDOWS
#endif


#define _WINSOCKAPI_ 
#include "windows.h"
/* Undefine this, the Windows definition clashes with the one we have */
#undef s_addr

/*
* Globally driven over-rides.
*/
#define __attribute__(x) 

#define __func__ __FUNCTION__

#define NORETURN __declspec(noreturn)
#define ATTR_UNUSED

//#define INLINE	_inline
//#define inline __inline

#define E_RTE_NO_TAILQ	(-1)

/* Include this header here, so that we can re-define EAL_REGISTER_TAILQ */
#include <rte_tailq.h>
#ifdef EAL_REGISTER_TAILQ(t)
#undef EAL_REGISTER_TAILQ(t)
#endif

/*
* Definition for registering TAILQs
* (This is a workaround for Windows in lieu of a constructor-like function)
*/
#define EAL_REGISTER_TAILQ(t) \
void init_##t(void); \
void init_##t(void) \
{ \
	if (rte_eal_tailq_register(&t) < 0) \
		rte_panic("Cannot initialize tailq: %s\n", t.name); \
}

/* Include this header here, so that we can re-define RTE_REGISTER_BUS */
#include <rte_bus.h>
#ifdef RTE_REGISTER_BUS(nm, bus)
#undef RTE_REGISTER_BUS(nm, bus)
#endif

/*
* Definition for registering a bus
* (This is a workaround for Windows in lieu of a constructor-like function)
*/
#define RTE_REGISTER_BUS(nm, bus) \
void businitfn_ ##nm(void) \
{\
	(bus).name = RTE_STR(nm);\
	rte_bus_register(&bus); \
}


/* 
* Global warnings control. Disable this to see warnings in the 
* include/rte_override files 
*/
#define DPDKWIN_NO_WARNINGS

#ifdef DPDKWIN_NO_WARNINGS
#pragma warning (disable : 94)	/* warning #94: the size of an array must be greater than zero */
#pragma warning (disable : 169)	/* warning #169: expected a declaration */
#endif


/*
* These definitions are to force a specific version of the defined function.
* For Windows, we'll always stick with the latest defined version.
*/
#define rte_lpm_create			rte_lpm_create_v1604
#define rte_lpm_add			rte_lpm_add_v1604
#define rte_lpm6_add			rte_lpm6_add_v1705
#define rte_lpm6_lookup			rte_lpm6_lookup_v1705
#define rte_lpm6_lookup_bulk_func	rte_lpm6_lookup_bulk_func_v1705
#define rte_lpm6_is_rule_present	rte_lpm6_is_rule_present_v1705

#define rte_distributor_request_pkt	rte_distributor_request_pkt_v1705
#define rte_distributor_poll_pkt	rte_distributor_poll_pkt_v1705
#define rte_distributor_get_pkt		rte_distributor_get_pkt_v1705
#define rte_distributor_return_pkt	rte_distributor_return_pkt_v1705
#define rte_distributor_returned_pkts	rte_distributor_returned_pkts_v1705
#define rte_distributor_clear_returns	rte_distributor_clear_returns_v1705
#define rte_distributor_process		rte_distributor_process_v1705
#define rte_distributor_flush		rte_distributor_flush_v1705
#define rte_distributor_create		rte_distributor_create_v1705

/* 
* Definitions and overrides for ethernet.h 
*/
#define u_char uint8_t
#define u_short uint16_t

#define __packed

#define __BEGIN_DECLS
#define __END_DECLS

/*
* sys/_cdefs.h 
*/
#define __extension__

/*
* sys/_iovec.h
*/
#define ssize_t size_t
#define SSIZE_T_DECLARED
#define _SSIZE_T_DECLARED
#define _SIZE_T_DECLARED

/*
* Linux to BSD termios differences
*/
#define TCSANOW 0

/* Support X86 architecture */
#define RTE_ARCH_X86

/*
* We can safely remove __attribute__((__packed__)). We will replace it with all structures
* being packed
*/
#pragma pack(1)

#include <rte_wincompat.h>

/* Include rte_common.h first to get this out of the way controlled */
#include "./rte_common.h"


#include <sys/types.h>
/* rte_pci.h must be included before we define typeof() to be nothing */
//#include "./rte_pci.h"

/* MSVC doesn't have a typeof */
//#define typeof(c)
//#define __typeof__(c)


#define __attribute__(x)

#define RTE_FORCE_INTRINSICS

#include "rte_config.h"

#define RTE_CACHE_ALIGN		__declspec(align(RTE_CACHE_LINE_SIZE))
#define RTE_CACHE_MIN_ALIGN	__declspec(align(RTE_CACHE_LINE_MIN_SIZE))

/* The windows port does not currently support dymamic loading of libraries, so fail these calls */
#define dlopen(lib, flag)   (0)
#define dlerror()           ("Not supported!")

/* Include time.h for struct timespec */
#include <time.h>
#ifndef _TIMESPEC_DEFINED
#define _TIMESPEC_DEFINED
#endif

typedef jmp_buf sigjmp_buf;
#define sigsetjmp(env, savemask) _setjmp((env))

/* function prototypes for those used exclusively by Windows */
void eal_create_cpu_map();


#ifdef __cplusplus
}
#endif

#endif /* _RTE_WIN_H_ */
