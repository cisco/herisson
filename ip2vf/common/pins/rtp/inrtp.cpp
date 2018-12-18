#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <pins/pins.h>
#include "common.h"
#include "log.h"
#include "tools.h"
#include "tcp_basic.h"
#include "rtpframe.h"

using namespace std;

#include "pins/pktTS.h"                                         /* PktTS hook */

/**********************************************************************************************
*
* CInRTP
*
***********************************************************************************************/

CInRTP::CInRTP(CModuleConfiguration* pMainCfg, int nIndex) : CIn(pMainCfg, nIndex)
{
    LOG_INFO("%s: --> <--", _name.c_str());
    _nType        = PIN_TYPE_RTP;
    _isListen     = true;
    _bStarted     = false;
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_OPTIONAL("mcastgroup", _mcastgroup, "");
    PROPERTY_REGISTER_OPTIONAL("interface", _interface,"");
    PROPERTY_REGISTER_OPTIONAL("ip", _ip, "");
    _waitForNextFrame = true;
#ifdef USE_NETMAP
    if(strncmp(_interface, "netmap-", 7) == 0)
        _udpSock = new Netmap();
    else
#endif
#ifdef HAVE_PROBE
        if((_udpSock = pktTSconstruct(_pConfig)) == 0)              /* PktTS hook */
#endif
            _udpSock = new UDP();
}

CInRTP::~CInRTP() 
{
    _udpSock->closeSocket();
    delete _udpSock;
}

void CInRTP::reset() 
{
}

int CInRTP::read(CvMIFrame* frame)
{
    LOG("%s: --> <--", _name.c_str());
    int ret = VMI_E_OK;

    //
    // Manage the connection
    //

    if( !_udpSock->isValid() ) {

        int result = E_OK;
        if( _isListen )
            result = _udpSock->openSocket(_mcastgroup, _ip, _port, true);
        else 
            result = _udpSock->openSocket((char*)_ip, NULL, _port, false ,_interface);
        if (result != E_OK) {
            LOG_ERROR("%s: can't create %s UDP socket on [%s]:%d on interface '%s'",
                _name.c_str(), (_isListen ? "listening" : "connected"), (_isListen ? "NULL" : _ip), _port, _interface[0] == '\0' ? "<default>" : _interface);
            return VMI_E_FAILED_TO_OPEN_SOCKET;
        }
        else 
            LOG_INFO("%s: ok to create %s UDP socket on [%s]:%d on interface '%s'", 
                _name.c_str(), (_isListen?"listening":"connected"), (_isListen?"NULL":_ip),_port, _interface[0]=='\0'?"<default>":_interface);

        // It's a new connection... must wait for next frame to allow frame alignement
        _waitForNextFrame = true;
    }

    //
    // Manage recv of data
    //

    if( _udpSock->isValid() ) {

        if (_waitForNextFrame) {

            // Wait for end of frame packet, for frame alignement
            LOG_INFO("%s: wait for next frame...", _name.c_str());
            int len, result;
            bool bEndOfFrame = false;
            char packet[RTP_MAX_FRAME_LENGTH];
            while (!bEndOfFrame) {
                len = RTP_MAX_FRAME_LENGTH;
                result = _udpSock->readSocket((char*)packet, &len);
                if( !_bStarted )
                    return VMI_E_CONNECTION_CLOSED;
                if (result < 0) {
                    LOG_ERROR("error when read RTP frame: size readed=%d, result=%d", len, result);
                    _udpSock->closeSocket();
                    return VMI_E_FAILED_TO_RCV_SOCKET;
                }
                else if (result == 0) {
                    LOG_INFO("the connection has been gracefully closed");
                    _udpSock->closeSocket();
                    return VMI_E_CONNECTION_CLOSED;
                }
                CRTPFrame frame((unsigned char*)packet, len);
                if (frame.isEndOfFrame()) {
                    bEndOfFrame = true;
                }
            }
            _waitForNextFrame = false;
        }

        // Create a new vMI frame from the udp input connection
        try {
            int result = frame->createFrameFromUDP(_udpSock, _nModuleId);
            if (result != VMI_E_OK) {
                if (result == VMI_E_FAILED_TO_RCV_SOCKET || result == VMI_E_CONNECTION_CLOSED)
                    // Not really an error...
                    _udpSock->closeSocket();
                else
                    LOG_ERROR("errors when trying to get vMI frame...");
                _waitForNextFrame = true;
                return VMI_E_INVALID_FRAME;
            }
        }
        catch (...) {
            LOG_ERROR("Major error when trying to create vMI frame from RTP content... seems data is corrupted. skip this frame!");
            _waitForNextFrame = true;
            return VMI_E_INVALID_FRAME;
        }
    }

    return ret;
}

void CInRTP::start()
{
    LOG("%s: -->", _name.c_str());
    _bStarted = true;
    LOG("%s: <--", _name.c_str());
}

void CInRTP::stop()
{
    LOG("%s: -->", _name.c_str());
    _bStarted = false;
    if (_udpSock && _udpSock->isValid())
    {
        _udpSock->closeSocket();
    }
    CIn::stop();
    LOG("%s: <--", _name.c_str());
}

PIN_REGISTER(CInRTP,"rtp");
