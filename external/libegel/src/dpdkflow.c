
#include "common.h"
#include "dpdk.h"

void
dpdkflowstat(Flow* flow, struct rte_mbuf* mb, int modulus)
{
	long int fno;
	struct ether_hdr *eth_hdr;

	eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr*);
	if(is_broadcast_ether_addr(&eth_hdr->d_addr))
		flow->nrxbroadcast++;
	else if(is_multicast_ether_addr(&eth_hdr->d_addr))
		flow->nrxmulticast++;

	fno = flow - flow->nic->flows;

	/*
	 * Quick hack for errant RTP frames, don't do all the proper
	 * decoding, just enough to weed them out for sanity's sake.
	 *
	 * When a multicast flow filter is enabled, if the multicast stream
	 * is already running, packets previously buffered by the NIC before
	 * it starts honouring the filtering show up in the drop queue.
	 */
	if(fno == 0 && RTE_BE_TO_CPU_16(eth_hdr->ether_type) == ETHER_TYPE_IPv4){
		if(mb->pkt_len == 1442 || mb->pkt_len == 1328){
			flow->nrx1400++;
			return;
		}
	}

	if(modulus == 0 || (flow->nrxpkt % modulus) == 0){
		printf("F %lu:"
			" %"PRIu64 " %"PRIu64 " %"PRIu64 " %"PRIu64
			" %"PRIu64 " %"PRIu64 " %"PRIu64 " %"PRIu64
			" %"PRIu64
			": %"PRIu64
			"\n",
			fno,
			flow->nrxpkt, flow->nrxpktfree, flow->nrxbroadcast,
			flow->nrxmulticast, flow->nrx1400, flow->nrxfdir,
			flow->nrxqfull, flow->nrxqnopkt, flow->nrxburst,
			flow->ntxpkt);
	}
}

static void
dpdkflowdrop(Flow* flow, struct rte_mbuf* mb)
{
	flow->nrxpkt++;
	dpdkflowstat(flow, mb, 100000);

	if(mb->ol_flags & PKT_RX_FDIR){
		flow->nrxfdir++;
		printf("F %lu: FDIR pkt:\n", flow - flow->nic->flows);
		printpkt(mb, 0);
	}

	rte_pktmbuf_free(mb);
	flow->nrxpktfree++;
}

static void
dpdkflowudp(Flow* flow, struct rte_mbuf* mb)
{
	void *pkt;

	flow->nrxpkt++;
	dpdkflowstat(flow, mb, 100000);

	if((pkt = dpdkpktalloc(mb)) == nil){
		flow->nrxqnopkt++;
		rte_pktmbuf_free(mb);
		flow->nrxpktfree++;
	}
	if(qsnbput(flow->recvq, pkt) == 0){
		flow->nrxqfull++;
		dpdkpktfree(pkt);
		flow->nrxpktfree++;
	}
	qkick(flow->recvq->cq8);

	while(Isoccupied((pkt = qsnbget(flow->freeq)))){
		dpdkpktfree(pkt);
		flow->nrxpktfree++;
	}
}

int
dpdkflow(int argc, char* argv[], void* rdata, uint* rlen)
{
	int r;
	Q** rq;
	Nic *nic;
	unsigned ui;
	long int fno;
	IPaddr ipaddr;
	Flow *flow, *freeflow;
	uint16_t port, udpport;
	char addrstr[RTE_MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)], *endptr;

	if(rdata == 0 || rlen == 0 || *rlen < sizeof(Q*)*2)
		return -1;

	/*
	 * flow pci-addr ip-addr udp-port [option ...]
	 */
	if(argc < 4)
		return -1;
	argc--; argv++;

	if((r = rte_eth_dev_get_port_by_name(argv[0], &port)) != 0){
		printf("flow: \"%s\": %s\n", argv[0], strerror(-r));
		return -1;
	}

	if((nic = dpdkgetnic(port)) == 0 || nic->addr == 0)
		return -1;

	argc--; argv++;
	if(ipaddrparse(&ipaddr, AF_UNSPEC, argv[0]) <= 0){
		printf("flow: \"%s\": invalid address: %s\n", nic->name, argv[0]);
		return -1;
	}
	ipaddrfmt(&ipaddr, addrstr);
	printf("flow: IP: %s -> %s\n", argv[0], addrstr);

	argc--; argv++;
	errno = 0;
	ui = strtol(argv[0], &endptr, 0);
	if(errno != 0 || *endptr != '\0' || ui >= 0x10000 || ui == 0){
		printf("flow: \"%s\": invalid udpport: %u\n", argv[0], ui);
		return -1;
	}
	udpport = ui;
	printf("flowinit: udpport %"PRIu16 "\n", udpport);

	freeflow = 0;
	for(flow = nic->flows; flow < nic->flows + nic->nextflow; flow++){
		if(flow->nic != 0)
			continue;
		if(flow->ipaddr.af == AF_UNSPEC){
			freeflow = flow;
			continue;
		}
		if(ipaddrcmp(&flow->ipaddr, &ipaddr) != 0)
			continue;

		/*
		 * To do here:
		 *	not necessarily an error, could add optional
		 *	argument processing for an already successfully
		 *	configured flow, or use a different command
		 *	for flow control.
		 */
		if(flow->udpport == udpport){
			printf("flow[%lu]: IP %s:%"PRIu16 " already configured\n",
				flow - nic->flows, addrstr, udpport);
			return -1;
		}
	}

	if(freeflow != 0)
		flow = freeflow;
	else if(nic->nextflow < nic->nflows)
		flow = nic->flows + nic->nextflow++;
	else{
		printf("flow: no free flows (out of %u)\n", nic->nflows);
		return -1;
	}

	fno = flow - nic->flows;
	printf("flow[%lu]: IP %s udpport %"PRIu16 "\n", fno, addrstr, udpport);

	flow->ipaddr = ipaddr;
	flow->udpport = udpport;

	if((flow->handle = flowudphandle(nic, flow, 0)) == 0)
		return -1;
	flow->f = dpdkflowudp;

	rq = rdata;
	flow->recvq = qcreate(nic->nrxdesc);			/* guess */
	flow->recvq->cq8 = q8alloc();					/* full metal jacket */
	flow->recvq->pq8 = q8alloc();
	*rq++ = flow->recvq;

	flow->freeq = qcreate(nic->nrxdesc);			/* guess */
	flow->freeq->cq8 = q8alloc();					/* full metal jacket */
	flow->freeq->pq8 = q8alloc();
	*rq = flow->freeq;

	rte_timer_init(&flow->timer);
	flow->nic = nic;

	if(ipaddrmulticast(&flow->ipaddr) == 0){
		flow->uri = 2;
		igmptimerset(flow, nic->hz);
	}

	return fno;
}

int
dpdkflowinit(Nic* nic)
{
	Flow *flow;

	nic->nflows = nic->nrxqueue;
	if((nic->flows = rte_zmalloc(0, sizeof(Flow) * nic->nflows, 0)) == 0){
		printf("dpdknic \"%s\": %u flow alloc", nic->name, nic->nflows);
		return -1;
	}
	nic->nextflow = 0;

	/*
	 * Don't really need a Flow struct or a queue for the drop flow rule,
	 * but it simplifies things to have a one-to-one between Flow and queue
	 * indices. IGMP will be Flow/queue index 1, and the rest for UDP.
	 *
	 * To do: ambiguity here on failure.
	 */
	flow = nic->flows;
	if((flow->handle = flowdrophandle(nic)) == 0)
		return -1;
	flow->f = dpdkflowdrop;

	flow->recvq = qcreate(nic->nrxdesc);			/* guess */
	flow->recvq->cq8 = q8alloc();					/* full metal jacket */
	flow->recvq->pq8 = q8alloc();

	flow->freeq = qcreate(nic->nrxdesc);			/* guess */
	flow->freeq->cq8 = q8alloc();
	flow->freeq->pq8 = q8alloc();
	rte_timer_init(&flow->timer);
	flow->nic = nic;
	nic->nextflow++;
	flow++;

	if((flow->handle = flowigmphandle(nic)) == 0)
		return -1;
	flow->f = igmpflow;

	flow->recvq = qcreate(nic->nrxdesc);			/* guess */
	flow->recvq->cq8 = q8alloc();					/* full metal jacket */
	flow->recvq->pq8 = q8alloc();

	flow->freeq = qcreate(nic->nrxdesc);			/* guess */
	flow->freeq->cq8 = q8alloc();					/* full metal jacket */
	flow->freeq->pq8 = q8alloc();
	rte_timer_init(&flow->timer);
	flow->nic = nic;
	nic->nextflow++;

	return 0;
}
