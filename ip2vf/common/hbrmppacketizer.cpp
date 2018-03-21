#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cmath>

#include "common.h"
#include "error.h"
#include "log.h"
#include "tools.h"
#include "hbrmppacketizer.h"

using namespace std;

/**********************************************************************************************
*
* CHBRMPPacketizer
*
***********************************************************************************************/

CHBRMPPacketizer::CHBRMPPacketizer() {

    _RTPPacketSize    = RTP_PACKET_SIZE;
    _HBRMPPacketSize  = _RTPPacketSize - RTP_HEADERS_LENGTH;
    _HBRMPPayloadSize = _HBRMPPacketSize - HBRMP_HEADERS_LENGTH;
    _seq = 0;
    _hbrmpTimestamp = 0;
    _frameCount = 0;
}

void CHBRMPPacketizer::setProfile(CSMPTPProfile*  profile) {

    if (profile)
        _profile.setProfile(profile->getProfileName().c_str());
}

int CHBRMPPacketizer::send(UDP* sock, char* mediabuffer, int mediabuffersize, int payloadtype) {

    if (_profile.getStandard() == SMPTE_NOT_DEFINED) {
        LOG_ERROR("SMPTE profile not initialized...");
        return VMI_E_BAD_INIT;
    }

    //LOG_INFO("rtp packet size=%d, hbrmp packet size=%d, payload size=%d", _RTPPacketSize, _HBRMPPacketSize, _HBRMPPayloadSize);

    if (sock && sock->isValid())
    {
        CRTPFrame rtpFrame((unsigned char*)_RTPframe, _RTPPacketSize);
        CHBRMPFrame hbrmpFrame((unsigned char*)_RTPframe + RTP_HEADERS_LENGTH, _RTPPacketSize - RTP_HEADERS_LENGTH);
        hbrmpFrame.initFixedHBRMPValuesFromProfile(&_profile);

        int bEndOfFrame = false;
        unsigned int remainingLen = mediabuffersize;
        unsigned int sentLen = 0;
        unsigned int packetSentNb = 0;
        unsigned int timestamp_ref = _hbrmpTimestamp;
        unsigned int payloadSent = 0;
        unsigned int oldPayloadSent = 0;
        unsigned int oldPixelSent = 0;
        unsigned int nbPacketToSkip = 0;
        char* p = mediabuffer;
        char* packetPayloadPtr = _RTPframe + RTP_HEADERS_LENGTH + HBRMP_HEADERS_LENGTH;

        while (remainingLen>0) {

            int result, len, marker = 0;
            int oldTimestamp = _hbrmpTimestamp;
            oldPayloadSent = payloadSent;

            // First, copy payload on the packet
            int payloadLen = MIN(remainingLen, (unsigned)_HBRMPPayloadSize);
            memcpy(packetPayloadPtr, p, payloadLen);
            remainingLen -= payloadLen;
            p += payloadLen;
            if (remainingLen == 0)
                marker = 1;

            // Manage end of frame stuffing
            if (payloadLen < _HBRMPPayloadSize)
            {
                LOG("padding payload=%d", _HBRMPPayloadSize - payloadLen);
                memset( packetPayloadPtr + payloadLen, 0, _HBRMPPayloadSize - payloadLen);
            }

            // Write correct headers for this packet
            _seq = (_seq + 1) % 65536;
            rtpFrame.writeHeader(_seq, marker, payloadtype);
            hbrmpFrame.writeHeader(_frameCount, _hbrmpTimestamp);

            // Then send UDP packet
            len = _RTPPacketSize;
            result = sock->writeSocket((char*)_RTPframe, &len);
            if (result != -1)
            {
                LOG("write (size=%d) to socket, result=%d, RTP packet #%d, frame #%d, payloadlen=%d, remaining=%d",
                    len, result, rtpFrame._seq, _frameCount, payloadLen,
                    remainingLen);
                sentLen += result;
            }
            else
            {
                LOG_ERROR("error writing (size=%d) to socket, result=%d, RTP packet #%d, frame #%d, payloadlen=%d, remaining=%d",
                    len, result, rtpFrame._seq, _frameCount, payloadLen,
                    remainingLen);
                //ret = -1;
            }
            packetSentNb++;

            // Calculate next timestamp
            payloadSent += payloadLen;
            unsigned int pixelSent = (
                payloadSent == 0 ?
                0 :
                (int)ceil(
                    (double)payloadSent * 8.0 / (10.0 * 2.0)));
            _hbrmpTimestamp = timestamp_ref + pixelSent * 2;
            if (_hbrmpTimestamp < (unsigned)oldTimestamp)
                LOG_INFO("_hbrmpTimestamp loop=%lu/%lu", _hbrmpTimestamp,
                    oldTimestamp);
            oldPixelSent = pixelSent;
        }
        _frameCount = (_frameCount + 1) % 256;
    }
    return VMI_E_OK;
}