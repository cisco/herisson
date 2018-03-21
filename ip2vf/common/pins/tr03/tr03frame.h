#ifndef _TR03FRAME_H
#define _TR03FRAME_H

#include <vector>

#include "rtpframe.h"

#define TRO3_HEADERS_LENGTH         2      // bytes: it's the Extended Sequence Number only
#define TRO3_LINE_HEADERS_LENGTH    6      // bytes

struct ScanLine {
    int lineNo;         // No of the scanline
    int dataoffset;     // Offset of data on the current scanline
    int pixeloffset;
    int packoffset;     // Offset of data on the current payload packet
    int datalen;        // len of data
    int rest;           // rest of data
    ScanLine() {
        lineNo      = -1;
        dataoffset  = 0;
        packoffset  = 0;
        datalen     = 0;
        rest        = 0;
    };
};

class CTR03Frame
{
public:
    unsigned char* _frame;
    unsigned int   _extseq;
    int            _framelen;          // it's the total available len, i.e. headers+payload
    int            _usedlen;
    unsigned char* _payload;
    int            _line;
    int            _w;
    int            _h;
    int            _depth;

    std::vector<ScanLine> _scanlines;

public:
    CTR03Frame();
    CTR03Frame(const unsigned char* frame, int len);
    ~CTR03Frame();

public:
    void setBuffer(const unsigned char* frame, int len) {
        _frame = (unsigned char*)frame; 
        _framelen = len;
    };
    void setFormat(int w, int h, int depth) {
        _w = w;
        _h = h;
        _depth = depth;
    };
    int prepare(int remainingscanlinelen, int scanlinelen, int remainingline);
    int getScanLineNb() {
        return (int)_scanlines.size();
    };
    bool addScanLine(const unsigned char* linedata, int lineNo, int size);
    void dumpHeader();
    void readHeader() { 
        extractData(); 
    };
    void writeHeader(unsigned int seq);
    
private:
    void extractData();
};



#endif //_TR03FRAME_H
