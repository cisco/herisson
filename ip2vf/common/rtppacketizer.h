#ifndef _RTPPACKETIZER_H
#define _RTPPACKETIZER_H

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>

#include "common.h"
#include "tcp_basic.h"
#include "frameheaders.h"
#include "rtpframe.h"


/**********************************************************************************************
*
* CRTPPacketizer
*
***********************************************************************************************/
class CRTPPacketizer
{
protected:
    int     _mtu;
    char    _RTPframe[RTP_MAX_FRAME_LENGTH];
    int     _UDPPacketSize;
    int     _RTPPacketSize;
    int     _RTPPayloadSize;
    int     _seq;

public:
    CRTPPacketizer(int mtu = 1500);
    ~CRTPPacketizer() {};

public:
    int  send(UDP* sock, char* mediabuffer, int mediabuffersize, int payloadtype);
};

#endif // _RTPPACKETIZER_H
