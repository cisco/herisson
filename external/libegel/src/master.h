#ifndef _MASTER_H_
#define _MASTER_H_

#define NRQST			16
#define NRQSTARGV		20
#define NRQSTCFG		256

typedef struct Rqst Rqst;
struct Rqst {
	Rqst*		next;									/* free list */
	Q8*			q8;
	int*		reply;

	int			argc;									/* request info */
	char*		argv[NRQSTARGV];
	char		cfg[NRQSTCFG];

	int			r;										/* reply info */
	void*		rdata;
	uint*		rlen;
};

int dpdkcfginit(const char*);
int dpdkrqst(const char*, void*, uint*);

typedef struct Pkt Pkt;
struct Pkt {
	void*		payload;					/* pointer into mbuf data */
	uint16_t	len;						/* ... and length of data */

	void*		mb;							/* rte_mbuf* */
};

#endif /*_MASTER_H_ */
