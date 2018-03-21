#include <cstdio> 
#include <cstdlib>
#include <cstring>          // strcmp
#include <cerrno>
#include <iostream>     // cout
#include <assert.h>
#ifdef _WIN32
#include <winsock2.h>    
#include <Ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#define READSOCKET(s, b, l)     recv(s, b, l, 0)
#define CLOSESOCKET             closesocket
#define MSG_NOSIGNAL            0
#else
#include <unistd.h>         // close
#include <sys/socket.h>     // socket, shutdown, listen, bind, 
#include <netinet/in.h>     // struct sockaddr_in
#include <arpa/inet.h>      // inet_addr
#include <netdb.h>          // gethostbyname
#include <sys/select.h>
#define CLOSESOCKET     close
#define READSOCKET      read
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <sys/types.h>
#include <assert.h>
#include <ifaddrs.h>
#endif

// cat /proc/sys/net/core/rmem_max
// sudo sysctl -w net.core.rmem_max=5000000

#include "log.h"
#include "tcp_basic.h"

#define SOCKET_IPV6
#define SOCKET_IPV6_BUFLEN  100
#define SOCKET_RCVBUF_SIZE  20000000

/* For backwards compatibility with previous versions of the TCP module */
#define c_INADDR_ANY C_INADDR_ANY
#define c_INADDR_ANY_REUSE C_INADDR_ANY_REUSE

const int LISTEN_QUEUE = 128;

int v_TCP_timeout = 1800;		/* timeout en seconde */  

void dumpSocketInfos(struct addrinfo *sockinfo) {

    LOG("   ai_flags -> %i\n", sockinfo->ai_flags) ;
    LOG("   ai_family -> %s\n", sockinfo->ai_family==AF_INET6?"AF_INET6":"AF_INET") ;
    LOG("   ai_socktype -> %s\n", sockinfo->ai_socktype==SOCK_STREAM?"SOCK_STREAM":"SOCK_DGRAM") ;
    LOG("   ai_protocol -> %i\n", sockinfo->ai_protocol) ;
    //LOG("dumpSocketInfos():    ai_addrlen -> %i\n", sockinfo->ai_addrlen) ;
    if( sockinfo->ai_family==AF_INET6 ) {
        char buffer[INET6_ADDRSTRLEN];
        struct sockaddr_in* saddr = (struct sockaddr_in*)sockinfo->ai_addr;
        const char* result = inet_ntop( AF_INET6, &(saddr->sin_addr), buffer, sizeof(buffer));
        if( result==NULL )
            LOG_ERROR("failed to convert address to string '%s'", strerror(errno));
        else
            LOG("   ai_addr hostname ->  %s\n", buffer);
    }
    else {
        struct sockaddr_in* saddr = (struct sockaddr_in*)sockinfo->ai_addr;
        LOG("   ai_addr hostname ->  %s\n", inet_ntoa(saddr->sin_addr));
    }
}

#ifdef _WIN32
//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetWindowsErrorAsString(const int errorMessageID)
{
	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	std::string message(messageBuffer, size);

	//Free the buffer.
	LocalFree(messageBuffer);

	return message;
}

#define WARN_LAST_W32_ERROR(message) {int lastError=WSAGetLastError();LOG_WARNING((std::string(message) + GetWindowsErrorAsString(lastError)).c_str());}
#else
	//do nothing on linux:
#define	WARN_LAST_W32_ERROR(message) 
#endif // _WIN32




/*
 *
 *
 *  Common
 *
 *
 */


int sock_bind_to_device(SOCKET sockfd, const char* bindToDevice) {
    socklen_t optlen;

#ifdef _WIN32
    optlen = 0;
#else
    if( bindToDevice!=NULL && strlen(bindToDevice)>0 ) {
        optlen = strlen(bindToDevice);
        if( setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (void*)bindToDevice, optlen) != 0 ) {
            LOG_ERROR("setsockopt(SO_BINDTODEVICE) failed for sockfd=%d for interface='%s', error='%s'\n", sockfd, bindToDevice, strerror(errno));
            return E_ERROR;
        }
        else
            LOG("ok to bind to device '%s'\n", bindToDevice);
    }
#endif

    return E_OK;
}

SOCKET
connect_client (const char *hostname,
                const char *service,
                int         family,
                int         socktype,
                bool        silent=false,
                const char*       bindToDevice=NULL)
{
    struct addrinfo hints, *res, *ressave;
    int n;
    SOCKET sockfd;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = family;
    hints.ai_socktype = socktype;

    n = getaddrinfo(hostname, service, &hints, &res);

    if( n < 0 ) { 
        LOG_ERROR("getaddrinfo error:: [%s]\n", gai_strerror(n));
        return INVALID_SOCKET;
    }

    ressave = res;

    int c = 0;
    sockfd = INVALID_SOCKET;
    while( res ) {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        
        if( !(sockfd < 0) ) {
            if( bindToDevice!=NULL )
                sock_bind_to_device( sockfd, bindToDevice );

            //
            if( !silent )
                LOG("getaddrinfo try %i, find:\n", ++c) ;
            dumpSocketInfos(res);

            if( !silent )
                LOG("   socket creation OK, now try to connect...\n");
            if( connect(sockfd, res->ai_addr, (int)res->ai_addrlen) == 0 )
                break;

            if( !silent )
                LOG_ERROR("can't connect: '%s'", strerror(errno));

            CLOSESOCKET( sockfd );
            sockfd = INVALID_SOCKET;
        }
        res = res->ai_next;
    }

    freeaddrinfo(ressave);
    return sockfd;
}

SOCKET
listen_server(const char *hostname,
              const char *service,
              int         family,
              int         socktype,
              const char*       bindToDevice=NULL,
              bool        bNoBlocking=false)
{
    struct addrinfo hints, *res, *ressave;
    int n;
    SOCKET sockfd;

    memset(&hints, 0, sizeof(struct addrinfo));

    /*
       AI_PASSIVE flag: the resulting address is used to bind
       to a socket for accepting incoming connections.
       So, when the hostname==NULL, getaddrinfo function will
       return one entry per allowed protocol family containing
       the unspecified address for that family.
    */

    hints.ai_flags    = AI_PASSIVE;
    hints.ai_family   = family;
    hints.ai_socktype = socktype;

    n = getaddrinfo(hostname, service, &hints, &res);

    if( n <0 ) {
        LOG_ERROR("getaddrinfo error:: [%s]\n", gai_strerror(n));
        return INVALID_SOCKET;
    }

#ifndef _WIN32
    if (bNoBlocking)
        res->ai_socktype = res->ai_socktype | SOCK_NONBLOCK;
#endif

    ressave = res;

    /*
       Try open socket with each address getaddrinfo returned,
       until getting a valid listening socket.
    */
    int c = 0;
    sockfd = INVALID_SOCKET;
    while( res ) {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        if( !(sockfd < 0) ) {
            if( bindToDevice!=NULL )
                sock_bind_to_device( sockfd, bindToDevice );

#ifdef _WIN32
            if (bNoBlocking) {
                u_long lmode = 1;  // mode=0 blocking, mode!=0 no blocking
                int iResult = ioctlsocket(sockfd, FIONBIO, &lmode);
                if (iResult != NO_ERROR)
                    LOG_ERROR("ioctlsocket failed with error: %ld\n", iResult);
            }
#endif

            LOG("getaddrinfo try %i, find:\n", ++c) ;
            dumpSocketInfos(res);

            /* Set reuse address and port option */
            int sock_opt;
            sock_opt = 1;
#ifdef _WIN32
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&sock_opt, sizeof(int));
#else
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(int));
#endif

            LOG("socket creation OK, now try to bind it...\n");
            if( bind(sockfd, res->ai_addr, (int)res->ai_addrlen) == 0 )
                break;

            LOG_ERROR("can't bind: ");perror("");
            CLOSESOCKET(sockfd);
            sockfd = INVALID_SOCKET;
        }
        res = res->ai_next;
    }

    if( sockfd == INVALID_SOCKET ) {
        freeaddrinfo(ressave);
        LOG_ERROR("socket error:: could not open socket\n");
        return INVALID_SOCKET;
    }

    listen(sockfd, LISTEN_QUEUE);

    freeaddrinfo(ressave);

    return sockfd;
}

static int get_ipv6_index_from_address(const struct in6_addr *interface_addr6)
{
    int interface_index = ~0;
#ifndef WIN32
    struct ifaddrs * addrs = NULL;
    getifaddrs(&addrs);
    struct ifaddrs * current = addrs;
    while (1)
    {
        if (((struct sockaddr_in6 *) current->ifa_addr)->sin6_family == AF_INET6
                && (memcmp(
                        &((struct sockaddr_in6 *) current->ifa_addr)->sin6_addr,
                        interface_addr6, sizeof(*interface_addr6))))
        {
            interface_index = if_nametoindex(current->ifa_name);
            break;
        }
        current = current->ifa_next;
        if (!current)
            break;
    }
    freeifaddrs(addrs);
#else
    PIP_ADAPTER_ADDRESSES addrs = NULL;
    int currentBufSize = 8192;
    int res = -1;
    do
    {
        addrs = (IP_ADAPTER_ADDRESSES *)malloc(currentBufSize);
        unsigned long outbuflen = currentBufSize;
        res = GetAdaptersAddresses(AF_INET6, 0, NULL, addrs, &outbuflen);
        if (res == ERROR_BUFFER_OVERFLOW)
        {
            currentBufSize *= 2;
            free(addrs);
        }
    }while (res == ERROR_BUFFER_OVERFLOW && currentBufSize < 65536);
    if (res == ERROR_BUFFER_OVERFLOW)
    {
        LOG_ERROR("Failed to do multicast subscribtion: GetAdaptersAddresses Buffer Overflow");
        return E_FATAL;
    }
    PIP_ADAPTER_ADDRESSES currentAddr = addrs;
    while (currentAddr)
    {
        PIP_ADAPTER_UNICAST_ADDRESS currentUnicastAddr = currentAddr->FirstUnicastAddress;
        while (currentUnicastAddr)
        {
            if (memcmp(&((PSOCKADDR_IN6)currentUnicastAddr->Address.lpSockaddr)->sin6_addr, &interface_addr6, sizeof(interface_addr6)))
            {
                interface_index = currentAddr->Ipv6IfIndex;
                break;
            }
            currentUnicastAddr = currentUnicastAddr->Next;
        }
        currentAddr = currentAddr->Next;
    }
    free(addrs);
#endif
    return interface_index;
}

/*
 *
 *
 * TCP
 *
 *
 */

TCP::TCP() 
{
    _sock        = INVALID_SOCKET;
    _sockClient  = INVALID_SOCKET;
    _TCP_timeout = v_TCP_timeout;
    _isListening = false;
    _conn_attemp = 0;
#ifdef _WIN32 
    WSADATA init_win32; 
    int result = WSAStartup(MAKEWORD(2,2), &init_win32);
    if( result != 0 ) {
       LOG("Failed to init Winsock: %d %d", result, WSAGetLastError());
    }
#endif
}

TCP::~TCP() 
{
    closeSocket();

#ifdef _WIN32 
    int result = WSACleanup();
    if( result != 0 ) {
       LOG_ERROR("Failed to cleanup Winsock: %d %d", result, WSAGetLastError());
    }
#endif
}

/*
*
*  openSocket
*
*/
int TCP::openSocket(const char* addr, int port, const char* bindToDevice)
{
    int result;
    LOG("--> <--");

    if( isValid() )
        closeSocket() ;

    // convert port number to string for getaddrinfo
    char service[32];
    sprintf(service, "%d", port);

    if( addr == NULL || strcmp(addr, C_INADDR_ANY_REUSE) == 0 || strcmp(addr, C_INADDR_ANY) == 0 )
    {
        /* 
         Open a listening socket 
         */   
        LOG("try opening listening socket on [%s]:%s", (addr==NULL?"NULL":addr), service);
        if (_sock == INVALID_SOCKET) {
#ifdef _WIN32 
            _sock = listen_server(NULL, service, AF_INET /*AF_UNSPEC*/, SOCK_STREAM, bindToDevice, true); // will create a socket listening on in6addr_any to accept either ipv4 or ipv6 connection
#else
            _sock = listen_server(NULL, service, AF_INET6 /*AF_UNSPEC*/, SOCK_STREAM, bindToDevice, true); // will create a socket listening on in6addr_any to accept either ipv4 or ipv6 connection
#endif
            if (_sock == INVALID_SOCKET) {
                LOG_ERROR("***ERROR*** failed to create listening socket");
                return E_FATAL;
            }
            result = listen(_sock, 5);
            if (result < 0) {
                LOG_ERROR("***ERROR*** failed to listen on port %s", service);
                _sock = INVALID_SOCKET;
                return E_FATAL;
            }
            LOG_INFO("Ok to listen on [%s]:%s", (addr == NULL ? "NULL" : addr), service);
            _isListening = true;
        }
        result = waitForClientConnection();
        if( result < 0 ) {
            return E_FATAL;
        }
        LOG("_sockClient=%d", _sockClient);
    }
    else
    {
        /*
         Open a connected socket
         */
        LOG("try opening connected socket on [%s]:%s", (addr==NULL?"NULL":addr), service);
        _sock = connect_client(addr, service, AF_UNSPEC, SOCK_STREAM, true, bindToDevice);
        if( _sock == INVALID_SOCKET ) {
            //LOG_ERROR("tcp::openSocket(): ***ERROR*** failed to create connected socket");
            return E_FATAL;
        }
        LOG_INFO("Ok to open connected socket on [%s]:%s", (addr==NULL?"NULL":addr), service);
        _sockClient = _sock;
    }

    return E_OK;
}

/*
*
*  get_in_port
*
*/
int get_in_port(struct sockaddr *sa)
{
    if( sa->sa_family == AF_INET ) {
        return ntohs((((struct sockaddr_in*)sa)->sin_port));
    }
    else if( sa->sa_family == AF_INET6 ) {
        return ntohs((((struct sockaddr_in6*)sa)->sin6_port));
    }

    return 0;
}


/*
*
*  openSocket
*
*/
int  TCP::waitForClientConnection() {
    int result = 0;
#ifdef SOCKET_IPV6
    struct sockaddr_in6 cli_addr;
#else
    struct sockaddr_in cli_addr;
#endif

    socklen_t clilen = sizeof(cli_addr);
    _sockClient = accept(_sock, (struct sockaddr *) &cli_addr, &clilen);
       
#ifdef _WIN32
	if (_sockClient == INVALID_SOCKET) {
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK || err == EAGAIN
#else
	if (_sockClient < 0) {
		int err = errno;
		if (err == EAGAIN
#endif
				) {
            // No blocking socket and no input connection... not an error
            //LOG_ERROR("ERROR on accept on port %d, err=%d", ntohs(get_in_port((struct sockaddr *)&cli_addr)), err);
            if (_conn_attemp == 0)
                LOG_INFO("No client detected... wait for a connection");
            _conn_attemp++;
        }
        else {
			LOG_ERROR("ERROR on accept on port %d, err=%d", ntohs(get_in_port((struct sockaddr *)&cli_addr)), err);
            closeSocket();
        }
        result = -1;
    }
    else
    {
        if (_conn_attemp >= 0)
            LOG_INFO("New client connected...");
#ifdef SOCKET_IPV6
        char client_addr_ipv6[SOCKET_IPV6_BUFLEN];
        inet_ntop(AF_INET6, &(cli_addr.sin6_addr), client_addr_ipv6, SOCKET_IPV6_BUFLEN);
        LOG_INFO("Incoming connection from client having IPv6 address: %s\n", client_addr_ipv6);
#endif
    }

    return result;
}

/*
*
*  closeSocket
*
*/
int TCP::closeSocket() 
{
	if (_isListening && _sockClient >= 0) {
		int closeResult=CLOSESOCKET(_sockClient);
		LOG((std::string("closed client socket with result ")+std::to_string(closeResult)).c_str());
	}

    _sockClient = INVALID_SOCKET;
	//default return value:
	int ret = E_OK;
    //clean up
	if( _sock!=INVALID_SOCKET )
    {
		int shutdownResult=shutdown(_sock, 2);
		if(shutdownResult < 0 ) {
			WARN_LAST_W32_ERROR("Failed to shutdown client connection: ");
			LOG("Failed to shutdown client connection.\n" );
			ret=E_FATAL;
        }

		int closeResult = CLOSESOCKET(_sock);
		if( closeResult < 0 ) {
			WARN_LAST_W32_ERROR("Failed to close socket: ");
			LOG("Failed to close socket.\n" );
			_sock = INVALID_SOCKET;
            ret=E_FATAL;
        }
        _sock = INVALID_SOCKET;
    }

    _isListening = false;
    _conn_attemp = 0;
    return ret;
}

/**
* reads in a blocking fashion from a non-blocking socket
*/
int  TCP::blockingReadSocket(const SOCKET &socketHandle,char *buffer,const int &len) {
#ifdef _WIN32
    /* Receive data */
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(_sockClient, &readSet);
    timeval timeout;
    //set a timeout for the read operation
    timeout.tv_sec = 5 * 60;
    timeout.tv_usec = 0;
    //wait until socket is ready to read:
    auto readyToRead = select((int)socketHandle, &readSet, NULL, NULL, &timeout);
    if (readyToRead == 1)
    {
        const  int bytes_read = READSOCKET(socketHandle, buffer, len);
        if (bytes_read <= 0) {
            LOG_WARNING((std::string("blocking read returnd with: ") + std::to_string(bytes_read)).c_str());
        }
        return bytes_read;
    }
    else {
        LOG_WARNING("timeout on select");
        //no bytes read
        return 0;
    }
#else
    //linux:
    return READSOCKET(socketHandle, buffer, len);
#endif
}

/*
*
*  readSocket
*
*/
int  TCP::readSocket(char *buffer, int *len) 
{
    int recv_len, recv_cnt;
    int retval;

    retval = E_AGAIN;      
    recv_len = 0;
    recv_cnt = 0;            
    while( retval == E_AGAIN )
    {
        /* Receive data */
        
        recv_len = blockingReadSocket( _sockClient, &(buffer[recv_cnt]), (*len)-recv_cnt );

        if (recv_len > 0)
        {
            recv_cnt += recv_len;
            if (recv_cnt == *len) retval = E_OK;
        }
        else
        {
            if (recv_len == 0) retval = E_RESET;
            if (recv_len == -1)
            {
                switch( errno )
                {
                case EAGAIN:
//#ifndef SOLARIS /* EWOULDBLOCK = EAGAIN for SunOS */
//                        case EWOULDBLOCK:
//#endif
                case ETIMEDOUT:
                case EPIPE:
                case EINTR: retval = E_AGAIN; break;

                case ECONNRESET:
                case ENOTCONN: retval = E_RESET; break;

                default: retval = E_FATAL;
                } /* end of switch */
            } /* end of recv_len == -1 */
        } /* end of recv_len <= 0 */
    } /* end of while */

    *len = recv_cnt;

    return retval;
}

/*
*
*  writeSocket
*
*/
int  TCP::writeSocket(char *buffer, int *len) 
{
   int send_len, cnt_send;
   int retval = E_OK;
   
   send_len = 0;
   cnt_send = 0;
   while (cnt_send < *len)
   {
      try {
        send_len = send( _sockClient, &(buffer[cnt_send]), (*len)-cnt_send, MSG_NOSIGNAL );
#ifdef _WIN32 
		//windows specific error handling:
		if (send_len == SOCKET_ERROR) {
			int windowsScoketsErrorCode = WSAGetLastError();
			std::string msg = std::string("Error sending to socket ") + GetWindowsErrorAsString(windowsScoketsErrorCode);
			LOG_WARNING(msg.c_str());
			throw std::runtime_error(msg) ;
		}
#endif
		//LOG("len=%d", send_len);
      } catch(...) {
          LOG_ERROR("catch exception");
          closeSocket();
          *len = 0;
          return E_RESET;
      }
      if (send_len > 0)  {
         cnt_send += send_len;
         if( send_len < *len ) {
             int err = errno;
             if( err==ECONNREFUSED || err==ENOTCONN ) {
                  closeSocket();
                  *len = 0;
                  return E_RESET;
             }
         }
      }
      else
      {
         if (send_len == 0) retval = E_FATAL;
         if (send_len == -1)
         {
            switch (errno)
            {
               case EAGAIN:
#ifndef SOLARIS /* EWOULDBLOCK = EAGAIN for SunOS */
               //case EWOULDBLOCK:
#endif
               case EIO:
               case EPIPE:
               case EINTR: retval = E_AGAIN; break;

               case ECONNRESET:
               case ENOTCONN: retval = E_RESET; break;

               default: retval = E_FATAL;
            } /* end of switch */
         } /* end of send_len == -1 */
         //LOG_ERROR("send_len==0, retval=%d", retval);
         // Just to manage SIGPIPE, Broken pipe
         closeSocket();
         *len = 0;
         return E_RESET;
         break; /* break while loop */
      }
   } /* end of while */

   *len = cnt_send;

   return retval;
}

/*
 *
 *
 * UDP
 *
 *
 */

UDP::UDP() 
{
    _sock = INVALID_SOCKET;
    _TCP_timeout = v_TCP_timeout;
#ifdef _WIN32 
    WSADATA init_win32; 
    int result = WSAStartup(MAKEWORD(2,2), &init_win32);
    if( result != 0 ) {
       LOG("Failed to init Winsock: %d %d", result, WSAGetLastError());
    }
#endif
}

UDP::~UDP() 
{
    if( _sock != INVALID_SOCKET) 
        closeSocket();
#ifdef _WIN32 
    int result = WSACleanup();
    if( result != 0 ) {
       LOG_ERROR("Failed to cleanup Winsock: %d %d", result, WSAGetLastError());
    }
#endif
}

// cat /proc/sys/net/core/rmem_max
// sudo sysctl -w net.core.rmem_max=5000000

void UDP::setTimeout(const int tcp_timeout) {
    _TCP_timeout = tcp_timeout;
}

int UDP::resolveEndpoints(const char* remote_addr, const char* local_addr, int port, bool modelisten, const char* ifname)
{
	int res = E_OK;
	_port = port;
	struct in_addr inaddr_any = { 0 };

	/* Build local and remote addresses */
	struct addrinfo *local_addr_info = NULL;
	struct addrinfo *remote_addr_info = NULL;
	struct addrinfo hints =
	{ 0 };
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_ADDRCONFIG;

	if (local_addr && *local_addr)
	{
		int res_addrinfo = getaddrinfo(local_addr, NULL, &hints,
			&local_addr_info);
		if (res_addrinfo)
		{
			LOG_ERROR("getaddrinfo(local_addr,...) failed: %s",
				gai_strerror(res_addrinfo));
			return E_FATAL;
		}
		hints.ai_family = local_addr_info->ai_family;

	}
	if (remote_addr && *remote_addr)
	{

		int res_addrinfo = getaddrinfo(remote_addr, NULL, &hints,
			&remote_addr_info);
		if (res_addrinfo)
		{
			LOG_ERROR("getaddrinfo(remote_addr,...) failed: %s",
				gai_strerror(res_addrinfo));
			return E_FATAL;
		}
	}
	/* Check consistency of addr_info */
	assert(
		!local_addr_info || !remote_addr_info
		|| local_addr_info->ai_family
		== remote_addr_info->ai_family);
	_af = remote_addr_info ?
		remote_addr_info->ai_family :
		(local_addr_info ? local_addr_info->ai_family : AF_INET6);
	if (_af == AF_INET)
	{
		_local_addr4 =
		{};
		_local_addr4.sin_family = AF_INET;
		_local_addr4.sin_port = modelisten ? htons(port) : 0;
		_local_addr4.sin_addr = local_addr_info ? ((struct sockaddr_in *)(local_addr_info->ai_addr))->sin_addr : inaddr_any;
		_remote_addr4 =
		{};
		_remote_addr4.sin_family = AF_INET;
		_remote_addr4.sin_port = modelisten ? 0 : htons(port);
		_remote_addr4.sin_addr = remote_addr_info ? ((struct sockaddr_in *)(remote_addr_info->ai_addr))->sin_addr : inaddr_any;
	}
	else
	{
		_local_addr6 =
		{};
		_local_addr6.sin6_family = AF_INET6;
		_local_addr6.sin6_port = modelisten ? htons(port) : 0;
		_local_addr6.sin6_addr = local_addr_info ? ((struct sockaddr_in6 *)(local_addr_info->ai_addr))->sin6_addr : in6addr_any;
		_remote_addr6 =
		{};
		_remote_addr6.sin6_family = AF_INET6;
		_remote_addr6.sin6_port = modelisten ? 0 : htons(port);
		_remote_addr6.sin6_addr = remote_addr_info ? ((struct sockaddr_in6 *)(remote_addr_info->ai_addr))->sin6_addr : in6addr_any;
	}
	if (local_addr_info)
		freeaddrinfo(local_addr_info);
	if (remote_addr_info)
		freeaddrinfo(remote_addr_info);

	return res;
}

int UDP::configureSocket(const char* remote_addr, const char* local_addr, int port, bool modelisten, const char* ifname)
{
	/*
	* Manage socket options
	*/
	int res = E_OK;
	// REUSE option for listening socket
	int optval;
	socklen_t optlen = sizeof(int);

	/* Set reuse address and port option */
	optval = 1;
	setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, optlen);

	// Socket receive buffer length option
#ifdef _WIN32
	if (getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (char*)&optval, &optlen) != 0)
#else
	if (getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (void*)&optval, &optlen) != 0)
#endif
		LOG_ERROR("getsockopt(SO_RCVBUF) failed, error='%s' bytes\n",
			strerror(errno));
	LOG_INFO("initial UDP socket receive buffer size (SO_RCVBUF)='%d'\n",
		optval);
	optval = SOCKET_RCVBUF_SIZE;
#ifdef _WIN32
	if (setsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (const char*)&optval, optlen) != 0)
#else
	if (setsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (void*)&optval, optlen) != 0)
#endif        
		LOG_ERROR("setsockopt(SO_RCVBUF) failed, error='%s'\n",
			strerror(errno));
#ifdef _WIN32
	if (getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (char*)&optval, &optlen) != 0)
#else
	if (getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (void*)&optval, &optlen) != 0)
#endif        
		LOG_ERROR("getsockopt(SO_RCVBUF) failed, error='%s'\n",
			strerror(errno));
	LOG_INFO("Set UDP socket receive buffer size (SO_RCVBUF) to '%d' bytes\n",
		optval);
	if (optval < SOCKET_RCVBUF_SIZE)
	{
		LOG_WARNING("warning, socket buffer too low ( < %d bytes)\n",
			SOCKET_RCVBUF_SIZE);
		LOG_WARNING(
			"please verify linux core parameter /proc/sys/net/core/rmem_max and update it if too low.\n");
	}

#ifdef _WIN32
	optval = 1;
	if (_af == AF_INET6 && setsockopt(_sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&optval, optlen) != 0)
		LOG_ERROR("You are using a pure v6 socket, you may not get some traffic...\n");
#endif
	/*
	* Open a new connection
	*/
	bool multicast =
		(_af == AF_INET
			&& (uint8_t)_remote_addr4.sin_addr.s_addr >= 224)
		|| (_af == AF_INET6
			&& (_remote_addr6.sin6_addr.s6_addr[0]
				== 255));
	if (modelisten)
	{

		struct in_addr interface_addr = _local_addr4.sin_addr;
		struct in6_addr interface_addr6 = _local_addr6.sin6_addr;
		if (multicast)
		{
			_local_addr4.sin_addr.s_addr = _remote_addr4.sin_addr.s_addr;
			_local_addr6.sin6_addr = _remote_addr6.sin6_addr;
		}
		res = bind(_sock,
			_af == AF_INET ?
			(struct sockaddr *) &_local_addr4 :
			(struct sockaddr *) &_local_addr6,
			_af == AF_INET ? sizeof(_local_addr4) : sizeof(_local_addr6));
		if (res == -1)
		{
#ifdef _WIN32 
			LOG_ERROR("Failed to bind socket, error=%d\n", WSAGetLastError());
#else
			LOG_ERROR("Failed to bind socket: '%s'\n", strerror(errno));
#endif
			return E_FATAL;
		}
		if (multicast && _af == AF_INET)
		{
			struct ip_mreq group =
			{ 0 };
			group.imr_interface = interface_addr;
			group.imr_multiaddr = _remote_addr4.sin_addr;
			if (setsockopt(_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
				(char *)&group, sizeof(group)) < 0)
			{
#ifndef _WIN32
				LOG_ERROR("Failed to join multicast group : %s \n", strerror(errno));
#else
				LOG_ERROR("Failed to join multicast group: %d \n", WSAGetLastError());
#endif
				return E_FATAL;
			}
		}
		if (multicast && _af == AF_INET6)
		{
			struct ipv6_mreq group =
			{ 0 };
			int interface_index = get_ipv6_index_from_address(&interface_addr6);
			group.ipv6mr_interface = interface_index;
			group.ipv6mr_multiaddr = _remote_addr6.sin6_addr;
			if (setsockopt(_sock, IPPROTO_IPV6, IP_ADD_MEMBERSHIP,
				(char *)&group, sizeof(group)) < 0)
			{
#ifndef WIN32
				LOG_ERROR("Failed to join multicast group: %s \n", strerror(errno));
#else
				LOG_ERROR("Failed to join multicast group: %d \n", WSAGetLastError());
#endif
				return E_FATAL;
			}
		}
	}
	else if (local_addr && *local_addr && !multicast)
	{
		res = bind(_sock,
			_af == AF_INET ?
			(struct sockaddr *) &_local_addr4 :
			(struct sockaddr *) &_local_addr6,
			_af == AF_INET ? sizeof(_local_addr4) : sizeof(_local_addr6));
		if (res == -1)
		{
#ifdef _WIN32
			LOG_ERROR("Failed to bind socket, error=%d\n", WSAGetLastError());
#else
			LOG_ERROR("Failed to bind socket: '%s'\n", strerror(errno));
#endif
			return E_FATAL;
		}
	}
	else if (local_addr && *local_addr && multicast && _af == AF_INET)
	{
		if (setsockopt(_sock, IPPROTO_IP, IP_MULTICAST_IF, (char *)&_local_addr4.sin_addr, sizeof(_local_addr4.sin_addr)) < 0)
		{
#ifndef WIN32
			LOG_ERROR("Failed to set multicast output interface: %s \n", strerror(errno));
#else
			LOG_ERROR("Failed to set multicast output interface: %d \n", WSAGetLastError());
#endif
			res = -1;
		}
	}
	else if (local_addr && multicast && _af == AF_INET6)
	{
		int interface_index = get_ipv6_index_from_address(&_local_addr6.sin6_addr);
		if (setsockopt(_sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char*)&interface_index, sizeof(interface_index)) < 0)
		{
#ifndef WIN32
			LOG_ERROR("Failed to set multicast output interface: %s \n", strerror(errno));
#else
			LOG_ERROR("Failed to set multicast output interface: %d \n", WSAGetLastError());
#endif
			res = -1;
		}
	}
	if (res == -1)
	{
		closeSocket();
		_sock = INVALID_SOCKET;
		return E_FATAL;
	}

	return E_OK;

}

int UDP::openSocket(const char* remote_addr,const char* local_addr, int port,
        bool modelisten,const char *ifname)
{
	int res = E_OK;
	res = resolveEndpoints(remote_addr, local_addr, port, modelisten, ifname);
    /* 
     * Create a new socket
     */
    _sock = socket(_af, SOCK_DGRAM, 0);

    if (_sock == INVALID_SOCKET)
    {
#ifdef _WIN32 
        LOG_ERROR("Failed to create socket, error=%d\n", WSAGetLastError());
#else
        LOG_ERROR("Failed to create socket: '%s'\n", strerror(errno));
#endif
        return E_FATAL;
    }

	/*
	 * Configure the socket 
	 */
	res = configureSocket(remote_addr, local_addr, port, modelisten, ifname);
	return res;
}

int  UDP::openRawSocket() {
#ifndef WIN32 
    _sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL));
#endif
    if (_sock == INVALID_SOCKET)
    {
#ifdef WIN32 
        LOG_ERROR("Failed to create socket, error=%d\n", WSAGetLastError());
#else
        LOG_ERROR("Failed to create socket: '%s'\n", strerror(errno));
#endif
        return E_FATAL;
    }
    // Socket receive buffer length option
    int optval;
    socklen_t optlen = sizeof(int);
#ifdef _WIN32
    if (getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (char*)&optval, &optlen) != 0)
#else
    if (getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (void*)&optval, &optlen) != 0)
#endif
        LOG_ERROR("getsockopt(SO_RCVBUF) failed, error='%s' bytes\n", strerror(errno));
    LOG_INFO("initial UDP socket receive buffer size (SO_RCVBUF)='%d'\n", optval);
    optval = SOCKET_RCVBUF_SIZE;
#ifdef _WIN32
    if (setsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (const char*)&optval, optlen) != 0)
#else
    if (setsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (void*)&optval, optlen) != 0)
#endif        
        LOG_ERROR("setsockopt(SO_RCVBUF) failed, error='%s'\n", strerror(errno));
#ifdef _WIN32
    if (getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (char*)&optval, &optlen) != 0)
#else
    if (getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (void*)&optval, &optlen) != 0)
#endif        
        LOG_ERROR("getsockopt(SO_RCVBUF) failed, error='%s'\n", strerror(errno));
    LOG_INFO("Set UDP socket receive buffer size (SO_RCVBUF) to '%d' bytes\n", optval);
    if (optval < SOCKET_RCVBUF_SIZE) {
        LOG_WARNING("warning, socket buffer too low ( < %d bytes)\n", SOCKET_RCVBUF_SIZE);
        LOG_WARNING("please verify linux core parameter /proc/sys/net/core/rmem_max and update it if too low.\n");
    }    return E_OK;
}

int  UDP::closeSocket() 
{
    LOG(" -->");
    if( _sock != INVALID_SOCKET )
    {
        // I know it's an UDP socket, but shutdown seems needed, otherwise it not unblock listening socket
        shutdown(_sock, 2);

        /*
        * close connection socket
        */
#ifdef _WIN32 
        if ( closesocket(_sock) < 0 )
#else
        if ( close(_sock) < 0 )
#endif
        {
            LOG_ERROR("Failed to close socket.\n" );
            return E_FATAL;
        }
    }
    _sock = INVALID_SOCKET;

    LOG(" <--");
    return E_OK;
}

int  UDP::readSocket(char *buffer, int *len) 
{
#ifdef _WIN32
    int size
#else
    socklen_t size
#endif
        = _af == AF_INET ? sizeof( struct sockaddr_in ) : sizeof( struct sockaddr_in6 );
    int result = recvfrom(_sock, buffer, *len, 0, _af == AF_INET ? (struct sockaddr*)&_remote_addr4 : (struct sockaddr*)&_remote_addr6, &size);
    if( result == -1 ) {
        *len = 0;
#ifdef _WIN32 
		auto windowsErrorCode = WSAGetLastError();
		LOG_ERROR("error occurred during recvfrom, error = %d %s", windowsErrorCode, GetWindowsErrorAsString(windowsErrorCode).c_str());
#else
        LOG_ERROR("error occurred during recvfrom: '%s'", strerror(errno));
#endif
    }
    else if (result == 0) {
        LOG_INFO("the connection has been gracefully closed");
    }
    else
        *len = result;
    LOG("recv %d bytes from port %d ", result, _port);
    return result;
}

/*
 * not completed
 */
/*
#define BATCH_SIZE  10
struct batch {
    int count;
    char buf[BATCH_SIZE][1500];
    struct iovec iovec[BATCH_SIZE][1];
    struct sockaddr src_addr[BATCH_SIZE];
    struct mmsghdr datagrams[BATCH_SIZE];
};
struct batch _batch;

int  UDP::readSocket2(char *buffer, int *len) 
{
    static int init=0;
    if( init == 0 ) {
        init = 1;
        _batch.count=0;
        for( int i=0; i<BATCH_SIZE; i++) {
            _batch.iovec[i][0].iov_base=&_batch.buf[i][0];
            _batch.iovec[i][0].iov_len=1500;
            _batch.datagrams[i].msg_hdr.msg_name=&_batch.src_addr[i];
            _batch.datagrams[i].msg_hdr.msg_namelen=sizeof(_batch.src_addr[i]);
            _batch.datagrams[i].msg_hdr.msg_iov=_batch.iovec[i];
            _batch.datagrams[i].msg_hdr.msg_iovlen=1;
            _batch.datagrams[i].msg_hdr.msg_control=0;
            _batch.datagrams[i].msg_hdr.msg_controllen=0;
        }
    }

    if( _batch.count>0 ) {
    }
    else {
        LOG("ask for new batch with recvmmsg");
        _batch.count = recvmmsg(_sock, _batch.datagrams, BATCH_SIZE, MSG_WAITFORONE, NULL);
        if( _batch.count == -1 ) {
            LOG_ERROR("error when recvmmsg=%s",strerror(errno));
        } else {
            LOG_ERROR("Ok, receive %d datagrams", _batch.count);
        }
    }
    int result = -1;
    if( result == -1 ) {
        *len = 0;
    }
    return result;
}
*/

int  UDP::writeSocket(char *buffer, int *len) 
{
    int size = _af == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
    int result = sendto(_sock, buffer, *len, 0,
            _af == AF_INET ?
                    (struct sockaddr*) &_remote_addr4 :
                    (struct sockaddr*) &_remote_addr6, size);
    if( result == -1 ) {
#ifdef _WIN32 
		auto windowsErrorCode = WSAGetLastError();
		LOG_ERROR("error=%d %s", windowsErrorCode, GetWindowsErrorAsString(windowsErrorCode).c_str());
#else
        LOG_ERROR("error='%s'", strerror(errno));
#endif
        return -1;
    }
    else if( result != *len ) {
        LOG_ERROR("failed to send full message, actually %d bytes on %d", *len, result);
        return -1;
    }
    LOG("send %d bytes to port %d ", result, _port);
    return result;
}

int  UDP::writeBatchedSocket(char **buffer, int count, int *len) {
    int result = 0;
#ifdef _WIN32
#else
/*    struct mmsghdr  datagrams[count];
    struct iovec    iovec[count];
    for( int i=0; i<count; i++ ) {
        memset(&iovec[i], 0, sizeof(iovec[i]));
        memset(&datagrams[i], 0, sizeof(datagrams[i]));
        iovec[i].iov_base = buffer[i];
        iovec[i].iov_len  = *len;
        datagrams[i].msg_hdr.msg_iov = &iovec[i];
        datagrams[i].msg_hdr.msg_iovlen = 1;
        datagrams[i].msg_hdr.msg_name=&_addr4;
        datagrams[i].msg_hdr.msg_namelen=sizeof( struct sockaddr_in );
    }
    result = sendmmsg(_sock, datagrams, count, 0);
    if( result==-1 ) {
        LOG_ERROR("failed to send full messages batch, actually %d bytes on %d, error=%s", *len, result, strerror(errno));
        exit(-1);
        return -1;
    }
    else {
        LOG("ok to send full messages batch, actually %d bytes on %d", *len, result);
    }
#endif
    return result;*/
#endif
    assert(0);
    return result;
}


#ifdef USE_NETMAP
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <poll.h>

/*
 * Create a netmap socket.
 *
 * @param addr         the destination IP(v4 as of now) address
 * @param port         the destination UDP port
 * @param deviceName   the name of the device
 * @return             0 if successful, -1 in case of an error
 *
 * The netmap socket will act as a regular UDP socket, except that packets
 * will be sent over a netmap interface instead of using the regular kernel API.
 * Such an interface can be a regular NIC or a virtual interface, for fast
 * communication with a packet processing engine (eg VPP)
 * Packets are crafted from scratch, which means that an IP address must be
 * provided for the interface
 *
 * deviceName must have the following format:
 *    netmap-<ip>-<netmap_device_name>
 *         where ip is the IP address to assign to the netmap interface
 *               netmap_device_name is the internal netmap interface name
 * netmap regular devices names are the same as in the kernel ("eth0", etc)
 * netmap local devices names use the following format:
 *    valeXX:YY<flags>
 *         where XX is the switch number and YY the port number inside the switch
 *         flags can be {0 for a master pipe or }0 for a slave pipe
 *
 */
int Netmap::openSocket(char* addr, int port, char* deviceName=NULL)
{
    if (!deviceName) {
        LOG_ERROR("called with no device name");
        return -1;
    }

    if (strncmp(deviceName, "netmap-", strlen("netmap-"))) {
        LOG_ERROR("called with invalid device name");
        LOG_ERROR("device name must have the following format:");
        LOG_ERROR("netmap-<ip>-<netmap_device_name>");
        return -1;
    }

    char *ipStr = deviceName + strlen("netmap-");
    char *netmapDeviceName;
    if (!(netmapDeviceName = strchr(ipStr, '-'))) {
        LOG_ERROR("called with invalid device name");
        LOG_ERROR("device name must have the following format:");
        LOG_ERROR("netmap-<ip>-<netmap_device_name>");
        return -1;
    }
    //Replace dash with 0 for inet_addr to work
    *netmapDeviceName++ = '\0';
    _my_ip = inet_addr(ipStr);


    struct nmreq nmr = {};
    nmr.nr_arg3 = 512;
    _nmd = nm_open(netmapDeviceName, &nmr, 0, NULL);
    if (_nmd == NULL) {
        LOG_ERROR("failed to open netmap device %s: %s", deviceName, strerror(errno));
        return -1;
    }
    _sock = 0;


    //Ethernet header
    struct ether_header *eth = (struct ether_header *) _headers;
    memcpy(&eth->ether_shost, "\xde\xad\xbe\xef\x99\x99", ETHER_ADDR_LEN); //XXX
    memcpy(&eth->ether_dhost, "\xff\xff\xff\xff\xff\xff", ETHER_ADDR_LEN); //XXX
    eth->ether_type = htons(ETHERTYPE_IP);

    //IP header
    struct iphdr *ip = (struct iphdr *) (eth + 1);
    memset(ip, 0, sizeof(*ip));
    ip->ihl = 5;
    ip->version = 4;
    ip->tot_len = 0;
    _ip_tot_len = &ip->tot_len;
    ip->ttl = 64;
    ip->protocol = IPPROTO_UDP;
    ip->saddr = _my_ip;
    ip->daddr = inet_addr(addr);
    _ip_csum = &ip->check;

    //UDP header
    struct udphdr *udp = (struct udphdr *) (ip + 1);
    udp->source = udp->dest = htons(port);
    udp->check = 0;
    udp->len = 0;
    _udp_len = &udp->len;
    return 0;
}

int Netmap::closeSocket()
{
    if (!isValid()) {
        errno = EINVAL;
        return -1;
    }
    nm_close(_nmd);
    _sock = INVALID_SOCKET;
    return 0;
}


int Netmap::readSocketNonBlocking(char *buffer, int *len)
{
    int n_rings = _nmd->last_rx_ring - _nmd->first_rx_ring + 1;
    int i, ring_idx;
    struct netmap_ring *ring = NULL;
    struct netmap_slot *slot;
    char *src_buf;

    //Find an available non-empty ring
    for (i = 0, ring_idx = _nmd->cur_rx_ring; i < n_rings; i++, ring_idx++) {
        if (ring_idx == _nmd->last_rx_ring) {
            ring_idx = _nmd->first_rx_ring;
        }
        ring = NETMAP_RXRING(_nmd->nifp, ring_idx);
        if (nm_ring_empty(ring)) {
            continue;
        }

        break;
    }

    if (i == n_rings) {
        //No non-empty rings available, try again later
        return -1;
    }

    slot = &ring->slot[ring->cur];
    if (slot->len < L2_L3_L4_HDR_SIZE) {
        LOG("got too small packet");
        return -1;
    }

    *len = (slot->len - L2_L3_L4_HDR_SIZE < *len) ? slot->len - L2_L3_L4_HDR_SIZE : *len;
    src_buf = NETMAP_BUF(ring, slot->buf_idx);

    //TODO filter out port and ip here
    memcpy(buffer, src_buf + L2_L3_L4_HDR_SIZE, *len);

    ring->head = ring->cur = nm_ring_next(ring, ring->cur);
    _nmd->cur_rx_ring = ring_idx;

    LOG("recv %d bytes from port %d", *len, _port);

    return *len;

}

int Netmap::readSocket(char *buffer, int *len)
{
    for (int tries = 0; /*tries < 10*/; tries++) {
        int ret = readSocketNonBlocking(buffer, len);
        if (ret != -1) {
            return ret;
        }
        LOG("no ring available, polling (try=%d)", tries);

        ioctl(_nmd->fd, NIOCRXSYNC, NULL);

    }

    LOG_ERROR("no ring available, giving up");
    errno = EAGAIN;
    return -1;
}

//http://www.scs.stanford.edu/histar/src/lind/asm-lind/checksum.h
/*
 * Checksums for x86-64
 * Copyright 2002 by Andi Kleen, SuSE Labs
 * with some code from asm-i386/checksum.h
 */
static inline uint16_t ip_fast_csum(const void *iph, unsigned int ihl)
{
    unsigned int sum;

    asm("  movl (%1), %0\n"
        "  subl $4, %2\n"
        "  jbe 2f\n"
        "  addl 4(%1), %0\n"
        "  adcl 8(%1), %0\n"
        "  adcl 12(%1), %0\n"
        "1: adcl 16(%1), %0\n"
        "  lea 4(%1), %1\n"
        "  decl %2\n"
        "  jne      1b\n"
        "  adcl $0, %0\n"
        "  movl %0, %2\n"
        "  shrl $16, %0\n"
        "  addw %w2, %w0\n"
        "  adcl $0, %0\n"
        "  notl %0\n"
        "2:"
        /* Since the input registers which are loaded with iph and ih
            are modified, we must also specify them as outputs, or gcc
            will assume they contain their original values. */
        : "=r" (sum), "=r" (iph), "=r" (ihl)
        : "1" (iph), "2" (ihl)
            : "memory");
    return (uint16_t)sum;
}

int Netmap::writeSocketNonBlocking(char *buffer, int *len)
{
    int n_rings = _nmd->last_tx_ring - _nmd->first_tx_ring + 1;
    int i, ring_idx;
    struct netmap_ring *ring = NULL;
    struct netmap_slot *slot;
    char *dst_buf;

    //Find an available non-empty ring
    for (i = 0, ring_idx = _nmd->cur_tx_ring; i < n_rings; i++, ring_idx++) {
        if (ring_idx == _nmd->last_tx_ring) {
            ring_idx = _nmd->first_tx_ring;
        }
        ring = NETMAP_TXRING(_nmd->nifp, ring_idx);
        if (nm_ring_empty(ring)) {
            continue;
        }

        break;
    }

    if (i == n_rings) {
        //No non-empty rings available, try again later
        errno = EAGAIN;
        return -1;
    }

    slot = &ring->slot[ring->cur];
    dst_buf = NETMAP_BUF(ring, slot->buf_idx);
    slot->len = sizeof(_headers) + *len;

    memcpy(dst_buf, _headers, sizeof(_headers));
    *_ip_tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + *len);
    *_udp_len = htons(sizeof(struct udphdr) + *len);
    *_ip_csum = 0;
    *_ip_csum = ip_fast_csum(_headers + sizeof(struct ether_header), sizeof(struct iphdr)/4);
    memcpy(dst_buf + sizeof(_headers), buffer, *len);

    _nmd->cur_tx_ring = ring_idx;
    ring->head = ring->cur = nm_ring_next(ring, ring->cur);

    LOG("sent %d bytes to port %d", *len, _port);
    return *len;
}

int Netmap::writeSocket(char *buffer, int *len)
{
    for (int tries = 0; /*tries < 10*/; tries++) {
        int ret = writeSocketNonBlocking(buffer, len);
        if (ret != -1) {
            return ret;
        }
        LOG("no ring available, syncing (try=%d)", tries);
        ioctl(_nmd->fd, NIOCTXSYNC, NULL);
    }
    LOG_ERROR("no ring available, giving up");
    errno = EAGAIN;
    return -1;
}

/*
 * @return the number of packets actually sent
 */
int Netmap::writeBatchedSocket(char **buffer, int count, int *len)
{
    int i;
    for (i = 0; i < count; i++) {
        if (writeSocket(buffer[i], len) < 0) {
            break;
        }
    }
    if (i == 0 && count > 0) {
        /* no packets sent, indicate an error */
        i = -1;
    }
    return i;
}



#endif //USE_NETMAP
