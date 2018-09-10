#include <cstdio>
#include <cstdlib>
#include <cstring>      // strerror
#include <string>
#include "yuv.h"        // for conversion
#include <fstream>      // for file saving
#include <iostream>     // for file saving
#include <thread>

#include "common.h"
#include "log.h"
#include "vmiframe.h"
#include "rtpframe.h"
#include "tools.h"

using namespace std;


CvMIFrame::CvMIFrame() {

    _frame_buffer = NULL;
    _media_buffer = NULL;
    _buffer_size  = 0;
    _frame_size   = 0;
    _media_size   = 0;
    _ref_counter  = 0;

    // By default, add a reference because of the caller which create this instance
    addRef();
}

CvMIFrame::CvMIFrame(CFrameHeaders &fh) : CvMIFrame() {

    _fh = fh;
    int frame_size = _fh.GetMediaSize() + CFrameHeaders::GetHeadersLength();
    _init_buffer(frame_size);
    _fh.WriteHeaders(_frame_buffer);
}

CvMIFrame::~CvMIFrame() {
    _reset();
}

void CvMIFrame::_reset() {

    if (_frame_buffer != NULL)
        delete[] _frame_buffer;
    _frame_buffer = NULL;
    _media_buffer = NULL;
    _buffer_size = 0;
    _frame_size = 0;
    _media_size = 0;
}

CvMIFrame & CvMIFrame::operator=(const CvMIFrame &other) {

    this->_ref_counter = 0;
    this->_frame_size = other._frame_size;
    this->_media_size = other._media_size;
    this->_init_buffer(this->_frame_size);
    memcpy(this->_frame_buffer, other._frame_buffer, other._frame_size);
    int result = this->_fh.ReadHeaders(this->_frame_buffer);
    if (result != VMI_E_OK) {
        LOG_ERROR("Incorrect header format... do we have corrupted data?");
    }
    return *this;
}

int CvMIFrame::addRef() {

    std::unique_lock<std::mutex> lock(_mtx);
    _ref_counter++;
    return _ref_counter;
}

int CvMIFrame::releaseRef() {

    std::unique_lock<std::mutex> lock(_mtx);
    _ref_counter--;
    return _ref_counter;
}

int CvMIFrame::_init_buffer(int framesize) {

    if (_frame_buffer == NULL || framesize > _buffer_size) {
        unsigned char* old_frame_buffer = NULL;
        if (_frame_buffer != NULL)
            old_frame_buffer = _frame_buffer;
        LOG_INFO("resize frame from %d to %d bytes", _buffer_size, framesize);
        _buffer_size = framesize;
        _frame_buffer = (unsigned char*)new unsigned char[_buffer_size];
        _media_buffer = (unsigned char*)_frame_buffer + CFrameHeaders::GetHeadersLength();
        if (_frame_buffer == NULL) {
            LOG_ERROR("failed to allocate to %d bytes", _buffer_size);
            _reset();
            return VMI_E_MEM_FAILED_TO_ALLOC;
        }
        if (old_frame_buffer != NULL) {
            // Keep the content of the old mem segment
            memcpy(_frame_buffer, old_frame_buffer, _frame_size);
            delete[] old_frame_buffer;
        }
    }
    _frame_size = framesize;
    _media_size = framesize - CFrameHeaders::GetHeadersLength();

    return VMI_E_OK;
}

void CvMIFrame::memset(int val) {
    
    if (_frame_buffer != NULL) {
        char* p = (char*)_frame_buffer;
        int size = sizeof(int);
        int i = 0;
        while (i < _buffer_size) {
            *((int*)p) = 0;
            p += size;
            i += size;
        }
    }
}


/**
* \brief Create a vMI frame of type video from an smpte frame object
* vMIFrame is released.
*
* \param hFrame handle to the vMIFrame
* \return the current ref counter for the vMIFrame, -1 if not found
*/
int CvMIFrame::createFrameFromSmpteFrame(CSMPTPFrame* smpteframe, SMPTEFRAME_BUFFERS srcBuffer, int moduleId) {

    if (smpteframe == NULL)
        return VMI_E_INVALID_PARAMETER;

    MEDIAFORMAT kindOfBuffer = smpteframe->getMediaBufferType(srcBuffer);

    // Retreive media size
    int media_size = -1;
    switch (kindOfBuffer) {
    case MEDIAFORMAT::VIDEO:
        media_size = smpteframe->getFrameWidth() * smpteframe->getFrameHeight() * 2 /*nb components per pixel*/ * smpteframe->getFrameDepth() / 8; 
        break;
    case MEDIAFORMAT::AUDIO:
        media_size = smpteframe->getFrameSize(); 
        break;
    case MEDIAFORMAT::ANC:
        // TODO
        break;
    default:
        // ERROR
        break;
    }

    int frame_size = media_size + CFrameHeaders::GetHeadersLength();
    _init_buffer(frame_size);

    // Set correctly headers
    switch (kindOfBuffer) {
    case MEDIAFORMAT::VIDEO:
        _fh.InitVideoHeadersFromProfile(smpteframe->getProfile()); 
        break;
    case MEDIAFORMAT::AUDIO:
        _fh.InitAudioHeadersFromSMPTE(AUDIOFMT::L24_PCM, SAMPLERATE::S_48KHz); 
        break;
    case MEDIAFORMAT::ANC:
        // TODO
        break;
    default:
        // ERROR
        break;
    }
    _media_size = smpteframe->extractMediaContent(srcBuffer, (char*)_media_buffer, media_size);
    _fh.SetMediaTimestamp(smpteframe->getTimestamp());
    _fh.SetMediaSize(_media_size);
    _fh.SetFrameNumber(smpteframe->getFrameNumber());
    _fh.SetModuleId(moduleId);
    _fh.WriteHeaders(_frame_buffer);

    return VMI_E_OK;
}

int CvMIFrame::createFrameFromMem(unsigned char* buffer, int buffer_size, int moduleId) {

    if (buffer == NULL)
        return VMI_E_INVALID_PARAMETER;

    int result = _fh.ReadHeaders(buffer);
    if (result != VMI_E_OK) {
        LOG_ERROR("Incorrect header format... do we have corrupted data?");
        return VMI_E_INVALID_FRAME;
    }
    int media_size = _fh.GetMediaSize();
    int frame_size = media_size + CFrameHeaders::GetHeadersLength();
    if (frame_size > buffer_size) {
        // This must not happen
        LOG_ERROR("Invalid frame: frame size (%d) is greater than buffer size (%d)", frame_size, buffer_size);
        return VMI_E_INVALID_FRAME;
    }
    _init_buffer(frame_size);
    memcpy(_frame_buffer, buffer, frame_size);
    _fh.SetModuleId(moduleId);
    _fh.WriteHeaders(_frame_buffer);

    return VMI_E_OK;
}

int CvMIFrame::createFrameUninitialized(int size) {

    int frame_size = size;
    int media_size = frame_size - CFrameHeaders::GetHeadersLength();
    _init_buffer(frame_size);
    _fh.SetMediaSize(_media_size);

    return VMI_E_OK;
}

int CvMIFrame::createFrameFromMediaSize(int size) {

    int media_size = size;
    int frame_size = media_size + CFrameHeaders::GetHeadersLength();
    _init_buffer(frame_size);
    _fh.SetMediaSize(_media_size);

    return VMI_E_OK;
}

int CvMIFrame::createFrameFromTCP(TCP* sock, int moduleId) {

    int result;

    // Some verifications
    if (sock == NULL)
        return VMI_E_INVALID_PARAMETER;

    if (_frame_size == 0)
        _init_buffer(CFrameHeaders::GetHeadersLength());
    if (_frame_buffer == NULL) {
        LOG("Failed to allocate memory for vMI frame");
        return VMI_E_MEM_FAILED_TO_ALLOC;
    }

    // Receive frame headers in first. It allows to know the size of mediaframe that will come after.
    int len = CFrameHeaders::GetHeadersLength();
    result = sock->readSocket((char*)_frame_buffer, &len);
    if (result != VMI_E_OK)
        return VMI_E_FAILED_TO_RCV_SOCKET;
    result = _fh.ReadHeaders(_frame_buffer);
    if (result != VMI_E_OK) {
        LOG_ERROR("Incorrect header format... do we have corrupted data?");
        return VMI_E_INVALID_FRAME;
    }
    _fh.SetModuleId(moduleId);
    _fh.WriteHeaders(_frame_buffer);
    //_fh.DumpHeaders();

    // Second, receive media content
    int media_size = _fh.GetMediaSize();
    int frame_size = media_size + CFrameHeaders::GetHeadersLength();
    _init_buffer(frame_size);
    len = _media_size;
    result = sock->readSocket((char*)_frame_buffer+ CFrameHeaders::GetHeadersLength(), &len);
    if (result != VMI_E_OK)
        return VMI_E_FAILED_TO_RCV_SOCKET;
    return VMI_E_OK;
}

int CvMIFrame::createFrameFromUDP(UDP* sock, int moduleId) {

    /*
     * Some validation...
     */
    
    if (sock == NULL)
        return VMI_E_INVALID_PARAMETER;

    if (_frame_size <= 0)
        _init_buffer(RTP_MAX_FRAME_LENGTH);
    if (_frame_buffer == NULL) {
        LOG_ERROR("Failed to allocate memory for vMI frame");
        return VMI_E_MEM_FAILED_TO_ALLOC;
    }

    /*
     * Keep the first packet. it must contain vMI headers.
     */
    int len, result;
    len = RTP_MAX_FRAME_LENGTH;
    char packet[RTP_MAX_FRAME_LENGTH];
    result = sock->readSocket(packet, &len);
    if (result < 0) {
        LOG_ERROR("error when read RTP frame: size readed=%d, result=%d", len, result);
        return VMI_E_FAILED_TO_RCV_SOCKET;
    }
    else if (result == 0) {
        LOG_INFO("the connection has been gracefully closed");
        return VMI_E_CONNECTION_CLOSED;
    }
    // copy RTP payload on frame buffer
    int payloadLen = len - RTP_HEADERS_LENGTH;
    memcpy((char*)_frame_buffer, packet + RTP_HEADERS_LENGTH, payloadLen);
    // then read headers
    result = _fh.ReadHeaders(_frame_buffer);
    if (result != VMI_E_OK) {
        LOG_INFO("Error on reading vMI headers from first packet!!! Do we lost packet?");
        return VMI_E_INVALID_FRAME;
    }
    _fh.SetModuleId(moduleId);
    _fh.WriteHeaders(_frame_buffer);

    // Get seq number of this first packet
    CRTPFrame frame((unsigned char*)packet, len);
    int lastSeq = frame._seq;

    // verify the size of media content
    int media_size = _fh.GetMediaSize();
    int frame_size = media_size + CFrameHeaders::GetHeadersLength();
    result = _init_buffer(frame_size);
    if (result != VMI_E_OK)
        return VMI_E_INVALID_FRAME;

    /*
     * Now, keep the remaining data...
     */
    //int offset = _headers.GetHeadersLength();
    char* p = (char*)_frame_buffer + payloadLen;
    int remainingLen = _frame_size - payloadLen;
    bool bEndOfFrame = false;

    sock->pktTSctl(1, _fh.GetMediaTimestamp());                 /* PktTS hook */
    while ( !bEndOfFrame ) {

        // First, keep the full RTP frame from the current UDP packet
        len = RTP_MAX_FRAME_LENGTH;
        result = sock->readSocket(packet, &len);
        if (result < 0) {
            LOG_ERROR("error when read RTP frame: size readed=%d, result=%d", len, result);
            return VMI_E_FAILED_TO_RCV_SOCKET;
        }
        else if (result == 0) {
            LOG_INFO("the connection has been gracefully closed");
            return VMI_E_CONNECTION_CLOSED;
        }

        CRTPFrame frame((unsigned char*)packet, len);
        if (frame._seq>lastSeq && frame._seq != lastSeq + 1) {
            LOG_ERROR("lost %d RTP packet (last=%d, current=%d), drop current frame", (frame._seq - (lastSeq + 1)), lastSeq, frame._seq);
            // Drop current frame
            return VMI_E_INVALID_FRAME;
        }
        // LOG_INFO("%s: recv RTP frame: size=%d, seq=%d", _name.c_str(), result, frame._seq);
        lastSeq = frame._seq;

        // Then read RTP payload on frame buffer
        int payloadLen = MIN(remainingLen, len - RTP_HEADERS_LENGTH);
        memcpy(p, packet + RTP_HEADERS_LENGTH, payloadLen);
        p += payloadLen;
        remainingLen -= payloadLen;
        //LOG_INFO("%sread (size=%d) from socket, result=%d, RTP packet #%d, payloadlen=%d, remaining=%d ", (frame.isEndOfFrame()?"####E###":""),len, result, frame._seq, payloadLen, remainingLen);

        if (frame.isEndOfFrame()) {
            if (remainingLen != 0)
                LOG_ERROR("end of frame marker found, but remaining data: remain=%d, RTP packet #%d", remainingLen, frame._seq);
            bEndOfFrame = true;
        }
        else if (remainingLen == 0) {
            LOG_ERROR("no more data to read, but end of frame marker not found... _frame_size=%d, RTP packet #%d", _frame_size, frame._seq);
            return VMI_E_INVALID_FRAME;
        }
    }
    sock->pktTSctl(0, _fh.GetMediaTimestamp());                 /* PktTS hook */

    return VMI_E_OK;
}

int CvMIFrame::createFrameFromHeaders(CFrameHeaders* fh) {

    _fh = *fh;
    int media_size = _fh.GetMediaSize();
    int frame_size = media_size + CFrameHeaders::GetHeadersLength();
    _init_buffer(frame_size);
    LOG("resize to %d", media_size);
    _fh.SetMediaSize(_media_size);
    _fh.WriteHeaders(_frame_buffer);
    return VMI_E_OK;
}

int CvMIFrame::setMediaContent(unsigned char* media_buffer, int media_size) {

    int frame_size = media_size + CFrameHeaders::GetHeadersLength();
    _init_buffer(frame_size);
    memcpy(_media_buffer, media_buffer, media_size);
    _fh.SetMediaSize(_media_size);
    _fh.WriteHeaders(_frame_buffer);
    return VMI_E_OK;
}


int CvMIFrame::copyFrameToMem(unsigned char* buffer, int size) {
    //LOG_INFO("size: frame=%d, media=%d, output=%d, buffer=0x%x", _frame_size, _media_size, size, buffer);
    if (size > _frame_size) {
        LOG_ERROR("ERROR, invalid size (%d>%d)", size, _frame_size);
        return VMI_E_INVALID_PARAMETER;
    }
    memcpy(buffer, _frame_buffer, size);
    return VMI_E_OK;
}

int CvMIFrame::copyMediaToMem(unsigned char* buffer, int size) {
    //LOG_INFO("size: frame=%d, media=%d, output=%d, buffer=0x%x", _frame_size, _media_size, size, buffer);
    if (size > _media_size) {
        LOG_ERROR("ERROR, invalid size (%d>%d)", size, _media_size);
        return VMI_E_INVALID_PARAMETER;
    }
    memcpy(buffer, _media_buffer, size);
    return VMI_E_OK;
}

int CvMIFrame::sendToTCP(TCP* sock) {

    if (sock && sock->isValid())
    {
        int len = _frame_size;
        int result = sock->writeSocket((char*)_frame_buffer, &len);
        if (result != E_OK || len == 0) {
            LOG_ERROR("error writing %d bytes on the TCP socket, result=%d", len, result);
            return VMI_E_FAILED_TO_SND_SOCKET;
        }
    }
    return VMI_E_OK;
}

int  CvMIFrame::_calculate_pixel_size_in_bits() {

    SAMPLINGFMT fmt = _fh.GetSamplingFmt();
    int ret = -1;
    switch (fmt) {
    case SAMPLINGFMT::BGRA:
    case SAMPLINGFMT::RGBA:
        ret = 4 * _fh.GetDepth();
        break;
    case SAMPLINGFMT::BGR:
    case SAMPLINGFMT::RGB:
        ret = 3 * _fh.GetDepth();
        break;
    case SAMPLINGFMT::YCbCr_4_2_2:
        ret = 2 * _fh.GetDepth();
        break;
    default:
        // Not supported
        ret = -1;
        break;
    }
    return ret;
}

bool CvMIFrame::_is_sampling_fmt_supported() {

    SAMPLINGFMT fmt = _fh.GetSamplingFmt();
    return (fmt == SAMPLINGFMT::BGR  ||
            fmt == SAMPLINGFMT::BGRA ||
            fmt == SAMPLINGFMT::RGB  ||
            fmt == SAMPLINGFMT::RGBA ||
            fmt == SAMPLINGFMT::YCbCr_4_2_2);
}

int  CvMIFrame::_refresh_from_headers() {

    // MEdiaheaders are "valid" if media_size is ok (for audio and video), or if w, h, bpp, and smpfmt are valid (for video only)
    if (_fh.GetMediaSize() > 0) {
        int frame_size = _fh.GetMediaSize() + CFrameHeaders::GetHeadersLength();
        _init_buffer(frame_size);
    }
    else if (_fh.GetMediaFormat() == MEDIAFORMAT::VIDEO && _fh.GetW()>0 && _fh.GetH()>0 && _fh.GetDepth()>0 ) {
        if( !_is_sampling_fmt_supported())
            return VMI_E_INVALID_PARAMETER;
        int media_size = _fh.GetW() * _fh.GetH() * _calculate_pixel_size_in_bits() / 8;
        _fh.SetMediaSize(media_size);
        int frame_size = media_size + CFrameHeaders::GetHeadersLength();
        _init_buffer(frame_size);
    }
    else {
        // can't calculate frame size...
        LOG_ERROR("can't calculate frame size... mediasize is invalid, and w,h,bpp too!");
        return VMI_E_INVALID_PARAMETER;
    }
    return VMI_E_OK;
}

void CvMIFrame::set_header(MediaHeader header, void* value) {
    try {
        switch (header) {
        case MODULE_ID:
            _fh.SetModuleId(*static_cast<int*>(value)); break;
        case MEDIA_FRAME_NB:
            _fh.SetFrameNumber(*static_cast<int*>(value)); break;
        case MEDIA_FORMAT:
            _fh.SetMediaFormat(*static_cast<MEDIAFORMAT*>(value)); break;
        case MEDIA_PAYLOAD_SIZE:
            _fh.SetMediaSize(*static_cast<int*>(value));
            // Need to recalculate frame size
            _refresh_from_headers();
            break;
        case MEDIA_TIMESTAMP:
            _fh.SetMediaTimestamp(*static_cast<unsigned int*>(value)); break;
        case VIDEO_WIDTH:
            _fh.SetW(*static_cast<int*>(value));
            // Need to recalculate frame size
            _refresh_from_headers();
            break;
        case VIDEO_HEIGHT:
            _fh.SetH(*static_cast<int*>(value));
            // Need to recalculate frame size
            _refresh_from_headers();
            break;
        case VIDEO_COLORIMETRY:
            _fh.SetColorimetry(*static_cast<COLORIMETRY*>(value)); break;
        case VIDEO_FORMAT:
            _fh.SetSamplingFmt(*static_cast<SAMPLINGFMT*>(value));
            // Need to recalculate frame size
            _refresh_from_headers();
            break;
        case VIDEO_DEPTH:
            _fh.SetDepth(*static_cast<int*>(value));
            // Need to recalculate frame size
            _refresh_from_headers();
            break;
        case AUDIO_NB_CHANNEL:
            _fh.SetChannelNb(*static_cast<int*>(value)); break;
        case AUDIO_FORMAT:
            _fh.SetAudioFmt(*static_cast<AUDIOFMT*>(value)); break;
        case AUDIO_SAMPLE_RATE:
            _fh.SetSampleRate(*static_cast<SAMPLERATE*>(value)); break;
        case AUDIO_PACKET_TIME:
            _fh.SetPacketTime(*static_cast<int*>(value)); break;
        case MEDIA_SRC_TIMESTAMP:
            _fh.SetSrcTimestamp(*static_cast<unsigned long long*>(value)); break;
        case MEDIA_IN_TIMESTAMP:
            _fh.SetInputTimestamp(*static_cast<unsigned long long*>(value)); break;
        case MEDIA_OUT_TIMESTAMP:
            _fh.SetOutputTimestamp(*static_cast<unsigned long long*>(value)); break;
        case NAME_INFORMATION:
            _fh.SetName(static_cast<const char*>(value)); break;
        default:
            break;
        }
    }
    catch (...) {

    }
    if( _frame_buffer != NULL )
        _fh.WriteHeaders(_frame_buffer);
}
void CvMIFrame::get_header(MediaHeader header, void* value) {
    try {
        switch (header) {
        case MODULE_ID:
            *static_cast<int*>(value) = _fh.GetModuleId(); break;
        case MEDIA_FRAME_NB:
            *static_cast<int*>(value) = _fh.GetFrameNumber(); break;
        case MEDIA_FORMAT:
            *static_cast<MEDIAFORMAT*>(value) = _fh.GetMediaFormat(); break;
        case MEDIA_TIMESTAMP:
            *static_cast<unsigned int*>(value) = _fh.GetMediaTimestamp(); break;
        case VIDEO_WIDTH:
            *static_cast<int*>(value) = _fh.GetW(); break;
        case VIDEO_HEIGHT:
            *static_cast<int*>(value) = _fh.GetH(); break;
        case VIDEO_COLORIMETRY:
            *static_cast<COLORIMETRY*>(value) = _fh.GetColorimetry(); break;
        case VIDEO_FORMAT:
            *static_cast<SAMPLINGFMT*>(value) = _fh.GetSamplingFmt(); break;
        case VIDEO_DEPTH:
            *static_cast<int*>(value) = _fh.GetDepth(); break;
        case AUDIO_NB_CHANNEL:
            *static_cast<int*>(value) = _fh.GetChannelNb(); break;
        case AUDIO_FORMAT:
            *static_cast<AUDIOFMT*>(value) = _fh.GetAudioFmt(); break;
        case AUDIO_SAMPLE_RATE:
            *static_cast<SAMPLERATE*>(value) = _fh.GetSampleRate(); break;
        case AUDIO_PACKET_TIME:
            *static_cast<int*>(value) = _fh.GetPacketTime(); break;
        case MEDIA_PAYLOAD_SIZE:
            *static_cast<int*>(value) = _fh.GetMediaSize(); break;
        case VIDEO_FRAMERATE_CODE:
            *static_cast<int*>(value) = _fh.GetFramerateCode(); break;
        case MEDIA_SRC_TIMESTAMP:
            *static_cast<unsigned long long*>(value) = _fh.GetSrcTimestamp(); break;
        case MEDIA_IN_TIMESTAMP:
            *static_cast<unsigned long long*>(value) = _fh.GetInputTimestamp(); break;
        case MEDIA_OUT_TIMESTAMP:
            *static_cast<unsigned long long*>(value) = _fh.GetOutputTimestamp(); break;
        case VIDEO_SMPTEFRMCODE:
            *static_cast<int*>(value) = _fh.GetSmpteframeCode(); break;
        case NAME_INFORMATION:
            *static_cast<const char**>(value) = _fh.GetName(); break;
        default:
            break;
        }
    }
    catch (...) {

    }
}

void CvMIFrame::refreshHeaders() {

    _fh.ReadHeaders(_frame_buffer);
    _refresh_from_headers();
}

void CvMIFrame::writeHeaders() {

    if (_frame_buffer != NULL)
        _fh.WriteHeaders(_frame_buffer);
}


