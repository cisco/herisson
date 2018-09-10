#include <cstdio>
#include <cstdlib>
#include <cstring>      // strerror
#include <string>

#include "common.h"
#include "log.h"
#include "hbrmpframe.h"
#include "frameheaders.h"
#include "smpteprofile.h"

// Based on SMPTE Standard ST 2022-6
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  Ext  |F|VSID |    FRCount    | R | S | FEC |  CF   | RESERVE |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  MAP  |     FRAME     |    FRATE      |SAMPLE | FMT - RESERVE |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |               Video timestamp(only if CF>0)                   |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |               Header extension(0nly if Ext > 0)               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
float CHBRMPFrame::_timestampbase = 0.0f;
#define HBRMP_FREQUENCY 148500000    // 27 MHz

FRAME_t g_FRAME[] = {
    { 0x10,  720,  486,  525, 0, 0 }, 
    { 0x11,  720,  576,  625, 0, 0 }, 
    { 0x20, 1920, 1080, 1125, 0, 0 }, 
    { 0x21, 1920, 1080, 1125, 1, 1 }, 
    { 0x22, 1920, 1080, 1125, 1, 2 }, 
    { 0x23, 2048, 1080, 1125, 1, 1 },
    { 0x24, 2048, 1080, 1125, 1, 2 },
    { 0x30, 1280,  720,  750, 1, 1 },
};
int g_FRAME_len = (int)sizeof(g_FRAME) / sizeof(FRAME_t);
FRATE_t g_FRATE[] = {
    { 0x10,  60.0f  },
    { 0x11,  59.94f },
    { 0x12,  50.0f  },
    { 0x14,  48.0f  },
    { 0x15,  47.95f },
    { 0x16,  30.0f  },
    { 0x17,  29.97f },
    { 0x18,  25.0f  },
    { 0x1a,  24.0f  },
    { 0x1b,  23.97f },
};
int g_FRATE_len = (int)sizeof(g_FRATE) / sizeof(FRATE_t);
SAMPLE_t g_SAMPLE[] = {
    { 0x01,  (int)YCbCr_4_2_2, 10 },
    { 0x02,  (int)YCbCr_4_4_4, 10 },
    { 0x05,  (int)YCbCr_4_2_2, 12 },
    { 0x06,  (int)YCbCr_4_4_4, 12 },
};
int g_SAMPLE_len = (int)sizeof(g_SAMPLE) / sizeof(SAMPLE_t);


CHBRMPFrame::CHBRMPFrame() {
    _frame = NULL;
    _ext = 0;
    _f = 0;
    _vsid = 0;
    _frcount = 0;
    _r = REF_FOR_TIMESTAMP::REFT_NOT_LOCKED;
    _s = 0;
    _fec = 0;
    _cf = CLOCK_FREQ::CF_NO_TIMESTAMP;
    _map = 0;
    _frm = 0;
    _frate = 0;
    _sample = 0;
    _timestamp = 0;
    _headerlen = 0;
    _framelen = 0;
    _payloadlen = 0;
}

CHBRMPFrame::CHBRMPFrame(const unsigned char* frame, int len) : CHBRMPFrame() {
    _frame = (unsigned char*)frame;
    _framelen = len;
    extractData();
}

CHBRMPFrame::~CHBRMPFrame() {
}

void CHBRMPFrame::dumpHeader()
{
    LOG_INFO("------");

    LOG_INFO("ext          = %d", _ext);
    LOG_INFO("f/vsid       = %d/%d", _f, _vsid);
    LOG_INFO("fr count     = %d", _frcount);
    LOG_INFO("R/S/fec      = %d/%d/%d", _r, _s, _fec);
    LOG_INFO("cf           = %d", _cf);
    LOG_INFO("MAP          = 0x%02x", _map);
    LOG_INFO("FRM/RATE/SMPL= 0x%02x/0x%02x/0x%02x", _frm, _frate, _sample);
    LOG_INFO("timestamp    = %lu", _timestamp);
    LOG_INFO("length       = %d (%d/%d)", _framelen, _headerlen, _payloadlen);
}

void CHBRMPFrame::extractData() {
    //LOG_DUMP(_frame, HBRMP_FIXED_LENGTH);
    _ext       =  (_frame[0] & 0b11110000) >> 4;
    _f         =  (_frame[0] & 0b00001000) >> 3;
    _vsid      =   _frame[0] & 0b00000111;
    _frcount   =   _frame[1];
    _r         = static_cast<REF_FOR_TIMESTAMP>((_frame[2] & 0b11000000) >> 6);
    _s         =  (_frame[2] & 0b00110000) >> 4;
    _fec       =  (_frame[2] & 0b00001110) >> 1;
    _cf        = GET_CLOCK_FREQUENCY(((_frame[2] & 0b00000001) << 3) + ((_frame[3] & 0b11100000) >> 5));
    _map       =  (_frame[4] & 0b11110000) >> 4;
    _frm       = ((_frame[4] & 0b00001111) << 4) + ((_frame[5] & 0b11110000) >> 4);
    _frate     = ((_frame[5] & 0b00001111) << 4) + ((_frame[6] & 0b11110000) >> 4);
    _sample    =  (_frame[6] & 0b00001111);
    _timestamp =  (_frame[8] << 24) + (_frame[9] << 16) + (_frame[10] << 8) + _frame[11];
    _headerlen  = HBRMP_FIXED_LENGTH + (_cf > 0 ? HBRMP_VIDEO_TIMESTAMP_LENGTH : 0) + (_ext > 0 ? _ext * 4 : 0);
    _payloadlen = _framelen - _headerlen;
}

void CHBRMPFrame::initFixedHBRMPValuesFromProfile(CSMPTPProfile* profile)
{
    // Pre-defined parameters
    _ext = 0;
    _f = 1;
    _cf = CLOCK_FREQ::CF_148_5_PER_1_001_MHZ;
    _map = 0x00;

    // Get 'FRAME' from profile detected
    for (int i = 0; i < g_FRAME_len; i++) {
        if (profile->getActiveWidth() == g_FRAME[i].horizontal_active &&
            profile->getActiveHeight() == g_FRAME[i].vertical_active && 
            profile->getScanlinesNb() == g_FRAME[i].vertical_total &&
            profile->isInterlaced() == (g_FRAME[i].sampling_struct == 0) ) {
            _frm = g_FRAME[i].code;
            LOG("Found valid 'FRAME' parameter: code=0x%x (h_active=%d, v_active=%d, v_total=%d, sampling_struct=%d)", g_FRAME[i].code, g_FRAME[i].horizontal_active, g_FRAME[i].vertical_active, g_FRAME[i].vertical_total, g_FRAME[i].sampling_struct);
            break;
        }
    }
    if (_frm == 0) {
        LOG_ERROR("can't found valid 'FRAME' parameter");
    }

    // Get 'FRATE' from profile detected
    for (int i = 0; i < g_FRATE_len; i++) {
        if (profile->getFramerate() == g_FRATE[i].frame_rate_in_hz) {
            _frate = g_FRATE[i].code;
            LOG("Found valid 'FRATE' parameter: code=0x%x (rate=%f)", g_FRATE[i].code, g_FRATE[i].frame_rate_in_hz);
            break;
        }
    }
    if (_frate == 0) {
        LOG_ERROR("can't found valid 'FRATE' parameter");
    }

    // Get 'SAMPLE' from profile detected
    for (int i = 0; i < g_SAMPLE_len; i++) {
        if (profile->getComponentsDepth() == g_SAMPLE[i].bit_depth) {
            _sample = g_SAMPLE[i].code;
            LOG("Found valid 'SAMPLE' parameter: code=0x%x (rate=%d, bit_depth=%d)", g_SAMPLE[i].code, g_SAMPLE[i].sampling_struct, g_SAMPLE[i].bit_depth);
            break;
        }
    }
    if (_sample == 0) {
        LOG_ERROR("can't found valid 'SAMPLE' parameter");
    }
}

void CHBRMPFrame::writeHeader(int frcount, unsigned int timestamp) {
    _timestamp = timestamp;
    _frcount = frcount%256;
    memset(_frame, 0, HBRMP_FIXED_LENGTH + (_cf > 0 ? HBRMP_VIDEO_TIMESTAMP_LENGTH : 0));
    _frame[0] |= ((_ext << 4) & 0b11110000);
    _frame[0] |= ((_f << 3) & 0b00001000);
    _frame[0] |= (_vsid & 0b00000100);
    _frame[1]  = _frcount & 0b11111111;
    _frame[2] |= (_r << 6) & 0b11000000;
    _frame[2] |= (_s << 4) & 0b00110000;
    _frame[2] |= (_fec << 4) & 0b00001110;
    _frame[2] |= (_cf >> 3) & 0b00000001;
    _frame[3] |= (_cf << 5) & 0b11100000;
    _frame[4] |= (_map << 4) & 0b11110000;
    _frame[4] |= (_frm >> 4) & 0b00001111;
    _frame[5] |= (_frm << 4) & 0b11110000;
    _frame[5] |= (_frate >> 4) & 0b00001111;
    _frame[6] |= (_frate << 4) & 0b11110000;
    _frame[6] |= _sample & 0b00001111;
    // add timestamp
    if (_cf != 3)
        _timestamp = 0;
    _frame[8]  = (_timestamp >> 24) & 0b11111111;
    _frame[9]  = (_timestamp >> 16) & 0b11111111;
    _frame[10] = (_timestamp >> 8) & 0b11111111;
    _frame[11] = _timestamp & 0b11111111;
}

void CHBRMPFrame::dumpPayload(int size) {
    LOG_DUMP((const char*)_frame+ _headerlen, size);
}
void CHBRMPFrame::dumpHeader10bits(int size)
{
    int pos = 0;
    unsigned char* p = _frame + _headerlen;
    char line[1024];
    char token[64];
    int tokens=0;
    line[0] = '\0';
    int w[4];
    while (pos <= size) {
        // 4 word of 10 bits = 40 bits = 5 bytes
        w[0] = ((p[0]             ) << 2) + ((p[1] & 0b11000000) >> 6);
        w[1] = ((p[1] & 0b00111111) << 4) + ((p[2] & 0b11110000) >> 4);
        w[2] = ((p[2] & 0b00001111) << 6) + ((p[3] & 0b11111100) >> 2);
        w[3] = ((p[3] & 0b00000011) << 8) + ((p[4]             )     );
        int nb = MIN(4, size - pos);
        tokens += nb;
        for (int i = 0; i < nb; i++) {
            SNPRINTF(token, 64, "%03x ", w[i]);
            strcat(line, token);
        }
        pos += 4; p += 4;
        if (tokens >= 16 || pos == size) {
            LOG_INFO("0x%s", line);
            tokens = 0;
            line[0] = '\0';
        }
    }
}


