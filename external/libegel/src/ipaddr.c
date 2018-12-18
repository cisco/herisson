
#ifdef _WIN32

#define _WINSOCKAPI_ 
#include <winsock2.h>    
#include <Ws2tcpip.h>

#else	// _WIN32

#endif	// _WIN32

#include "common.h"
#define _IN_H_
#include <rte_ip.h>
#include "ipaddr.h"
/*
 * This relies on some macros contained in DPDK header files.
 * To make it independent of dpdk.h and just use "#include ipaddr.h"
 * would need to have compatible replacements for, e.g. 
 *	 RTE_MAX, IPv4, IS_IPV4_MCAST, rte_(be|cpu)_to_(be|cpu)_32
 */
#include "dpdk.h"


int
ipaddrparse(IPaddr* ipaddr, int af, char* src)
{
	int r;
	void *dst;

	memset(ipaddr, 0, sizeof(IPaddr));

	switch(af){
	default:
		printf("AF_? (%d): %s\n", af, strerror(EAFNOSUPPORT));
		return -1;

	case AF_UNSPEC:									/* 0 */
		/*FALLTHROUGH*/

	case AF_INET:
		dst = &ipaddr->ip4addr;
		if((r = inet_pton(AF_INET, src, dst)) == 1){
			ipaddr->ip4addr = rte_be_to_cpu_32(ipaddr->ip4addr);
			ipaddr->af = AF_INET;
			return r;
		}
		if(af == AF_INET)
			break;
		/*FALLTHROUGH*/

	case AF_INET6:
		dst = ipaddr->ip6addr;
		if((r = inet_pton(AF_INET6, src, dst)) == 1){
			ipaddr->af = AF_INET6;
			return r;
		}
		break;
	}

	return 0;
}

int
ipaddrcmp(IPaddr* ip0, IPaddr* ip1)
{
	if(ip0->af != ip1->af)
		return -1;

	switch(ip0->af){
	default:
		printf("AF_? (%d): %s\n", ip0->af, strerror(EAFNOSUPPORT));
		break;

	case AF_INET:
		if(ip0->ip4addr == ip1->ip4addr)
			return 0;
		break;

	case AF_INET6:
		if(memcmp(&ip0->ip6addr, &ip1->ip6addr, sizeof(ip0->ip6addr)) == 0)
			return 0;
		break;
	}

	return -1;
}

char*
ipaddrfmt(IPaddr* ip, char* addrstr)
{
	int alen;
	uint32_t ip4addr;

	alen = RTE_MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN);
	addrstr[0] = 0;

	switch(ip->af){
	default:
		snprintf(addrstr, alen, "ipaddrfmt: AF_(%d) not supported\n", ip->af);
		break;

	case AF_INET:
		ip4addr = rte_cpu_to_be_32(ip->ip4addr);
		if(inet_ntop(AF_INET, &ip4addr, addrstr, alen) == 0)
			snprintf(addrstr, alen, "ipaddrfmt: AF_INET: %s", strerror(errno));
		break;

	case AF_INET6:
		if(inet_ntop(AF_INET6, &ip->ip6addr, addrstr, alen) == 0)
			snprintf(addrstr, alen, "ipaddrfmt: AF_INET6: %s", strerror(errno));
		break;
	}

	return addrstr;
}

int
ipaddrmulticast(IPaddr* ipaddr)
{
	switch(ipaddr->af){
	default:									/* unnecessary, but necessary */
		return -1;

	case AF_INET:
		if(IS_IPV4_MCAST(ipaddr->ip4addr))
			return 0;
		break;

	case AF_INET6:
		if(ipaddr->ip6addr[0] == 0xff)
			return 0;
		break;
	}

	return -1;
}
