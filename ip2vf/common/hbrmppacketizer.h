#ifndef _HBRMPPACKETIZER_H
#define _HBRMPPACKETIZER_H

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>

#include "common.h"
#include "tcp_basic.h"
#include "frameheaders.h"
#include "rtpframe.h"
#include "pins/st2022/smpteprofile.h"
#include "pins/st2022/hbrmpframe.h"


/**********************************************************************************************
*
* CHBRMPPacketizer
*
***********************************************************************************************/
class CHBRMPPacketizer
{
protected:
    char    _RTPframe[RTP_MAX_FRAME_LENGTH];
    int     _RTPPacketSize;
    int     _HBRMPPacketSize;
    int     _HBRMPPayloadSize;
    int     _seq;
    CSMPTPProfile  _profile;
    unsigned int _hbrmpTimestamp;
    unsigned int _frameCount;

public:
    CHBRMPPacketizer();
    ~CHBRMPPacketizer() {};

public:
    void setProfile(CSMPTPProfile*  profile);
    int  send(UDP* sock, char* mediabuffer, int mediabuffersize, int payloadtype);
};

#endif // _HBRMPPACKETIZER_H
