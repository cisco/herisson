#ifndef _DPDK_H_
#define _DPDK_H_

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_flow.h>
#include <rte_cycles.h>
#include <rte_timer.h>
#include <rte_malloc.h>

#include "ipaddr.h"
#include "q.h"

typedef struct Flow Flow;
typedef struct Nic Nic;

struct Flow {
	Nic*			nic;
	struct rte_flow* handle;

	IPaddr			ipaddr;							/* */
	uint16_t		udpport;						/* */

	void			(*f)(Flow*, struct rte_mbuf*);
	Q*				recvq;
	Q*				freeq;

	struct rte_timer timer;							/* IGMP */
	uint16_t		uri;							/* IGMP */
	struct rte_mbuf* mbreport;						/* IGMP */
	struct rte_mbuf* mbleave;						/* IGMP */

	uint64_t		nrxpkt;
	uint64_t		nrxpktfree;

	uint64_t		nrxbroadcast;
	uint64_t		nrxmulticast;
	uint64_t		nrx1400;
	uint64_t		nrxfdir;

	uint64_t		nrxqfull;
	uint64_t		nrxqnopkt;
	uint64_t		nrxburst;

	uint64_t		ntxpkt;
};

struct Nic {
	char*			addr;
	char			name[32];
	uint16_t		port;
	IPaddr			ipaddr;							/* local */
	unsigned		lid;							/* lcore ID */
	uint64_t		hz;								/* lcore TSC Hz */

	uint16_t		nrxqueue;						/* start configurable */
	uint16_t		nrxdesc;						

	uint16_t		ntxqueue;
	uint16_t		ntxdesc;

	uint16_t		burst;							/*  end configurable */

	Flow*			flows;
	uint16_t		nextflow;
	uint16_t		nflows;							/* == nrxqueue */

	struct rte_mempool* mempool;
};

/*
 * dpdk.c
 */
int dpdkeal(int, char*[], void*, uint*);			/* cfg */

/*
 * dpdkflow.c
 */
int dpdkflow(int, char*[], void*, uint*);			/* cfg */
int dpdkflowinit(Nic*);

/*
 * dpdknic.c
 */
int dpdknic(int, char*[], void*, uint*);			/* cfg */
int dpdkexit(int, char*[], void*, uint*);			/* cfg */

/*
 * flow.c
 */
struct rte_flow* flowdrophandle(Nic*);
struct rte_flow* flowigmphandle(Nic*);
struct rte_flow* flowudphandle(Nic*, Flow*, int);

/*
 * igmp.c
 */
void igmpfmt(void*);
void igmpflow(Flow*, struct rte_mbuf*);
void igmpleave(Flow*);
void igmpquery(Nic*, struct rte_mbuf*);
void igmpshutdown(Nic*);
int igmptimerset(Flow*, uint64_t);

/*
 * print.c
 */
void ethaddrfmt(char*, uint16_t, struct ether_addr*);
void printpkt(struct rte_mbuf*, int);
void printpkthex(struct rte_mbuf*, int);

/*
 * Odds and ends.
 */
struct rte_mempool* dpdkgetmempool(uint16_t);
Nic* dpdkgetnic(uint16_t);
void* dpdkpktalloc(struct rte_mbuf*);
void dpdkpktfree(void*);
void dpdkflowstat(Flow*, struct rte_mbuf*, int);

int dpdkdone;

uint64_t ticktock;

/*
 * Work-around of a compilation error with ICC on invocations of the
 * rte_be_to_cpu_16() function.
 */
#ifdef __GCC__
#define RTE_BE_TO_CPU_16(be_16_v)  rte_be_to_cpu_16((be_16_v))
#define RTE_CPU_TO_BE_16(cpu_16_v) rte_cpu_to_be_16((cpu_16_v))
#else
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
#define RTE_BE_TO_CPU_16(be_16_v)  (be_16_v)
#define RTE_CPU_TO_BE_16(cpu_16_v) (cpu_16_v)
#else
#define RTE_BE_TO_CPU_16(be_16_v) \
	(uint16_t) ((((be_16_v) & 0xFF) << 8) | ((be_16_v) >> 8))
#define RTE_CPU_TO_BE_16(cpu_16_v) \
	(uint16_t) ((((cpu_16_v) & 0xFF) << 8) | ((cpu_16_v) >> 8))
#endif
#endif /* __GCC__ */

#endif /* _DPDK_H_ */
