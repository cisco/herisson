#ifndef _IPADDR_H_
#define _IPADDR_H_

typedef struct IPaddr IPaddr;

struct IPaddr {
	int				af;
	union {
		uint32_t	ip4addr;						/* Note: host order */
		uint8_t		ip6addr[16];
	};
};

int ipaddrparse(IPaddr*, int af, char*);
int ipaddrcmp(IPaddr*, IPaddr*);
char* ipaddrfmt(IPaddr*, char*);
int ipaddrmulticast(IPaddr*);

#endif /* _IPADDR_H_ */
