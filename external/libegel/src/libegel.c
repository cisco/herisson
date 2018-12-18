
#include "common.h"
#include <pthread.h>									/* needed by q.h */

#include "q.h"
#include "libegel.h"
#include "master.h"

#define MkHandle(t, f, p)	((((t) & 0xff)<<16)|(((f) & 0xff)<<8)|((p) & 0xff))
#define NFLOWS		16

typedef struct LQ LQ;
typedef struct LNic LNic;

struct LQ {
	Q*		recvq;
	Q*		freeq;
};

struct LNic {
	LQ		lq[256];
};

static LNic lnics[RTE_MAX_ETHPORTS];
static char p2pciaddr[RTE_MAX_ETHPORTS][32];

eg_error
libegel_init(struct eg_config* config)
{
	int r;

	if(config == 0)
		return -1;
	if(config->eal_config_str == 0)
		return 0;
	r = dpdkcfginit(config->eal_config_str);

	return r;
}

eg_error
libegel_rqst(const char* cfg, void* rdata, uint* rlen)
{
	return dpdkrqst(cfg, rdata, rlen);
}

#ifndef HIP4
#define HIP4_FMT "%u.%u.%u.%u"
#define HIP4(addr)							\
	(unsigned)((unsigned char*)&addr)[3],	\
	(unsigned)((unsigned char*)&addr)[2],	\
	(unsigned)((unsigned char*)&addr)[1],	\
	(unsigned)((unsigned char*)&addr)[0]
#endif /* HIP4 */

eg_slot_handle
libegel_config_slot(struct eg_slot_config* slot_config)
{
	int r;
	LQ rdata;
	uint rlen;
	const char *p;
	uint16_t port;
	uint32_t ip4addr;
	char cfg[NRQSTCFG];

	/*
	 * Turn a binary eg_slot_config request of
	 *
	 *	iface_pci_addr iface_ip_addr mcast_group udp_port
	 *
	 * into two text-based requests:
	 *
	 *	nic iface_pci_addr iface_ip_addr
	 *	flow iface_pci_addr mcast_group udp_port
	 *
	 * The returned handle is an integer encoded with the NIC#,
	 * flow# on that NIC, and an 'F' for type identification.
	 */
	p = slot_config->iface_pci_addr;
	ip4addr = ntohl(slot_config->iface_ip_addr);
	snprintf(cfg, NRQSTCFG, "nic %s "HIP4_FMT "", p, HIP4(ip4addr));
	printf("libegel_slot_config: %s\n", cfg);

	if((r = dpdkrqst(cfg, 0, 0)) < 0 || r >= RTE_MAX_ETHPORTS)
		return -1;
	port = r;
	printf("libegel_slot_config: %s: port %u\n", cfg, port);

	if(p2pciaddr[port][0] == 0)
		strncpy(p2pciaddr[port], p, sizeof(p2pciaddr[0])-1);

	ip4addr = ntohl(slot_config->mcast_group);
	snprintf(cfg, NRQSTCFG, "flow %s "HIP4_FMT " %u",
		p2pciaddr[port], HIP4(ip4addr), ntohs(slot_config->udp_port));
	printf("libegel_slot_config: %s\n", cfg);

	rlen = sizeof(rdata);
	if((r = dpdkrqst(cfg, &rdata, &rlen)) < 0 || r >= NFLOWS)
		return -1;
	printf("libegel_slot_config: %s: returns %#x, rlen %u\n", cfg, r, rlen);
	printf("libegel_slot_config: %p %p\n", rdata.recvq, rdata.freeq);

	lnics[port].lq[r] = rdata;

	return MkHandle('F', r, port);
}

/*
 * Still to be decided on...
 */
eg_slot_handle
libegel_new_slot(void)
{
	// TODO
	return -1;
}

eg_error
libegel_delete_slot(eg_slot_handle sh)
{
	// TODO
 	((void)(sh));										/* USED */
	return -1;
}

uint32_t
libegel_rx_pkts_burst(eg_slot_handle sh, eg_pkt_desc* pkts, uint32_t len)
{
	LQ* lq;
	void *pkt;
	uint32_t nb;

	if(((sh>>16) & 0xff) != 'F')
		return -1;
	lq = &lnics[sh & 0xff].lq[(sh>>8) & 0xff];

	/*
	 * Q: OK to return 0 or should wait for at least one?
	 *		will wait for now, seems to use less CPU.
	 */
	*pkts++ = qsget(lq->recvq);
	for(nb = 1; nb < len; nb++){
		if(Isfree(pkt = qsnbget(lq->recvq)))
			break;

		*pkts++ = pkt;
	}

	return nb;
}

eg_error
libegel_free_pkt(eg_slot_handle sh, eg_pkt_desc pkt)
{
	LQ* lq;

	if(((sh>>16) & 0xff) != 'F')
		return -1;
	lq = &lnics[sh & 0xff].lq[(sh>>8) & 0xff];

	qsput(lq->freeq, pkt);								/* wait or no wait? */

	return 0;
}

uint8_t*
libegel_get_udp_payload(eg_slot_handle sh, eg_pkt_desc pkt)
{
	((void)(sh)); 										/* USED */

	if(pkt == 0)
		return 0;
	return ((Pkt*)pkt)->payload;
}

int32_t
libegel_get_udp_payload_len(eg_slot_handle sh, eg_pkt_desc pkt)
{
	((void)(sh));										/* USED */

	if(pkt == 0)
		return -1;
	return ((Pkt*)pkt)->len;
}
