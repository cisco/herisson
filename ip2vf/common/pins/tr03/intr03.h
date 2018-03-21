#ifndef INTR03_H
#define INTR03_H
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#else
#include <netinet/ip_icmp.h>   //Provides declarations for icmp header
#include <netinet/udp.h>   //Provides declarations for udp header
#include <netinet/tcp.h>   //Provides declarations for tcp header
#include <netinet/ip.h>    //Provides declarations for ip header
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <unistd.h>     // usleep
#endif

#include <pins/pins.h>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "tcp_basic.h"
#include "rtpframe.h"
#include "tr03frameparser.h"
using namespace std;

/*
 * @brief Reads tr03 from an rtp socket
 */
class CInTR03 : public CIn, public CFrameHeaders {
public:
    UDP *_udpSock;
    bool _isListen;
    int _lastSeq;
    int _frameNb;
    unsigned char _RTPframe[RTP_MAX_FRAME_LENGTH];

    int _port;
    int _w;
    int _h;
    int _fmt;
    const char* _ip;
    const char* _zmqip;
    const char* _interface;
public:
    shared_ptr<CTR03FrameParser> _tr03FrameParser;

    CInTR03(CModuleConfiguration* pMainCfg, int nIndex);

    /*
     * @brief destroy and close sockets
     */
    virtual ~CInTR03();

    void reset();

    int  read(CvMIFrame* frame);

};




#endif
