
#include "common.h"
#include <signal.h>
#include <pthread.h>

#include "dpdk.h"
#include "master.h"

#include <rte_net.h>

#ifdef _WIN32 
	pthread_t null_pthread_id = { .p = NULL,.x = 0, };
	#define INVALID_PTHREAD_ID	null_pthread_id
#else	// _WIN32
	#define INVALID_PTHREAD_ID	0
#endif	// _WIN32*/

void*
dpdkpktalloc(struct rte_mbuf* mb)
{
	Pkt *pkt;
	uint16_t len;
	void *payload;
	uint32_t sw_packet_type;
	struct rte_net_hdr_lens hdr_lens;

	/*
	 * Get the packet header lengths, verify it's a UDP packet,
	 * and try to prepend a pseudo header for the packet data in
	 * the rte_mbuf headroom.
	 *
	 * Maybe check (mb->ol_flags & FDIR) to ensure it was
	 * directed by the flow filter?
	 */
	sw_packet_type = rte_net_get_ptype(mb, &hdr_lens, RTE_PTYPE_ALL_MASK);
	if((sw_packet_type & RTE_PTYPE_L4_MASK) != RTE_PTYPE_L4_UDP)
		return 0;

	len = hdr_lens.l2_len + hdr_lens.l3_len + hdr_lens.l4_len;
	payload = rte_pktmbuf_mtod_offset(mb, void*, len);
	len = mb->pkt_len - len;

	if((pkt = (Pkt*)rte_pktmbuf_prepend(mb, sizeof(Pkt))) == 0)
		return 0;

	pkt->payload = payload;
	pkt->len = len;
	pkt->mb = mb;

	return pkt;
}

void
dpdkpktfree(void* pkt)
{
	rte_pktmbuf_free(((Pkt*)pkt)->mb);
}

typedef struct Master Master;
struct Master {
	pthread_t		tid;

#ifdef _WIN32 
	// Note: this is a quick workaround corresponding to the size issue of pthread_t under windows
	int tt;
#endif	// _WIN32*/

	pthread_mutex_t	cfglock;
	pthread_cond_t	cfgwait;
	Rqst			cfg;

	struct rte_timer timer;							/* unused currently */
	uint64_t		hz;								/* TSC Hz; ditto */
};

Master master = {
	//.tid			= 0,

	.cfglock		= PTHREAD_MUTEX_INITIALIZER,
	.cfgwait		= PTHREAD_COND_INITIALIZER,
	.cfg = {
		.reply		= nil,
		.argc		= 0,
		.r			= -1,
	}
};

static Q* rqstq;

static pthread_mutex_t reqstallocmutex = PTHREAD_MUTEX_INITIALIZER;
static Rqst* rqstfreelist;

static Rqst*
rqstalloc(void)
{
	Rqst *rqst;

	pthread_mutex_lock(&reqstallocmutex);
	if((rqst = rqstfreelist) != nil){
		rqstfreelist = rqst->next;
		rqst->reply = nil;
		rqst->argc = 0;
		rqst->r = -1;
	}
	else
		printf("rqstalloc: no free rqsts\n");
	pthread_mutex_unlock(&reqstallocmutex);

	return rqst;
}

static void
rqstfree(Rqst* rqst)
{
	assert(rqst->reply == nil);
	rqst->next = rqstfreelist;
	rqstfreelist = rqst;
	pthread_mutex_unlock(&reqstallocmutex);
}

static int
rqstallocinit(int nrqst)
{
	int r;
	Rqst *rqst;

	if((rqst = rte_zmalloc("Rqst", sizeof(Rqst) * nrqst, 0)) == 0){
		printf("rqstallocinit: no memory\n");
		return 0;
	}

	for(r = 0; r < nrqst; r++){
		rqst->q8 = q8alloc();
		rqstfree(rqst++);
	}

	return r;
}

static int
rqstparse(Rqst* rqst, const char* cfg)
{
	int argc;
	char *rock;
	unsigned n;

	/*
	 * Simple tokenise, no UTF, quoting, etc.
	 * If you want that, you know where it is...
	 */
	if((n = strlen(cfg)) >= sizeof(rqst->cfg))
		n = sizeof(rqst->cfg)-1;
	memmove(rqst->cfg, cfg, n);
	rqst->cfg[n] = 0;

	if((rqst->argv[0] = strtok_r(rqst->cfg, " \t\r\n", &rock)) == 0)
		return 0;

	for(argc = 1; argc < NRQSTARGV; argc++){
		if((rqst->argv[argc] = strtok_r(0, " \t\r\n", &rock)) == 0)
			break;
	}

	return rqst->argc = argc;						/* assignment */
}

int
dpdkrqst(const char* cfg, void* rdata, uint* rlen)
{
	int argc, r;
	Rqst *rqst;

	/*
	 * If cfg == nil here or cfg == "" below,
	 * return success for a no-op.
	 */
	if(cfg == 0)
		return 0;

	/*
	 * Allocated Rqst has .reply, .argc, .r pre-initialised.
	 */
	if((rqst = rqstalloc()) == 0)
		return -1;
	rqst->rdata = rdata;
	rqst->rlen = rlen;
	if((argc = rqstparse(rqst, cfg)) == 0){
		rqstfree(rqst);
		return 0;
	}

	/*
	 * Send the rqst, wait for done, tidy up, free.
	 */
	qmput(rqstq, rqst);
	qkick(rqstq->cq8);
	qwait(rqst->q8, rqst->reply != nil);

	r = rqst->r;
	rqst->reply = nil;

	rqstfree(rqst);

	return r;
}

/*
 * Below here runs in context of master thread,
 * above in the context of the library interface.
 */
struct command {
	const char*	name;
	int			(*f)(int, char*[], void*, uint*);
	int			isrdata;
} commands[] = {
	{ "eal",	dpdkeal,	0, },
	{ "nic",	dpdknic,	0, },
	{ "flow",	dpdkflow,	1, },
	{ "exit",	dpdkexit,	0, },
	{ 0, 0, 0, }
};

static void
rqstexecute(Rqst* rqst)
{
	struct command *cmd;

	/*
	 * Rqst already has .r initialised to -1.
	 */
	if(rqst->argc < 1)
		return;

	for(cmd = commands; cmd->name != 0; cmd++){
		if(strcmp(cmd->name, rqst->argv[0]) != 0)
			continue;

		switch(cmd->isrdata){
		default:
			/*FALLTHROUGH*/
		case 0:
			rqst->rdata = 0;
			rqst->rlen = 0;
			break; 
		case 1:
			if(rqst->rdata == 0 || rqst->rlen == 0)
				return;
			break;
		}
		rqst->r = cmd->f(rqst->argc, rqst->argv, rqst->rdata, rqst->rlen);

		return;
	}

	printf("rqstexecute: %s: invalid command\n", rqst->argv[0]);
}

static void*
dpdkthread(void* arg)
{
	Master *m;
	Rqst *rqst;

#ifndef _WIN32 
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	pthread_sigmask(SIG_SETMASK, &set, 0);
#endif

	m = arg;
	printf("dpdkthread: ... TID %"PRIu64 " lid %u\n", m->tid, rte_lcore_id());

	pthread_mutex_lock(&m->cfglock);
	if(dpdkeal(m->cfg.argc, m->cfg.argv, 0, 0) < 0){
		printf("dpdkthread: *E* dpdkeal failed\n");
		m->cfg.reply = &m->cfg.r;
		pthread_cond_signal(&m->cfgwait);
		pthread_mutex_unlock(&m->cfglock);
		pthread_exit(NULL);
	}

	rqstq = qcreate(NRQST);
	rqstq->cq8 = q8alloc();
	rqstq->pq8 = q8alloc();

	rqstallocinit(NRQST);

	rte_timer_init(&m->timer);
	m->hz = rte_get_tsc_hz();
	printf("dpdkthread: Hz %"PRIu64 "\n", m->hz);

	m->cfg.reply = &m->cfg.r;
	pthread_cond_signal(&m->cfgwait);
	pthread_mutex_unlock(&m->cfglock);

	while(dpdkdone == 0){
		if(Isoccupied((rqst = qsget(rqstq)))){
			rqstexecute(rqst);
			rqst->reply = &rqst->r;
			qkick(rqst->q8);
		}
	}

	printf("dpdkthread main loop done\n");

	rte_eal_mp_wait_lcore();
	printf("rte_eal_mp_wait_lcore done\n");

	pthread_exit(NULL);

	return NULL;
}

int
dpdkcfginit(const char* cfg)
{
	int r;
	Master *m;

	m = &master;
	pthread_mutex_lock(&m->cfglock);
	if(m->cfg.reply != 0){
		pthread_mutex_unlock(&m->cfglock);
		printf("dpdkcfginit: DPDK already initialised\n");
		return -1;
	}

	if(rqstparse(&master.cfg, cfg) == 0){
		pthread_mutex_unlock(&m->cfglock);
		printf("dpdkcfginit: invalid cfg: \"%s\"\n", cfg);
		return -1;
	}

	if((r = pthread_create(&m->tid, nil, dpdkthread, m)) != 0){
		printf("dpdkcfginit: pthread_create: %s\n", strerror(r));
		m->tid = INVALID_PTHREAD_ID;
		pthread_mutex_unlock(&m->cfglock);
		return -1;
	}
	
	while(m->cfg.reply == 0){
		if((r = pthread_cond_wait(&m->cfgwait, &m->cfglock)) != 0){
			pthread_mutex_unlock(&m->cfglock);
			printf("dpdkcfginit: pthread_cond_wait: %s\n", strerror(r));
			return -1;
		}
	}

	pthread_mutex_unlock(&m->cfglock);

	return 0;
}
