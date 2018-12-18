#ifndef _IN_H
#define _IN_H

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <mutex>

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
* Base class for all vMI Input pin
*
***********************************************************************************************/
class CIn 
{
protected:
    PinType     _nType;         /* Type of the pin */
    std::string _name;          /* Name of the input pin. Basically, it's based on "%MODULE_NAME%[%index%]" */
    int         _nIndex;        /* Index of the pin on the list of module pins */
    int         _nModuleId;     /* Id of the module that own the pin */
    bool        _firstFrame;    /* Flag to indicate that first frame is under receive */
    MEDIAFORMAT         _mediaformat;   /* Media format for the input stream */
    PinConfiguration*   _pConfig;       /* Pin configuration */
    bool        _bStarted;              /* Start flag */

public:
    CIn(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~CIn() {};

    /* Accessors */
    int               getType()              { return _nType; };
    PinConfiguration* getConfiguration()     { return _pConfig; };
    bool              isStreamingType()      { return STREAMING_TYPE(_nType); };
    bool              isMemoryType()         { return MEMORY_TYPE(_nType);    };
    TransportType     getTransportType();    
    int               getIndex()             { return _nIndex;                };

    virtual void start() {};                 /* Start the pin (will start the stream processing) */
    virtual void stop() {};                  /* Stop the pin */
public:
    /*
     * Interface to implement for each kind of pin
     */
    virtual int  read(CvMIFrame* frame) = 0; /* called when nead to get a frame from the pin */
    virtual void reset() = 0;                /* reset */
};

/**********************************************************************************************
*
* CInTCP
*
* vMI Input pin to receive/process TCP stream
*
***********************************************************************************************/
class CInTCP : public CIn
{
public:
    TCP         _tcpSock;       /* TCP object */
    bool        _isListen;      /* listen flag */
    const char* _ip;            /* ip (for server socket) */
    const char* _interface;     /* ip of the network interface to use (client socket) */
    int         _port;          /* port to use (client or server socket) */
public:
    CInTCP(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~CInTCP();
    virtual void stop();
public:
    int  read(CvMIFrame* frame);
    void reset();
};

/**********************************************************************************************
*
* CInRTP
*
* vMI Input pin to receive/process vMI RTP stream. Typcally used internally to a pipeline 
* to transport vMI frame between two modules not located on same compute node.
*
***********************************************************************************************/
class CInRTP : public CIn
{
public:
    UDP*    _udpSock;           /* UDP socket to rcv data */
    bool    _isListen;          /* listening flag */
    bool    _waitForNextFrame;  /* "Wait for next frame" mode flag */
    const char*   _interface;   /* ip of the network interface to rcv data (for server socket) */
    const char*   _ip;          /* ip (for client socket) */
    const char*   _mcastgroup;  /* mcast group to join for multicast stream */
    int     _port;              /* port to use (client or server socket)*/
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
* vMI Input pin to receive/process shared mem stream. Typcally used internally to a 
* pipeline to transport vMI frame between two modules located on same compute node.
*
***********************************************************************************************/
class CInMem : public CIn
{
public:
#ifndef _WIN32
    int    _shm_id;         /* shared memory identifier  */ 
#else
    HANDLE _shm_id;         /* shared memory identifier  */ 
#endif
    int    _shm_size;       /* size of shared memory in bytes */
    int    _shm_key;        /* key of the shared memory to get  */ 
    char*  _shm_data;       /* pointer to the shared memory */
    int    _shm_nbseg;      /* number of segments, i.e. nb of frame to store */
    UDP    _udpSock;        /* UDP socket used for notification */
    int    _port;           /* port to use to receive notification that a new frame is available */
    const char* _interface; /* ip of the network interface to use for notification */
    long long   _sessionId; /* session id of the current shared meme configuration */
public:
    CInMem(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~CInMem();
private:
    void _checkMemorySegment(int shmkey, int shmsize, long long sessionId);
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
* vMI Input pin to receive/process stream contained on a file. Typically used at the
* beginning of the pipeline to ingest a dumped strream (pcap files)
*
***********************************************************************************************/
class CInFile : public CIn
{
public:
    std::ifstream _f;               /* file stream object */
    int     _file_size;             /* file size (calculated) */
    int     _frame_time_in_microsec;/* duration between two begin of frame (calculated from _fps) */
    float   _fps;                   /* frame per second */
    const char* _filename;          /* file name */
    double  _time;                  /* save timing associated with last frame */

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
* vMI Input pin to receive/process SMPTE 2022-6 stream. Typically used with vMI_demux at
* the beginning of a pipeline.
*
***********************************************************************************************/
/* struct used for double bufferring smpte frame inside the pin */
struct SmpteFrameBuffer {
    CSMPTPFrame   _frame;
    std::mutex    _lock;
};
class CInSMPTE : public CIn
{
private:
    CDMUXDataSource* _source;           /* Source of data */
    std::vector<SmpteFrameBuffer*> _smpteFrameArray;    /* Array of SMPTE frame for the double buffering */
    SmpteFrameBuffer* _currentFrame;    /* pointer to the SMPTE frame currently processed by vMI */
    bool            _firstFrame;        /* first frame flag */
    int             _fmt;               /* wanted bit per pixel components on provided vMI frame (8 or 10) */
    CQueue<int>     _q;                 /* queue of index of SmpteFrameBuffer */
    std::thread     _t;                 /* separate thread for CSMPTETFrame accretion */
    int             _nbSMPTEFrameToQueue;
    int             _onlyvideo;         /* demux only video flag */
    SMPTE_STANDARD_SUITE _streamType;   /* SMPTE type detected on input. SMPTE_2022_6 or SMPTE_2110_20 */

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
