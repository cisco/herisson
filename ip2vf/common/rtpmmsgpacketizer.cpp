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
* CRTPmmsgPacketizer
*
***********************************************************************************************/

CRTPmmsgPacketizer::CRTPmmsgPacketizer(int mtu) : CUDPPacketizer(mtu) {

    for (int i = 0; i < MMSGPACKETIZER_NB_SEGMENTS; i++ )
        _RTPframe[i] = new char[RTP_MAX_FRAME_LENGTH];
    _UDPPacketSize = _mtu - IP_HEADERS_LENGTH;
    _RTPPacketSize = _UDPPacketSize - UDP_HEADERS_LENGTH;
    _RTPPayloadSize = _RTPPacketSize - RTP_HEADERS_LENGTH;
    _seq = 0;
    _payloadtype = 98;
}

CRTPmmsgPacketizer::~CRTPmmsgPacketizer() {
    for (int i = 0; i < MMSGPACKETIZER_NB_SEGMENTS; i++)
        delete [] _RTPframe[i];
}

int CRTPmmsgPacketizer::send(UDP* sock, char* buffer, int buffersize) {

    if (sock && sock->isValid())
    {
        CRTPFrame frame[MMSGPACKETIZER_NB_SEGMENTS];
        int len[MMSGPACKETIZER_NB_SEGMENTS];
        for (int i = 0; i < MMSGPACKETIZER_NB_SEGMENTS; i++) {
            frame[i].setBuffer((unsigned char*)_RTPframe[i], _RTPPacketSize);
            len[i] = _RTPPacketSize;
        }

        int bEndOfFrame = false;
        int remainingLen = buffersize;
        unsigned int packetSentNb = 0;
        char* p = buffer;

        while (remainingLen>0) {

            int result, marker = 0;

            // First, construct the full RTP frame
            int nbSegments = 0;
            for (int i = 0; i < MMSGPACKETIZER_NB_SEGMENTS; i++) {
                nbSegments++;
                int payloadLen = MIN(remainingLen, _RTPPayloadSize);
                memcpy(_RTPframe[i] + RTP_HEADERS_LENGTH, p, payloadLen);
                remainingLen -= payloadLen;
                p += payloadLen;
                if (remainingLen == 0)
                    marker = 1;
                frame[i].writeHeader(_seq, marker, _payloadtype);
                _seq = (_seq + 1) % 65536;
                if (payloadLen < _RTPPayloadSize) {
                    LOG("padding payload=%d", _RTPPayloadSize - payloadLen);
                    ::memset(_RTPframe + RTP_HEADERS_LENGTH + payloadLen, 0, _RTPPayloadSize - payloadLen);
                    break;
                }
            }
            //frame.dumpHeader((char*)_RTPframe);

            // Then send UDP packet
            result = sock->writeBatchedSocket(_RTPframe, len, nbSegments);
            if (result != -1) {
                LOG("write (size=%d) to socket, result=%d, remaining=%d", nbSegments, result, remainingLen);
            }
            else {
                LOG_ERROR("error write (size=%d) to socket, result=%d, remaining=%d",
                    nbSegments, result, remainingLen);
                return VMI_E_FAILED_TO_SND_SOCKET;
            }

            // Micro pacing
            packetSentNb += nbSegments;
            if (packetSentNb % 512 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
    return VMI_E_OK;
}
