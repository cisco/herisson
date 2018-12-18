
#include "common.h"
#include "dpdk.h"

struct igmp_hdr {
	uint8_t		type;
	uint8_t		mrt;
	uint16_t	checksum;
	uint32_t	group;
} __attribute__((__packed__));

static inline uint16_t
iphdr_cksum(const void* buf, size_t len)
{
	uint16_t cksum;

	if((cksum = rte_raw_cksum(buf, len)) == 0xffff)
		return cksum;

	return (uint16_t)~cksum;
}

static struct rte_mbuf*
igmp2template(Flow* flow, uint8_t type)
{
	uint16_t offset;
	struct rte_mbuf *mb;
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;
	struct igmp_hdr *igmp_hdr;

	mb = rte_pktmbuf_alloc(flow->nic->mempool);

	eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr*);
	eth_hdr->d_addr.addr_bytes[0] = 0x01;
	eth_hdr->d_addr.addr_bytes[1] = 0x00;
	eth_hdr->d_addr.addr_bytes[2] = 0x5e;
	eth_hdr->d_addr.addr_bytes[3] = (flow->ipaddr.ip4addr>>16) & 0x7f;
	eth_hdr->d_addr.addr_bytes[4] = (flow->ipaddr.ip4addr>>8) & 0xff;
	eth_hdr->d_addr.addr_bytes[5] = flow->ipaddr.ip4addr & 0xff;
	rte_eth_macaddr_get(flow->nic->port, &eth_hdr->s_addr);
	eth_hdr->ether_type = RTE_CPU_TO_BE_16(ETHER_TYPE_IPv4);
	offset = sizeof(struct ether_hdr);

	mb->data_len = mb->pkt_len = offset;

	ipv4_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv4_hdr*, offset);
	ipv4_hdr->version_ihl = 0x40|5;
	ipv4_hdr->type_of_service = 0xc0;
	ipv4_hdr->total_length = RTE_CPU_TO_BE_16(4*5 + sizeof(*igmp_hdr));
	ipv4_hdr->packet_id = 0;
	ipv4_hdr->fragment_offset = 0;
	ipv4_hdr->time_to_live = 1;
	ipv4_hdr->next_proto_id = IPPROTO_IGMP;
	ipv4_hdr->hdr_checksum = 0;
	ipv4_hdr->src_addr = rte_cpu_to_be_32(flow->nic->ipaddr.ip4addr);
	switch(type){
	default:
		printf("igmp2template: invalid IGMP type %#x\n", type);
		rte_pktmbuf_free(mb);
		return 0;
	case 0x16:
		ipv4_hdr->dst_addr = rte_cpu_to_be_32(flow->ipaddr.ip4addr);
		break;
	case 0x17:
		ipv4_hdr->dst_addr = rte_cpu_to_be_32(IPv4(224, 0, 0, 2));
		break;
	}
	ipv4_hdr->hdr_checksum = iphdr_cksum(ipv4_hdr, sizeof(*ipv4_hdr));

	offset += sizeof(struct ipv4_hdr);
	mb->data_len = mb->pkt_len = offset;

	igmp_hdr = rte_pktmbuf_mtod_offset(mb, struct igmp_hdr*, offset);
	igmp_hdr->type = type;
	igmp_hdr->mrt = 0;
	igmp_hdr->checksum = 0;
	igmp_hdr->group = rte_cpu_to_be_32(flow->ipaddr.ip4addr);
	igmp_hdr->checksum = iphdr_cksum(ipv4_hdr,
						sizeof(*ipv4_hdr) + sizeof(*igmp_hdr));
	
	offset += sizeof(struct igmp_hdr);
	mb->data_len = mb->pkt_len = offset;
	//printpkt(mb, 64);

	return mb;
}

static struct rte_mbuf*
igmp2report(Flow* flow)
{
	if(flow->mbreport == 0)
		flow->mbreport = igmp2template(flow, 0x16);
	return rte_pktmbuf_clone(flow->mbreport, flow->nic->mempool);
}

static struct rte_mbuf*
igmp2leave(Flow* flow)
{
	if(flow->mbleave == 0)
		flow->mbleave = igmp2template(flow, 0x17);
	return rte_pktmbuf_clone(flow->mbleave, flow->nic->mempool);
}

void
igmpfmt(void* p)
{
	struct igmp_hdr *igmp_hdr;

	/* simplistic, will do for now */
	igmp_hdr = p;
	printf(" - IGMP packet: type = %#x, MRT %u, group %#x",
		igmp_hdr->type,
		igmp_hdr->mrt,
		rte_be_to_cpu_32(igmp_hdr->group));
}

void
igmpleave(Flow* flow)
{
	struct rte_mbuf *mb;

	printf("F %lu: IGMP leave\n", flow - flow->nic->flows);
	rte_timer_stop_sync(&flow->timer);
	mb = igmp2leave(flow);
	rte_eth_tx_burst(flow->nic->port, 1, &mb, 1);
	flow->ntxpkt++;
}

void
igmpshutdown(Nic* nic)
{
	Flow *flow;

	for(flow = nic->flows; flow < nic->flows + nic->nextflow; flow++){
		if(flow->nic == 0 || flow->ipaddr.af != AF_INET)
			continue;
		if(ipaddrmulticast(&flow->ipaddr) != 0)
			continue;

		igmpleave(flow);
	}
}

static struct igmp_hdr*
igmpgethdr(struct rte_mbuf* mb)
{
	char buf[128];
	uint16_t offset;
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;
	struct igmp_hdr *igmp_hdr;

	eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr*);
	if(RTE_BE_TO_CPU_16(eth_hdr->ether_type) != ETHER_TYPE_IPv4){
		rte_get_rx_ol_flag_list(mb->ol_flags, buf, sizeof(buf));
		printf("igmpgethdr: not an IPV4 packet: ol_flags: %s\n", buf);
		return 0;
	}
	offset = sizeof(struct ether_hdr);

	ipv4_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv4_hdr*, offset);
	if(ipv4_hdr->next_proto_id != IPPROTO_IGMP){
		rte_get_rx_ol_flag_list(mb->ol_flags, buf, sizeof(buf));
		printf("igmpgethdr: not an IGMP packet: ol_flags: %s\n", buf);
		return 0;
	}
	offset += (ipv4_hdr->version_ihl & IPV4_HDR_IHL_MASK) * IPV4_IHL_MULTIPLIER;

	igmp_hdr = rte_pktmbuf_mtod_offset(mb, void*, offset);
	printf("igmpgethdr: IGMP packet: type = %#x, MRT %u, group %#x\n",
		igmp_hdr->type,
		igmp_hdr->mrt,
		rte_be_to_cpu_32(igmp_hdr->group));

	return igmp_hdr;
}

static void
igmptimer(struct rte_timer* timer, void* arg)
{
	Nic *nic;
	Flow *flow;
	long int fno;
	struct rte_mbuf *mb;

	((void)(timer));										/* USED */

	flow = arg;
	nic = flow->nic;
	if(flow->ipaddr.af == AF_UNSPEC || ipaddrmulticast(&flow->ipaddr) != 0)
		return;

	mb = igmp2report(flow);
	rte_eth_tx_burst(nic->port, 1, &mb, 1);
	flow->ntxpkt++;

	fno = flow - nic->flows;
	printf("F %lu: URI %u\n", fno, flow->uri);
	if(flow->uri != 0){
		igmptimerset(flow, nic->hz*flow->uri);
		flow->uri--;
	}
}

int
igmptimerset(Flow* flow, uint64_t ticks)
{
	int r;

	r = rte_timer_reset(
			&flow->timer, ticks, SINGLE, flow->nic->lid, igmptimer, flow
		);
	if(r < 0)
		printf("igmptimerset: F[%lu]:  ticks %"PRIu64 " : r %d\n",
			flow - flow->nic->flows, ticks, r);

	return r;
}

void
igmpquery(Nic* nic, struct rte_mbuf* query)
{
	Flow *flow;
	long int fno;
	uint32_t group;
	uint64_t mrt, now;
	struct igmp_hdr *igmp_hdr;

	if((igmp_hdr = igmpgethdr(query)) == 0)
		return;
	if(igmp_hdr->type != 0x11){
		printf("igmpquery: type %#x, MRT %u, group %#x\n", igmp_hdr->type, 
			igmp_hdr->mrt, rte_be_to_cpu_32(igmp_hdr->group));
		return;
	}

	/*
	 * MRT is in units of 1/10 second; approximate it in hz.
	 */
	mrt = igmp_hdr->mrt * nic->hz/10;
	printf("igmpquery: mrt %"PRIu64 " from %u * %"PRIu64 "\n",
		mrt, igmp_hdr->mrt, nic->hz/10);
	group = rte_be_to_cpu_32(igmp_hdr->group);

	for(flow = nic->flows; flow < nic->flows + nic->nextflow; flow++){
		/*
		 * IGMP is IPv4 only. Thank goodness the request was for
		 * "IGMP support", and no ,mention of version, IPv6, or "MLD".
		 * Whew.
		 */
		if(flow->nic == 0 || flow->ipaddr.af != AF_INET)
			continue;
		if(ipaddrmulticast(&flow->ipaddr) != 0)
			continue;

		fno = flow - nic->flows;
		/*
		 * Nothing to do if not a general query and no match.
		 */
		if(group != 0 && flow->ipaddr.ip4addr != group){
			printf("igmpquery: F[%lu]: group %#x, ip4addr %#x\n",
				fno, group, flow->ipaddr.ip4addr);
			continue;
		}

		/*
		 * If there is an URI pending, no need to do anything,
		 * a join/report is about to go out; this should take care
		 * of the only situation the timer can be in RUNNING or CONFIG.
		 * Assumption here is MRT will not be <= 1s.
		 *
		 * Using the internal timer state here is not really right,
		 * but want to make sure all the bases are covered.
		 */
		printf("igmpquery: F[%lu]: uri %d timer state %d\n",
			fno, flow->uri, flow->timer.status.state);
		if(flow->uri != 0)
			continue;

		now = rte_rdtsc();
		switch(flow->timer.status.state){
		default:
		case RTE_TIMER_RUNNING:
		case RTE_TIMER_CONFIG:
			/*
			 * RUNNING/CONFIG should not happen.
			 * What to do if it does?
			 */
			continue;

		case RTE_TIMER_STOP:
			/*
			 * No current timer, good to go. What random time is it?
			 */
			break;

		case RTE_TIMER_PENDING:
			/*
			 * Pending timer. Reset to a random value only if the
			 * requested MRT is less than the remaining value of the
			 * running timer. Highly unlikely.
			 */
			if(now < flow->timer.expire){
				if(mrt >= flow->timer.expire - now)
					continue;
				rte_timer_stop(&flow->timer);
			}
			break;
		}
		igmptimerset(flow, (now % mrt)/2 + 1);
	}
}

void
igmpflow(Flow* flow, struct rte_mbuf* mb)
{
	igmpquery(flow->nic, mb);

	flow->nrxpkt++;
	dpdkflowstat(flow, mb, 0);
	rte_pktmbuf_free(mb);
	flow->nrxpktfree++;
}
