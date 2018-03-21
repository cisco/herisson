#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>
#include <algorithm>    // for std::fill_n
#include <chrono>


#include "common.h"
#include "log.h"
#include "tools.h"
#include "rtpframe.h"
#include "circularbuffer.h"

CCircularRcvBuffer::CCircularRcvBuffer() {
    _index    = -1;
    _bInit    = false;
    _buffer   = NULL;
    _seqArray = NULL;
    _nbElmt   = -1;
    _closed   = true;
    _size     = 0;
    _q        = NULL;
    _samplesize = -1;
};

CCircularRcvBuffer::~CCircularRcvBuffer() {
    close();
};

int CCircularRcvBuffer::init(CQueue<int>* q, const char* remote_addr, const char* local_addr, int port, int nbElmt, int index) {

    LOG_INFO("[%d] --> (port=%d, nbElmt=%d)", index, port, nbElmt);

    if (nbElmt <= 0 || port <= 0)
        return VMI_E_INVALID_PARAMETER;

    close();

    _q        = q;
    _index    = index;
    _closed   = false;
    _nbElmt   = nbElmt;
    _buffer   = new char[_nbElmt*RTP_PACKET_SIZE];
    _seqArray = new int[_nbElmt];
    _wr_idx   = -1;
    _rd_idx   = -1;
    std::fill(_seqArray, _seqArray+_nbElmt, -1);

    if (!_udpSock.isValid())
        int result = _udpSock.openSocket(remote_addr, local_addr, port, true);

    _th_rcv = std::thread([this] { _rcv_thread(); });

    _bInit = true;

    LOG_INFO("[%d] <--", _index);
    return VMI_E_OK;
};

int CCircularRcvBuffer::close() {

    if (!_bInit)
        return VMI_E_OK;

    _closed = true;

    // Unlock any blocking readSocket()
    if (_udpSock.isValid())
        _udpSock.closeSocket();

    // wait for _th_sec closing
    LOG_INFO("[%d] wait for end of thread", _index);
    if (_th_rcv.joinable()) {
        _th_rcv.join();
    }
    LOG_INFO("[%d] end of thread ok", _index);

    if (_buffer)
        delete[] _buffer;
    _buffer   = NULL;
    if (_seqArray)
        delete[] _seqArray;
    _seqArray = NULL;

    return VMI_E_OK;
};

void* CCircularRcvBuffer::_rcv_thread() {

    LOG_INFO("[%d] -->", _index);
    int seq = 0;
    CRTPFrame rtpframe;
    while (!_closed) {
//#define _DEBUG
#ifdef _DEBUG
        static auto start = std::chrono::system_clock::now();
        std::chrono::duration<double> diff = std::chrono::system_clock::now() - start;
        // Simulate a stream interruption on line 0 between start+3.0s and start+6.0s
        if (_index == 0) {
            if (diff.count() > 3.0 && diff.count() < 6.0) {
                std::this_thread::yield();
                continue;
            }
        }
        // Simulate a stream interruption on line 1 between start+9.0s and start+10.0s
        if (_index == 1) {
            if (diff.count() > 9.0 && diff.count() < 10.0) {
                std::this_thread::yield();
                continue;
            }
        }
#endif
        seq = write();
        if (seq < 0) {
            //LOG_ERROR("[%d] error when reading RTP frame: seq=%d", _index, seq);
            //abort();
            //break;
        }
        else {
            // No error, push the packet to the datasourceSPSRTP
            int p = (_index << 16) + (seq & 0xFFFF);
            _q->push(p);
            _lastRcvSeq = seq;
        }
        if (_closed) {
            LOG_INFO("[%d] closing...", _index);
            break;
        }

        //if( result%1000==0) LOG_INFO("Rcv packet #%d", result);
        //std::this_thread::yield();
    }
    LOG_INFO("[%d] <--", _index);
    return 0;
}
#include <iostream>
int CCircularRcvBuffer::write() {
    int result = -1;
    int len = RTP_PACKET_SIZE;

    std::unique_lock<std::mutex> lock(_mtx);

    if (_buffer == NULL || _seqArray == NULL)
        return result;
        
    //inc the write pointer for next packet to read
    int old_wr_idx = _wr_idx;
    _wr_idx = (_wr_idx + 1) % _nbElmt;

    if (_wr_idx > 0 && _rd_idx < 0) _rd_idx = 0;
    if (_wr_idx == _rd_idx) {
        // We don't read fast enough... 
        // For master, wait for free slot: i.e. don't inc the write pointer, and return (do nothing)
        // For secundary, delete oldest RTP packet: i.e. inc the read pointer
        if (_isMaster) {
            //LOG_INFO("[%d] we don't read fast enough..., _rd_idx=%d, _wr_idx=%d, q=%d", _index, _rd_idx, _wr_idx, _q->size());
            _wr_idx = old_wr_idx;
            return -1;
        }
        else
            _rd_idx = (_wr_idx + 1) % _nbElmt;
    }
    _seqArray[_wr_idx] = -1;

    // Read one packet from RTP stream and write it on the buffer at correct place
    char* wr_ptr = _buffer + (_wr_idx * RTP_PACKET_SIZE);
    result = _udpSock.readSocket(wr_ptr, &len);
    if (result <= 0) {
        if( !_closed)
            LOG_ERROR("[%d] error when reading RTP frame: result=%d", _index, result);
        result = -1; // Be carefull, result==0 is an "error", but return value is -1 if error. If return 0, it's a packet seq nb.
        return result;
    }
    if (_samplesize == -1)
        _samplesize = result;

    // Get seq number of this RTP packet, and update the seq array
    CRTPFrame rtpframe;
    rtpframe.setBuffer((unsigned char*)wr_ptr, len);
    rtpframe.readHeader();
    _seqArray[_wr_idx] = rtpframe._seq;
    result = rtpframe._seq;
    _size = (_wr_idx > _rd_idx) ? (_wr_idx - _rd_idx) : (65536-(_rd_idx- _wr_idx));
    _writed++;

    //if (rtpframe._seq % 1000 == 0) LOG_INFO("[%d] write packet#%d at pos %d [%d..%d]", _index, _seqArray[_wr_idx], _wr_idx, _seqArray[_rd_idx<0?0: _rd_idx], _seqArray[_wr_idx]);
   // if ((rtpframe._seq % 1000 == 0) && _index == 1) LOG_INFO("[%d] write packet#%d at pos %d [%d..%d], _size=%d", _index, _seqArray[_wr_idx], _wr_idx, _rd_idx, _wr_idx, _size);

    return result;
};

int CCircularRcvBuffer::read(int wantedSeq, char* buffer, int buflen) {
    int result = -1;

    std::unique_lock<std::mutex> lock(_mtx);

    if (_buffer == NULL || _seqArray == NULL)
        return result;

    _rd_idx = (_rd_idx + 1) % _nbElmt;
    int lastseq = _seqArray[_rd_idx];
    if (lastseq == wantedSeq) {
        char* rd_ptr = _buffer + _rd_idx*RTP_PACKET_SIZE;
        memcpy(buffer, rd_ptr, RTP_PACKET_SIZE);
        result = lastseq;
        _seqArray[_rd_idx] = -1;
        _readed++;
        //if(_index == 1) LOG_INFO("[%d]  found pck#%d at pos %d, _readed=%d, _writed=%d", _index, wantedSeq, _rd_idx, _readed, _writed);
    }
    lock.unlock();

    if(result==-1){
        LOG("[%d] want packet#%d but find %d#, rd=%d, wr=%d , val=[%d..%d]", _index, wantedSeq, lastseq, _rd_idx, _wr_idx, _seqArray[_rd_idx], _seqArray[_wr_idx]);
        //LOG_ERROR("[%d] want %d, find %d at pos %d", _index, wantedSeq, lastseq, _rd_idx);
        result = searchFor(wantedSeq, buffer, buflen);
    }

    return result;
}

int CCircularRcvBuffer::searchFor(int wantedSeq, char* buffer, int buflen) {

    int result = -1;

    if (_buffer == NULL || _seqArray == NULL)
        return result;

    std::unique_lock<std::mutex> lock(_mtx);

    for (int i = 0; i < _nbElmt; i++) {
        //if (_seqArray[_rd_idx] == -1)
        //    continue;
        //if (min == -1 || min > _seqArray[_rd_idx]) min = _seqArray[_rd_idx];
        //if (max == -1 || max < _seqArray[_rd_idx]) max = _seqArray[_rd_idx];
        if (wantedSeq == _seqArray[i]) {
            // Found!!!
            char* rd_ptr = _buffer + (i * RTP_PACKET_SIZE);
            memcpy(buffer, rd_ptr, RTP_PACKET_SIZE);
            result = i;
            _rd_idx = i;
            LOG("[%d] found pck#%d at pos %d", _index, wantedSeq, i);
            return result;
        }
    }
#define _DEBUG
#ifdef _DEBUG
    _dump();
#endif
    LOG_ERROR("[%d] can't find wanted pck#%d", _index, wantedSeq);

    return result;
}

void  CCircularRcvBuffer::_dump() {

    if (_rd_idx<_wr_idx)
        LOG_INFO("[%d] struct [0   r=%d...w=%d   %d]", _index, _rd_idx, _wr_idx, _nbElmt);
    else 
        LOG_INFO("[%d] struct [0...w=%d   r=%d...%d]", _index, _wr_idx, _rd_idx, _nbElmt);
    
    if(_seqArray[_rd_idx]<_seqArray[_wr_idx])
        LOG_INFO("[%d] stored packets# [%d...%d]", _index, _seqArray[_rd_idx], _seqArray[_wr_idx]);
    else
        LOG_INFO("[%d] stored packets# [%d...65535][0...%d]", _index, _seqArray[_rd_idx], _seqArray[_wr_idx]);

}
