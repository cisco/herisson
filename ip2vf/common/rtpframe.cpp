#include <cstdio>
#include <cstdlib>
#include <cstring>      // strerror
#include <string>

#include "log.h"
#include "tools.h"
#include "rtpframe.h"

// Based on RFC-3550 (RTP) and SMPTE Standard ST 2022-6
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                           timestamp                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |           synchronization source (SSRC) identifier            |
//   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//

#define HBRM_BYTE(a)    ((unsigned char)hbrm_payload_header[a])
#define SMPTE_FREQUENCY 27000000    // 27 MHz

double CRTPFrame::_timestampbase = 0.0f;

CRTPFrame::CRTPFrame() {

    _frame = NULL;
    _v = 0;
    _p = 0;
    _x = 0;
    _cc = 0;
    _m = 0;
    _pt = 0;
    _seq = 0;
    _timestamp = 0;
    _ssrc = 0; // 0x12345600;
    _framelen = 0;
}

CRTPFrame::CRTPFrame(const unsigned char* frame, int len) : CRTPFrame() {
    _frame = (unsigned char*)frame;
    _framelen = len;
    extractData();
}

CRTPFrame::~CRTPFrame() {
}

unsigned int CRTPFrame::getTimestamp() {
    if (_timestampbase == 0.0f)
        _timestampbase = tools::getCurrentTimeInS();
    unsigned int timestamp = (unsigned int)(SMPTE_FREQUENCY*(tools::getCurrentTimeInS() - _timestampbase));
    //LOG_INFO("timestamp  = %u, cur=%f, base=%f", timestamp, tools::getCurrentTimeInS(), _timestampbase);
    return timestamp;
}

void CRTPFrame::dumpHeader()
{
    LOG_INFO("RTPFrame #%08d, ext=%d, v=%02d, type=%02d, marker=%d, t=%lu, SSRC=0x%x", _seq, _x, _v, _pt, _m, _timestamp, _ssrc);

    LOG("version    = %d", _v);
    LOG("marker     = %d", _m);
    LOG("pt         = %d", _pt);
    LOG("seq        = %d", _seq);
    LOG("timestamp  = %u", _timestamp);
    LOG("_ssrc      = %u", _ssrc);
}

void CRTPFrame::extractData() {

    if (_frame == NULL)
        return;
    try {
        //LOG_DUMP(_frame, RTP_HEADERS_LENGTH);
        _v  = (_frame[0] & 0b11000000) >> 6;
        _p  = (_frame[0] & 0b00100000) >> 5;
        _x  = (_frame[0] & 0b00010000) >> 4;
        _cc = (_frame[0] & 0b00001111);
        _m  = (_frame[1] & 0b10000000) >> 7;
        _pt = (_frame[1] & 0b01111111);
        _seq= (_frame[2] << 8) + _frame[3];
        _timestamp = (_frame[4] << 24) + (_frame[5] << 16) + (_frame[6] << 8)  + _frame[7];
        _ssrc      = (_frame[8] << 24) + (_frame[9] << 16) + (_frame[10] << 8) + _frame[11];
    }
    catch (...) {
        LOG_ERROR("Major error when reading RTP headers... seems data is corrupted. skip this frame!");
        return;
    }
}

void CRTPFrame::writeHeader(int seq, int marker, int pt) {

    // according to the rfc 3550, _timestamp is 27mHz clock
    if (_frame == NULL)
        return;
    try {
        _timestamp = 0;
        _seq = seq;
        _m = marker;
        memset(_frame, 0, RTP_HEADERS_LENGTH);
        _frame[0]  = (2 << 6);
        _frame[1]  = (_m << 7);
        _frame[1] |= (pt & 0b01111111);
        _frame[2]  = _seq >> 8;
        _frame[3]  = _seq & 0b11111111;
        _frame[4]  = (_timestamp >> 24) & 0b11111111;
        _frame[5]  = (_timestamp >> 16) & 0b11111111;
        _frame[6]  = (_timestamp >> 8) & 0b11111111;
        _frame[7]  = _timestamp & 0b11111111;
        _frame[8]  = (_ssrc >> 24) & 0b11111111;
        _frame[9]  = (_ssrc >> 16) & 0b11111111;
        _frame[10] = (_ssrc >> 8) & 0b11111111;
        _frame[11] = _ssrc & 0b11111111;
    }
    catch (...) {
        LOG_ERROR("Major error when writing RTP headers... seems data is corrupted. skip this frame!");
        return;
    }
}

void CRTPFrame::overrideSeqNumber(int seq) {

    _seq = seq;
    if (_frame == NULL)
        return;
    try {
        _frame[2] = _seq >> 8;
        _frame[3] = _seq & 0b11111111;
    }
    catch (...) {
        LOG_ERROR("Major error when overriding seq number... seems data is corrupted. skip this frame!");
        return;
    }
}

