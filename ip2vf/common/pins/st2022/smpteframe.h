#ifndef _SMPTEFRAME_H
#define _SMPTEFRAME_H

#define SMPTE_PACKET_LENGTH    1376   // in case of Jumbo frames

#include <vector>
#include <thread>

#include "smpteprofile.h"
#include "queue.h"
#include "common.h"
#include "libvMI.h"


enum SMPTEFRAME_BUFFERS {
    BUFFER_NONE     = 0,
    VIDEO_BUFFER_0 = 1,     // from _frame
    AUDIO_BUFFER_0,         // from _frame
    ANC_BUFFER_0,           // from _frame
    VIDEO_BUFFER_1,         // from _halfframe1
    AUDIO_BUFFER_1,         // from _halfframe1
    ANC_BUFFER_1,           // from _halfframe1
    VIDEO_BUFFER_2,         // from _halfframe2
    AUDIO_BUFFER_2,         // from _halfframe2
    ANC_BUFFER_2,           // from _halfframe2
};

class CRTPFrame;

class CSMPTPFrame
{
private:
    unsigned char* _frame;
    unsigned char* _writer;
    int     _actualframelen;
    int     _completeframelen;  // Frame length with padding
    int     _activeframelen;    // Frame length without padding: i.e. real Media Octets per frame
    int     _nbPacket;
    bool    _waitForNextFrame;
    bool    _formatDetected;
    int     _lastSeq;
    int     _lastFc;
    bool    _firstPacket;
    bool    _firstFrame;
    int     _frameCounter;
    bool    _bFrameComplete;
    bool    _bIncludeAudio;
    bool    _bVideoOnly;
    int     _nPadding;
    unsigned int    _timestamp;
    INTERLACED_MODE _audioFmt;
    CSMPTPProfile   _profile;
    CQueue<int>     _q;

    // SMPTE 3GBits specific
    bool           _isDemultiplexed;
    unsigned char* _halfframe1;
    unsigned char* _halfframe2;
    std::vector<std::thread> _workers;

public:
    CSMPTPFrame();
    ~CSMPTPFrame();

public:
    void initNewFrame();
    void resetFrame();
    void addRTPPacket(CRTPFrame* pPacket);
    void abortCurrentFrame();
    void insertAudioContentToSMPTEFrame(unsigned char* buffer, int size);
    void insertVideoContentToSMPTEFrame(char* buffer);
    void convertVideoFrameTo8Bits(unsigned char* pOutputBuffer);
    bool isComplete() {
        return _bFrameComplete;
    };
    int getFrameSize() {
        return _completeframelen - _nPadding;
    };
    int getBufferSize() {
        return _completeframelen;
    };
    int getFrameNumber() {
        return _frameCounter;
    };
    void setFrameNumber(int frameCounter) {
        _frameCounter = frameCounter;
    };
    unsigned int getTimestamp() {
        return _timestamp;
    };
    unsigned char* getBuffer() {
        return _frame;
    };
    void injectFrameData(unsigned char* buffer, int bufferLen);
    void dropFrame() {
        _reset();
    }
    void prepareFrame();
    /*void setInterlacedMode(bool flag) {
        _interlaced = flag;
    }*/
    void setVideoOnlyMode(bool flag) {
        _bVideoOnly = flag;
    }
    int     getFrameWidth()  { return _profile.getActiveWidth();      };
    int     getFrameHeight() { return _profile.getActiveHeight();     };
    int     getFrameDepth()  { return _profile.getFrameDepth();       };
    bool    isMultiplexed()  { return _profile.isMultiplexed();       };
    SMPTE_STANDARD getStandard() { return _profile.getStandard(); };
    SMPTE_STANDARD setProfile(const char* format);
    SMPTE_STANDARD setProfile(CSMPTPProfile profile);
    CSMPTPProfile* getProfile() { return &_profile; };

    void    dumpVideoBuffer(char* filename);
    void    dumpSMPTEFullFrame(const char* filename);
    void    compareSMPTEFrame(const char* filename1, const char* filename2);
    void    loadFromFile(const char* filename);

    std::deque<SMPTEFRAME_BUFFERS>     _qAvailableBuffers;
    int     getNbOfMediaBuffer() { return (int)_qAvailableBuffers.size();  };
    SMPTEFRAME_BUFFERS getNextAvailableMediaBuffer();
    static MEDIAFORMAT getMediaBufferType(SMPTEFRAME_BUFFERS buffer);
    int  extractMediaContent(SMPTEFRAME_BUFFERS buffer, char* pOutputBuffer, int sizeOfOutputBuffer);

    CSMPTPFrame& operator=(const CSMPTPFrame& other);

private:
    void _firstFrameInit(int completeFrameSize);
    void _reset();
    int  _calculatePadding(unsigned char* buffer, int bufferLen);
    void _dumpYUVBufferAsRGB(unsigned char* pOutputBuffer);
    int  _detectEAV10bits(unsigned char* buffer, int size8bits);
    int  _extractChannelValue(unsigned char* buffer, int startpos10bits);
    bool _detectFormat();
    int  _extractSMPTEVideoContent(char* pOutputBuffer, int sizeOfOutputBuffer, unsigned char* src);
    int  _extractSMPTEAudioContent(char* pOutputBuffer, int sizeOfOutputBuffer, unsigned char* src);
    int  _demuxSMPTE425MBDLFrame();
    bool _checkAndValidate(int fullFrameLen);
    void _analyse();
    void _computeCRC();
    int  _getXYZ(bool isEAV, int lineIdx);

    static void _demux_process(CSMPTPFrame* frame, int in, int out, int count);
};

#endif //_SMPTEFRAME_H
