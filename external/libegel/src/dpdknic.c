
#include "common.h"
#include "dpdk.h"

static Nic nics[RTE_MAX_ETHPORTS];

static const struct rte_eth_conf eth_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.ignore_offload_bitfield = 1,
	},
};

Nic*
dpdkgetnic(uint16_t port)
{
	if(port >= RTE_MAX_ETHPORTS)
		return 0;

	return &nics[port];
}

__attribute__((used))
static void
dpdknicstop(Nic* nic)
{
	uint16_t port;

	port = nic->port;
	rte_eth_promiscuous_disable(port);
	rte_eth_dev_stop(port);
}

static int
dpdknicstart(Nic* nic)
{
	int r;
	uint16_t port;

	/*
	 * Start the Ethernet port.
	 */
	port = nic->port;
	if((r = rte_eth_dev_start(port)) < 0)
		return r;
	rte_eth_promiscuous_enable(port);

	return 0;
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mempool passed as a parameter.
 */
static int
dpdknicinit(Nic* nic, struct rte_mempool* mempool)
{
	int r;
	uint16_t port, q;
	struct rte_eth_conf eth_conf;
	struct rte_eth_txconf txconf;
	struct rte_eth_dev_info dev_info;

	port = nic->port;
	if(!rte_eth_dev_is_valid_port(port))
		return -1;

	eth_conf = eth_conf_default;
	rte_eth_dev_info_get(port, &dev_info);
	if(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		eth_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	/*
	 * Configure the Ethernet device:
	 *	perfect hashing for flow filtering;
	 *	no RSS.
	 * To do:
	 *	dump default dev_conf to see what's there.
	 */
	eth_conf.fdir_conf.mode = RTE_FDIR_MODE_PERFECT;
	eth_conf.rx_adv_conf.rss_conf.rss_hf = 0;
	r = rte_eth_dev_configure(
			port,
			nic->nrxqueue,
			nic->ntxqueue,
			&eth_conf);
	if(r != 0)
		return r;

	r = rte_eth_dev_adjust_nb_rx_tx_desc(
			port,
			&nic->nrxdesc,
			&nic->ntxdesc);
	if(r != 0)
		return r;

	for(q = 0; q < nic->nrxqueue; q++){
		r = rte_eth_rx_queue_setup(
				port,
				q,
				nic->nrxdesc,
				rte_eth_dev_socket_id(port),
				NULL,
				mempool);
		if(r < 0)
			return r;
	}

	txconf = dev_info.default_txconf;
	txconf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
	txconf.offloads = eth_conf.txmode.offloads;
	for(q = 0; q < nic->ntxqueue; q++){
		r = rte_eth_tx_queue_setup(
				port,
				q,
				nic->ntxdesc,
				rte_eth_dev_socket_id(port),
				&txconf);
		if(r < 0)
			return r;
	}

	/*
	 * Used to start the Ethernet port here:
	 *	display the MAC address;
	 *	enable promiscuous mode and print;
	 *	print current link status.
	 *
	 * Moved out to a separate function for more flexible
	 * flexible initialisation before receiving packets?
	 * Q: unfortunately, punting until after flows are set seems to cause 
	i40e_flow_add_del_fdir_filter(): Conflict with existing flow director rules!
	 * messages, though the flows do seem to function properly. Hmmm?
	return 0;
	 */
	return dpdknicstart(nic);
}

static int
optargu16(const char* arg, const char* optarg, uint16_t* u16)
{
	unsigned ui;
	char *endptr;

	errno = 0;
	ui = strtol(optarg, &endptr, 0);
	if(errno != 0 || *endptr != '\0' || ui >= 0x10000 || ui == 0){
		printf("optargu16: --%s=%s invalid, not changed\n", arg, optarg);
		return -1;
	}
	*u16 = ui;

	return 0;
}

static int
dpdknicargs(int argc, char* argv[], Nic* nic)
{
	int r, x;
	struct ether_addr addr;
	struct rte_eth_link link;
	char macaddr[ETHER_ADDR_FMT_SIZE];
	char addrstr[RTE_MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];

	opterr = 0;

	for(;;){
		static struct option options[] = {
			{ "rxq",				1,	0,	0 },				/*  0 */
			{ "rxd",				1,	0,	0 },				/*  1 */
			{ "txq",				1,	0,	0 },				/*  2 */
			{ "txd",				1,	0,	0 },				/*  3 */
			{ "burst",				1,	0,	0 },				/*  4 */

			{ "start",				0,	0,	0 },				/*  5 */
			{ "stop",				0,	0,	0 },				/*  6 */
			{ "cfg",				0,	0,	0 },				/*  7 */
			{ "status",				0,	0,	0 },				/*  8 */
			{ 0,					0,	0,	0 }					/* add here */
		};

		switch(getopt_long(argc, argv, "", options, &x)){
		default:
			printf("dpdknic usage...\n");
			return -1;

		case -1:
			break;

		case 0:
			/*
			 * Some options can only be set before the NIC
			 * is operational; if a new value is out of range, leave
			 * current setting untouched (not a failure).
			 */
			if(nic->addr == 0){
				switch(x){
				default:
					printf("dpdknicargs: mission impossible %d...\n", x);
					return -1;

				case 0:
					optargu16(options[x].name, optarg, &nic->nrxqueue);
					break;

				case 1:
					optargu16(options[x].name, optarg, &nic->nrxdesc);
					break;

				case 2:
					optargu16(options[x].name, optarg, &nic->ntxqueue);
					break;

				case 3:
					optargu16(options[x].name, optarg, &nic->ntxdesc);
					break;

				case 4:
					optargu16(options[x].name, optarg, &nic->burst);
					break;
				}
				continue;
			}

			switch(x){
			default:
				printf("dpdknicargs: mission impossible %d...\n", x);
				return -1;

			case 5:
				/*
				 * Disabled for now, see comment in dpdknicinit().
				dpdknicstart(nic);
				 */
				break;

			case 6:
				/*
				 * Ditto.
				dpdknicstop(nic);
				 */
				break;

			case 7:
				printf("NIC: addr %s; IP %s; port %"PRIu16 ";\n", 
					nic->addr, ipaddrfmt(&nic->ipaddr, addrstr), nic->port);
				printf("     rxq %"PRIu16 " rxd %"PRIu16,
					nic->nrxqueue, nic->nrxdesc);
				printf(" txq %"PRIu16 " txd %"PRIu16 "; burst %"PRIu16 "\n",
					nic->ntxqueue, nic->ntxdesc, nic->burst);
				break;

			case 8:
				rte_eth_macaddr_get(nic->port, &addr);
				ethaddrfmt(macaddr, ETHER_ADDR_FMT_SIZE, &addr);
				printf("ETH port %"PRIu16 ": MAC: %s\n", nic->port, macaddr);

				printf("ETH port %"PRIu16 ": promiscuous: %d\n",
					nic->port, rte_eth_promiscuous_get(nic->port));
				printf("ETH port %"PRIu16 ": allmulticast: %d\n",
					nic->port, rte_eth_allmulticast_get(nic->port));

				rte_eth_link_get(nic->port, &link);
				printf("ETH port %"PRIu16 ": speed %u duplex %u status %u\n",
						nic->port, link.link_speed, link.link_duplex,
						link.link_status);
				break;
			}
			continue;
		}
		break;
	}

	r = optind-1;
	optind = 1;

	return r;
}

static int dpdkflowcore(void*);

int
dpdknic(int argc, char* argv[], void* rdata, uint* rlen)
{
	int r;
	Nic *nic;
	IPaddr ipaddr;
	uint16_t port;
	struct rte_mempool* mempool;
	char addrstr[RTE_MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];

	((void)(rdata)); ((void)(rlen));						/* USED */

	/*
	 * nic pci-addr ip-addr [option ...]
	 */
	if(argc < 3)
		return -1;

	argc--; argv++;
	if((r = rte_eth_dev_get_port_by_name(argv[0], &port)) != 0){
		printf("dpdknic: \"%s\": %s\n", argv[0], strerror(-r));
		return -1;
	}

	if((nic = dpdkgetnic(port)) == 0)
		return -1;
	if(nic->addr == 0){
		strncpy(nic->name, argv[0], sizeof(nic->name)-1);
		nic->port = port;
		nic->nrxqueue = 8;
		nic->nrxdesc = 512;
		nic->ntxqueue = 4;
		nic->ntxdesc = 1024;
		nic->burst = 32;
	}

	argc--; argv++;
	if(ipaddrparse(&ipaddr, AF_UNSPEC, argv[0]) <= 0){
		printf("dpdknic: \"%s\": invalid address: %s\n", nic->name, argv[0]);
		return -1;
	}

	if(nic->ipaddr.af != AF_UNSPEC){
		if(ipaddrcmp(&nic->ipaddr, &ipaddr) != 0){
			printf("dpdknic \"%s\": inconsistent IP address: %s",
				nic->name, ipaddrfmt(&nic->ipaddr, addrstr));
			printf(" != %s\n", ipaddrfmt(&ipaddr, addrstr));

			return -1;
		}
	}
	else
		nic->ipaddr = ipaddr;

	while((r = dpdknicargs(argc, argv, nic)) > 0){
		argc -= r;
		argv += r;
	}
	if(r < 0)
		return -1;

	/*
	 * Have a valid NIC configuration, if already initialised,
	 * done, otherwise complete configuration, initialise port,
	 * and initialise flow mechanism.
	 *
	 * Everything appears to come in on queue 0 until flows are initialised.
	 * Queue 0 is the dumpster, queue 1 is IGMP.
	 *
	 * Only fill in nic->addr if initialisation succeeds.
	 */
	if(nic->addr != 0)
		return nic->port;

	if((mempool = dpdkgetmempool(nic->port)) == 0){
		printf("dpdknic \"%s\": port %"PRIu16 ": mempool\n", nic->name, port);
		return -1;
	}
	if(dpdknicinit(nic, mempool) != 0){
		printf("dpdknic \"%s\": Can't init port %"PRIu16 "\n", nic->name, port);
		return -1;
	}
	if(dpdkflowinit(nic) < 0){
		printf("dpdknic \"%s\": Can't init drop/IGMP flows\n", nic->name);
		return -1;
	}

	nic->mempool = mempool;
	nic->addr = nic->name;

	/*
	 * Launch the per-NIC processing thread on it's own lcore.
	 * Needs work...
	 *
	 * Returns 0 if OK, -BUSY if lcore not in WAIT state.
	 * rte_get_lcore_state(id) returns WAIT, RUNNING, FINISHED.
	 *
	 * Should run a loop to find one here...
	 * Wrong to return -1 if the NIC is marked
	 * as running (nic->addr != 0).
	 */
	nic->lid = rte_get_next_lcore(-1, 1, 1);
	if((r = rte_eal_remote_launch(dpdkflowcore, nic, nic->lid)) < 0){
		printf("dpdknic: launch on lid %u: %s\n", nic->lid, strerror(-r));

		return -1;
	}

	return nic->port;
}

static int
dpdkflowcore(void* arg)
{
	Nic *nic;
	Flow *flow;
	uint64_t now, past;
	uint16_t i, nrx, port, q;
	struct rte_mbuf **burst, *mb;

	nic = arg;
	nic->hz = rte_get_tsc_hz();
	printf("dpdkflowcore: lid %u Hz %"PRIu64 "\n", nic->lid, nic->hz);

	if((burst = rte_zmalloc(0, sizeof(struct rte_mbuf*) * nic->burst, 0)) == 0){
		printf("dpdkflowcore: id %u: no memory\n", rte_lcore_id());
		return -1;
	}

	port = nic->port;
	past = 0;
	while(dpdkdone == 0){
		for(q = 0; q < nic->nrxqueue; q++){
			if((nrx = rte_eth_rx_burst(port, q, burst, nic->burst)) == 0)
				continue;

			flow = nic->flows + q;
			for(i = 0; i < nrx; i++){
				mb = burst[i];
				if(q >= nic->nextflow){
					printf("Q %u (> %u): unexpected pkt\n", q, nic->nextflow);
					printpkt(mb, 0);
					rte_pktmbuf_free(mb);
					continue;
				}

				if(flow->nic != 0 && flow->f != 0)
					flow->f(flow, mb);
				else
					rte_pktmbuf_free(mb);
			}

			if(flow->nic != 0 && nrx > flow->nrxburst)
				flow->nrxburst = nrx;
		}

		now = rte_rdtsc();
		if(unlikely((now - past) > nic->hz/10)){
			rte_timer_manage();
			past = now;
		}
	}

	/*
	 * To do:
	 *	IGMP leave (and any timers);
	 *	flush any mbufs in queues;
	 *	terminate above loop on nic->something;
	 *	streamline inner loop;
	 * and things like
	rte_flow_flush(port, &error);
	rte_eth_dev_stop(port);
	rte_eth_dev_close(port);
	 */
	igmpshutdown(nic);

	rte_free(burst);

	return 0;
}
