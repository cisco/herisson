
#include "common.h"
#include "dpdk.h"

void
ethaddrfmt(char* buf, uint16_t size, struct ether_addr* ethaddr)
{
	snprintf(buf, size, "%02x:%02x:%02x:%02x:%02x:%02x",
		 ethaddr->addr_bytes[0],
		 ethaddr->addr_bytes[1],
		 ethaddr->addr_bytes[2],
		 ethaddr->addr_bytes[3],
		 ethaddr->addr_bytes[4],
		 ethaddr->addr_bytes[5]);
}

#include <rte_net.h>

void
printpkthex(struct rte_mbuf* mb, int max)
{
	int i, n;
	uint8_t *p;

	p = rte_pktmbuf_mtod_offset(mb, uint8_t*, 0);

	if(max == 0)
		max = 64;
	if((n = (int)mb->pkt_len) > max)
			n = max;
	printf("\n");
	for(i = 0; i < n; i++){
		printf(" %02" PRIx8, *p++);
		if((i % 16) == 15)
			printf("\n");
	}
	printf("\n");
}

void
printpkt(struct rte_mbuf* mb, int max)
{
	uint32_t sw_packet_type;
	struct ether_hdr *eth_hdr;
	struct rte_net_hdr_lens hdr_lens;
	uint16_t eth_type, is_encapsulation;
	char buf[256], macaddr[ETHER_ADDR_FMT_SIZE];

	eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr*);

	ethaddrfmt(macaddr, ETHER_ADDR_FMT_SIZE, &eth_hdr->s_addr);
	printf("  src=%s", macaddr);

	ethaddrfmt(macaddr, ETHER_ADDR_FMT_SIZE, &eth_hdr->d_addr);
	printf(" - dst=%s", macaddr);

	eth_type = RTE_BE_TO_CPU_16(eth_hdr->ether_type);
	printf(" - type=0x%04x - length=%u - nb_segs=%d",
	       eth_type, (unsigned)mb->pkt_len, (int)mb->nb_segs);

	is_encapsulation = RTE_ETH_IS_TUNNEL_PKT(mb->packet_type);

	sw_packet_type = rte_net_get_ptype(mb, &hdr_lens,
		RTE_PTYPE_ALL_MASK);
	rte_get_ptype_name(sw_packet_type, buf, sizeof(buf));
	printf(" - sw ptype: %s", buf);
	if(sw_packet_type & RTE_PTYPE_L2_MASK)
		printf(" - l2_len=%d", hdr_lens.l2_len);
	if(sw_packet_type & RTE_PTYPE_L3_MASK)
		printf(" - l3_len=%d", hdr_lens.l3_len);
	if(sw_packet_type & RTE_PTYPE_L4_MASK)
		printf(" - l4_len=%d", hdr_lens.l4_len);
	if(sw_packet_type & RTE_PTYPE_TUNNEL_MASK)
		printf(" - tunnel_len=%d", hdr_lens.tunnel_len);
	if(sw_packet_type & RTE_PTYPE_INNER_L2_MASK)
		printf(" - inner_l2_len=%d", hdr_lens.inner_l2_len);
	if(sw_packet_type & RTE_PTYPE_INNER_L3_MASK)
		printf(" - inner_l3_len=%d", hdr_lens.inner_l3_len);
	if(sw_packet_type & RTE_PTYPE_INNER_L4_MASK)
		printf(" - inner_l4_len=%d", hdr_lens.inner_l4_len);

	if(is_encapsulation){
		printf(" - encapsulated, bailing\n");
		return;
	}

	if(sw_packet_type & RTE_PTYPE_L3_MASK){
		struct ipv4_hdr *ipv4_hdr;
		struct ipv6_hdr *ipv6_hdr;
		struct udp_hdr *udp_hdr;
		uint8_t l2_len, l3_len, l4_proto;

		l2_len  = sizeof(struct ether_hdr);

		/* Do not support ipv4 option field */
		if(RTE_ETH_IS_IPV4_HDR(sw_packet_type)){
			ipv4_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv4_hdr*, l2_len);
			//l3_len = (ipv4_hdr->version_ihl & IPV4_HDR_IHL_MASK) *
            //              IPV4_IHL_MULTIPLIER;
			l3_len = hdr_lens.l3_len;
			l4_proto = ipv4_hdr->next_proto_id;
		}
		else{
			l3_len = sizeof(struct ipv6_hdr);
			ipv6_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv6_hdr*, l2_len);
			l4_proto = ipv6_hdr->proto;
		}
		if(l4_proto == IPPROTO_UDP && RTE_ETH_IS_IPV4_HDR(sw_packet_type)){
			/* to be finished */
			udp_hdr = rte_pktmbuf_mtod_offset(mb,
							  struct udp_hdr*,
							  l2_len + l3_len);
			printf(" - UDP packet: dst = %#x, dst port %#x, len %u",
				rte_be_to_cpu_32(ipv4_hdr->dst_addr),
				RTE_BE_TO_CPU_16(udp_hdr->dst_port),
				RTE_BE_TO_CPU_16(udp_hdr->dgram_len)-8);
			printpkthex(mb, max);
		}
		else if(l4_proto == IPPROTO_IGMP){
			printpkthex(mb, max);
			igmpfmt(rte_pktmbuf_mtod_offset(mb, void*, l2_len + l3_len));
		}
	}
	printf("\n");
	rte_get_rx_ol_flag_list(mb->ol_flags, buf, sizeof(buf));
	printf("  ol_flags: %s\n", buf);
}
