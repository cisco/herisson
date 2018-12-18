#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <errno.h>

#ifdef _WIN32

#define INET_ADDRSTRLEN  22	// as define in ws2ipdef.h
#define uint	unsigned int

#include <rte_windows.h>
#include <rte_ethdev.h>

#else // _WIN32

#include <arpa/inet.h>

#endif //_WIN32

#include <getopt.h>

#endif /*_COMMON_H_ */
