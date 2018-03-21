/**
* \file framefactory.h
* \brief class to manage IP2VF internal format frames
* \author A.Taldir
* \version 0.1
* \date 01 august 2016
*
* It allow to manage IP2VF internal format frames (audio and video) allow to read/write headers
*
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>      // strerror
#include <string>

#include "common.h"
#include "log.h"
#include "frameheaders.h"
#include <pins/pins.h>

#define HEADERS_VERSION_MAJOR   2
#define HEADERS_VERSION_MINOR   0
#define HEADERS_VERSION_PATCH   2

#define COMMON_HEADER_LENGTH    32      // in bytes
#define MEDIA_HEADER_OFFSET     COMMON_HEADER_LENGTH 
#define MEDIA_HEADER_LENGTH     12       // in bytes
#define EXT_HEADER_OFFSET       MEDIA_HEADER_OFFSET+MEDIA_HEADER_LENGTH  
#define EXT_HEADER_LENGTH       16      // in bytes

#define EXTRACT_INTEGER(p, i)   ((p[i+0] << 24) + (p[i+1] << 16) + (p[i+2] << 8) + p[i+3])
#define EXTRACT_LONG_LONG(p, i) (((unsigned long long)p[i+0] << 56) + ((unsigned long long)p[i+1] << 48) + ((unsigned long long)p[i+2] << 40) + ((unsigned long long)p[i+3] << 32) + (p[i+4] << 24) + (p[i+5] << 16) + (p[i+6] << 8) + p[i+7])


CFrameHeaders::CFrameHeaders() {
    // Common headers
    _versionmajor   = HEADERS_VERSION_MAJOR;
    _versionminor   = HEADERS_VERSION_MINOR;
    _versionpatch   = HEADERS_VERSION_PATCH;
    _framenb        = 0;
    _moduleid       = -1;
    _mediafmt       = MEDIAFORMAT::NONE;
    _headerlen      = FRAME_HEADER_LENGTH;
    _mediasize      = 0;
    _mediatimestamp = 0;
    _srctimestamp   = 0;
    // media video
    _w              = 0;
    _h              = 0;
    _frametype      = FRAMETYPE::FRAME;
    _odd            = FIELDTYPE::ODD;
    _pgroupsize     = 0;
    _colorimetry    = COLORIMETRY::SMPTE240M;
    _samplingformat = SAMPLINGFMT::YCbCr_4_2_2;
    _depth          = 8;
    _framerateCode  = g_FRATE[7].code;   // 25 fps
    _smpteframeCode = 0;
    // media audio
    _channelnb      = 0;
    _audiofmt       = AUDIOFMT::L16_PCM;
    _samplerate     = SAMPLERATE::S_44_1KHz;
    _packettime     = 0;
    //ext
    _inputtimestamp = 0;
    _outputtimestamp= 0;
};

/*!
* \fn CopyHeader
* \brief Copy headers from one CFrameHeaders instance to another
*
* \param from instance of CFrameHeaders to copy from
*/
void CFrameHeaders::CopyHeaders(const CFrameHeaders* from) {
    // Common headers
    _versionmajor   = from->_versionmajor;
    _versionminor   = from->_versionminor;
    _versionpatch   = from->_versionpatch;
    _framenb        = from->_framenb;
    _moduleid       = from->_moduleid;
    _mediafmt       = from->_mediafmt;
    _mediasize      = from->_mediasize;
    _mediatimestamp = from->_mediatimestamp;
    _srctimestamp   = from->_srctimestamp;
    // Media Audio/video spécific part
    _w              = from->_w;
    _h              = from->_h;
    _frametype      = from->_frametype;
    _odd            = from->_odd;
    _pgroupsize     = from->_pgroupsize;
    _colorimetry    = from->_colorimetry;
    _samplingformat = from->_samplingformat;
    _depth          = from->_depth;
    _framerateCode  = from->_framerateCode;
    _smpteframeCode = from->_smpteframeCode;
    _channelnb      = from->_channelnb;
    _audiofmt       = from->_audiofmt;
    _samplerate     = from->_samplerate;
    _packettime     = from->_packettime;
    // Ext part
    _inputtimestamp = from->_inputtimestamp;
    _outputtimestamp= from->_outputtimestamp;
}

int CFrameHeaders::WriteHeaders(unsigned char* buffer, int frame_nb) {

    if (buffer == NULL) {
        LOG_ERROR("Invalid buffer");
        return VMI_E_INVALID_PARAMETER;
    }

    if (frame_nb > -1)
        _framenb = frame_nb;

    memset(buffer, 0, FRAME_HEADER_LENGTH);

    buffer[0] = FRAME_HEADER_MAGIC_1;
    buffer[1] = FRAME_HEADER_MAGIC_2;
    buffer[2] = FRAME_HEADER_MAGIC_3;
    buffer[3] = FRAME_HEADER_MAGIC_4;

    buffer[4] = _versionmajor & 0b11111111;
    buffer[5] = _versionminor & 0b11111111;
    buffer[6] = _versionpatch & 0b11111111;
    buffer[7] = 0;  // Reserved

    buffer[8] =  (_framenb >> 24) & 0b11111111;
    buffer[9] =  (_framenb >> 16) & 0b11111111;
    buffer[10] = (_framenb >> 8) & 0b11111111;
    buffer[11] = _framenb & 0b11111111;

    buffer[12] = _moduleid;
    buffer[13] = (int)_mediafmt;
    buffer[14] = 0;  // Reserved
    buffer[15] = 0;  // Reserved

    buffer[16] = (_mediasize >> 24) & 0b11111111;
    buffer[17] = (_mediasize >> 16) & 0b11111111;
    buffer[18] = (_mediasize >> 8) & 0b11111111;
    buffer[19] = _mediasize & 0b11111111;

    buffer[20] = (_mediatimestamp >> 24) & 0b11111111;
    buffer[21] = (_mediatimestamp >> 16) & 0b11111111;
    buffer[22] = (_mediatimestamp >> 8) & 0b11111111;
    buffer[23] = _mediatimestamp & 0b11111111;

    buffer[24] = (_srctimestamp >> 56) & 0b11111111;
    buffer[25] = (_srctimestamp >> 48) & 0b11111111;
    buffer[26] = (_srctimestamp >> 40) & 0b11111111;
    buffer[27] = (_srctimestamp >> 32) & 0b11111111;

    buffer[28] = (_srctimestamp >> 24) & 0b11111111;
    buffer[29] = (_srctimestamp >> 16) & 0b11111111;
    buffer[30] = (_srctimestamp >> 8) & 0b11111111;
    buffer[31] = _srctimestamp & 0b11111111;

    if (_mediafmt == MEDIAFORMAT::VIDEO) {
        buffer[MEDIA_HEADER_OFFSET + 0]  = (_w >> 8) & 0b11111111;
        buffer[MEDIA_HEADER_OFFSET + 1]  = _w & 0b11111111;
        buffer[MEDIA_HEADER_OFFSET + 2]  = (_h >> 8) & 0b11111111;
        buffer[MEDIA_HEADER_OFFSET + 3]  = _h & 0b11111111;

        buffer[MEDIA_HEADER_OFFSET + 4]  = (_frametype << 7) & 0b10000000;
        buffer[MEDIA_HEADER_OFFSET + 4] |= (_odd << 6)      & 0b01000000;
        buffer[MEDIA_HEADER_OFFSET + 4] |= _pgroupsize      & 0b00111111;
        buffer[MEDIA_HEADER_OFFSET + 5]  = (_colorimetry << 5) & 0b11100000;
        buffer[MEDIA_HEADER_OFFSET + 5] |= _samplingformat    & 0b00011111;
        buffer[MEDIA_HEADER_OFFSET + 6]  = _depth & 0b11111111;
        buffer[MEDIA_HEADER_OFFSET + 7]  = _framerateCode & 0b11111111;
        buffer[MEDIA_HEADER_OFFSET + 8]  = _smpteframeCode & 0b11111111;
    }
    else 
    {
        buffer[MEDIA_HEADER_OFFSET + 0]  = _channelnb & 0b11111111;
        buffer[MEDIA_HEADER_OFFSET + 1]  = (_audiofmt << 4) & 0b11110000;
        buffer[MEDIA_HEADER_OFFSET + 1] |= _samplerate & 0b00001111;
        buffer[MEDIA_HEADER_OFFSET + 2]  = (_packettime >> 8) & 0b11111111;
        buffer[MEDIA_HEADER_OFFSET + 3]  = _packettime & 0b11111111;
        buffer[MEDIA_HEADER_OFFSET + 4]  = 0;  // reserved
        buffer[MEDIA_HEADER_OFFSET + 5]  = 0;  // reserved
        buffer[MEDIA_HEADER_OFFSET + 6]  = 0;  // reserved
        buffer[MEDIA_HEADER_OFFSET + 7]  = 0;  // reserved
    }

    buffer[EXT_HEADER_OFFSET + 0] = (_inputtimestamp >> 56) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 1] = (_inputtimestamp >> 48) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 2] = (_inputtimestamp >> 40) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 3] = (_inputtimestamp >> 32) & 0b11111111;

    buffer[EXT_HEADER_OFFSET + 4] = (_inputtimestamp >> 24) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 5] = (_inputtimestamp >> 16) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 6] = (_inputtimestamp >> 8) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 7] = _inputtimestamp & 0b11111111;

    buffer[EXT_HEADER_OFFSET + 8]  = (_outputtimestamp >> 56) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 9]  = (_outputtimestamp >> 48) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 10] = (_outputtimestamp >> 40) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 11] = (_outputtimestamp >> 32) & 0b11111111;

    buffer[EXT_HEADER_OFFSET + 12] = (_outputtimestamp >> 24) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 13] = (_outputtimestamp >> 16) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 14] = (_outputtimestamp >> 8) & 0b11111111;
    buffer[EXT_HEADER_OFFSET + 15] = _outputtimestamp & 0b11111111;

    return VMI_E_OK;
}

int CFrameHeaders::ReadHeaders(unsigned char* buffer) {

    if (buffer == NULL) {
        LOG_ERROR("Invalid buffer");
        return VMI_E_INVALID_PARAMETER;
    }

    unsigned char* p = buffer;
 
    // Verify headers magic number
    if (p[0] != FRAME_HEADER_MAGIC_1 || p[1] != FRAME_HEADER_MAGIC_2 || p[2] != FRAME_HEADER_MAGIC_3 || p[3] != FRAME_HEADER_MAGIC_4) {
        LOG_ERROR("headers magic numbers not found...");
        return VMI_E_INVALID_HEADERS;
    }

    _versionmajor = p[4];
    _versionminor = p[5];
    _versionpatch = p[6];

    // Verify version before go further
    if (_versionmajor != HEADERS_VERSION_MAJOR || _versionminor != HEADERS_VERSION_MINOR || _versionpatch != HEADERS_VERSION_PATCH) {
        LOG_ERROR("headers version ERROR, receive v%d.%d.%d, but expected v%d.%d.%d", _versionmajor, _versionminor, _versionpatch, 
            HEADERS_VERSION_MAJOR, HEADERS_VERSION_MINOR, HEADERS_VERSION_PATCH);
        return VMI_E_INVALID_HEADER_VERSION;
    }

    _framenb = EXTRACT_INTEGER(p, 8);
    _moduleid  = p[12];
    _mediafmt  = static_cast<MEDIAFORMAT>(p[13]);
    _mediasize = EXTRACT_INTEGER(p, 16);

    _mediatimestamp = EXTRACT_INTEGER(p, 20);
    _srctimestamp   = EXTRACT_LONG_LONG(p, 24);

    p = (unsigned char*)buffer + MEDIA_HEADER_OFFSET;
    if (_mediafmt == MEDIAFORMAT::VIDEO) {
        _w              = (p[0] << 8) + p[1];
        _h              = (p[2] << 8) + p[3];
        _frametype      = static_cast<FRAMETYPE>((p[4] & 0b10000000) >> 7);
        _odd            = static_cast<FIELDTYPE>((p[4] & 0b01000000) >> 6);
        _pgroupsize     = (p[4] & 0b00111111);
        _colorimetry    = static_cast<COLORIMETRY>((p[5] & 0b11100000) >> 5);
        _samplingformat = static_cast<SAMPLINGFMT>(p[5] & 0b00011111);
        _depth          = p[6];
        _framerateCode  = p[7];
        _smpteframeCode = p[8];
    }
    else
    {
        _channelnb  = p[0];
        _audiofmt   = static_cast<AUDIOFMT>((p[1] & 0b11110000) >> 4);
        _samplerate = static_cast<SAMPLERATE>(p[1] & 0b00001111);
        _packettime = (p[2] << 8) + p[3];
    }

    p = (unsigned char*)buffer + EXT_HEADER_OFFSET;
    _inputtimestamp  = EXTRACT_LONG_LONG(p, 0);
    _outputtimestamp = EXTRACT_LONG_LONG(p, 8);

    return VMI_E_OK;
}


void CFrameHeaders::DumpHeaders(unsigned char* frame)
{
    LOG_INFO("------");

    if (frame != NULL) {
        int byte_per_line = 8;
        for (int i = 0; i < FRAME_HEADER_LENGTH; i += byte_per_line) {
            char token[8];
            char line[128];
            line[0] = '\0';
            int nb = MIN(byte_per_line, FRAME_HEADER_LENGTH - i);
            for (int j = 0; j < nb; j++) {
                SNPRINTF(token, 8, "0x%02x ", frame[i + j]);
                STRCAT(line, token);
            }
            LOG_INFO("    %s", line);
        }
    }
    LOG_INFO("_version = (%d, %d, %d)", _versionmajor, _versionminor, _versionpatch);
    LOG_INFO("_count = %d", _framenb);
    LOG_INFO("_moduleid = %d", _moduleid);
    LOG_INFO("_mediafmt = %d", _mediafmt);
    LOG_INFO("_mediasize = %d", _mediasize);
    LOG_INFO("_mediatimestamp = %lu", _mediatimestamp);
    LOG_INFO("_srctimestamp = %llu", _srctimestamp);
    if (_mediafmt == MEDIAFORMAT::VIDEO) {
        LOG_INFO("(_w, _h) = (%d, %d)", _w, _h);
        LOG_INFO("_frametype = %d", _frametype);
        LOG_INFO("_odd = %d", _odd);
        LOG_INFO("_pgroupsize = %d", _pgroupsize);
        LOG_INFO("_colorimetry = %d", _colorimetry);
        LOG_INFO("_samplingformat = %d", _samplingformat);
        LOG_INFO("_depth = %d", _depth);
        LOG_INFO("_framerateCode = 0x%x", _framerateCode);
        LOG_INFO("_smpteframeCode = 0x%x", _smpteframeCode);
    }
    else if (_mediafmt == MEDIAFORMAT::AUDIO)
    {
        LOG_INFO("_channelnb = %d", _channelnb);
        LOG_INFO("_audiofmt = %d", _audiofmt);
        LOG_INFO("_samplerate = %d", _samplerate);
        LOG_INFO("_packettime = %d", _packettime);
    }
}

void CFrameHeaders::InitVideoHeadersFromSMPTE(int w, int h, SAMPLINGFMT samplingfmt, bool interlaced)
{
    _mediafmt = MEDIAFORMAT::VIDEO;
    _mediatimestamp = 0;
    _srctimestamp = 0;
    _w = w;
    _h = h;
    _frametype = (interlaced ? FRAMETYPE::FIELD : FRAMETYPE::FRAME);
    _odd = FIELDTYPE::ODD;
    _pgroupsize     = 5;
    _colorimetry    = COLORIMETRY::SMPTE240M;
    _samplingformat = samplingfmt;
    _depth          = 10;
    _framerateCode  = g_FRATE[7].code;   // 25 fps
}
void CFrameHeaders::InitVideoHeadersFromRTP(int w, int h, SAMPLINGFMT samplingfmt, int depth, bool interlaced) {
    _mediafmt = MEDIAFORMAT::VIDEO;
    _mediatimestamp = 0;
    _srctimestamp = 0;
    _w = w;
    _h = h;
    _frametype = (interlaced ? FRAMETYPE::FIELD : FRAMETYPE::FRAME);
    _odd = FIELDTYPE::ODD;
    _pgroupsize = 5;
    _colorimetry = COLORIMETRY::SMPTE240M;
    _samplingformat = samplingfmt;
    _depth = depth;
    _framerateCode = g_FRATE[7].code;   // 25 fps
}

void CFrameHeaders::InitVideoHeadersFromTR03(int w, int h, SAMPLINGFMT samplingfmt, int depth, bool interlaced)
{
    InitVideoHeadersFromRTP(w, h, samplingfmt, depth, interlaced);
    _pgroupsize = 2;
}
void CFrameHeaders::InitAudioHeadersFromSMPTE(AUDIOFMT audiofmt, SAMPLERATE samplerate)
{
    _mediafmt = MEDIAFORMAT::AUDIO;
    _mediatimestamp = 0;
    _srctimestamp = 0;
    _audiofmt = audiofmt;
    _samplerate = samplerate;
}

void CFrameHeaders::InitVideoHeadersFromProfile(CSMPTPProfile* smpteProfile) {
    //smpteProfile->dumpProfile();
    _mediafmt = MEDIAFORMAT::VIDEO;
    _mediatimestamp = 0;
    _srctimestamp = 0;
    _w = smpteProfile->getActiveWidth();
    _h = smpteProfile->getActiveHeight();
    _frametype = (smpteProfile->isInterlaced() ? FRAMETYPE::FIELD : FRAMETYPE::FRAME);
    _odd = FIELDTYPE::ODD;
    _pgroupsize = 5;
    _colorimetry = COLORIMETRY::SMPTE240M;
    _samplingformat = SAMPLINGFMT::YCbCr_4_2_2;
    _depth = smpteProfile->getComponentsDepth();
    // Search for framerate code followings value
    _framerateCode = 0;
    for (int i = 0; i < g_FRATE_len; i++) {
        if (smpteProfile->getFramerate() == g_FRATE[i].frame_rate_in_hz) {
            _framerateCode = g_FRATE[i].code;
            LOG("Found valid 'FRATE' parameter: code=0x%x, rate=%f", g_FRATE[i].code, g_FRATE[i].frame_rate_in_hz);
            break;
        }
    }
    if(_framerateCode == 0)
        LOG_ERROR("can't find valid 'FRATE' parameter for rate=%f", smpteProfile->getFramerate());
    _smpteframeCode = smpteProfile->getFRAMEcode();
}

CSMPTPProfile CFrameHeaders::GetProfile() {
    CSMPTPProfile profile;
    profile.initProfileFromIP2VF(this);
    return profile;
}


