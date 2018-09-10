#ifndef _DMUXDATASOURCE_H
#define _DMUXDATASOURCE_H

#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>

#include "queue.h"
#include "circularbuffer.h"
#include "tcp_basic.h"
#include "moduleconfiguration.h"

enum DataSourceType {
    TYPE_UNDEFINED = -1,  // No type
    TYPE_SOCKET = 1,   
    TYPE_FILE,
    TYPE_SMPTE_2022_7,
};

/**********************************************************************************************
*
* CDMUXDataSource: Base class for data sources
*
***********************************************************************************************/

class CDMUXDataSource
{
protected:
    std::mutex  _cs;    // for critical section
    bool        _closed;
    int         _samplesize;
    PinConfiguration *_pConfig;
    DataSourceType _type;

public:
    CDMUXDataSource();
    virtual ~CDMUXDataSource() {};
    PinConfiguration* getConfiguration(){
        return _pConfig;
    };
    DataSourceType getType() {
        return _type;
    };

    int getSampleSize() { return _samplesize; };

public:
    // Create a Data Source according to the pin configuration
    static CDMUXDataSource* create(PinConfiguration *pconfig);

    // Interface to implement
    virtual void init(PinConfiguration *pconfig) = 0;
    virtual int  read(char* buffer, int size) = 0;
    virtual void waitForNextFrame() = 0;
    virtual void close() = 0;
};

/**********************************************************************************************
*
* CFileDataSource: class for file source base
*
***********************************************************************************************/

class CFileDataSource : public CDMUXDataSource
{
protected:
    std::ifstream   _f;
    long long       _time;
    int             _pcapFile;
    int             _fileOffset;
    int             _packetOffset;
    const char*     _filename;
    float           _fps;

public:
    CFileDataSource();
    virtual ~CFileDataSource();

public:

    void setFrameRate(float fps);

    void init(PinConfiguration *pconfig);
    int  read(char* buffer, int size);
    void waitForNextFrame();
    void close();
};


/**********************************************************************************************
*
* CCachedFileDataSource: class for cached file source base
*
***********************************************************************************************/

class CCachedFileDataSource : public CDMUXDataSource
{
protected:
    long long       _time;
    int             _pcapFile;
    int             _fileOffset;
    int             _packetOffset;
    const char*     _filename;
    float           _fps;
    unsigned char*  _cache;
    unsigned char*  _p;
    int             _size;

public:
    CCachedFileDataSource();
    virtual ~CCachedFileDataSource();

public:

    void setFrameRate(float fps);

    void init(PinConfiguration *pconfig);
    int  read(char* buffer, int size);
    void waitForNextFrame();
    void close();
};

/**********************************************************************************************
*
* CRTPDataSource: class for network RTP source base
*
***********************************************************************************************/

class CRTPDataSource : public CDMUXDataSource
{
protected:
    UDP*        _udpSock;
    int         _port;
    const char* _zmqip;
    const char* _ip;
    bool        _firstPacket;

public:
    CRTPDataSource();
    virtual ~CRTPDataSource();

public:
#ifdef HAVE_PROBE
    void pktTSctl(int, unsigned int);
#endif

    void init(PinConfiguration *pconfig);
    int  read(char* buffer, int size);
    void waitForNextFrame();
    void close();
};

/**********************************************************************************************
*
* CSPSRTPDataSource: class for network "Seamless Protection Switching" RTP source base
*
***********************************************************************************************/

#define DMUX_2022_7_NB_SOURCES  2
struct SingleSource {
    CCircularRcvBuffer _in;
    std::chrono::system_clock::time_point _lastRcvEvent;
    bool _isOnline;
};
class CSPSRTPDataSource : public CDMUXDataSource
{
protected:
    SingleSource   _src[DMUX_2022_7_NB_SOURCES];
    SingleSource*  _master;
    SingleSource*  _secondary;
    bool        _bInit;
    CQueue<int> _q;
    int         _nextSeq;
    int         _lastSeq;
    int         _nPacketsLost;
    int         _port;
    int         _port2;
    const char *_mcastgroup;
    const char *_mcastgroup2;
    const char *_ip;
    const char *_ip2;
    double      _offline_threshold_in_s;

    void _switch_sources();

public:
    CSPSRTPDataSource();
    virtual ~CSPSRTPDataSource();

//private:
//    void* _rcv_thread(CCircularRcvBuffer* buffer, int index);

public:
    void init(PinConfiguration *pconfig);
    int  read(char* buffer, int size);
    void waitForNextFrame();
    void close();
};

/**********************************************************************************************
*
* CRIODataSource: class for optimized RIO-based data source
*
***********************************************************************************************/
#ifdef _WIN32

#include <WinSock2.h>
#include <MSWsock.h>
#include <WS2tcpip.h>
class CRIODataSource : public CDMUXDataSource, public UDP
{
	typedef struct ERIO_BUF : public RIO_BUF
	{
		int pkt_len;
	} ERIO_BUF;
protected:
	int         _port;
	const char* _zmqip;
	const char* _ip;
	bool        _firstPacket;
	GUID        _functionTableId;
	RIO_EXTENSION_FUNCTION_TABLE _rio;
	RIO_CQ      _completionQueue;
	RIO_RQ      _requestQueue;
	char*       _recvBufferPtr;
	char*       _addrBufferPtr;
	RIO_BUFFERID _recvBufferId;
	RIO_BUFFERID _addrBufferId;
	ERIO_BUF*     _addrRioBufs;
	ERIO_BUF*     _recvRioBufs;
	__int64      _addrRioBufIndex;
	__int64      _recvRioBufIndex;
	DWORD        _pendingReadPkts;
	DWORD        _addrRioBufTotalCount;
	DWORD        _recvRioBufTotalCount;
	DWORD        _numReceived;
	ULONG        _pendingRecvs;
	int          _rioCore;
public:
	static int initWSA();
	CRIODataSource();
	virtual int openSocket(const char* remote_addr, const char* local_addr, int port,
		bool modelisten, const char *ifname = NULL);
	virtual ~CRIODataSource();
	virtual int  readSocket(char *buffer, int *len);

public:
	void init(PinConfiguration *pconfig);
	int  read(char* buffer, int size);
	void waitForNextFrame();
	void close();
};
#endif //_WIN32

#endif //_DMUXDATASOURCE_H

