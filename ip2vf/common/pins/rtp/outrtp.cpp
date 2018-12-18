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

int COutRTP::send(CvMIFrame* frame) {

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
#ifdef _DEBUG_RTP
        int p1 = frame->getFrameSize();
        int p2, p3;
        frame->get_header(VIDEO_WIDTH, (void*)&p2);
        frame->get_header(VIDEO_HEIGHT, (void*)&p3);
#endif
        int result = _packetizer.send(_udpSock, (char*)frame->getFrameBuffer(), frame->getFrameSize());
        if (result != VMI_E_OK) {
            ret = -1;
        }
#ifdef _DEBUG_RTP
        try {
            int pp2, pp3;
            if (p1 != frame->getFrameSize()) {
                LOG_ERROR("1 Frame size has been changed... Data corrupted?");
            }
            frame->get_header(VIDEO_WIDTH, (void*)&pp2);
            frame->get_header(VIDEO_HEIGHT, (void*)&pp3);
            if (pp2 != p2 || pp3 != p3) {
                LOG_ERROR("1 Frame size has been changed (%d/%d, %d/%d)... Data corrupted?", p2, pp2, p3, pp3);
            }
            frame->refreshHeaders();
            if (p1 != frame->getFrameSize()) {
                LOG_ERROR("2 Frame size has been changed... Data corrupted?");
            }
            frame->get_header(VIDEO_WIDTH, (void*)&pp2);
            frame->get_header(VIDEO_HEIGHT, (void*)&pp3);
            if (pp2 != p2 || pp3 != p3) {
                LOG_ERROR("2 Frame size has been changed (%d/%d, %d/%d)... Data corrupted?", p2, pp2, p3, pp3);
            }
        }
        catch (...) {
            LOG_ERROR("Exception when try to refresh Headers... Data corrupted?");
        }
#endif
    }

    return ret;
}

bool COutRTP::isConnected() 
{
    return _udpSock->isValid();
}

PIN_REGISTER(COutRTP,"rtp");
