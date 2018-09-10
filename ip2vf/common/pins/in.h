#ifndef _IN_H
#define _IN_H

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>

#include <pins/st2022/smpteframe.h>
#include <pins/st2022/datasource.h>
#include "rtpframe.h"
#include "common.h"
#include "tcp_basic.h"
#include "frameheaders.h"
#include "framecounter.h"
#include "moduleconfiguration.h"
#include <pins/pinfactory.h>
#include <configurable.h>
#include "vmiframe.h"

#define PACKET_SIZE 1428

/**********************************************************************************************
*
* CIn
*
***********************************************************************************************/
class CIn 
{
protected:
    PinType     _nType;
    std::string _name;
    int         _nIndex;
    int         _nModuleId;
    bool        _firstFrame;
    //CFrameHeaders     _headers;
    MEDIAFORMAT         _mediaformat;
    PinConfiguration*   _pConfig;
    bool            _bStarted;

public:
    CIn(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~CIn() {};

    // Accessors
    int               getType()              { return _nType; };
    PinConfiguration* getConfiguration()     { return _pConfig; };
    bool              isStreamingType()      { return STREAMING_TYPE(_nType); };
    bool              isMemoryType()         { return MEMORY_TYPE(_nType);    };
    TransportType     getTransportType();    
    int               getIndex()             { return _nIndex;                };

    virtual void start() {};
    virtual void stop() {};
public:
    // Interface to implement
    virtual int  read(CvMIFrame* frame) = 0;
    virtual void reset() = 0;
};

/**********************************************************************************************
*
* CInTCP
*
***********************************************************************************************/
class CInTCP : public CIn
{
public:
    TCP  _tcpSock;
    bool _isListen;
    const char* _ip;
    const char* _interface;
    int _port;
public:
    CInTCP(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~CInTCP();
public:
    int  read(CvMIFrame* frame);
    void reset();
    virtual void stop();
};

/**********************************************************************************************
*
* CInRTP
*
***********************************************************************************************/
class CInRTP : public CIn
{
public:
    UDP *_udpSock;
    bool _isListen;
    bool _waitForNextFrame;
    int  _lastSeq;
    int  _format;       // input format: 0=ip2vf frame, 1=raw video frame
    unsigned int  _frameCounter;
    unsigned char _RTPframe[RTP_MAX_FRAME_LENGTH];
    const char* _interface;
    const char* _ip;
    const char* _mcastgroup;
    int _port;
    int _w;
    int _h;
public:
    CInRTP(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~CInRTP();
public:
    int  read(CvMIFrame* frame);
    void reset();
    virtual void start();
    virtual void stop();
};

/**********************************************************************************************
*
* CInMem
*
***********************************************************************************************/
class CInMem : public CIn
{
public:
#ifndef _WIN32
    int    _shm_id;       /* return value from shmget()    */ 
#else
    HANDLE _shm_id;
#endif
    int    _shm_size;       /* size to be passed to shmget() */
    int    _shm_key;        /* key to be passed to shmget()  */ 
    char*  _shm_data;
    int    _shm_nbseg;      /* number of segments */
    UDP    _udpSock;
    int    _port;           /* port to use to receive notification that a new frame is available */
    //int    _shm_wr_pt;
    const char* _interface;
    long long _sessionId;
public:
    CInMem(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~CInMem();
private:
    void _checkMemorySegment(int shmkey, long long sessionId);
public:
    int  read(CvMIFrame* frame);
    void reset() {};
    void start();
    void stop();
};

/**********************************************************************************************
*
* CInFile
*
***********************************************************************************************/
class CInFile : public CIn
{
public:
    std::ifstream _f;
    int     _file_size;
    int     _frame_time_in_microsec;
    float   _fps;
    const char* _filename;
    double  _time;

public:
    CInFile(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~CInFile();

public:
    int  read(CvMIFrame* frame);
    void reset() {};
};

/**********************************************************************************************
*
* CInSMPTE
*
***********************************************************************************************/
#include <mutex>
#include <thread>
struct SmpteFrameBuffer {
    CSMPTPFrame   _frame;
    std::mutex    _lock;
};
class CInSMPTE : public CIn
{
private:
    CDMUXDataSource* _source;
    std::vector<SmpteFrameBuffer*> _smpteFrameArray;
    SmpteFrameBuffer* _currentFrame;
    bool            _firstFrame;
    int             _fmt;
    CQueue<int>     _q;
    std::thread     _t;
    int             _nbSMPTEFrameToQueue;
    int             _onlyvideo;
    SMPTE_STANDARD_SUITE _streamType;

public:
    CInSMPTE(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~CInSMPTE();
private:
    int _rcv_process();
    SmpteFrameBuffer*  _get_next_frame();
public:
    int  read(CvMIFrame* frame);
    void reset() {};
    void start();
    void stop();
};


#endif //_IN_H
