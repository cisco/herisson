#ifndef _UDPPACKETIZER_H
#define _UDPPACKETIZER_H

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>

#include "common.h"
#include "tcp_basic.h"
#include "rtpframe.h"
#include "pins/st2022/smpteprofile.h"



/**********************************************************************************************
*
* CUDPPacketizer
*
***********************************************************************************************/
class CUDPPacketizer
{
protected:
    int     _mtu;
    int     _UDPPacketSize;

public:
    CUDPPacketizer(int mtu = 1500) : _mtu(mtu) {};
    virtual ~CUDPPacketizer() {};

public:
    virtual int send(UDP* sock, char* buffer, int buffersize) = 0;
};

/**********************************************************************************************
*
* CRTPPacketizer
*
***********************************************************************************************/
class CRTPPacketizer : CUDPPacketizer
{
protected:
    char    _RTPframe[RTP_MAX_FRAME_LENGTH];
    int     _RTPPacketSize;
    int     _RTPPayloadSize;
    int     _seq;
    int     _payloadtype;

public:
    CRTPPacketizer(int mtu = 1500);
    ~CRTPPacketizer() {};

public:
    void setPayloadType(int payloadtype) { _payloadtype = payloadtype ; };
    int  send(UDP* sock, char* buffer, int buffersize);
};

/**********************************************************************************************
*
* CRTPPacketizer
*
***********************************************************************************************/
#define MMSGPACKETIZER_NB_SEGMENTS   64
class CRTPmmsgPacketizer : CUDPPacketizer
{
protected:
    char*   _RTPframe[MMSGPACKETIZER_NB_SEGMENTS];
    int     _RTPPacketSize;
    int     _RTPPayloadSize;
    int     _seq;
    int     _payloadtype;

public:
    CRTPmmsgPacketizer(int mtu = 1500);
    ~CRTPmmsgPacketizer();

public:
    void setPayloadType(int payloadtype) { _payloadtype = payloadtype; };
    int  send(UDP* sock, char* buffer, int buffersize);
};


/**********************************************************************************************
*
* CHBRMPPacketizer
*
***********************************************************************************************/
class CHBRMPPacketizer : CRTPPacketizer
{
protected:
    int     _RTPPacketSize;
    int     _HBRMPPacketSize;
    int     _HBRMPPayloadSize;
    CSMPTPProfile  _profile;
    unsigned int _hbrmpTimestamp;
    unsigned int _frameCount;

public:
    CHBRMPPacketizer(int mtu = 1500);
    ~CHBRMPPacketizer() {};

public:
    void setProfile(CSMPTPProfile* profile);
    int  send(UDP* sock, char* buffer, int buffersize);
};

#endif // _UDPPACKETIZER_H
