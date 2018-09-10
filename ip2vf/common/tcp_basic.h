#ifndef _TCPBASIC_H
#define _TCPBASIC_H

#ifndef _WIN32
#define USE_NETMAP
#endif

#ifdef _WIN32
#include <Ws2tcpip.h>       // struct sockaddr_in
#else
#define SOCKET  int
#include <netinet/in.h>     // struct sockaddr_in
#endif

#ifndef INVALID_SOCKET
    #define INVALID_SOCKET  -1
#endif
#define E_OK 0
#define E_AGAIN 1
#define E_RESET 2
#define E_ERROR 3
#define E_FATAL -1

#define C_INADDR_ANY            "INADDR_ANY"
#define C_INADDR_ANY_REUSE      "INADDR_ANY_REUSE"      /* Reuse address and port */

class TCP 
{ 
private:
    SOCKET  _sock;
    SOCKET  _sockClient;
    int     _port;
    int     _TCP_timeout;
    bool    _isListening;
    struct sockaddr_in _addr;
    int     _conn_attemp;
public:
    TCP();
    virtual ~TCP();
private:
    int waitForClientConnection();
public:
    void init(const int tcp_timeout);
    int  openSocket(const char* addr, int port, const char* bindToDevice=NULL);
    int  closeSocket();
    int  readSocket(char *buffer, int *len);
    int  blockingReadSocket(const SOCKET &socketHandle,char *buffer, const int &len);
    int  writeSocket(char *buffer, int *len);
    bool isValid() { return _sockClient!=INVALID_SOCKET; };
};  // TCP


class UDP 
{ 
protected:
    SOCKET _sock;
    int    _port;
    int    _TCP_timeout;
    int    _af;
    struct sockaddr_in _local_addr4;
    struct sockaddr_in6 _local_addr6;
    struct sockaddr_in _remote_addr4;
    struct sockaddr_in6 _remote_addr6;
public:
    UDP();
    virtual ~UDP();

    void setTimeout(const int tcp_timeout);
	int resolveEndpoints(const char* remote_addr, const char* local_addr, int port, bool modelisten = false, const char* ifname = NULL);
	int configureSocket(const char* remote_addr, const char* local_addr, int port, bool modelisten = false, const char* ifname = NULL);
    virtual int  openSocket(const char* remote_addr,const char* local_addr, int port, bool modelisten = false,const char* ifname = NULL);
    int  openRawSocket();
    virtual int  closeSocket();
    virtual int  readSocket(char *buffer, int *len);
    //int  readSocket2(char *buffer, int *len);
    virtual int  writeSocket(char *buffer, int *len);
    virtual int  writeBatchedSocket(char **buffer, int *len, int count);
    bool isValid() { return _sock!=INVALID_SOCKET; };
    SOCKET getSock() { return _sock; };

    virtual void pktTSctl(int, unsigned int = 0, long long = 0) {};
};  // UDP

#ifdef USE_NETMAP

#define NETMAP_WITH_LIBS
#define L2_L3_L4_HDR_SIZE (14+20+8)
#include "netmap_user.h"

class Netmap : public UDP
{
private:
    struct nm_desc *_nmd = NULL;
    char _headers[L2_L3_L4_HDR_SIZE];
    uint16_t *_ip_tot_len = NULL;
    uint16_t *_ip_csum = NULL;
    uint16_t *_udp_len = NULL;
    uint32_t _my_ip = 0;
public:
    Netmap() {};
    ~Netmap() { if (isValid()) { closeSocket(); } };

    virtual int  openSocket(char* addr, int port, char* bindToDevice);
    virtual int  closeSocket();
    virtual int  readSocket(char *buffer, int *len);
    virtual int  writeSocket(char *buffer, int *len);
    virtual int  writeBatchedSocket(char **buffer, int count, int *len);

private:
    int readSocketNonBlocking(char *buffer, int *len);
    int writeSocketNonBlocking(char *buffer, int *len);

};
#endif // USE_NETMAP

#endif // _TCPBASIC_H

