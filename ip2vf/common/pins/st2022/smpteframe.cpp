#include <cstdio>
#include <cstdlib>
#include <cstring>      // strerror
#include <string>
#include "yuv.h"        // for conversion
#include <fstream>      // for file saving
#include <iostream>     // for file saving
#include <thread>

#include <pins/st2022/hbrmpframe.h>
#include "rtpframe.h"
#include "common.h"
#include "log.h"
#include "smpteframe.h"
#include "smptecrc.h"
#include "tools.h"
#include "audiopacket.h"

using namespace std;

#define SMPTE_MEDIA_PACKET_SIZE   1375
#define COMPUTE_CRC_FLAG          false     // Flag to enabled/disabled the CRC computation when create a SMPTE frame (remux feature) 

                        
int EAV_DoubleChannel[6] = { 0x3ff, 0x3ff, 0x000, 0x000, 0x000, 0x000 };
int len_EAV_DoubleChannel = sizeof(EAV_DoubleChannel) / sizeof(int);
int EAV_SingleChannel[3] = { 0x3ff, 0x000, 0x000 };
int len_EAV_SingleChannel = sizeof(EAV_SingleChannel) / sizeof(int);

CSMPTPFrame::CSMPTPFrame() {
    _frame              = NULL;
    _writer             = NULL; 
    _halfframe1         = NULL;
    _halfframe2         = NULL;
    _isDemultiplexed    = false;
    _actualframelen     = 0;
    _completeframelen   = 0;        // Frame length with padding
    _activeframelen     = 0;        // Frame length without padding: i.e. real Media Octets per frame
    _nbPacket           = 0;
    _lastSeq            = -1;
    _lastFc             = -1;
    _firstPacket        = true;
    _firstFrame         = true;
    _frameCounter       = 0;
    _waitForNextFrame   = true;
    _bFrameComplete     = false;
    _bIncludeAudio      = false;
    _bVideoOnly         = false;
    _nPadding           = 0;
    _timestamp          = 0;
    _formatDetected     = false;

    _audioFmt           = INTERLACED_MODE::NOT_DEFINED;
}

CSMPTPFrame::~CSMPTPFrame() {
    if (_frame != NULL)
        delete _frame;
    if (_halfframe1 != NULL)
        delete _halfframe1;
    if (_halfframe2 != NULL)
        delete _halfframe2;
}

CSMPTPFrame& CSMPTPFrame::operator=(const CSMPTPFrame& other)
{
    // check for self-assignment
    if (&other == this)
        return *this;

    if (_completeframelen != other._completeframelen || _frame == NULL) {
        LOG_INFO("########### CREATE NEW BUFFER");
        if (_frame != NULL)
            delete _frame;
        _completeframelen = other._completeframelen;
        _frame = new unsigned char[_completeframelen];
        LOG_INFO("########### new=0x%p, other=0x%x, size=%d", _frame, other._frame, _completeframelen);
    }
    memcpy(_frame, other._frame, _completeframelen);
    return *this;
}

/**********************************************************************************************
*
* Frame processing
*
***********************************************************************************************/

/*!
* \fn _reset
* \brief reset frame information parameters
*
*/
void CSMPTPFrame::_reset() {

    _bFrameComplete = false;
    _firstPacket    = true;
    _actualframelen = 0;
    _writer         = _frame;
    _nbPacket       = 0;
    _timestamp      = 0;
}

/*!
* \fn initNewFrame
* \brief init class parameter before receive a new frame
*
*/
void CSMPTPFrame::initNewFrame() {

    _reset();
}

/*!
* \fn abortCurrentFrame
* \brief choose to abort the current frame... will reset internal parameter, and will wait for next end
* of RTP frame before starting to accumulate again packets
*
*/
void CSMPTPFrame::abortCurrentFrame() {

    _reset();
    _waitForNextFrame = true;
}

/*!
* \fn addRTPPacket
* \brief process a new received RTP packet, part of the current frame
*
* \param pPacket pointer to the RTP packet
*/
void CSMPTPFrame::addRTPPacket(CRTPFrame* pPacket) {

    // Verify packet type validity
    if (pPacket->_pt != 98) {
        LOG_ERROR("RTP packet with incorect payload type: frame._pt=%d, seq=%d", pPacket->_pt, pPacket->_seq);
        return;
    }

    // verify RTP sequence number continuity
    if (!_waitForNextFrame && !_firstPacket  && (_lastSeq != -1) && (pPacket->_seq != ((_lastSeq + 1) % 65536)) )
    {
        LOG_ERROR("lost %d RTP packet (last=%d, current=%d), drop current frame #%d", (pPacket->_seq - (_lastSeq + 1))+(pPacket->_seq<_lastSeq?65536:0), _lastSeq, pPacket->_seq, _frameCounter);
        // Drop current frame
        //_frameCounter++;
        _waitForNextFrame = true;
    }
    _lastSeq = pPacket->_seq;

    //LOG_INFO("recv RTP packet seq=%d", pPacket->_seq);

    // From this point, if wait for next frame, drop any data until next RTP packet with an "end of frame" marker
    if (_waitForNextFrame) {
        //LOG_INFO("drop packet until end of frame", _name);
        if (pPacket->isEndOfFrame()) {
            LOG_INFO("current frame #%d dropped, rtp packet #%d", _frameCounter, pPacket->_seq);
            _waitForNextFrame = false;
            _reset();
        }
        return;
    }

    // Access to HBRMP content
    CHBRMPFrame* hbrmp = pPacket->getHBRMPFrame();
    //LOG_INFO("rtp timestamp=%lu", pPacket->_timestamp);
    //LOG_INFO("hbrmp timestamp=%lu", hbrmp->_cf);
    if (_firstPacket) {
        //LOG_INFO("receive first packet");
        //pPacket->dumpHeader();
        //hbrmp->dumpHeader();
        // If SMPTE profile is not set, try to detect it from hbrmp headers :
        // see 7289943.pdf documentation, "Transport of High Bit Rate Media Signals over IP Network (HBRMT)"
        if (_profile.getStandard() == SMPTE_NOT_DEFINED)
            _profile.initProfileFromHBRMP(hbrmp);
        if (_profile.getStandard() == SMPTE_NOT_DEFINED) {
            LOG_ERROR("Error: This SMPTE format is not supported. Abort!");
            LOG_INFO("Abort.");
            exit(0);
        }

        //LOG_DUMP10BITS((const char*)hbrmp->getPayload(), 32);
        //LOG_INFO("timestanmp=%lu", hbrmp->_timestamp);
        // Keep hbrmptimestamp from the first packet (will be overwritten)
        _timestamp = hbrmp->getTimestamp();                     /* PktTS hook */
        _firstPacket = false;
    }
    if (_lastFc < 0) _lastFc = hbrmp->getFrameCounter();
    if (_lastFc != hbrmp->getFrameCounter()) {
        _lastFc = hbrmp->getFrameCounter();
    }
    _actualframelen += hbrmp->getPayloadLen();
    _nbPacket++;

    // Add payload content to the current frame
    if (!_firstFrame && _writer != 0) {
        if (((int)(_writer - _frame) + hbrmp->getPayloadLen()) > _completeframelen) {
            LOG_INFO("ERROR BUFFER OVERFLOW: used size=%d/%d, _actualframelen=%d, to write=%d", 
                (_writer - _frame), _completeframelen, _actualframelen, hbrmp->getPayloadLen());
            _waitForNextFrame = true;
            delete hbrmp;
            _reset();
            return;
        }
        else {
            //LOG_INFO("_writer############>");
            memcpy(_writer, hbrmp->getPayload(), hbrmp->getPayloadLen());
            _writer += hbrmp->getPayloadLen();
        }
    }

    // Manage end of frame
    if (pPacket->isEndOfFrame()) {

        //LOG_INFO("Frame #%d, _nbPacket=%d, _actualframelen=%d", _frameCounter, _nbPacket, _actualframelen);

        // Keep hbrmptimestamp from last packet
        _timestamp = hbrmp->getTimestamp();
        _frameCounter = hbrmp->getFrameCounter();

        if (_firstFrame) {
            LOG_INFO("First frame initialisation --------->");
            if (_profile.getStandard() == SMPTE_NOT_DEFINED) {
                LOG_ERROR("Error: SMPTE standard has not been detected properly! Abort!");
                exit(0);
            }
            if (_actualframelen < _profile.getTransportFrameSize() || _actualframelen> _profile.getTransportFrameSize()+RTP_PACKET_SIZE) {
                LOG_ERROR("Error: we don't received correct amount of data for this frame:");
                LOG_ERROR("... We received %d bytes", _actualframelen);
                LOG_ERROR("... Profile [%s] say that frame length is %d bytes", _profile.getProfileName().c_str(), _profile.getTransportFrameSize());
                abortCurrentFrame();
                delete hbrmp;
                return;
            }
            _nPadding = _actualframelen - _profile.getTransportFrameSize();
            _firstFrameInit(_actualframelen);
            _activeframelen = _completeframelen - _nPadding;
            LOG_INFO("first complete frame: received frame length=%d, profile frame length=%d padding=%d", _completeframelen, _profile.getTransportFrameSize(), _nPadding);
            if (!_checkAndValidate(_activeframelen)) {
                // An error occured, as a frame with not same length than exected. It's a fatal error with abort because
                // we can have buffer overflow...
                LOG_ERROR("Stream format validation failed. Abort!");
                abortCurrentFrame();
                delete hbrmp;
                return;
            }
            LOG_INFO("First frame initialisation <---------");
            _firstFrame = false;
        }

        if ((_actualframelen - _nPadding) == _activeframelen) {
            // The current frame seems valid, size is correct
           _bFrameComplete = true;
            //_frameCounter++;
            _qAvailableBuffers.clear();
            switch (_profile.getStandard())
            {
            case SMPTE_STANDARD::SMPTE_292M:
            case SMPTE_STANDARD::SMPTE_425MlvlA:
            case SMPTE_STANDARD::SMPTE_259M:
                _qAvailableBuffers.push_front(VIDEO_BUFFER_0);
                if (!_bVideoOnly) {
                    _qAvailableBuffers.push_front(AUDIO_BUFFER_0);
                    _qAvailableBuffers.push_front(ANC_BUFFER_0);
                }
                break;
            case SMPTE_STANDARD::SMPTE_425MlvlBDL:
                _qAvailableBuffers.push_front(VIDEO_BUFFER_1);
                if (!_bVideoOnly) {
                    _qAvailableBuffers.push_front(AUDIO_BUFFER_1);
                    _qAvailableBuffers.push_front(ANC_BUFFER_1);
                }
                _qAvailableBuffers.push_front(VIDEO_BUFFER_2);
                if (!_bVideoOnly) {
                    _qAvailableBuffers.push_front(AUDIO_BUFFER_2);
                    _qAvailableBuffers.push_front(ANC_BUFFER_2);
                }
                _isDemultiplexed = false;
                break;
            default:
                break;
            }
        }
        else {
            // Error, 
            LOG_ERROR("Incorrect frame size. receive '%d' bytes, correct size is '%d' bytes.", (_actualframelen - _nPadding), _activeframelen);
            LOG_ERROR("Frame dropped!");
            _reset();
        }
        _nbPacket = 0;
    }

    delete hbrmp;
}

/*!
* \fn setProfile
* \brief set the SMPTE profile for the stream

* \param format string which represent a profile (as '1080i25' for exemple)
*
* \return true if format detected, false otherwise
*/
SMPTE_STANDARD CSMPTPFrame::setProfile(const char* format) {

    LOG("Set profile to %s", format);
    _profile.setProfile(format);
    return _profile.getStandard();
}

SMPTE_STANDARD CSMPTPFrame::setProfile(CSMPTPProfile profile) {

    LOG("Set profile to %s", profile.getProfileName());
    _profile = profile;
    return _profile.getStandard();
}

/*!
* \fn _detectFormat
* \brief *** DEPRECATED *** Manually detect stream format SMPTE standard of a SMPTE frame along the frame size 
* (to use when receive the first complete frame)
*
* \return true if format detected, false otherwise
*/
bool CSMPTPFrame::_detectFormat()
{
    _formatDetected = true;
    int nPixelsPerLines = 0;

    LOG_INFO("SMPTE format detection for frame length = %d bytes", _activeframelen);

    if (_activeframelen == 6187500) {                   // 1080p30
        _profile.setProfile("1080p30");
        LOG_INFO("SMPTE 292M detected");
    }
    else if (_activeframelen == 7425000) {              // 1080p25
        _profile.setProfile("1080p25");
        LOG_INFO("SMPTE 292M detected");
    }
    else if (_activeframelen == 3093750) {              // 720p60, 720p59.94
        _profile.setProfile("720p60");
        LOG_INFO("SMPTE 292M detected");
    }
    else if (_activeframelen == 12375000) {             // 1080p60, 1080p59.94
        _profile.setProfile("1080p60");
        LOG_INFO("SMPTE 424M detected");
    }
    else if (_activeframelen == 14850000) {             // 1080p50
        _profile.setProfile("1080p50");
        LOG_INFO("SMPTE 424M detected");
    }
    else {
        // NOT SUPPORTED
        LOG_ERROR("warning: unsupported frame size: size=%d", _activeframelen);
        _formatDetected = false;
    }

    if (_formatDetected) {
        LOG_INFO("w=%d, h=%d, bits=%d, %d pixels per line", _profile.getActiveWidth(), _profile.getActiveHeight(), _profile.getFrameDepth(), _profile.getScanlineWidth());
    }

    return _formatDetected;
}

bool CSMPTPFrame::_checkAndValidate(int fullFrameLen) {
    if (_profile.getStandard() == SMPTE_NOT_DEFINED)
        return true;
    else {
        if (fullFrameLen != _profile.getTransportFrameSize()) {
            LOG_ERROR("Received frame size (%d bytes) != profile frame size (%d bytes)", fullFrameLen, _profile.getTransportFrameSize());
            return false;
        }
    }

    return true;
}

/*!
* \fn _detectEAV10bits
* \brief detect EAV on the current scanline (in 10 bits)
*
* \param buffer pointer to the scanline buffer
* \param size8bits size of buffer (in 8 bits) to search for EAV
* \return position (warning: position for 10 bits words, not for byte)
*/
int CSMPTPFrame::_detectEAV10bits(unsigned char* buffer, int size8bits) {
    int size10bits = 8 * size8bits / 10;
    //int* EAV = (_profile.isInterlaced() ? EAV_Interlaced : EAV_NoInterlaced);
    //int lenEAV = (_profile.isInterlaced() ? len_EAV_Interlaced : len_EAV_NoInterlaced);
    int* EAV = EAV_DoubleChannel;
    int lenEAV = len_EAV_DoubleChannel;
    //LOG_INFO("size=%d, size10=%d, len=%d", size8bits, size10bits, lenEAV);
    int pos10bits = 0;
    int w;
    //unsigned char* p = buffer;
    while (pos10bits <= size10bits - lenEAV) {
        w = tools::get10bitsWord(buffer, pos10bits);
        if (w == EAV[0]) {
            for (int i = 1; i < lenEAV; i++) {
                if (tools::get10bitsWord(buffer, pos10bits + i) != EAV[i])
                    break;
                if (i == lenEAV - 1) {
                    LOG_INFO("... pos10=%d", pos10bits);
                    return pos10bits;
                }
            }
        }
        pos10bits++;
    }
    //LOG_INFO("can't found ANC packet start seq");
    return -1;
}

/*!
* \fn _firstFrameInit
* \brief complete the init when receive the first frame (then we know the frame size)
*
* \param completeFrameSize size of a complete frame
*/
void CSMPTPFrame::_firstFrameInit(int completeFrameSize)
{
    // Keep the size of a full frame (with padding, i.e. multiple of RTP frame)
    _completeframelen = completeFrameSize;

    // Init the buffer
    if (_frame != NULL)
        delete[] _frame;
    _frame = new unsigned char[_completeframelen];
    
    // For now, force padding, as 
    //_nPadding = 1272;
    LOG_INFO("first frame init, videoFrameLen=%d, padding=%d", _completeframelen, _nPadding);
}

/*!
* \fn _calculatePadding
* \brief calculate padding at the end of the last packet
*
* \param buffer pointer to the last RTP packet
* \param bufferLen size of buffer 
* \return padding size
*/
int CSMPTPFrame::_calculatePadding(unsigned char* buffer, int bufferLen)
{
    // Calculate padding on buffer
    int nPadding = 0;
    for (int n = bufferLen - 1; n >= 0; n--) {
        if (buffer[n] == 0)
            nPadding++;
        else
            break;
    }
    return nPadding;
}

/*!
* \fn prepareFrame
* \brief prepare frame before construct frame to stream (use by Muxer)
*
*/
unsigned int SMPTE_CONST_LINE_10[] = { 0x0,   0x3ff, 0x3ff, 0x241, 0x101, 0x104, 0x185, 0x206, 0x200, 0x101, 0x2d2, 0x0,   0x3ff, 0x3ff, 0x260, 0x260,
                         0x110, 0x140, 0x200, 0x110, 0x200, 0x110, 0x200, 0x120, 0x200, 0x120, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200,
                         0x200, 0x170 };
unsigned int SMPTE_CONST_LINE_572[] = { 0x0,   0x3ff, 0x3ff, 0x241, 0x101, 0x104, 0x185, 0x206, 0x200, 0x101, 0x2d2 };
void CSMPTPFrame::prepareFrame()
{
    unsigned char* p;

    //
    // calculate frame geometry
    // 

    // calculate size of active video frame in bytes
    _activeframelen = _profile.getScanlineSize() * _profile.getScanlinesNb();
    LOG_INFO("_activeframelen=%d, _nPadding=%d", _activeframelen, _nPadding);

    // calculate the padding in bytes (
    int nbPacket = _activeframelen / SMPTE_MEDIA_PACKET_SIZE;
    if ((_activeframelen % SMPTE_MEDIA_PACKET_SIZE) != 0) {
        nbPacket = nbPacket + 1;
    }
    _nPadding = nbPacket * SMPTE_MEDIA_PACKET_SIZE - _activeframelen;
    //_nPadding = 1272;
    LOG_INFO("_activeframelen=%d, nbPacket=%d", _activeframelen, nbPacket);

    // calculate size of the full SMPTE frame, then alloc memory
    _completeframelen = _activeframelen + _nPadding;
    LOG_INFO("_completeframelen=%d, _nPadding=%d", _completeframelen, _nPadding);
    _firstFrameInit(_completeframelen);
    if (_completeframelen == 0) {
        LOG_ERROR("Calculated frame length = 0. Aborting!!!");
        exit(0);
    }
    memset(_frame, 0, _completeframelen);

    //
    // prepare smpte frame with predefine content that will not change from frame to frame: 
    // - init all "pixels" of the SMPTE frame to black (default for SMPTE)
    // - insert EAV and SAV for each scanline
    // - insert XYZ and LN
    // CRC will not be managed here as it will change with each active frame
    //
    int j=0;
    for (int i = 0; i < _profile.getScanlinesNb(); i++)
    {
        // Get pointer to the beginning of the scanline
        p = _frame + i*_profile.getScanlineSize();

        // Set default value for luma and chroma channel (black) for full scanline. These values
        // will be overrided when we will write active image, but will remain everywhere else.
        for (j = 0; j < _profile.getScanlineWidth()*_profile.getComponentsNb(); j++)
            tools::set10bitsWord(p, j, tools::isEven(j)?0x200:0x040);

        // Insert EAV at the start of scanline
        int* EAV = EAV_DoubleChannel;
        int lenEAV = len_EAV_DoubleChannel;
        for (j = 0; j < lenEAV; j++)
            tools::set10bitsWord(p, j, EAV[j]);

        // Insert XYZ (see st0292-1-2012.pdf, p16)
        int xyz0 = _getXYZ(true, i);
        int xyz1 = xyz0;
        tools::set10bitsWord(p, j++, xyz0);
        tools::set10bitsWord(p, j++, xyz1);

        // Insert LN (see st0292-1-2012.pdf, p6)
        int b8 = ((i + 1) >> 6) & 0b1;
        int ln0 = (b8 == 0 ? 0b1000000000 : 0) + (((i + 1) & 0b01111111) << 2);
        int ln1 = 0b1000000000 + (((i + 1) & 0b11110000000) >> 5);
        tools::set10bitsWord(p, j++, ln0);
        tools::set10bitsWord(p, j++, ln0);
        tools::set10bitsWord(p, j++, ln1);
        tools::set10bitsWord(p, j++, ln1);

        //
        // Then insert SAV for the current scanline juste before active video content
        //
        xyz0 = _getXYZ(false, i);
        xyz1 = xyz0;
        for (j = 0; j < lenEAV; j++)
            tools::set10bitsWord(p + _profile.getXOffset() - 10, j, EAV[j]);
        tools::set10bitsWord(p + _profile.getXOffset() - 10, j++, xyz0);
        tools::set10bitsWord(p + _profile.getXOffset() - 10, j++, xyz1);
    }

    // Some specific stuff:
    // line 0: set first CRC... it seems it's always the same
    tools::set10bitsWord(_frame, 12, 0x2f7);
    tools::set10bitsWord(_frame, 13, 0x2bb);
    tools::set10bitsWord(_frame, 14, 0x1e8);
    tools::set10bitsWord(_frame, 15, 0x23C);

    // line 9:
    p = _frame + 9*_profile.getScanlineSize();
    for (int i = 0; i < sizeof(SMPTE_CONST_LINE_10) / sizeof(int); i++) {
        tools::set10bitsWord(p, 17 + i * 2, SMPTE_CONST_LINE_10[i]);
    }
    p = _frame + 571 * _profile.getScanlineSize();
    for (int i = 0; i < sizeof(SMPTE_CONST_LINE_572) / sizeof(int); i++) {
        tools::set10bitsWord(p, 17 + i * 2, SMPTE_CONST_LINE_572[i]);
    }

    //Verification:
    /*for (int i = 0; i < _profile.getScanlinesNb(); i++) {
        LOG_INFO("line=%d", i);
        unsigned char* pp = _frame + i*_profile.getScanlineSize();
        LOG_DUMP10BITS((const char*)pp, 16);
        LOG_DUMP10BITS((const char*)pp + _profile.getXOffset() - 10, 16);
    }*/
}

/*!
* \fn resetFrame
* \brief Do some stuff on the SMPTE frame to reinit it to a new frame before insert a new video frame.
*
*/
void CSMPTPFrame::resetFrame() {
    // Reset Audio stuff (horizontal blanking)
    for (int i = 0; i < _profile.getScanlinesNb(); i++)
    {
        // Get pointer to the beginning of the scanline
        unsigned char* p = _frame + i*_profile.getScanlineSize();

        // Set default value for luma and chroma channel (black) for full scanline. These values
        // will be overrided when we will write active image, but will remain everywhere else.
        for (int j = 8; j < 225; j++) {
            tools::set10bitsWord(p, j*2,   0x200);
            tools::set10bitsWord(p, j*2+1, 0x040);
        }
    }
}

/*!
* \fn _extractSMPTEVideoContent
* \brief extract video content from SMPTE frame, and put it as uncompressed image on provided output buffer
*
* \param pOutputBuffer pointer to the buffer where to copy video media content
* \param sizeOfOutputBuffer size of pOutputBuffer
* \param src source buffer
* \return size of the copied video data
*/
int  CSMPTPFrame::_extractSMPTEVideoContent(char* pOutputBuffer, int sizeOfOutputBuffer, unsigned char* src) {

    // check size...
    int mediasize = _profile.getActiveWidth() * _profile.getActiveHeight() * _profile.getComponentsNb() * _profile.getComponentsDepth() / 8;
    if (mediasize > sizeOfOutputBuffer) {
        LOG_ERROR("Can't extract video frame, the provided video buffer is too small... (%d bytes and need %d bytes)", sizeOfOutputBuffer, mediasize);
        return 0;
    }
    if (src == NULL)
        return 0;

    int nTotalCopied = 0;
    int nActiveLineSize = _profile.getActiveWidth() * _profile.getComponentsNb() * _profile.getComponentsDepth() / 8;
    char* dest = pOutputBuffer;
    //LOG_INFO("copy from 0x%x to 0x%x, activelinesize=%d", p, _frame, nActiveLineSize);
    for (int i = 0; i < _profile.getActiveHeight(); i++) {
        int line = i + _profile.getYOffsetF1();
        if (_profile.isInterlaced()) {
            if (ISEVEN(i))
                line = i / 2 + _profile.getYOffsetF1();
            else
                line = i / 2 + _profile.getYOffsetF2();
        }
        //LOG_INFO("...copy line=%d, p=%d, src=%d", line, p- pOutputBuffer, (line * _profile.getScanlineSize()) + _profile.getXOffset());
        memcpy(dest, src + (line * _profile.getScanlineSize()) + _profile.getXOffset(), nActiveLineSize);
        dest += nActiveLineSize;
        nTotalCopied += nActiveLineSize;
    }
    //LOG_INFO("nTotalActiveCol=%d/%d, nTotalCopied=%d", nTotalActiveCol, _nTotalLineSize, nTotalCopied);
    return nTotalCopied;
}

/*!
* \fn _demux_process
* \brief job that demux a part of a SMPTE424M frame
*
* \param frame CSMPTPFrame object
* \param in offset on the input buffer
* \param out offset on the output buffer
* \param count nb of 10bits words to process
* \return 
*/
void CSMPTPFrame::_demux_process(CSMPTPFrame* frame, int in, int out, int count) {

    int w[4];
    unsigned char* src = frame->_frame;
    unsigned char* dest1 = frame->_halfframe1;
    unsigned char* dest2 = frame->_halfframe2;
    //LOG_INFO("####### into workers thread [%d], in=%d, out=%d, nbToPrcoceed=%d", index, in, out, nbToPrcoceed);
    for (int i = 0; i < count; i++) {
        w[0] = ((src[in + 0]) << 2) + ((src[in + 1] & 0b11000000) >> 6);
        w[1] = ((src[in + 1] & 0b00111111) << 4) + ((src[in + 2] & 0b11110000) >> 4);
        w[2] = ((src[in + 2] & 0b00001111) << 6) + ((src[in + 3] & 0b11111100) >> 2);
        w[3] = ((src[in + 3] & 0b00000011) << 8) + ((src[in + 4]));

        in += 5;
        tools::set10bitsWord(dest1, out, w[0]);
        tools::set10bitsWord(dest1, out + 1, w[1]);
        tools::set10bitsWord(dest2, out, w[2]);
        tools::set10bitsWord(dest2, out + 1, w[3]);
        out += 2;
    }
}

#define USE_THREADS_POOL
/*!
* \fn _demuxSMPTE424MFrame
* \brief demux the current frame, if it's a SMPTE425M Dual link frame, on two SMPTE292M frame
*
* \return VMI_E_OK if Ok, error code otherwise
*/
int CSMPTPFrame::_demuxSMPTE425MBDLFrame() {

    if (_profile.getStandard() != SMPTE_STANDARD::SMPTE_425MlvlBDL) {
        // No needed
        return VMI_E_OK;
    }

    // Demultiplex a new SMPTE 425M frame
    int nHalfFrameSize = _profile.getFrameSize();
    int nbWords10bits = 8 * nHalfFrameSize / 10;

    if (_halfframe1 == NULL) {
        _halfframe1 = new unsigned char[nHalfFrameSize];
        LOG_INFO("_halfframe1=0x%x, size=%d, nb words 10 bits=%d", _halfframe1, nHalfFrameSize, nbWords10bits);
    }
    if (_halfframe2 == NULL) {
        _halfframe2 = new unsigned char[nHalfFrameSize];
        LOG_INFO("_halfframe2=0x%x, size=%d, nb words 10 bits=%d", _halfframe2, nHalfFrameSize, nbWords10bits);
    }

    //LOG_INFO("_completeframelen=%d, _activeframelen=%d", _completeframelen, _activeframelen);

    //LOG_INFO("Extract new SMPTE frame");
    long long t1 = tools::getCurrentTimeInMilliS();
#ifdef USE_THREADS_POOL
    int nbThread = 10;
    for (int i = 0; i < nbThread; i++) {
        int nbWordsToProcess = nbWords10bits / nbThread;
        int in = i * nbWordsToProcess * 5 / 2;
        int out = i * nbWordsToProcess * 2 / 2;
        _workers.push_back(std::thread(_demux_process, this, in, out, nbWordsToProcess / 2));
    }
    for (std::thread &t : _workers) {
        if (t.joinable()) {
            t.join();
        }
    }
    _workers.clear();
#else
    int in = 0; int out = 0, w[4];
    //LOG_DUMP10BITS((const char*)_frame, 16);
    while (out < nbWords10bits) {
        // Read 4 10bits words, the two first seems to contain first frame, the two last for last frame

        w[0] = ((_frame[in + 0]) << 2) + ((_frame[in + 1] & 0b11000000) >> 6);
        w[1] = ((_frame[in + 1] & 0b00111111) << 4) + ((_frame[in + 2] & 0b11110000) >> 4);
        w[2] = ((_frame[in + 2] & 0b00001111) << 6) + ((_frame[in + 3] & 0b11111100) >> 2);
        w[3] = ((_frame[in + 3] & 0b00000011) << 8) + ((_frame[in + 4]));

        in += 5;
        tools::set10bitsWord(_halfframe1, out, w[0]);
        tools::set10bitsWord(_halfframe1, out + 1, w[1]);
        out += 2;
    }
#endif
    _isDemultiplexed = true;
    long long t2 = tools::getCurrentTimeInMilliS();
    //LOG_INFO("Process take %d ms", (int)(t2 - t1));

    return VMI_E_OK;
}



/*!
* \fn _extractChannelValue
* \brief Extract channel value on a 24bits value
*
* \param buffer pointer to the scanline buffer
* \param startpos10bits position to the first words of the 4 words of channel
* \return in (4bytes) with sample value
*/
int CSMPTPFrame::_extractChannelValue(unsigned char* buffer, int startpos10bits) {
    int UDW0 = tools::get10bitsWord(buffer, startpos10bits);
    int UDW1 = tools::get10bitsWord(buffer, startpos10bits + 2);
    int UDW2 = tools::get10bitsWord(buffer, startpos10bits + 4);
    int UDW3 = tools::get10bitsWord(buffer, startpos10bits + 6);
    int audioDta = ((UDW0 & 0b11110000) >> 4) + 
        ((UDW1 & 0b11111111) << 4) + 
        ((UDW2 & 0b11111111) << 12) + 
        ((UDW3 & 0b00001111) << 20);
    return audioDta;
}

/*!
* \fn _extractSMPTEAudioContent
* \brief Extract audio data from current SMPTE frame
*
* \param pOutputBuffer pointer to the output buffer
* \param sizeOfOutputBuffer size of pOutputBuffer
* \param src source buffer
* \return size of the copied audio data
*/
int CSMPTPFrame::_extractSMPTEAudioContent(char* pOutputBuffer, int sizeOfOutputBuffer, unsigned char* src) {

    if (src == NULL)
        return 0;

    AudioPacket audiopck;

    if (_audioFmt != INTERLACED_MODE::NOT_DEFINED)
        audiopck.SetInterlacedMode(_audioFmt);

    //LOG_INFO("--> <--");
    int nTotalAudioSize = 0;
    int nTotalAudioDataChunks = 0;
    int nTotalAudioControlPacket = 0;
    int pos10bits;
    unsigned char* pDest = (unsigned char*)pOutputBuffer;

    // Iterate on each SMPTE scan line to find audio packets on horizontal blanking data
    // Note that audio data packets are located on Ancillary data space of Cb/Cr data stream, and
    // audio control packets are located on Ancillary data space of Y data stream (on line 9 and 571 for 1080/60i system)
    for (int i = 0; i < _profile.getScanlinesNb(); i++) {
        unsigned char* p = (unsigned char*)src + i*_profile.getScanlineSize();
        int packet = 0;     // Number of audio packet processed on this line
        //if (i == 8) LOG_DUMP10BITS((const char*)p, 32);

        // Search for audio data packets
        pos10bits = 16; // Audio packet start are located on ancillary data space of the SMPTE line, then right after EAV and line header.
        while (pos10bits > -1) {
            pos10bits = audiopck.DetectADF10bits(p, _profile.getXOffset(), pos10bits);
            if (pos10bits > -1) {
                _bIncludeAudio = true;
                nTotalAudioDataChunks++;
                packet++;

                if (_audioFmt == INTERLACED_MODE::NOT_DEFINED)
                    _audioFmt = audiopck.GetInterlacedMode();

                audiopck.ReadSMPTEAudioData(p, pos10bits);

                // Write the audio data
                int writed = audiopck.Write(pDest + nTotalAudioSize, i);
                nTotalAudioSize += writed;
                
                //if (i == 1123) LOG_INFO("line %d, detect ADF on pos=%d, DID=0x%03x", i, (pos10bits), audiopck.GetDID());
                pos10bits += audiopck.GetPacketSize()*2;    // audio data packets shall be contiguous with each other on one horizontal ancillary data block 
            }
        }
        //LOG_INFO("line %d, nb audio packet=%d", i, (packet));

        // Search for audio control packets (on Y data stream
        pos10bits = 17;
        while (pos10bits > -1) {
            pos10bits = audiopck.DetectADF10bits(p, _profile.getXOffset(), pos10bits);
            if (pos10bits > -1) {
                int readed = audiopck.ReadSMPTEAudioData(p, pos10bits);
                if (_audioFmt == INTERLACED_MODE::NOT_DEFINED)
                    _audioFmt = audiopck.GetInterlacedMode();

                if (readed > 0) {
                    nTotalAudioControlPacket++;
                    // Write the audio data
                    int writed = audiopck.Write(pDest + nTotalAudioSize, i);
                    nTotalAudioSize += writed;

                    //LOG_INFO("line %d, detect control packet on pos=%d, DID=0x%03x, writed=%d", i, (pos10bits), audiopck.GetDID(), writed);
                    pos10bits += audiopck.GetPacketSize() * 2;  // audio data packets shall be contiguous with each other
                }
                else 
                    break;
            }
        }
    }

    //LOG_INFO("nTotalAudioDataChunks=%d, nTotalAudioControlPacket=%d, nTotalAudioSize=%d", nTotalAudioDataChunks, nTotalAudioControlPacket, nTotalAudioSize);

    return nTotalAudioSize;
}

/*!
* \fn insertAudioContentToSMPTEFrame
* \brief Insert audio data into SMPTE frame
*
* \param buffer pointer to the buffer which contains audio data (each int contains a 10 bits words)
* \param size size of the buffer in bytes
*/
void CSMPTPFrame::insertAudioContentToSMPTEFrame(unsigned char* buffer, int size)
{
    // If we receive Audio before any Video stuff, SMPTE frame is not already init. In this case, we drop the audio buffer.
    if (_frame == NULL) {
        LOG_INFO("receive audio but video is not already initialized... Drop this buffer.");
        return;
    }

    AudioPacket audiopck;
    int oldLine = -1;
    int nTotalAudioDataChunks = 0;
    int nTotalAudioControlPacket = 0;
    int nbDataPacketsWritten = 0;
    int nbDataPacketsWrittenPerLine = 0;
    int nbControlPacketsWrittenPerLine = 0;
    int nbAudioPacket = size / ((audiopck.GetAudioDataPacketSize()+1)*sizeof(int));
    LOG("size=%d, nbAudioPacket=%d", size, nbAudioPacket);
    unsigned char* pSrc = buffer;
    unsigned char* pDest = NULL;
    for (int i = 0; i < nbAudioPacket; i++) {
        // Get line number of the current audio packet
        int line = *((int*)pSrc); 
        pSrc += sizeof(int);
        if (line != oldLine) {
            //LOG_INFO("write %d audio packet and %d control packet on line %d", nbDataPacketsWrittenPerLine, nbControlPacketsWrittenPerLine, oldLine);
            oldLine = line;
            nbDataPacketsWrittenPerLine = 0;
            nbControlPacketsWrittenPerLine = 0;
            pDest = (unsigned char*)_frame + line*_profile.getScanlineSize();
        }
        // read the audio packet
        int readed = audiopck.Read(pSrc);
        if (audiopck.IsControlPacket()) {
            nTotalAudioControlPacket++;
            //LOG_INFO("audio control packet detected on line %d, writed on %d", line, (17 + nbControlPacketsWrittenPerLine * audiopck.GetAudioControlPacketSize() * 2));
            pSrc += audiopck.GetAudioDataPacketSize() * sizeof(int);
            audiopck.WriteSMPTEAudioData(pDest, 17 + nbControlPacketsWrittenPerLine * audiopck.GetAudioControlPacketSize()*2);
            nbControlPacketsWrittenPerLine++;
        }
        else {
            nTotalAudioDataChunks++;
            //if (line == 1123)LOG_INFO("data packet detected on line %d, writed on %d", line, (16 + nbDataPacketsWrittenPerLine * audiopck.GetAudioDataPacketSize() * 2));
            pSrc += audiopck.GetAudioDataPacketSize() * sizeof(int);
            audiopck.WriteSMPTEAudioData(pDest, 16 + nbDataPacketsWrittenPerLine * audiopck.GetAudioDataPacketSize()*2);
            nbDataPacketsWritten++;
            nbDataPacketsWrittenPerLine++;
        }
    }
    LOG("write %d audio packet and %d control packet", nTotalAudioDataChunks, nTotalAudioControlPacket);
}

/*!
* \fn insertVideoContentToSMPTEFrame
* \brief Insert video data into SMPTE frame
*
* \param buffer pointer to the buffer which contains video uncompressed frame
*/
void CSMPTPFrame::insertVideoContentToSMPTEFrame(char* buffer)
{
    //LOG_INFO("buffer=0x%x _frame=0x%x", buffer, _frame);
    // Insert Active video (only on proper scanline)
    int nActiveLineSize = _profile.getActiveWidth() * _profile.getComponentsNb() * _profile.getComponentsDepth() / 8;
    unsigned char* p = (unsigned char*)buffer;
    int nTotalCopied = 0;
    for (int i = 0; i < _profile.getActiveHeight(); i++) {
       int line = i + _profile.getYOffsetF1();
        if (_profile.isInterlaced()) {
            if (ISEVEN(i))
                line = i / 2 + _profile.getYOffsetF1();
            else
                line = i / 2 + _profile.getYOffsetF2();
        }
        memcpy(_frame + line*_profile.getScanlineSize() + _profile.getXOffset(), p, nActiveLineSize);
        //LOG_INFO("cp %d bytes at %d", nActiveLineSize, line*_profile.getScanlineSize() + _profile.getXOffset());
        p += nActiveLineSize;
        nTotalCopied += nActiveLineSize;
    }

    // Now new video content is inserted, need to compute again CRC
    if ( COMPUTE_CRC_FLAG )
        _computeCRC();

    //_analyse();
}

//(see st0292-1-2012.pdf, p6)
void CSMPTPFrame::_computeCRC() {
    CSMPTPCrc crcC;
    CSMPTPCrc crcY;
    unsigned char* p;

    for (int i = 0; i < _profile.getScanlinesNb(); i++) {
        //
        // Calculate CRC for the current scanline, both for Luminance and Chrominance
        //
        p = (unsigned char*)_frame + i*_profile.getScanlineSize();
        crcC.compute_crc18_scanline(p + _profile.getXOffset(), _profile.getActiveWidth() + 6, 0, 2);
        crcY.compute_crc18_scanline(p + _profile.getXOffset(), _profile.getActiveWidth() + 6, 1, 2);
        //
        // Then insert the CRC after EAV+LN of the next scanline, except for last line
        //
        //LOG_INFO("line=%d,6              %x %x %x %x", i, crcC.getCRC0(), crcC.getCRC1(), crcY.getCRC0(), crcY.getCRC1());
        if (i < _profile.getScanlinesNb() - 1) {
            p = (unsigned char*)_frame + (i + 1)*_profile.getScanlineSize();
            int j = 12;
            tools::set10bitsWord(p, j++, crcC.getCRC0());
            tools::set10bitsWord(p, j++, crcY.getCRC0());
            tools::set10bitsWord(p, j++, crcC.getCRC1());
            tools::set10bitsWord(p, j++, crcY.getCRC1());
        }
    }
}

/*!
* \fn convertVideoFrameTo8Bits
* \brief convert the 10bits uncompressed video frame to 8bits
*
* \param buffer pointer to the buffer on which put the resulted 8bits video frame
*/
void CSMPTPFrame::convertVideoFrameTo8Bits(unsigned char* pOutputBuffer)
{
    int w[4];
    unsigned char* out = pOutputBuffer;

    // Don't try to convert video frame if format is not properly detected
    if (!_formatDetected) {
        LOG_WARNING("format undetected");
        return;
    }

    for (int i = 0; i < _profile.getActiveHeight(); i++) {
        int line = i + _profile.getYOffsetF1();
        if (_profile.isInterlaced()) {
            if( ISEVEN(i) )
                line = i / 2 + _profile.getYOffsetF1();
            else
                line = i / 2 + _profile.getYOffsetF2();
        }

        unsigned char* in = _frame + line*_profile.getScanlineSize() + _profile.getXOffset();
        for (int j = 0; j < _profile.getActiveWidth() / 2; j++) {
            w[0] = (((int)in[0]) << 2) + (((int)in[1] & 0b11000000) >> 6);
            w[1] = (((int)in[1] & 0b00111111) << 4) + (((int)in[2] & 0b11110000) >> 4);
            w[2] = (((int)in[2] & 0b00001111) << 6) + (((int)in[3] & 0b11111100) >> 2);
            w[3] = (((int)in[3] & 0b00000011) << 8) + (((int)in[4]));
            for (int k = 0; k < 4; k++)
            {
                out[k] = (unsigned char)(w[k] >> 2);
            }
            in += 5;
            out += 4;
        }
    }
    int nActiveFrameSizeInByte = _profile.getActiveHeight() * (_profile.getActiveWidth() * _profile.getComponentsNb() * _profile.getComponentsDepth() / 8);
    LOG_INFO("convert %d bytes / %d", (out - pOutputBuffer), nActiveFrameSizeInByte);

    _dumpYUVBufferAsRGB(pOutputBuffer);
}

/*!
* \fn _dumpYUVBufferAsRGB
* \brief convert YUV8bits video frame to rgb, then dump it on a file
*
* \param buffer pointer to the buffer on which put the resulted 8bits video frame
*/
void CSMPTPFrame::_dumpYUVBufferAsRGB(unsigned char* pOutputBuffer) {
    // test only
    int rgb_output_size = _profile.getActiveWidth() * _profile.getActiveHeight() * 3;
    unsigned char* rgb = new unsigned char[rgb_output_size];
    //unsigned char* p = pOutputBuffer;

    YCbCr2RGB((char*)rgb, (char*)pOutputBuffer, _profile.getActiveWidth() * _profile.getActiveHeight());
    LOG_INFO("ok");

    std::ofstream f;
    f.open("test8bits.rgb", ios::out | ios::binary);
    if (f.is_open()) {
        f.write((char*)rgb, rgb_output_size);
        f.close();
    }

    //DUMP_PIXEL_AT(rgb, 1920, 1080, 1690, 764);
    DUMP_RGBPIXEL_AT(rgb, 1920, 1080, 850, 450);
    DUMP_YUV8PIXEL_AT(pOutputBuffer, 1920, 1080, 850, 450);

    delete rgb;

    /*f.open("test8bits.yuv", ios::out | ios::binary);
    if (f.is_open()) {
    f.write((char*)pOutputBuffer, nTotalCopied);
    f.close();
    }*/
}

void CSMPTPFrame::dumpVideoBuffer(char* filename)
{
    /*LOG_INFO("on '%s', size=%d %s", filename, _completeframelen, _bFrameComplete?"(is completed)":"");
    std::ofstream f;
    f.open(filename, ios::out | ios::binary);
    if (f.is_open()) {
        f.write((char*)_frame, _completeframelen);
        f.close();
    }*/
}
void CSMPTPFrame::loadFromFile(const char* filename)
{
    LOG_INFO("load from '%s', size=%d", filename, _completeframelen);
    std::ifstream f;
    f.open(filename, ios::in | ios::binary);
    if (f.is_open()) {
        f.read((char*)_frame, _completeframelen);
        f.close();
    }
}

void CSMPTPFrame::injectFrameData(unsigned char* buffer, int bufferLen)
{
    LOG("size=%d ", bufferLen);

    if (_frame == NULL) {
        _nPadding = _calculatePadding(buffer, bufferLen);
        _firstFrameInit(bufferLen);
    }
    memcpy(_frame, buffer, bufferLen);
    _bFrameComplete = true;
    _frameCounter++;
}

/*!
* \fn _analyse
* \brief analyse a frame and display some informations. DO NOT DO THAT EACH FRAME (resource consumption)
*
*/
static int g_oldCRC[4];
void CSMPTPFrame::_analyse()
{
    CSMPTPCrc ccrc, ycrc;
    unsigned int result;
    unsigned char* p;
    int nbActiveLine = 0;

    if (tools::get10bitsWord((unsigned char*)_frame, 0) == 0x00)
        return;

    LOG_INFO("----------------------- w=%dxh=%d", _profile.getActiveWidth(), _profile.getActiveHeight());
    for (int i = 0; i < _profile.getScanlinesNb()-1; i++) {
        p = (unsigned char*)_frame + i*_profile.getScanlineSize();
        //if ( i < 40) {
        //if ( i >= 1000 ) {
        if (i >= 21 && i < 40) {
            LOG_INFO("line  = %d, width=%d, offset=%d, oldcrc=[%x,%x,%x,%x]", i, _profile.getActiveWidth(), _profile.getXOffset(), g_oldCRC[0], g_oldCRC[1], g_oldCRC[2], g_oldCRC[3]);
            // Analyse XYZ
            int xyz = tools::get10bitsWord(p, 6);
            int v = ((xyz & 0b0010000000) >> 7);
            if (v == 0) nbActiveLine++;
            LOG_INFO("xyz=0x%x, f=%d, v=%s, h=%s", xyz, 
                ((xyz&0b0100000000)>>8),
                v==1?"vblank":"active line", 
                ((xyz & 0b0001000000) >> 6)==1? "hblank" : "active picture"); 
            if (v == 0) 
            {
                // Analyse CRC number
                result = ccrc.compute_crc18_scanline(p + _profile.getXOffset(), _profile.getActiveWidth() + 6, 0, 2);
                result = ycrc.compute_crc18_scanline(p + _profile.getXOffset(), _profile.getActiveWidth() + 6, 1, 2);
                LOG_INFO("line=                  %x %x %x %x", ccrc.getCRC0(), ycrc.getCRC0(), ccrc.getCRC1(), ycrc.getCRC1());
                //LOG_DUMP10BITS((const char*)p, 16);
                for (int k = 0; k < 4; k++) g_oldCRC[k] = tools::get10bitsWord(p, 12 + k);
                LOG_DUMP10BITS((const char*)p, 16);
            }
        }
    }
    LOG_INFO("nbActiveLine  = %d", nbActiveLine);
    LOG_INFO("-----------------------");
}

/*
* the following structure allow to retreive P3, P2, P1 and P0 corresponding to F, V and H values
*/
struct XYZ_t {
    int     F;
    int     V;
    int     H;
    int     P3;
    int     P2;
    int     P1;
    int     P0;
};
XYZ_t g_XYZ[] = {
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 1, 1, 1, 0, 1 },
    { 0, 1, 0, 1, 0, 1, 1 },
    { 0, 1, 1, 0, 1, 1, 0 },
    { 1, 0, 0, 0, 1, 1, 1 },
    { 1, 0, 1, 1, 0, 1, 0 },
    { 1, 1, 0, 1, 1, 0, 0 },
    { 1, 1, 1, 0, 0, 0, 1 },
};
int g_XYZ_len = 8;

int CSMPTPFrame::_getXYZ(bool isEAV, int lineIdx) {
    int xyz = 0;
    int l = lineIdx + 1;

    if (_profile.getStandard() == SMPTE_NOT_DEFINED)
        return 0;

    // Find F, V and H
    int h = (isEAV ? 1 : 0);
    
    int f = 0;  // always for progressive stream
    if (_profile.isInterlaced())
        f = (l > (_profile.getYOffsetF1() + _profile.getActiveHeight() / 2+3) ? 1 : 0);
    
    int v = 0;
    if (l < _profile.getYOffsetF1()+1)
        v = 1;
    if (_profile.isInterlaced()) {
        if (l > (_profile.getYOffsetF1() + _profile.getActiveHeight()/2) && l< (_profile.getYOffsetF2()+1) )
            v = 1;
        if( l >= _profile.getYOffsetF2() + _profile.getActiveHeight() / 2+1)
            v = 1;
    }
    else {
        if (l > (_profile.getYOffsetF1()+_profile.getActiveHeight()) )
            v = 1;
    }
    xyz = 0b1000000000 | (f << 8) | (v << 7) | (h << 6);

    // Find protection bits
    for (int i = 0; i < g_XYZ_len; i++)
        if ((g_XYZ[i].F == f) &&
            (g_XYZ[i].V == v) &&
            (g_XYZ[i].H == h)) {
            xyz = xyz | (g_XYZ[i].P3 << 5) | (g_XYZ[i].P2 << 4) | (g_XYZ[i].P1 << 3) | (g_XYZ[i].P0 << 2);
            return xyz;
        }
    // Error
    LOG_ERROR("can't find tuple (f, v, h)=(%d, %d, %d) on g_XYZ[]", f, v, h);
    return 0;
}

void CSMPTPFrame::dumpSMPTEFullFrame(const char* filename) {

    LOG_INFO("#### on '%s', size=%d %s", filename, _activeframelen, _bFrameComplete ? "(is completed)" : "");
    //LOG_DUMP10BITS((const char*)_frame, 64);
    std::ofstream f;
    f.open(filename, ios::out | ios::binary);
    if (f.is_open()) {
        f.write((char*)_frame, _activeframelen);
        f.close();
    }
}

void CSMPTPFrame::compareSMPTEFrame(const char* filename1, const char* filename2){

    bool bIncludeCRCError = false;

    LOG_INFO("-->");
    LOG_INFO("#### debug differences between '%s' and '%s'", filename1, filename2);
    std::ifstream in1(filename1, std::ifstream::ate | std::ifstream::binary);
    std::ifstream in2(filename2, std::ifstream::ate | std::ifstream::binary);

    // Compare size
    int size1 = (int)in1.tellg();
    int size2 = (int)in2.tellg();
    if (size1 != size2) {
        LOG_ERROR("Error in size (#1=%d, #2=%d)", size1, size2);
        return;
    }
    in1.seekg(0, std::ios::beg);
    in2.seekg(0, std::ios::beg);
    LOG_INFO("Size is correct (%d bytes)...", size1);

    // Get content
    char* file1Content = new char[size1];
    char* file2Content = new char[size2];
    if (in1.is_open()) {
        in1.read(file1Content, size1);
        in1.close();
    }
    else {
        LOG_ERROR("Error, file #1 '%s' is not open", filename1);
        return;
    }
    if (in2.is_open()) {
        in2.read(file2Content, size2);
        in2.close();
    }
    else {
        LOG_ERROR("Error, file #2 '%s' is not open", filename2);
        return;
    }
    LOG_INFO("Content loaded...");

    // Analyse each scanline
    int error = 0;
    int crcerror = 0;
    unsigned char *p1, *p2;
    int pos1 = 0, pos2 = 0;
    bool inDiff = false;
    int oldW1 = -1, oldW2 = -1, w1 = -1, w2 = -1;
    for (int i = 0; i < _profile.getScanlinesNb() - 1; i++) {
        p1 = (unsigned char*)file1Content + i*_profile.getScanlineSize();
        p2 = (unsigned char*)file2Content + i*_profile.getScanlineSize();
        if (i ==2) {
            LOG_DUMP10BITS((const char*)p1, 64);
            LOG_INFO("------------------");
            LOG_DUMP10BITS((const char*)p2, 64);
        }
        for (int j = 0; j < _profile.getScanlineWidth()*_profile.getComponentsNb(); j++) {
            oldW1 = w1; oldW2 = w2;
            w1 = tools::get10bitsWord(p1, j);
            w2 = tools::get10bitsWord(p2, j);
            if (j == 0 && inDiff) {
                LOG_INFO("-line-%04d - diff on pos [%d, %d]", i-1, pos1, _profile.getScanlineWidth()*_profile.getComponentsNb() - 1);
                inDiff = false;
            }
            if (w1 != w2) {
                //if( i==9 && j<20)
                //    LOG_INFO("L%04d - pos=%d, w1/w2 = %0x/%0x", i, j, w1, w2);
                if (!inDiff) {
                    pos1 = j;
                    //LOG_INFO("-line-%04d - diff at pos %d", i, j);
                    error++;
                    inDiff = true;
                }
            }
            else {
                if (inDiff) {
                    bool bIsCRCError = false;
                    pos2 = j - 1;
                    if( pos2>=pos1 && pos1>=12 && pos1<=15 && pos2>=12 && pos2<=15) {
                        crcerror++;
                        bIsCRCError = true;
                        LOG_INFO("-line-%04d - crc diff on pos [%d, %d]", i, pos1, pos2);
                    }
                    if (bIncludeCRCError || !bIsCRCError) {
                        if(pos1 == pos2)
                            LOG_INFO("-line-%04d - diff on pos [%d, %d], val=%x/%x", i, pos1, pos2, oldW1, oldW2);
                        else 
                            LOG_INFO("-line-%04d - diff on pos [%d, %d]", i, pos1, pos2);
                    }
                    inDiff = false;
                }

            }
        }
    }
    if( error>0 || crcerror>0)
        LOG_ERROR("Diff on content detected... nb error=%d, nb crcerror=%d", error, crcerror);
    else
        LOG_INFO("Content validated...");
    LOG_INFO("<--");
}

/*!
* \fn getNextAvailableMediaBuffer
* \brief return the next available media buffer for this SMPTE frame
*
* \return an SMPTEFRAME_BUFFERS elements
*/
SMPTEFRAME_BUFFERS CSMPTPFrame::getNextAvailableMediaBuffer() {

    if (_qAvailableBuffers.size() > 0) {
        SMPTEFRAME_BUFFERS ret(std::move(_qAvailableBuffers.back()));
        _qAvailableBuffers.pop_back();
        return ret;
    }
    return BUFFER_NONE;
}

/*!
* \fn getMediaBufferType
* \brief return the kind of the SMPTEFRAME_BUFFERS buffer
*
* \param buffer element of SMPTEFRAME_BUFFERS
* \return MEDIAFORMAT 
*/
MEDIAFORMAT CSMPTPFrame::getMediaBufferType(SMPTEFRAME_BUFFERS buffer) {

    switch (buffer) {
    case VIDEO_BUFFER_0:
    case VIDEO_BUFFER_1:
    case VIDEO_BUFFER_2:
        return MEDIAFORMAT::VIDEO;
        break;
    case AUDIO_BUFFER_0:
    case AUDIO_BUFFER_1:
    case AUDIO_BUFFER_2:
        return MEDIAFORMAT::AUDIO;
        break;
    case ANC_BUFFER_0:
    case ANC_BUFFER_1:
    case ANC_BUFFER_2:
        return MEDIAFORMAT::ANC;
        break;
    default:
        break;
    }
    return MEDIAFORMAT::NONE;
}

/*!
* \fn extractMediaContent
* \brief extract media content to a provided buffer
*
* \param buffer src buffer, must be a SMPTEFRAME_BUFFERS
* \param pOutputBuffer output buffer
* \param sizeOfOutputBuffer size of pOutputBuffer
* \return int size of copied content
*/
int  CSMPTPFrame::extractMediaContent(SMPTEFRAME_BUFFERS buffer, char* pOutputBuffer, int sizeOfOutputBuffer) {

    // In case of 424M Dual link/dual stream, need to demux first
    if ((_profile.getStandard() == SMPTE_STANDARD::SMPTE_425MlvlBDL) && !_isDemultiplexed)
        _demuxSMPTE425MBDLFrame();
   
    // Then extract wanted buffer
    switch (buffer) {
    case VIDEO_BUFFER_0:
        return _extractSMPTEVideoContent(pOutputBuffer, sizeOfOutputBuffer, _frame); break;
    case VIDEO_BUFFER_1:
        return _extractSMPTEVideoContent(pOutputBuffer, sizeOfOutputBuffer, _halfframe1); break;
    case VIDEO_BUFFER_2:
        return _extractSMPTEVideoContent(pOutputBuffer, sizeOfOutputBuffer, _halfframe2); break;
    case AUDIO_BUFFER_0:
        return _extractSMPTEAudioContent(pOutputBuffer, sizeOfOutputBuffer, _frame); break;
    case AUDIO_BUFFER_1:
        return _extractSMPTEAudioContent(pOutputBuffer, sizeOfOutputBuffer, _halfframe1); break;
    case AUDIO_BUFFER_2:
        return _extractSMPTEAudioContent(pOutputBuffer, sizeOfOutputBuffer, _halfframe2); break;
    case ANC_BUFFER_0:
    case ANC_BUFFER_1:
    case ANC_BUFFER_2:
        // TODO
        break;
    default:
        break;
    }
    return 0;
}
