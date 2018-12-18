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
#include "packetizer.h"

/**********************************************************************************************
*
* COut
*
* Base class for all vMI output pin
*
***********************************************************************************************/
class COut {
protected:
    PinType     _nType;         /* Type of the pin */
    std::string _name;          /* Name of the pin. Basically, it's based on "%MODULE_NAME%[%index%]" */
    int         _nIndex;        /* Index of the pin on the list of module pins */
    int         _nModuleId;     /* Id of the module that own the pin */
    bool        _firstFrame;    /* Flag to indicate that first frame is under sending */
    MEDIAFORMAT _mediaformat;   /* Media format for the output stream */
    PinConfiguration* _pConfig; /* Pin configuration */

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
    /* 
     * Interface to implement
     */
    virtual int  send(CvMIFrame* frame) = 0;
    virtual bool isConnected() = 0;
};

/**********************************************************************************************
*
* COutTCP
*
* vMI output pin to propagate a vMI stream with a TCP connection
*
***********************************************************************************************/
class COutTCP : public COut
{
    TCP  _tcpSock;          /* TCP object used to send vMI frames */
    bool _isListen;         /* listen flag */
    int  _flags;
    int  _port;             /* port to use (client or server socket) */
    const char* _ip;        /* ip (for client socket) */
    const char* _interface; /* ip of the network interface for server socket */
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
* vMI output pin to propagate a vMI stream with RTP. Typcally used internally to a 
* pipeline to transport vMI frame between two modules not located on same compute node.
*
***********************************************************************************************/
class COutRTP : public COut
{
    UDP *_udpSock;          /* UDP socket to use to send data */
    bool _isMulticast;
    int  _mtu;

    const char* _ip;        /* ip (for client socket) */
    const char* _interface; /* ip of the network interface to rcv data (for server socket) */
    const char* _mcastgroup;/* mcast group to join for multicast stream */
    int _port;              /* port to use (client or server socket)*/

    CRTPPacketizer _packetizer; /* kind of packetiser to use */
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
    int    _shm_id;         /* shared memory identifier  */ 
#else
    HANDLE _shm_id;         /* shared memory identifier  */ 
#endif
    int    _shm_key;        /* key of the shared memory to get  */ 
    int    _shm_size;       /* size of shared memory in bytes */
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
    double _last_time;
    float  _frame_rate;
    unsigned char* _rgb_buffer;
    int    _rgb_buffer_len;
    int    _frame_w;
    int    _frame_h;

    const char* _ip;
    const char* _interface;
    int _port;
    int _depth;
    int _ratio;
    int _fps;
public:
    COutThumbSocket(CModuleConfiguration* pMainCfg, int nIndex);
    ~COutThumbSocket();
public:
    int  send(CvMIFrame* frame);
    bool isConnected();
};



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
* COutStorage
*
***********************************************************************************************/
class COutStorage : public COut
{
    const char* _ip;
    const char* _interface;
    const char* _mcastgroup;
    int _mtu;
    int _port;

public:
    COutStorage(CModuleConfiguration* pMainCfg, int nIndex);
    ~COutStorage();

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
