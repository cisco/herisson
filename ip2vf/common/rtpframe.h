#ifndef _RTPFRAME_H
#define _RTPFRAME_H

#include <pins/st2022/hbrmpframe.h>
#include <pins/tr03/tr03frame.h>

#define RTP_MAX_FRAME_LENGTH    65535   // in case of Jumbo frames
#define RTP_HEADERS_LENGTH      12      // in bytes
#define IP_HEADERS_LENGTH       20
#define UDP_HEADERS_LENGTH      8

class CRTPFrame
{
public:
    unsigned char* _frame;

    int _v;
    int _p;
    int _x;
    int _cc;
    int _m;
    int _pt;
    int _seq;
    unsigned int _timestamp;
    static double _timestampbase;
    unsigned int _ssrc;
    int _framelen;

public:
    CRTPFrame();
    CRTPFrame(const unsigned char* frame, int len);
    ~CRTPFrame();

private:
    unsigned int getTimestamp();

public:
    void setBuffer(const unsigned char* frame, int len) {
        _frame =(unsigned char*)frame; 
        _framelen = len;
    };
    void dumpHeader();
    void readHeader() { 
        extractData(); 
    };
    void writeHeader(int seq, int marker, int pt);
    void overrideSeqNumber(int seq);
    bool isEndOfFrame() { 
        return _m==1; 
    };

    CHBRMPFrame* getHBRMPFrame() {
        return new CHBRMPFrame((_frame + RTP_HEADERS_LENGTH), _framelen- RTP_HEADERS_LENGTH);
    };
    CTR03Frame* getTR03Frame() {
        return new CTR03Frame((_frame + RTP_HEADERS_LENGTH), _framelen - RTP_HEADERS_LENGTH);
    };

    
    /**
     * @return the actual bytes of the original RTP data packet
     */
    unsigned char *getData(){
        return _frame;
    }
    
    /**
     * @return the size of the rtp packet
     */
    unsigned int getSize(){
        return this->_framelen;
    }

private:
    void extractData();
};

#endif //_RTPFRAME_H
