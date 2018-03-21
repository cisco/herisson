#ifndef _OUT_H
#define _OUT_H

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <pins/st2022/smpteframe.h>
#include "common.h"
#include "tcp_basic.h"
#include "frameheaders.h"
#include "framecounter.h"
#include "rtpframe.h"
#include "moduleconfiguration.h"
#include "vmiframe.h"
#include "vmistreamer.h"

/**********************************************************************************************
*
* COut
*
***********************************************************************************************/
class COut {
protected:
    PinType     _nType;
    std::string _name;
    int         _nIndex;
    int         _nModuleId;
    bool        _firstFrame;
    MEDIAFORMAT _mediaformat;
    PinConfiguration* _pConfig;

public:
    COut(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~COut(){};

    // Accessors
    int               getType()              { return _nType;   };
    PinConfiguration* getConfiguration()     { return _pConfig; };
    bool              isStreamingType()      { return STREAMING_TYPE(_nType); };
    bool              isMemoryType()         { return MEMORY_TYPE(_nType);    };
    TransportType     getTransportType();

public:
    // Interface to implement
    virtual int  send(CvMIFrame* frame) = 0;
    virtual bool isConnected() = 0;
};

/**********************************************************************************************
*
* COutTCP
*
***********************************************************************************************/
class COutTCP : public COut
{
    TCP  _tcpSock;
    bool _isListen;
    int  _flags;
    int  _format;       // input format: 0=ip2vf frame, 1=raw video frame

    int  _port;
    const char* _ip;
    const char* _interface;
public:
    COutTCP(CModuleConfiguration* pMainCfg, int nIndex);
    ~COutTCP();
public:
    int  send(CvMIFrame* frame);
    bool isConnected();
};

/**********************************************************************************************
*
* COutRTP
*
***********************************************************************************************/
class COutRTP : public COut
{
    UDP *_udpSock;
    bool _isMulticast;
    int  _mtu;
    int  _UDPPacketSize;
    int  _RTPPacketSize;
    int  _payloadSize;

    const char* _ip;
    const char* _interface;
    const char* _mcastgroup;
    int _port;

    unsigned int  _seq;
    unsigned int  _frameCount;
    unsigned char _RTPframe[RTP_MAX_FRAME_LENGTH];
public:
    COutRTP(CModuleConfiguration* pMainCfg, int nIndex);
    ~COutRTP();
public:
    int  send(CvMIFrame* frame);
    bool isConnected();
};

/**********************************************************************************************
*
* COutMem
*
***********************************************************************************************/
class COutMem : public COut
{
public:
#ifndef _WIN32
    int    _shm_id;         /* return value from shmget()    */
#else
    HANDLE _shm_id;
#endif
    int    _shm_key;        /* key to be passed to shmget()  */
    int    _shm_size;       /* size to be passed to shmget() */
    char*  _shm_data;
    int    _shm_nbseg;
    int    _shm_wr_pt;
    UDP    _udpSock;
    const char*  _ip ;      /* ip to use to notify receiver that a new frame is available   */
    int    _port ;          /* port to use to notify receiver that a new frame is available */
    const char* _interface;
    long long _sessionId;
public:
    COutMem(CModuleConfiguration* pMainCfg, int nIndex);
    ~COutMem();
public:
    int  send(CvMIFrame* frame);
    bool isConnected();
};

/**********************************************************************************************
*
* COutDevNull
*
***********************************************************************************************/
class COutDevNull : public COut
{
public:
    COutDevNull(CModuleConfiguration* pMainCfg, int nIndex);
    ~COutDevNull();
public:
    int  send(CvMIFrame* frame) ;   // TODO
    bool isConnected() {return true;};
};


/**********************************************************************************************
*
* COutThumbSocket
*
***********************************************************************************************/
class COutThumbSocket : public COut, public CFrameCounter
{
    TCP    _tcpSock;
    bool   _isListen;
    int    _output_fps;
    double _last_time;
    float  _frame_rate;
    unsigned char* _rgb_buffer;
    int    _rgb_buffer_len;
    int    _frame_w;
    int    _frame_h;

    const char* _ip;
    const char* _interface;
    int _port;
    int _w;
    int _h;
    int _depth;
    int _ratio;
    float _fps;
public:
    COutThumbSocket(CModuleConfiguration* pMainCfg, int nIndex);
    ~COutThumbSocket();
public:
    int  send(CvMIFrame* frame);
    bool isConnected();
};

#ifndef DO_NOT_COMPILE_X264

/**********************************************************************************************
*
* COutx264
*
***********************************************************************************************/
class COutx264 : public COut
{
    TCP  _tcpSock;
    bool _isListen;
    int _port;
    int _w;
    int _h;
    int  _format;       // input format: 0=ip2vf frame, 1=raw video frame
    bool _bWaitForNextKeyFrame;
    bool _bQuit;
    int            _internal_buffer_size;
    unsigned char* _internal_buffer;
    std::thread _th;
    std::mutex _mutex;
    std::condition_variable _cond_var;
    const char* _ip;
    const char* _interface;
public:
    COutx264(CModuleConfiguration* pMainCfg, int nIndex);
    ~COutx264();
protected:
    void _x264_encode_process();
public:
    int  send(CvMIFrame* frame);
    bool isConnected();
};
#endif

/**********************************************************************************************
*
* COutSMPTE
*
***********************************************************************************************/
class COutSMPTE: public COut
{
    CBasevMIStreamer *streamer;
    const char* _ip;
    const char* _interface;
    const char* _mcastgroup;
    int _mtu;
    int _port;
    int _port2;
    bool _useDeltacast;
    const char* _smptefmt;
    SMPTE_STANDARD_SUITE _standard;

public:
    COutSMPTE(CModuleConfiguration* pMainCfg, int nIndex);
    ~COutSMPTE();

public:
    int send(CvMIFrame* frame);
    bool isConnected();
};

/**********************************************************************************************
*
* COutTR03
*
***********************************************************************************************/
class COutTR03 : public COut
{
    UDP *_udpSock;
    bool _isMulticast;
    int  _mtu;
    int  _UDPPacketSize;
    int  _RTPPacketSize;
    int  _payloadSize;
    //int  _w;        // size w in pixels
    //int  _h;        // size h in pixels
    //int  _depth;    // bits by pixels
    int  _pgroup;
    int  _linesize; // full line size in bytes
    int  _linepayloadsize; // full line size in bytes + headers
    unsigned int  _seq;
    unsigned int  _frameCount;
    unsigned char _RTPframe[RTP_MAX_FRAME_LENGTH];
    const char* _ip;
    const char * _interface;
    const char * _mcastgroup;
    int _port;

public:
    COutTR03(CModuleConfiguration* pMainCfg, int nIndex);
    ~COutTR03();
public:
    int  send(CvMIFrame* frame);
    bool isConnected();
};


#endif //_OUT_H
