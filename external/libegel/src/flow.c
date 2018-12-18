#ifdef _WIN32

#ifdef __GCC__
#define RTE_BE_TO_CPU_16(be_16_v)  rte_be_to_cpu_16((be_16_v)) 
#define RTE_CPU_TO_BE_16(cpu_16_v) rte_cpu_to_be_16((cpu_16_v)) 
#else // __GCC__
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN 
#define RTE_BE_TO_CPU_16(be_16_v)  (be_16_v) 
#define RTE_CPU_TO_BE_16(cpu_16_v) (cpu_16_v) 
#else // RTE_BYTE_ORDER == RTE_BIG_ENDIAN 
#define RTE_BE_TO_CPU_16(be_16_v) \
	(uint16_t) ((((be_16_v) & 0xFF) << 8) | ((be_16_v) >> 8)) 
#define RTE_CPU_TO_BE_16(cpu_16_v) \
	(uint16_t) ((((cpu_16_v) & 0xFF) << 8) | ((cpu_16_v) >> 8)) 
#endif // RTE_BYTE_ORDER == RTE_BIG_ENDIAN 
#endif /* __GCC__ */

#else   // _WIN32

#define RTE_BE_TO_CPU_16(be_16_v)  rte_be_to_cpu_16((be_16_v)) 
#define RTE_CPU_TO_BE_16(cpu_16_v) rte_cpu_to_be_16((cpu_16_v)) 

#endif  // _WIN32

#include "common.h"
#include "dpdk.h"

#define MAX_PATTERN_NUM		4

static struct rte_flow_item eth_item = {
	RTE_FLOW_ITEM_TYPE_ETH,
	0, 0, 0
};
static struct rte_flow_item end_item = {
	RTE_FLOW_ITEM_TYPE_END,
	0, 0, 0
};
static struct rte_flow_action end_action = {
	RTE_FLOW_ACTION_TYPE_END,
	0
};

static struct rte_flow*
flowcreate(
	uint16_t port,
	struct rte_flow_attr* attr,
	struct rte_flow_item pattern[],
	struct rte_flow_action action[])
{
	int r;
	struct rte_flow *handle;
	struct rte_flow_error error;

	if((r = rte_flow_validate(port, attr, pattern, action, &error)) < 0){
		printf("rte_flow_validate: returns %d (%s)\n", r, strerror(-r));

		return 0;
	}
	if((handle = rte_flow_create(port, attr, pattern, action, &error)) == 0)
		printf("rte_flow_create: error type %d, message: %s\n",
			error.type, error.message? error.message: "(no stated reason)");

	return handle;
}

struct rte_flow*
flowdrophandle(Nic* nic)
{
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[MAX_PATTERN_NUM];
	struct rte_flow_action action[MAX_PATTERN_NUM];

	/*
	 * Attempt to make a low-priority rule to drop
	 * incoming packets not covered by other rules.
	 * Max priority is 0, min is ??? - let's say 8...
	 * ...but no: i40e doesn't support priorities:-(.
	 *
	 * Alternative might be "isolated" mode, but again
	 * i40e is lacking.
	 * Concern about trying to use a "default drop" rule
	 * is there are words that say adding, dropping flows
	 * dynamically can alter the rule processing order,
	 * which would be a concern.
	 *
	 * With the "drop all ethernet packets" rule below,
	 * the spanning tree, etc. chatter disappears, but there
	 * is still the occasional locally administered IPv6
	 * multicast makes it though to queue 0.
	 */
	memset(&attr, 0, sizeof(struct rte_flow_attr));
	attr.ingress = 1;

	pattern[0] = eth_item;
	pattern[1] = end_item;

	/*
	 * Create the action sequence:
	 *	drop;
	 *	done.
	 */
	action[0].type = RTE_FLOW_ACTION_TYPE_DROP;
	action[1] = end_action;

	/*
	 * Validate and try to create the flow rule.
	 */
	return flowcreate(nic->port, &attr, pattern, action);
}

struct rte_flow*
flowigmphandle(Nic* nic)
{
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[MAX_PATTERN_NUM];
	struct rte_flow_action action[MAX_PATTERN_NUM];
	struct rte_flow_action_queue queue;
	struct rte_flow_item_ipv4 ip_spec;
	struct rte_flow_item_ipv4 ip_mask;

	/*
	 * IGMP has a fixed, known queue on this NIC, #1.
	 *
	 * This may change once we have more IGMP structure in
	 * place.
	 */

	/*
	 * Set the rule attribute: ingress packets only.
	 *
	 * Create the pattern stack:
	 *	first item is eth, match everything;
	 *	second item is Ipv4, match the All Hosts multicast group address
	 *	destination and IGMP protocol number.
	 */
	memset(&attr, 0, sizeof(struct rte_flow_attr));
	attr.ingress = 1;

	pattern[0] = eth_item;

	memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
	memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));
	ip_spec.hdr.next_proto_id = IPPROTO_IGMP;
	ip_mask.hdr.next_proto_id = 0xff;
	ip_spec.hdr.dst_addr = rte_cpu_to_be_32(IPv4(224, 0, 0, 1));
	ip_mask.hdr.dst_addr = rte_cpu_to_be_32(0xffffffff);
	ip_spec.hdr.src_addr = 0;
	ip_mask.hdr.src_addr = 0;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[1].spec = &ip_spec;
	pattern[1].last = 0;
	pattern[1].mask = &ip_mask;

	pattern[2] = end_item;

	/*
	 * Create the action sequence:
	 *	move a match to the known queue;
	 *	done.
	 */
	queue.index = 1;
	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = &queue;

	action[1] = end_action;

	/*
	 * Validate and try to create the flow rule.
	 */
	return flowcreate(nic->port, &attr, pattern, action);
}

struct rte_flow*
flowudphandle(Nic* nic, Flow* flow, int drop)
{
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[MAX_PATTERN_NUM];
	struct rte_flow_action action[MAX_PATTERN_NUM];
	struct rte_flow_action_queue queue;
	struct rte_flow_item_ipv4 ip_spec;
	struct rte_flow_item_ipv4 ip_mask;
	struct rte_flow_item_udp udp_spec;
	struct rte_flow_item_udp udp_mask;

	/*
	 * This is all single-threaded at this point.
	 */

	/*
	 * Set the rule attribute: ingress packets only.
	 *
	 * Create the pattern stack:
	 *	first item is eth, match everything;
	 *	second item is IPv4, match the unicast or multicast group address;
	 *	third is match the UDP port.
	 */
	memset(&attr, 0, sizeof(struct rte_flow_attr));
	attr.ingress = 1;

	pattern[0] = eth_item;

	memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
	memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));
	ip_spec.hdr.dst_addr = rte_cpu_to_be_32(flow->ipaddr.ip4addr);
	ip_mask.hdr.dst_addr = rte_cpu_to_be_32(0xffffffff);
	ip_spec.hdr.src_addr = 0;
	ip_mask.hdr.src_addr = 0;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[1].spec = &ip_spec;
	pattern[1].last = 0;
	pattern[1].mask = &ip_mask;

	memset(&udp_spec, 0, sizeof(struct rte_flow_item_udp));
	memset(&udp_mask, 0, sizeof(struct rte_flow_item_udp));
	udp_spec.hdr.dst_port = RTE_CPU_TO_BE_16(flow->udpport);
	udp_mask.hdr.dst_port = RTE_CPU_TO_BE_16(0xffff);
	pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
	pattern[2].spec = &udp_spec;
	pattern[2].last = 0;
	pattern[2].mask = &udp_mask;

	pattern[3] = end_item;

	/*
	 * Create the action sequence:
	 *	either drop or move a match to the given queue;
	 *	done.
	 */
	switch(drop){
	default:
		action[0].type = RTE_FLOW_ACTION_TYPE_DROP;
		break;
	case 0:
		queue.index = flow - nic->flows;
		action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
		action[0].conf = &queue;
		break;
	}
	action[1] = end_action;

	/*
	 * Validate and try to create the flow rule.
	 */
	return flowcreate(nic->port, &attr, pattern, action);
}
