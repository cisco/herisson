#include <cstdio> 
#include <cstdlib>
#include <cstring>          // strcmp
#include <cerrno>
#include <iostream>     // cout
#ifdef _WIN32
#include <winsock2.h>    
#include <Ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#define READSOCKET(s, b, l)     recv(s, b, l, 0)
#define CLOSESOCKET             closesocket
#define MSG_NOSIGNAL            0
#define SETSOCKOPT_REF          (const char*)
#else
#include <unistd.h>         // close
#include <sys/socket.h>     // socket, shutdown, listen, bind, 
#include <netinet/in.h>     // struct sockaddr_in
#include <arpa/inet.h>      // inet_addr
#include <netdb.h>          // gethostbyname
#include <sys/select.h>
#define CLOSESOCKET     close
#define READSOCKET      read
#define SETSOCKOPT_REF  
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <sys/types.h>
#include <assert.h>
#include <ifaddrs.h>
#endif

#include "log.h"
#include "mcastutil.h"

int
get_addr (const char *hostname,
          const char *service,
          int         family,
          int         socktype,
          struct sockaddr_storage *addr)
{
    struct addrinfo hints, *res, *ressave;
    int n, sockfd, retval;

    retval = -1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = family;
    hints.ai_socktype = socktype;

    n = getaddrinfo(hostname, service, &hints, &res);

    if (n <0) {
        LOG_ERROR( "getaddrinfo error:: [%s]\n", gai_strerror(n));
        return retval;
    }

    ressave = res;

    sockfd=-1;
    while (res) {
        sockfd = (int)socket(res->ai_family,
                        res->ai_socktype,
                        res->ai_protocol);

        if (!(sockfd < 0)) {
            if (bind(sockfd, res->ai_addr, (int)res->ai_addrlen) == 0) {
                CLOSESOCKET(sockfd);
                memcpy(addr, res->ai_addr, sizeof(*addr));
                retval=0;
                break;
            }

            CLOSESOCKET(sockfd);
            sockfd=-1;
        }
        res=res->ai_next;
    }

    freeaddrinfo(ressave);

    return retval;
}

int
joinGroup(int sockfd, int loopBack, int mcastTTL,
          struct sockaddr_storage *addr)
{
    int r1, r2, r3, retval;

    retval=-1;

    switch (addr->ss_family) {
        case AF_INET: {
            struct ip_mreq      mreq;

            mreq.imr_multiaddr.s_addr=
                ((struct sockaddr_in *)addr)->sin_addr.s_addr;
            mreq.imr_interface.s_addr= INADDR_ANY;

            r1= setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP,
                            SETSOCKOPT_REF &loopBack, sizeof(loopBack));
            if (r1<0)
                LOG_ERROR("joinGroup:: IP_MULTICAST_LOOP:: ");

            r2= setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL,
                            SETSOCKOPT_REF &mcastTTL, sizeof(mcastTTL));
            if (r2<0)
               LOG_ERROR("joinGroup:: IP_MULTICAST_TTL:: ");

            r3= setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                            SETSOCKOPT_REF &mreq, sizeof(mreq));
            if (r3<0)
                LOG_ERROR("joinGroup:: IP_ADD_MEMBERSHIP:: ");

        } break;

        case AF_INET6: {
           struct ipv6_mreq    mreq6;

           memcpy(&mreq6.ipv6mr_multiaddr,
                  &(((struct sockaddr_in6 *)addr)->sin6_addr),
                  sizeof(struct in6_addr));

           mreq6.ipv6mr_interface= 0; // cualquier interfaz

           r1= setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, 
                            SETSOCKOPT_REF &loopBack, sizeof(loopBack));
           if (r1<0)
               LOG_ERROR("joinGroup:: IPV6_MULTICAST_LOOP:: ");

           r2= setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, 
                            SETSOCKOPT_REF &mcastTTL, sizeof(mcastTTL));
           if (r2<0)
               LOG_ERROR("joinGroup:: IPV6_MULTICAST_HOPS::  ");

           r3= setsockopt(sockfd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, 
                            SETSOCKOPT_REF &mreq6, sizeof(mreq6));
           if (r3<0)
              LOG_ERROR("joinGroup:: IPV6_ADD_MEMBERSHIP:: ");

        } break;

        default:
            r1=r2=r3=-1;
    }

    if ((r1>=0) && (r2>=0) && (r3>=0))
        retval=0;

    return retval;
}

int isMulticast(struct sockaddr_storage *addr)
{
    int ret = -1;

    switch (addr->ss_family) {
        case AF_INET: {
            struct sockaddr_in *addr4=(struct sockaddr_in *)addr;
            ret = IN_MULTICAST(ntohl(addr4->sin_addr.s_addr));
        } break;

        case AF_INET6: {
            struct sockaddr_in6 *addr6=(struct sockaddr_in6 *)addr;
            ret = IN6_IS_ADDR_MULTICAST(&addr6->sin6_addr);
        } break;

        default:
           ;
    }

    return ret;
} 
