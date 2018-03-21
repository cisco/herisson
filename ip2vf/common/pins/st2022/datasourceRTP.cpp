#include <cstdio>
#include <cstdlib>
#include <string>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "datasource.h"
#include "moduleconfiguration.h"
#include "configurable.h"
using namespace std;

CRTPDataSource::CRTPDataSource() 
    : CDMUXDataSource() 
{
    _udpSock    = NULL;
    _samplesize = RTP_PACKET_SIZE;  // by default, will be refresh 
}

CRTPDataSource::~CRTPDataSource() {
    if (_udpSock!=NULL) {
        _udpSock->closeSocket();
        delete _udpSock;
        _udpSock = NULL;
    }
}

void CRTPDataSource::init(PinConfiguration *pconfig)
{
    int result; 
    _pConfig = pconfig;
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_OPTIONAL("mcastgroup", _zmqip, "");
    PROPERTY_REGISTER_OPTIONAL("ip", _ip, "");
    if (_port == -1) {
        LOG_ERROR("Invalid configuration. Exit. (port=%d)", _port);
    }

    // This allow to setup a network RTP stream 
    LOG_INFO("data stream from port '%d'",_port);
    if (!_udpSock)
        _udpSock = new UDP();

    if (_udpSock && !_udpSock->isValid())
        result = _udpSock->openSocket(_zmqip, _ip, _port, true);

    _firstPacket = true;
}

void CRTPDataSource::waitForNextFrame()
{
    // Do nothing for this source
}

int CRTPDataSource::read(char* buffer, int size)
{
    int result = -1;

    if (_udpSock && _udpSock->isValid()) {
        int len = size;
        result = _udpSock->readSocket(buffer, &len);
        if (result>0 && _firstPacket) {
            _samplesize = result;
            LOG_INFO("Detect sample size=%d", _samplesize);
            _firstPacket = false;
        }
    }

    return result;
}

void CRTPDataSource::close()
{
    LOG_INFO("-->");
    if (_udpSock && _udpSock->isValid())
    {
        _udpSock->closeSocket();
        delete _udpSock;
        _udpSock = NULL;
    }
    LOG_INFO("<--");
}

