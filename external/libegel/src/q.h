/*─────────────────────────────────────────────────────────────────────────────┐
│ September, 2016, Sape Mullender & Jim McKie                                  │
│                                                                              │
│ Copyright ©2016/2017 by Cisco Systems, Inc.                                  │
│ All rights reserved.                                                         │
└─────────────────────────────────────────────────────────────────────────────*/
/*
 * This has been pruned a little specifically for this application.
 */
#include <assert.h>

void	sysfatal(const char *fmt, ...) __attribute__ ((noreturn));

/* Make life easier with locks ************************************************/
typedef	pthread_mutex_t	Plock;

void	plock(Plock*);
void	punlock(Plock*);

#define nil ((void*)0)

char* argv0;

/* Queues, exchange queues, waiting on queues  ********************************/
enum {
	/* Machine dependent [duh!] */
	Cachelinebits = 6,
	Cacheline = 1 << Cachelinebits,
};

#define Roundcache(n)	(((n) + Cacheline - 1) & ~(Cacheline - 1))
#define Fillcache(n)	((Cacheline - (n)) & (Cacheline - 1))

typedef struct Q Q;
typedef struct Q8 Q8;

struct Q8 {
	Plock			lock;		/* for coordinating sleep/wakeup */
	pthread_cond_t	cond;		/* for actual wakeup */
	volatile int	waiting;	/* owner's waiting (potentially) */
};

struct Qhalf {
	volatile int	c;		/* count: # of items processed */
	int				x;		/* Exchange Q? */
	int				m;		/* n - 1 == mask */
	Plock			lock;	/* for multiple consumers/producers */
};

struct Q {
	struct Qhalf	c;
	uint8_t			_f1[Fillcache(sizeof(struct Qhalf))];
	struct Qhalf	p;
	uint8_t			_f2[Fillcache(sizeof(struct Qhalf))];
	char			*name;	/* useful during debugging */
	Q8				*cq8;	/* nil or consumer's sleep/wakeup coordination */
	Q8				*pq8;	/* nil or producer's sleep/wakeup coordination */
	uint8_t			_f3[Fillcache(2*sizeof(void*))];
	void*			d[0];
};

#define	Makeodd(x)		((void*)((uintptr_t)(x) | 1))
#define	Makeeven(x)		((void*)((uintptr_t)(x) & ~(uintptr_t)1))
#define	Isfree(x)		(((uintptr_t)(x) & 1))
#define	Isoccupied(x)	(((uintptr_t)(x) & 1) == 0)
#define Qfree			((void*)1)


Q*	qcreate(unsigned);
Q*	qxcreate(unsigned, void* (*)(void));
Q8*	q8alloc(void);

static inline void
qkick(Q8 *q8)
{
	if(q8 && q8->waiting){
		if(pthread_cond_broadcast(&q8->cond) != 0)
			sysfatal("qkick: pthread_cond_broadcast");
	}
}

/* Wait until condition becomes true */
#define qwait(q8, condition) \
	plock(&(q8)->lock); \
	(q8)->waiting++;	/* increment/decrement under lock */ \
	while(! (condition)){ \
		if(pthread_cond_wait(&(q8)->cond, &(q8)->lock) != 0) \
			sysfatal("qwait: pthread_cond_wait"); \
	} \
	(q8)->waiting--; \
	punlock(&(q8)->lock)

#ifdef notdef
/* Wait until condition becomes true */
#define qwaittimed(q8, condition, t) \
	plock(&(q8)->lock); \
	(q8)->waiting++;	/* increment/decrement under lock */ \
	while(! (condition)){ \
		int r; struct timespec ts = time2spec(t); \
		r = pthread_cond_timedwait(&(q8)->cond, &(q8)->lock, &ts); \
		if(r == ETIMEDOUT) break; \
		if(r != 0) sysfatal("qwaittimed: pthread_cond_wait"); \
	} \
	(q8)->waiting--; \
	punlock(&(q8)->lock)
#endif /* notdef */

#define qwaitonce(q8)	do{ \
		plock(&(q8)->lock); \
		if(pthread_cond_wait(&(q8)->cond, &(q8)->lock) != 0) \
			sysfatal("qwaitonce: pthread_cond_wait"); \
		punlock(&(q8)->lock); \
	}while(0)

/* Peek in the consumer queue */
static inline void*
qsnbpeek(Q *q, int o)
{
	return q->d[(q->c.c + o) & q->c.m];
}

/* Poke in the producer queue */
static inline void*
qsnbpoke(Q *q, int o)
{
	return q->d[(q->p.c + o) & q->p.m];
}

static inline void*
qsnbget(Q *q)
{
	void *e, **p;

	assert(q->c.x == 0);
	p = &q->d[q->c.c & q->c.m];
	if(Isfree((e = *p))){
		qkick(q->pq8);
		return e;	/* Non blocking */
	}
	*p = Qfree;
	q->c.c++;	/* only on success */
	return e;
}

static inline void*
qsget(Q *q)
{
	void *e, **p;

	assert(q->c.x == 0);
	p = &q->d[q->c.c++ & q->c.m];
	while(Isfree((e = *p))){
		qkick(q->pq8);
		/* wait ... */
		qwait(q->cq8, Isoccupied(*p));
	}
	*p = Qfree;
	return e;
}

static inline void*
qmnbget(Q *q)
{
	void *e, **p;

	assert(q->c.x == 0);
	plock(&q->c.lock);
	if(Isoccupied((e = *(p = &q->d[q->c.c & q->c.m])))){
		*p = Qfree;
		q->c.c++;
	}
	punlock(&q->c.lock);
	return e;
}

static inline void*
qmget(Q *q)
{
	void *e, **p;

	assert(q->c.x == 0);
	plock(&q->cq8->lock);
	if(Isfree((e = *(p = &q->d[q->c.c & q->c.m])))){
		qkick(q->pq8);
		/* wait ... */
		q->cq8->waiting++;	/* increment/decrement under lock */
		while(Isfree((e = *(p = &q->d[q->c.c & q->c.m]))))
			if(pthread_cond_wait(&q->cq8->cond, &q->cq8->lock) != 0)
				sysfatal("qmget: pthread_cond_wait");
		q->cq8->waiting--;
	}
	*p = Qfree;
	q->c.c++;
	punlock(&q->cq8->lock);
	return e;
}

static inline int
qsnbput(Q *q, void *e)
{
	void **p;

	assert(q->p.x == 0);
	p = &q->d[q->p.c & q->p.m];
	if(Isoccupied(*p)){
		qkick(q->cq8);
		return 0;
	}
	*p = e;
	q->p.c++;	/* only on success */
	return 1;
}

static inline void
qsput(Q *q, void *e)
{
	void **p;

	assert(q->p.x == 0);
	p = &q->d[q->p.c++ & q->p.m];
	if(Isoccupied(*p)){
		qkick(q->cq8);
		qwait(q->pq8, Isfree(*p));
	}
	*p = e;
}

static inline int
qmnbput(Q *q, void *e)
{
	void **p;

	assert(q->p.x == 0);
	plock(&q->p.lock);
	if(*(p = &q->d[q->p.c & q->p.m])){
		qkick(q->cq8);
		punlock(&q->p.lock);
		return 0;
	}
	*p = e;
	q->p.c++;
	punlock(&q->p.lock);
	return 1;
}

static inline void
qmput(Q *q, void *e)
{
	void **p;

	assert(q->p.x == 0);
	plock(&q->p.lock);
	if(Isoccupied(*(p = &q->d[q->p.c & q->p.m]))){
		qkick(q->cq8);
		q->pq8->waiting++;	/* increment/decrement under lock */
		while(Isoccupied(*(p = &q->d[q->p.c & q->p.m])))
			if(pthread_cond_wait(&q->pq8->cond, &q->p.lock) != 0)
				sysfatal("qmput: pthread_cond_wait");
		q->pq8->waiting--;
	}
	*p = e;
	q->p.c++;
	punlock(&q->p.lock);
}

static inline void*
qxsnbget(Q *q, void *x)
{
	void *e, **p;

	assert(q->c.x);
	p = &q->d[q->c.c & q->c.m];
	if(Isfree((e = *p))){
		qkick(q->pq8);
		return e;	/* Non blocking */
	}
	*p = Makeodd(x);
	q->c.c++;
	return e;
}

static inline void*
qxsget(Q *q, void *x)
{
	void *e, **p;

	assert(q->c.x);
	p = &q->d[q->c.c++ & q->c.m];
	while(Isfree((e = *p))){
		qkick(q->pq8);
		qwait(q->cq8, Isoccupied(*p));
	}
	*p = Makeodd(x);
	return e;
}

static inline void*
qxmnbget(Q *q, void *x)
{
	void *e, **p;

	assert(q->c.x);
	plock(&q->c.lock);
	if(Isoccupied((e = *(p = &q->d[q->c.c & q->c.m])))){
		*p = Makeodd(x);
		q->c.c++;
	}
	punlock(&q->c.lock);
	return e;
}

static inline void*
qxmget(Q *q, void *x)
{
	void *e, **p;

	assert(q->c.x);
	plock(&q->cq8->lock);
	if(Isfree((e = *(p = &q->d[q->c.c & q->c.m])))){
		qkick(q->pq8);
		q->cq8->waiting++;	/* increment/decrement under lock */
		while(Isfree((e = *(p = &q->d[q->c.c & q->c.m]))))
			if(pthread_cond_wait(&q->cq8->cond, &q->cq8->lock) != 0)
				sysfatal("qxmget: pthread_cond_wait");
		q->cq8->waiting--;
	}
	*p = Makeodd(x);
	q->c.c++;
	punlock(&q->cq8->lock);
	return e;
}

static inline void*
qxsnbput(Q *q, void *e)
{
	void **p, *x;

	assert(q->p.x);
	p = &q->d[q->p.c & q->p.m];
	if(Isoccupied((x = *p))){
		qkick(q->cq8);
		return Qfree;
	}
	*p = e;
	q->p.c++;
	return Makeeven(x);
}

static inline void*
qxsput(Q *q, void *e)
{
	void **p, *x;

	assert(q->p.x);
	p = &q->d[q->p.c++ & q->p.m];
	if(Isoccupied((x = *p))){
		qkick(q->cq8);
		/* wait ... */
		qwait(q->pq8, Isfree((x = *p)));
	}
	*p = e;
	return Makeeven(x);
}

static inline void*
qxmnbput(Q *q, void *e)
{
	void **p, *x;

	assert(q->p.x);
	plock(&q->p.lock);
	if(Isoccupied((x = *(p = &q->d[q->p.c & q->p.m])))){
		qkick(q->cq8);
		punlock(&q->p.lock);
		return Qfree;
	}
	*p = e;
	q->p.c++;
	punlock(&q->p.lock);
	return Makeeven(x);
}

static inline void*
qxmput(Q *q, void *e)
{
	void **p, *x;

	assert(q->p.x);
	plock(&q->pq8->lock);
	if(Isoccupied((x = *(p = &q->d[q->p.c & q->p.m])))){
		qkick(q->cq8);
		/* wait ... */
		q->pq8->waiting++;	/* increment/decrement under lock */
		while(Isoccupied((x = *(p = &q->d[q->p.c & q->p.m]))))
			if(pthread_cond_wait(&q->pq8->cond, &q->pq8->lock) != 0)
				sysfatal("qxmput: pthread_cond_wait");
		q->pq8->waiting--;
	}
	*p = e;
	q->p.c++;
	punlock(&q->pq8->lock);
	return Makeeven(x);
}
