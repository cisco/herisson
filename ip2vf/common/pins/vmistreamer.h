#ifndef _VMISTREAMER_H
#define _VMISTREAMER_H

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>

#include "common.h"
#include "tcp_basic.h"
#include "frameheaders.h"
#include "vmiframe.h"
#include "rtpframe.h"
#include "moduleconfiguration.h"
#include <pins/st2022/smpteprofile.h>
#include "hbrmppacketizer.h"


/**********************************************************************************************
*
* CBasevMIStreamer
*
***********************************************************************************************/
class CBasevMIStreamer
{
protected:
    bool                _init;
    PinConfiguration*   _pConfig;
    char*               _ip;
    char*               _mcastgroup;
    char*               _ifname;
    int                 _port;

public:
    CBasevMIStreamer(const char* ip, const char* mcastgroup, int port, PinConfiguration *pconfig, const char* ifname=NULL) {
        _pConfig = pconfig;
        _ip = (ip == NULL) ? NULL : STRDUP(ip);
        _mcastgroup = (mcastgroup == NULL) ? NULL : STRDUP(mcastgroup);
        _ifname = (ifname == NULL) ? NULL : STRDUP(ifname);
        _port = port;
    };
    virtual ~CBasevMIStreamer() {
        if (_ip) free(_ip);
        if (_mcastgroup) free(_mcastgroup);
        if (_ifname) free(_ifname);
    };

public:
    PinConfiguration* getConfiguration() {
        return _pConfig;
    }

public:
    // Interface to implement
    virtual int  send(CvMIFrame* frame) = 0;
    virtual bool isConnected() = 0;

};





/**********************************************************************************************
*
* CvMIStreamerCisco2022_6
*
***********************************************************************************************/
class CvMIStreamerCisco2022_6 : public CBasevMIStreamer
{
protected:
    CHBRMPPacketizer  _packetizer;
    struct tFrameStruc
    {
        CSMPTPFrame* frame;
        bool complete;
    };
    UDP     _udpSock;
    bool    _isMulticast;
    unsigned int _frameCount;
    unsigned int _hbrmpTimestamp;
    bool    _firstvMIFrame;
    bool    _firstCompletedFrame;
    struct tFrameStruc _frame;
    int     _curFrameNb;

public:
    CvMIStreamerCisco2022_6(const char* ip, const char* mcastgroup, int port, PinConfiguration *pconfig, const char* ifname = NULL);
    ~CvMIStreamerCisco2022_6();

public:
    // Interface to implement
    int  send(CvMIFrame* frame);
    bool isConnected();
};


#endif // _VMISTREAMER_H
