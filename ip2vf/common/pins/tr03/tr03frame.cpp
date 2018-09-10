#include <cstdio>
#include <cstdlib>
#include <cstring>      // strerror
#include <string>

#include "log.h"
#include "rtpframe.h"
#include "tr03frame.h"

// Based on RFC-4175 (RTP) and VSF_TR-03_DRAFT_2015-10-19.pdf
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    | V |P|X|   CC  |M|    PT       |       Sequence Number         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                           Time Stamp                          |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                             SSRC                              |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |   Extended Sequence Number    |            Length             |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |F|          Line No            |C|           Offset            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |            Length             |F|          Line No            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |C|           Offset            |                               .
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               .
//    .                                                               .
//    .                 Two (partial) lines of video data             .
//    .                                                               .
//    +---------------------------------------------------------------+
//

CTR03Frame::CTR03Frame() {
    _frame      = NULL;
    _extseq     = 0;
    _framelen   = 0;
    _usedlen    = 0;
    _w          = -1;
    _h          = -1;
    _depth      = -1;
}

CTR03Frame::CTR03Frame(const unsigned char* frame, int len) : CTR03Frame() {
    _frame = (unsigned char*)frame;
    _framelen = len;
}

CTR03Frame::~CTR03Frame() {
}

/*
    The goal of "prepare()" is to optimise the TR03 frame creation: it will calculate how lines we can store in this packet,
    and it will save space for corresponding headers.
    We can do not use this, but in this case we have to store temporaly the data in a separate buffer waiting to know
    how line we have, and then write the full frame at the end of the process: we have several additional memcpy to do.

    will return the size on the scanline which doesn't go in the current packet (will be re-use as "remainingscanlinelen"
    for next packet). 0 mean that we begin at the start of a scanline for next packet.
*/
int CTR03Frame::prepare(int remainingscanlinelen, int scanlinelen, int remainingline) {
    if (_scanlines.size() > 0)
        _scanlines.clear();

    int firstscanlineoffset = (remainingscanlinelen==0) ? 0 : scanlinelen - remainingscanlinelen;
    int freeframeLen = _framelen - TRO3_HEADERS_LENGTH;
    int linepayloadsize = scanlinelen + TRO3_LINE_HEADERS_LENGTH;
    int line = 0;
    int remain = 0;
    int scanlinerest = 0;
    int packoffset = 0;
    //LOG("prepare TR03: rest=%d, framelen=%d, scanline=%d", remainingscanlinelen, _framelen, scanlinelen);
    while (freeframeLen > TRO3_LINE_HEADERS_LENGTH && remainingline > 0)
    {
        ScanLine scanline;

        scanline.dataoffset = (line == 0 ? firstscanlineoffset : 0);
        int curlinepayloadsize = linepayloadsize - scanline.dataoffset;
        //LOG("    f=%d, curlinepayloadsize=%d, freeframeLen=%d", firstscanlineoffset, curlinepayloadsize, freeframeLen);
        if (curlinepayloadsize > freeframeLen) {
            // Can't store a full scanline
            curlinepayloadsize = freeframeLen;
        }
        scanline.datalen = curlinepayloadsize - TRO3_LINE_HEADERS_LENGTH;
        scanline.rest = scanlinelen - scanline.dataoffset - scanline.datalen;
        scanline.packoffset = packoffset;
        scanline.pixeloffset = scanline.dataoffset / 2;
        packoffset += scanline.datalen;
        scanlinerest = scanline.rest;
        //LOG("...[%d], [%d,%d], rest=%d", _scanlines.size(), scanline.offset, scanline.len, scanline.rest);

        _scanlines.push_back(scanline);
        freeframeLen -= curlinepayloadsize;
        line++;
        remainingline--;
    }
    _payload = _frame + TRO3_HEADERS_LENGTH + TRO3_LINE_HEADERS_LENGTH * _scanlines.size();
    _line = 0;
    //LOG("...nbligne=%d, rest=%d, payload=*+%d", _scanlines.size(), scanlinerest, (int)(_payload- _frame));

    return scanlinerest;
}

/*
Add scanline data on the packet. Note that this packet must be previously "prepared".
Will also update TR03 headers for this scanline
*/
bool CTR03Frame::addScanLine(const unsigned char* linedata, int lineNo, int size) 
{
    if (_line >= _scanlines.size()) {
        LOG_ERROR("exceed scan line (add no%d, avail=%d)", _line, _scanlines.size());
        return 0;
    }

    LOG("Add line %d (offset=%d, len=%d, rest=%d)", lineNo, _scanlines[_line].dataoffset, _scanlines[_line].datalen, _scanlines[_line].rest);

    bool bLastLineInPacket = (_line == (_scanlines.size() - 1));
    bool bLineIsCompleted = (_scanlines[_line].rest == 0);

    // Set the current line nb
    _scanlines[_line].lineNo = lineNo;

    // Copy the correct scanline data at the correct place on the packet
    memcpy(_payload + _scanlines[_line].packoffset, linedata + _scanlines[_line].dataoffset, _scanlines[_line].datalen);

    // Set correct headers
    int headerOffset = TRO3_HEADERS_LENGTH + _line*TRO3_LINE_HEADERS_LENGTH;
    _frame[headerOffset]    = (_scanlines[_line].datalen >> 8) & 0b11111111;
    _frame[headerOffset+1]  = (_scanlines[_line].datalen) & 0b11111111;
    _frame[headerOffset+2]  = 0;
    _frame[headerOffset+2] |= (lineNo >> 8) & 0b01111111;
    _frame[headerOffset+3]  = lineNo & 0b11111111;
    _frame[headerOffset+4]  = 0;
    _frame[headerOffset+4] |= ((bLastLineInPacket?0:1) << 7) & 0b10000000;
    _frame[headerOffset+4] |= (_scanlines[_line].pixeloffset >> 8) & 0b01111111;
    _frame[headerOffset+5]  = (_scanlines[_line].pixeloffset) & 0b11111111;

    _line++;

    return bLineIsCompleted;
}

void CTR03Frame::dumpHeader()
{
    int cc = 1;
    int offset = 0;
    int esn = (_frame[offset] << 8) + _frame[offset + 1];
    offset += 2;
    LOG("esn = %d", esn);
    while (cc == 1) {
        int len = (_frame[offset] << 8) + _frame[offset + 1];
        offset += 2;
        int f = (_frame[offset] & 0b10000000) >> 7;
        int lineno = ((_frame[offset] & 0b01111111) << 8) + _frame[offset + 1];
        offset += 2;
        cc = (_frame[offset] & 0b10000000) >> 7;
        int off = ((_frame[offset] & 0b01111111) << 8) + _frame[offset + 1];
        offset += 2;
        LOG("line[%03d].len    = %d", lineno, len);
        LOG("line[%03d].f      = %d", lineno, f);
        LOG("line[%03d].cc     = %d", lineno, cc);
        LOG("line[%03d].offset = %d", lineno, off);
    }
}

void CTR03Frame::extractData() {
    _extseq = (_frame[0] << 24) + (_frame[1] << 16);
}

void CTR03Frame::writeHeader(unsigned int seq) {
    _frame[0] = (seq >> 24) & 0b11111111;
    _frame[1] = (seq >> 16) & 0b11111111;
}


