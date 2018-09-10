#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <pins/pins.h>
#include "common.h"
#include "log.h"
#include "tools.h"
#include "rtpframe.h"
#include "tr03frame.h"
#include "tcp_basic.h"

using namespace std;

/**********************************************************************************************
*
* COutTR03
*
***********************************************************************************************/

// Note about the RTP Packet
COutTR03::COutTR03(CModuleConfiguration* pMainCfg, int nIndex) : COut(pMainCfg, nIndex)
{
    LOG("%s: --> <-- ", _name.c_str());
    _nType          = PIN_TYPE_TR03;
    _seq            = 0;
    PROPERTY_REGISTER_MANDATORY("ip", _ip, "");
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_OPTIONAL("mtu", _mtu, 1500);
    //PROPERTY_REGISTER_OPTIONAL("w", _w, 1920);
    //PROPERTY_REGISTER_OPTIONAL("h", _h, 1080);
    //PROPERTY_REGISTER_OPTIONAL("fmt", _depth, 20);
    PROPERTY_REGISTER_OPTIONAL("interface", _interface, "");
    PROPERTY_REGISTER_OPTIONAL("mcastgroup", _mcastgroup, "");
    _isMulticast = !!_mcastgroup[0];
    //_linesize       = _w * _depth / 8;
    //_linepayloadsize = _linesize + TRO3_LINE_HEADERS_LENGTH;
    //_pgroup         = tools::getPPCM(_depth, 8);
    _frameCount     = 0;
    _UDPPacketSize  = _mtu - IP_HEADERS_LENGTH;
    _RTPPacketSize  = _UDPPacketSize - UDP_HEADERS_LENGTH;
    _payloadSize    = _RTPPacketSize - RTP_HEADERS_LENGTH - TRO3_HEADERS_LENGTH;
    LOG_INFO("%s: udp packet size=%d, rtp packet size=%d, payload size=%d", _name.c_str(), _UDPPacketSize, _RTPPacketSize, _payloadSize);
    LOG_INFO("%s: line size=%d, line payload size=%d, pgroup=%d", _name.c_str(), _linesize, _linepayloadsize, _pgroup);
    if( _mtu > RTP_MAX_FRAME_LENGTH ) {
        // TODO: issue
    }
#ifdef USE_NETMAP
    _udpSock = (strncmp(_interface, "netmap-", 7) == 0) ? new Netmap() : new UDP();
#else
    _udpSock = new UDP();
#endif
}

COutTR03::~COutTR03()
{
    if (_udpSock) {
        _udpSock->closeSocket();
        delete _udpSock;
    }
}

int COutTR03::send(CvMIFrame* vmiFrame)
{
    char* buffer = (char*) vmiFrame->getMediaBuffer();
    //LOG_INFO("%s: --> <--  buffer=0x%x", _name.c_str(), buffer);
    int result = E_OK;
    int ret = 0;

    if (buffer == NULL)
        return ret;

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
    LOG("%s: _udpSock->isValid()=%d", _name.c_str(), _udpSock->isValid());
    if( _udpSock->isValid() )
    {
        unsigned char* p = (unsigned char*)buffer;

        CFrameHeaders* headers = vmiFrame->getMediaHeaders();
        _linesize = headers->GetW() * headers->GetDepth() / 8;
        _linepayloadsize = _linesize + TRO3_LINE_HEADERS_LENGTH;
        _pgroup = tools::getPPCM(headers->GetDepth(), 8);

        // Create frames (RTP and TR03) that will be used to transfer this video frame
        CRTPFrame frame(_RTPframe, _RTPPacketSize);
        CTR03Frame* pTR03frame = frame.getTR03Frame();
        pTR03frame->setFormat(headers->GetW(), headers->GetH(), headers->GetDepth() / 8);

        // Iterate to each scanline to encapsulate on TR03 packet
        int bEndOfFrame = false;
        int lineNo = 0;
        int scanlinerest = 0;
        int scanlinetoprocess = headers->GetH();
        while (scanlinetoprocess > 0) {
            int len, marker = 0;

            // Prepare this TR03 frame (analyse how much scanlines or part of scanlines can be stored on this packet)
            // This step is needed to prevent lot of memcpy
            scanlinerest = pTR03frame->prepare(scanlinerest, _linesize, scanlinetoprocess);

            // Add scanline on the packet
            for (int i = 0; i < pTR03frame->getScanLineNb(); i++) {
                bool isComplete = pTR03frame->addScanLine(p, lineNo, _linesize);
                if (isComplete) {
                    scanlinetoprocess--;
                    lineNo++;
                    p += _linesize;
                }
            }

            // write RTP packet header. Not that the TR03 headers part has been updated when addScanLine
            if (scanlinetoprocess == 0)
                marker = 1;
            pTR03frame->writeHeader(_seq);
            frame.writeHeader(_seq, marker, 98);
            //pTR03frame->dumpHeader();

            // Send the packet
            len = _RTPPacketSize;
            result = _udpSock->writeSocket((char*)_RTPframe, &len);
            if (result != -1) {
                LOG("%s: write (size=%d) to socket, result=%d, RTP packet #%d, frame #%d, scanlinetoprocess=%d",
                    _name.c_str(), len, result, frame._seq, _frameCount, scanlinetoprocess);
            }
            else {
                LOG_ERROR("%s: error write (size=%d) to socket, result=%d, RTP packet #%d, frame #%d, scanlinetoprocess=%d",
                    _name.c_str(), len, result, frame._seq, _frameCount, scanlinetoprocess);
                ret = -1;
            }

            // Update the packet seq number
            _seq = (_seq+1) % MAX_UNSIGNED_INT32;
        }

        _frameCount++;
    }

    return ret;
}

bool COutTR03::isConnected()
{
    return _udpSock->isValid();
}

PIN_REGISTER(COutTR03,"tr03")
