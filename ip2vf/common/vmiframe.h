#ifndef _VMIFRAME_H
#define _VMIFRAME_H


#include "common.h"
#include "frameheaders.h"
#include "./pins/st2022/smpteframe.h"
#include "tcp_basic.h"
#include "libvMI.h"

/*
*  Contain a single vMIFrame
*/

class CvMIFrame
{
    unsigned char* _frame_buffer;
    unsigned char* _media_buffer;
    int            _buffer_size;
    int            _frame_size;
    int            _media_size;
    CFrameHeaders  _fh;

    int            _ref_counter;
    std::mutex     _mtx;

public:
    CvMIFrame();
    CvMIFrame(CFrameHeaders &fh);
    ~CvMIFrame();

    void _reset();
    int  _init_buffer(int framesize);
    int  _refresh_from_headers();
    bool _is_sampling_fmt_supported();
    int  _calculate_pixel_size_in_bits();

public:
    CvMIFrame & operator=(const CvMIFrame &other);

    // Accesseurs
    unsigned char* getMediaBuffer() { return _media_buffer; };
    unsigned char* getFrameBuffer() { return _frame_buffer; };
    int getMediaSize() { return _media_size; };
    int getFrameSize() { return _media_size + CFrameHeaders::GetHeadersLength(); };
    CFrameHeaders* getMediaHeaders() { return &_fh; };
    void memset(int val);

    // Ref counter management
    int addRef();
    int releaseRef();
    int getRef() { return _ref_counter; };

    // Frame creation
    int createFrameFromSmpteFrame(CSMPTPFrame* smpteframe, SMPTEFRAME_BUFFERS srcBuffer, int moduleId);
    int createFrameFromMem(unsigned char* buffer, int buffer_size, int moduleId);
    int createFrameUninitialized(int size);
    int createFrameFromMediaSize(int size);
    int createFrameFromTCP(TCP* sock, int moduleId);
    int createFrameFromUDP(UDP* sock, int moduleId);
    int createFrameFromHeaders(CFrameHeaders* fh);

    // Media content management
    int setMediaContent(unsigned char* media_buffer, int media_size);

    // Frame export
    int copyFrameToMem(unsigned char* buffer, int size);
    int copyMediaToMem(unsigned char* buffer, int size);
    int sendToTCP(TCP* sock);

    // HEaders management
    void set_header(MediaHeader header, void* value);
    void get_header(MediaHeader header, void* value);
    void refreshHeaders();
    void writeHeaders();
};

#endif //_VMIFRAME_H
