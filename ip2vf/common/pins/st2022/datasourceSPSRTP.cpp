#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>
#include <algorithm>    // for std::fill_n
#include <signal.h>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "datasource.h"
#include "rtpframe.h"
#include "moduleconfiguration.h"
#include "configurable.h"

using namespace std;

#define DEFAULT_PACKET_NB       30000
#define DEFAULT_THRESHOLD_IN_S  0.02

CSPSRTPDataSource::CSPSRTPDataSource()
    : CDMUXDataSource() 
{
    _bInit              = false;
    _closed             = true;
    _pConfig            = nullptr;
    _nextSeq            = -1;
    _type               = DataSourceType::TYPE_SMPTE_2022_7;
    _nPacketsLost       = 0;
    _samplesize         = RTP_PACKET_SIZE;  // by default, will be refresh 
    _offline_threshold_in_s = DEFAULT_THRESHOLD_IN_S;
}

CSPSRTPDataSource::~CSPSRTPDataSource() {
}

void CSPSRTPDataSource::init(PinConfiguration *pconfig) {

    if (!_closed)
        return;

    LOG_INFO("--> ");
    _pConfig = pconfig;
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_MANDATORY("port2", _port2, -1);
    PROPERTY_REGISTER_OPTIONAL("mcastgroup", _mcastgroup, "");
    PROPERTY_REGISTER_OPTIONAL("ip", _ip, "");
    PROPERTY_REGISTER_OPTIONAL("mcastgroup2", _mcastgroup2, _mcastgroup);
    PROPERTY_REGISTER_OPTIONAL("ip2", _ip2, _ip);
    _bInit = false;

    // This allow to setup a network RTP stream 
    LOG_INFO("Data stream from port '%d' and '%d' ", _port, _port2);


    // Determine main and secundary streams. Main stream is the one with the latest packets. It will assure
    // that when a packet missed on the main stream, the corresponding packet has been already received on
    // the secundary stream
    _src[0]._in.init(&_q, _mcastgroup, _ip, _port, DEFAULT_PACKET_NB, 0);
    _src[0]._isOnline = true;
    _src[0]._lastRcvEvent = std::chrono::system_clock::now();
    _master = &_src[0];
    _master->_in.setMaster(true);

    _src[1]._in.init(&_q, _mcastgroup2, _ip2, _port2, DEFAULT_PACKET_NB, 1);
    _src[1]._isOnline = true;
    _src[1]._lastRcvEvent = std::chrono::system_clock::now();
    _secondary = &_src[1];
    _secondary->_in.setMaster(false);

    _closed = false;
    _bInit = true;
    LOG_INFO("<--");
}

void CSPSRTPDataSource::waitForNextFrame() {

    // Do nothing for this source
}

void CSPSRTPDataSource::_switch_sources() {

    LOG_INFO("Switch source [%d] <--> [%d]", _master->_in.getIndex(), _secondary->_in.getIndex());
    SingleSource* temp = _master;
    _master = _secondary;
    _secondary = temp;
    _master->_in.setMaster(true);
    _secondary->_in.setMaster(false);
}

int CSPSRTPDataSource::read(char* buffer, int size) {

    int result = VMI_E_OK;
    int len = size;

    //LOG_INFO("-->, _closed=%d, _bInit=%d", _closed, _bInit);

    if (!_bInit)
        return VMI_E_ERROR;

    if (!_closed) {

        //static int count = 0;
        //if ((++count) % 10000 == 0) LOG_INFO("%d events, queue size=%d", count, _q.size());

        // If we lost packets on main, try to find it on secundary 
        if (_nPacketsLost>0) {
            //LOG_INFO("--> _nPacketsLost=%d", _nPacketsLost);
            if (_secondary->_isOnline) {
                //LOG_INFO("sec is online, try to read #%d", _nextSeq);
                result = _secondary->_in.searchFor(_nextSeq, buffer, size);
                if (result == -1) {
                    LOG_ERROR("can't find missing packet #%d on secondary (nbPacketLost=%d, queue size=%d)", _nextSeq, _nPacketsLost, _q.size());
                    _nPacketsLost = 0;
                    _nextSeq = -1;
                    return VMI_E_ERROR;
                }
                else {
                    LOG("Ok to keep packet%d (nbPacketLost=%d, queue size=%d)", _nextSeq, _nPacketsLost, _q.size());
                    _nPacketsLost--;
                    if (_nextSeq == _lastSeq) {
                        LOG_INFO("_nextSeq == _lastSeq");
                        _nPacketsLost = 0;
                    }
                    _nextSeq = (_nextSeq + 1) % 65536;
                    result = size;
                }
                return result;
            }
            return VMI_E_ERROR;
        }

        // Wait for any rcv events from sources
        int event = _q.pop();

        // detect exit condition
        if (event == -1 || _closed)
            return VMI_E_ERROR;

        // Keep the source index and the rtp seq number
        int seq = event & 0xFFFF;
        int source = event >> 16;
        //LOG_INFO("[%d]--> receive packet=%d, size of queue=%d", source, seq, _q.size());

        // If errors...
        if (seq == -1)
            return VMI_E_ERROR;
        if( source <0 || source >1 )
            return VMI_E_ERROR;

        // Manage online state of "current" source (the one that produce the event)
        _src[source]._lastRcvEvent = std::chrono::system_clock::now();
        if( !_src[source]._isOnline )
            LOG_INFO("Source [%d] is now online", source);
        _src[source]._isOnline = true;

        // Offline sources detection
        auto now = std::chrono::system_clock::now();
        bool bSwitchOccured = false;
        for (int i = 0; i < DMUX_2022_7_NB_SOURCES; i++) {
            std::chrono::duration<double> diff = now - _src[i]._lastRcvEvent;
            if (_src[i]._isOnline && diff.count() > _offline_threshold_in_s) {
                LOG_INFO("Source [%d] is offline", i);
                _src[i]._isOnline = false;
                if (i == _master->_in.getIndex()) {
                    // The offline source is the master source... switch between main and secundary
                    LOG_INFO("Switch source [%d] <--> [%d]", _master->_in.getIndex(), _secondary->_in.getIndex());
                    _switch_sources();
                    bSwitchOccured = true;
                }
            }
        }
        if (bSwitchOccured) {
            _nextSeq = -1;
            return VMI_E_ERROR;
        }

        // At this state, if the source is the secundary, nothing to do
        if (source != _master->_in.getIndex())
            return VMI_E_NOT_PRIMARY_SRC;

        if (_nextSeq == -1) {
            _nextSeq = seq;
            _nPacketsLost = 0;
        }
        _lastSeq = seq;
        if (_nextSeq != _lastSeq) {
            _nPacketsLost = (_lastSeq - _nextSeq) > 0 ? (_lastSeq - _nextSeq) : (65536 - _nextSeq + _lastSeq);
            _nPacketsLost++; 
            LOG_ERROR("recv packet#%d, expected #%d, diff=%d (queue size=%d)", _lastSeq, _nextSeq, _nPacketsLost, _q.size());
            return VMI_E_PACKET_LOST;
        }
        int len = size;
        result = _master->_in.read(_nextSeq, buffer, size);
        if (result == -1) {
            LOG_ERROR("result=%d, len=%d, size=%d (queue size=%d)", result, len, size, _q.size());
            return VMI_E_ERROR;
        }
        else {
            //LOG_INFO("--> receive packet=%d, read=%d, size of queue=%d", seq, _nextSeq, _q.size());
            _nextSeq = (_nextSeq + 1) % 65536;
            result = size;
            _samplesize = _master->_in.getSampleSize();
        }
    }

    //LOG_INFO("<--");
    return result;
}

void CSPSRTPDataSource::close() {

    LOG_INFO("-->");

    _closed = true;

    for (int i = 0; i < DMUX_2022_7_NB_SOURCES; i++)
        _src[i]._in.close();

    // unblock the queue
    _q.push(-1);

    LOG_INFO("<--");
}

