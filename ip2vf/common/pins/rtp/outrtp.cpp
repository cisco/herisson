#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <pins/pins.h>
#include "common.h"
#include "log.h"
#include "tools.h"
#include "rtpframe.h"
#include "tcp_basic.h"

using namespace std;

/**********************************************************************************************
*
* COutRTP
*
***********************************************************************************************/

COutRTP::COutRTP(CModuleConfiguration* pMainCfg, int nIndex) : COut(pMainCfg, nIndex)
{
    LOG("%s: --> <-- ", _name.c_str());
    _nType          = PIN_TYPE_RTP;
    PROPERTY_REGISTER_MANDATORY("ip", _ip, "");
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_OPTIONAL("interface", _interface, "");
    PROPERTY_REGISTER_OPTIONAL("mtu", _mtu, 1500);
    PROPERTY_REGISTER_OPTIONAL("mcastgroup", _mcastgroup, "");
    _isMulticast       = !!_mcastgroup[0];
    _seq            = 0;
    _frameCount     = 0;
    _UDPPacketSize  = _mtu - IP_HEADERS_LENGTH;
    _RTPPacketSize  = _UDPPacketSize - UDP_HEADERS_LENGTH;
    _payloadSize    = _RTPPacketSize - RTP_HEADERS_LENGTH;
    if( _mtu > RTP_MAX_FRAME_LENGTH ) {
        // TODO: issue
    }
#ifdef USE_NETMAP
    _udpSock = (strncmp(_interface, "netmap-", 7) == 0) ? new Netmap() : new UDP();
#else
    _udpSock = new UDP();
#endif
}

COutRTP::~COutRTP() 
{
    _udpSock->closeSocket();
    delete _udpSock;
}

int COutRTP::send(CvMIFrame* frame)
{

    //LOG_INFO("%s: --> <--  buffer=0x%x", _name.c_str(), buffer);
    int result = E_OK;
    int ret = 0;

    //
    // Manage the connection
    //
    if( !_udpSock->isValid() ) {
        if( _isMulticast )
            result = _udpSock->openSocket(_mcastgroup, _ip, _port, false, _interface);
        else 
            result = _udpSock->openSocket((char*)_ip, NULL, _port, false, _interface);
        if( result != E_OK ) 
            LOG_ERROR("%s: can't create %s UDP socket on [%s]:%d on interface '%s'", 
                _name.c_str(), (_isMulticast?"listening":"connected"), (_isMulticast?"NULL":_ip), _port, _interface[0]=='\0'?"<default>":_interface);
        else
            LOG_INFO("%s: Ok to create %s UDP socket on [%s]:%d on interface '%s'", 
                _name.c_str(), (_isMulticast?"listening":"connected"), (_isMulticast?"NULL":_ip), _port, _interface[0]=='\0'?"<default>":_interface);
    }

    //
    // Send data
    //
    if( _udpSock->isValid() )
    {
        int result = frame->sendToRTP(_udpSock, _mtu, _seq);
        if (result != VMI_E_OK) {
            ret = -1;
        }
        _frameCount++;
    }

    return ret;
}

bool COutRTP::isConnected() 
{
    return _udpSock->isValid();
}

PIN_REGISTER(COutRTP,"rtp");
