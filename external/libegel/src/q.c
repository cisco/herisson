/*─────────────────────────────────────────────────────────────────────────────┐
│ September, 2016, Sape Mullender & Jim McKie                                  │
│                                                                              │
│ Copyright ©2016/2017 by Cisco Systems, Inc.                                  │
│ All rights reserved.                                                         │
└─────────────────────────────────────────────────────────────────────────────*/
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>								/* posix_memalign() */
#include <string.h>								/* memset() */
#include <stdarg.h>								/* errno */
#include <errno.h>

#ifdef _WIN32
#define posix_memalign(p, a, s) (((*(p)) = _aligned_malloc((s), (a))), *(p) ?0 :errno)
#define HAVE_STRUCT_TIMESPEC
#else	// _WIN32
#endif	// _WIN32

#include <pthread.h>							/* needed by q.h */
#include "common.h"
#include "q.h"
#include <rte_malloc.h>


Q8*
q8alloc(void)
{
	Q8* q8;

	if((q8 = rte_zmalloc("Q8*", sizeof(*q8), Cacheline)) == 0)
		sysfatal("q8alloc: aligned_alloc");
	if(pthread_mutex_init(&q8->lock, nil) != 0
	|| pthread_cond_init(&q8->cond, nil) != 0)
		sysfatal("q8alloc: pthread_init");
	q8->waiting = 0;
	return q8;
}

Q*
qcreate(unsigned n)
{
	unsigned i;
	Q *q;

	for(i = 1; i < (1<<21); i <<= 1)
		if(i >= n)
			break;
	assert(i <= (1<<20));
	n = i;		/* n is now certainly a power of two */
	if((q = rte_zmalloc("Q*", n*sizeof(void*) + sizeof(*q), Cacheline)) == 0)
		sysfatal("qcreate: aligned_alloc");
	for(i = 0; i < n; i++)
		q->d[i] = Makeodd(nil);
	q->c.m = n - 1;
	q->p.m = n - 1;
	if(pthread_mutex_init(&q->c.lock, nil) != 0
	|| pthread_mutex_init(&q->p.lock, nil) != 0)
		sysfatal("qcreate: pthread_mutex_init");
	return q;
}

Q*
qxcreate(unsigned n, void* (*fill)(void))
{
	unsigned i;
	Q *q;

	for(i = 1; i < (1<<21); i <<= 1)
		if(i >= n)
			break;
	n = i;		/* n is now certainly a power of two */
	if((q = rte_zmalloc("Q*", n*sizeof(void*) + sizeof(*q), Cacheline)) == 0)
		sysfatal("qxcreate: aligned_alloc");
	/* Zero the whole queue: nil vs non-nil is used */
	for(i = 0; i < n; i++)
		q->d[i] = Makeodd(fill ? fill() : nil);
	q->c.x = 1;
	q->p.x = 1;
	q->c.m = n - 1;
	q->p.m = n - 1;
	if(pthread_mutex_init(&q->c.lock, nil) != 0
	|| pthread_mutex_init(&q->p.lock, nil) != 0)
		sysfatal("qcreate: pthread_mutex_init");
	return q;
}

void
plock(Plock *l)
{
	if(pthread_mutex_lock(l) != 0)
		sysfatal("plock: pthread_mutex_lock");
}

void
punlock(Plock *l)
{
	if(pthread_mutex_unlock(l) != 0)
		sysfatal("punlock: pthread_mutex_unlock");
}

__attribute__((noreturn)) void
sysfatal(const char *fmt, ...)
{
	char buf[256];
	va_list arg;

	va_start(arg, fmt);
	vsnprintf(buf, sizeof buf, fmt, arg);
	va_end(arg);

	if(errno)
		fprintf(stderr, "%s: %s: %s\n",
			argv0 ? argv0 : "<prog>",
			buf, strerror(errno));
	else
		fprintf(stderr, "%s: %s\n", argv0 ? argv0 : "<prog>", buf);
	exit(EXIT_FAILURE);
}
