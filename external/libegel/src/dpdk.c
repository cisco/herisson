
#include "common.h"
#include "dpdk.h"

/*
 */
static struct rte_mempool* mempool;

static unsigned num_mbufs = 8191;
static unsigned mbuf_cache_size = 256;
static uint16_t mbuf_buf_size = RTE_MBUF_DEFAULT_BUF_SIZE;

struct rte_mempool*
dpdkgetmempool(uint16_t port)
{
	((void)(port));										/* USED */

	return mempool;
}

static int
dpdkealargs(int argc, char* argv[])
{
	int r, x;
	unsigned ui;
	char *endptr;

	opterr = 0;

	for(;;){
		static struct option options[] = {
			{"num-mbufs",			1,	0,	0 },
			{"mbuf-cache-size",		1,	0,	0 },
			{"mbuf-buf-size",		1,	0,	0 },
			{0,						0,	0,	0 }
		};

		switch(getopt_long(argc, argv, "", options, &x)){
		default:
			return -1;

		case -1:
			break;

		case 0:
			switch(x){
			default:
				return -1;

			case 0:
				errno = 0;
				ui = strtol(optarg, &endptr, 0);
				if(errno != 0 || *endptr != '\0' || ui <= 1024){
					printf("Option --%s=%s invalid, using %u\n",
						options[x].name, optarg, num_mbufs);
					break;
				}
				num_mbufs = ui;
				break;

			case 1:
				errno = 0;
				ui = strtol(optarg, &endptr, 0);
				if(errno != 0 || *endptr != '\0' || ui < 128){
					printf("Option --%s=%s invalid, using %u\n",
						options[x].name, optarg, mbuf_cache_size);
					break;
				}
				mbuf_cache_size = ui;
				break;

			case 2:
				errno = 0;
				ui = strtol(optarg, &endptr, 0);
				if(errno != 0 || *endptr != '\0' || ui >= 0x10000 || ui == 0){
					printf("Option --%s=%s invalid, using %u\n",
						options[x].name, optarg, mbuf_buf_size);
					break;
				}
				mbuf_buf_size = ui;
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

int
dpdkeal(int argc, char* argv[], void* rdata, uint* rlen)
{
	int nport, r;

	/*
	 * Process EAL options and configuration options
	 * for creating the mempool.
	 *
	 * rte_eal_init() *should* fail if tried more than once...
	 *
	 * Note: the argument count returned is one more than the
	 * actual count to account for either argv[0] or "--".
	 */
	((void)(rdata)); ((void)(rlen));						/* USED */
	if((r = rte_eal_init(argc, argv)) < 0){
		printf("eal: EAL init\n");
		return -1;
	}
	argc -= r;
	argv += r;

	while((r = dpdkealargs(argc, argv)) > 1){
		argc -= r;
		argv += r;
	}

	if(r < 0)
		return -1;

#ifdef _WIN32
	if ((nport = rte_eth_dev_count()) == 0) {
#else	// _WIN32
	if((nport = rte_eth_dev_count_avail()) == 0){
#endif	// _WIN32
		printf("eal: No usable ports\n");
		return -1;
	}

	/*
	 * To do:
	 * this is simplistic, takes no account of NUMA, etc.
	 */
	mempool = rte_pktmbuf_pool_create(
					"MBUF_POOL",
					num_mbufs * nport,
					mbuf_cache_size,
					0,
					mbuf_buf_size,
					rte_socket_id());
	if(mempool == NULL){
		printf("eal: can't create mempool\n");
		return -1;
	}

	rte_timer_subsystem_init();

	return r;
}

int
dpdkexit(int argc, char* argv[], void* rdata, uint* rlen)
{
	((void)(argv));	((void)(rdata)); ((void)(rlen));		/* USED */

	printf("exit: argc %d: TBF\n", argc);
	dpdkdone = 1;

	return 0;
}
