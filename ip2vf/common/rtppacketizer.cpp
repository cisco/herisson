#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>       // std::this_thread

#include "common.h"
#include "error.h"
#include "log.h"
#include "tools.h"
#include "packetizer.h"

using namespace std;

/**********************************************************************************************
*
* CRTPPacketizer
*
***********************************************************************************************/

CRTPPacketizer::CRTPPacketizer(int mtu) : CUDPPacketizer(mtu) {

    _UDPPacketSize = _mtu - IP_HEADERS_LENGTH;
    _RTPPacketSize = _UDPPacketSize - UDP_HEADERS_LENGTH;
    _RTPPayloadSize = _RTPPacketSize - RTP_HEADERS_LENGTH;
    _seq = 0;
    _payloadtype = 98;
}

int CRTPPacketizer::send(UDP* sock, char* buffer, int buffersize) {

    if (sock && sock->isValid())
    {
        CRTPFrame frame((unsigned char*)_RTPframe, _RTPPacketSize);
        int bEndOfFrame = false;
        int remainingLen = buffersize;
        unsigned int packetSentNb = 0;
        char* p = buffer;

        while (remainingLen>0) {

            int len, result, marker = 0;

            // First, construct the full RTP frame
            int payloadLen = MIN(remainingLen, _RTPPayloadSize);
            memcpy(_RTPframe + RTP_HEADERS_LENGTH, p, payloadLen);
            remainingLen -= payloadLen;
            p += payloadLen;
            if (remainingLen == 0)
                marker = 1;
            frame.writeHeader(_seq, marker, _payloadtype);
            _seq = (_seq + 1) % 65536;
            if (payloadLen < _RTPPayloadSize) {
                LOG("padding payload=%d", _RTPPayloadSize - payloadLen);
                ::memset(_RTPframe + RTP_HEADERS_LENGTH + payloadLen, 0, _RTPPayloadSize - payloadLen);
            }
            //frame.dumpHeader((char*)_RTPframe);

            // Then send UDP packet
            len = _RTPPacketSize;
            result = sock->writeSocket(_RTPframe, &len);
            if (result != -1) {
                LOG("write (size=%d) to socket, result=%d, RTP packet #%d, payloadlen=%d, remaining=%d",
                    len, result, frame._seq, payloadLen, remainingLen);
            }
            else {
                LOG_ERROR("error write (size=%d) to socket, result=%d, RTP packet #%d, payloadlen=%d, remaining=%d",
                    len, result, frame._seq, payloadLen, remainingLen);
                return VMI_E_FAILED_TO_SND_SOCKET;
            }

            // Micro pacing
            packetSentNb++;
            if (packetSentNb % 500 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
    return VMI_E_OK;
}

