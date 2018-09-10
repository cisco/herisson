#ifndef _CIRCULARBUFFER_H
#define _CIRCULARBUFFER_H

#include <mutex>
#include <thread>
#include "queue.h"
#include "tcp_basic.h"

class CCircularRcvBuffer {

    int         _index;
    int         _nbElmt;
    UDP         _udpSock;
    char*       _buffer;
    int*        _seqArray;
    int         _wr_idx;
    int         _rd_idx;
    int         _lastRcvSeq;
    std::mutex  _mtx;
    bool        _bInit;
    bool        _closed;
    std::thread _th_rcv;
    int         _size;
    int         _readed;
    int         _writed;
    CQueue<int>* _q;
    bool        _isMaster;
    int         _samplesize;

    void* _rcv_thread();

    void  _dump();

public:
    CCircularRcvBuffer();
    ~CCircularRcvBuffer();

    int  init(CQueue<int>* q, const char* remote_addr, const char* local_addr, int port, int nbElmt, int index);
    int  close();
    int  write();
    int  read(int wantedSeq, char* buffer, int buflen);
    int  searchFor(int wantedSeq, char* buffer, int buflen);
    int  getLastRecvSeq() { return _lastRcvSeq;  };
    int  getIndex() { return _index; };
    bool isMaster() { return _isMaster; };
    void setMaster(bool flag) { _isMaster = flag; };
    int  getSampleSize() { return _samplesize; };
};

#endif // _CIRCULARBUFFER_H
