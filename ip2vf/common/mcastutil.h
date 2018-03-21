#ifndef _MCASTUTIL_H
#define _MCASTUTIL_H

int get_addr (const char *hostname,
          const char *service,
          int         family,
          int         socktype,
          struct sockaddr_storage *addr);

int joinGroup(int sockfd, int loopBack, int mcastTTL, struct sockaddr_storage *addr);

int isMulticast(struct sockaddr_storage *addr);

#endif //_MCASTUTIL_H
