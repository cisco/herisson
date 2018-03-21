#include <cstdio>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "frameheaders.h"
#include "hbrmpframe.h"
#include "smpteprofile.h"

using namespace std;

/*!
* \brief: LIST OF SUPPORTED SMPTE PROFILE FOR IP2VF
*/
SMPTEProfile g_profile[] = {
    // name                 standard           MAP    w     h   frm_w  frm_h comp depth f1_range  f2_range   rate  interlaced multiplexed
    { "525i29.97",          SMPTE_259M,       0x00,  720,  486,  858,  525, 2,   10,   20,  263, 283,  525, 29.97f,  true,  false },
    { "625i25",             SMPTE_259M,       0x00,  720,  576,  864,  625, 2,   10,   23,  310, 336,  623,  25.0f,  true,  false },
    { "1080p30",            SMPTE_292M,       0x00, 1920, 1080, 2200, 1125, 2,   10,   42, 1121,   0,    0,  30.0f, false,  false }, // 274M, system 7
    { "1080p29.97",         SMPTE_292M,       0x00, 1920, 1080, 2200, 1125, 2,   10,   42, 1121,   0,    0, 29.97f, false,  false }, // 274M, system 8
    { "1080p25",            SMPTE_292M,       0x00, 1920, 1080, 2640, 1125, 2,   10,   42, 1121,   0,    0,  25.0f, false,  false }, // 274M, system 9
    { "1080i50",            SMPTE_292M,       0x00, 1920, 1080, 2640, 1125, 2,   10,   21,  560, 584, 1123,  25.0f,  true,  false },
    { "720p60",             SMPTE_292M,       0x00, 1280,  720, 1650,  750, 2,   10,   26,  745,   0,    0,  60.0f, false,  false },
    { "720p59.94",          SMPTE_292M,       0x00, 1280,  720, 1650,  750, 2,   10,   26,  745,   0,    0, 59.94f, false,  false },
    { "720p50",             SMPTE_292M,       0x00, 1280,  720, 1980,  750, 2,   10,   26,  745,   0,    0,  50.0f, false,  false },
    { "1080i59.94",         SMPTE_292M,       0x00, 1920, 1080, 2200, 1125, 2,   10,   21,  560, 584, 1123, 29.97f,  true,  false }, // 274M, system 5
    { "1080i60",            SMPTE_292M,       0x00, 1920, 1080, 2200, 1125, 2,   10,   21,  560, 584, 1123,  30.0f,  true,  false }, // 274M, system 4
    { "1080p50 LvlB DL",    SMPTE_425MlvlBDL, 0x01, 1920, 1080, 2640, 1125, 2,   10,   42, 1121,   0,    0,  50.0f, false,   true },
    { "1080p59.94 LvlB DL", SMPTE_425MlvlBDL, 0x01, 1920, 1080, 2200, 1125, 2,   10,   42, 1121,   0,    0, 59.94f, false,   true },
    { "1080p60 LvlB DL",    SMPTE_425MlvlBDL, 0x01, 1920, 1080, 2200, 1125, 2,   10,   42, 1121,   0,    0,  60.0f, false,   true },
    { "1080p50 LvlB A",     SMPTE_425MlvlA,   0x00, 1920, 1080, 2640, 1125, 2,   10,   42, 1121,   0,    0,  50.0f, false,   true },
    { "1080p59.94 LvlA",    SMPTE_425MlvlA,   0x00, 1920, 1080, 2200, 1125, 2,   10,   42, 1121,   0,    0, 59.94f, false,   true },
    { "1080p60 LvlA",       SMPTE_425MlvlA,   0x00, 1920, 1080, 2200, 1125, 2,   10,   42, 1121,   0,    0,  60.0f, false,   true },
};


CSMPTPProfile::CSMPTPProfile() {
    _framelen       = 0;        // Frame length with padding
    _nScanlineSize  = 0;        // Frame length without padding: i.e. real Media Octets per frame
    _offsetX        = 1800;
    _offsetY        = 20;       // starting line for the active video (cf st0299-1-2009.pdf, p18)
    _smpteframeCode = 0x00;
    _smpteProfile.smpteStandard = SMPTE_NOT_DEFINED;
    _smpteProfile.name = "[not defined]";
}

CSMPTPProfile::~CSMPTPProfile() {
}

/*!
* \fn setProfile
* \brief search for a profile indentified by its name (i.e. for exemple "1080i59.94")
*
* \param hbrmp pointer to the HBRMT headers
*
* \return SMPTE_STANDARD if format detected, false otherwise
*/
SMPTE_STANDARD CSMPTPProfile::setProfile(const char* format) {
    bool found = _findProfile(format);
    if (found) {
        LOG_INFO("Set profile to [%s]", _smpteProfile.name.c_str());
        _nScanlineSize = _smpteProfile.nNbPixelsPerScanline * _smpteProfile.nComponentsNb * _smpteProfile.nBitDepth / 8;
        _framelen = _smpteProfile.nScanLinesNb * _nScanlineSize;
        _transportframelen = _framelen;
        if(_smpteProfile.smpteStandard == SMPTE_425MlvlBDL)
            _transportframelen = 2* _framelen;
        _offsetX = (_smpteProfile.nNbPixelsPerScanline - _smpteProfile.nActivePixelsNb) * _smpteProfile.nComponentsNb * _smpteProfile.nBitDepth / 8;
        LOG_INFO("Scan line size = %d bytes", _nScanlineSize);
        LOG_INFO("Frame length = %d bytes / Transport Frame length = %d bytes ", _framelen, _transportframelen);
        LOG_INFO("Offset on scan line = %d bytes (offsetX)", _offsetX);
        return _smpteProfile.smpteStandard;
    }
    LOG_ERROR("Can't find SMPTE profile '%s' on known profiles list", format);
    return SMPTE_NOT_DEFINED;
}

/*!
* \fn _findProfile
* \brief search for a profile indentified by its name (i.e. for exemple "1080i59.94") 
*
* \param hbrmp pointer to the HBRMT headers
*
* \return SMPTE_STANDARD if format detected, false otherwise
*/
bool CSMPTPProfile::_findProfile(const char* format) {
    int size = sizeof(g_profile) / sizeof(SMPTEProfile);
    for (int i = 0; i < size; i++) {
        if (g_profile[i].name.compare(format) == 0) {
            _smpteProfile = g_profile[i];
            return true;
        }
    }
    return false;
}

/*!
* \fn initProfileFromHBRMP
* \brief try to detect the profile from the hbrmt headers
*
* \param hbrmp pointer to the HBRMT headers
*
* \return SMPTE_STANDARD if format detected, false otherwise
*/
SMPTE_STANDARD CSMPTPProfile::initProfileFromHBRMP(CHBRMPFrame* hbrmp) 
{
    LOG_INFO("try to identify SMPTE stream");
    if (hbrmp && hbrmp->getVideoSourceFrmFormat() > 0) {

        int map = hbrmp->getVideoSourceMapFormat();
        switch (map) {
        case 0x00: LOG_INFO("Found valid 'MAP' parameter: code=0x%x, means='%s'", map, "Direct sample structure as per SMPTE ST 292-1, SMPTE ST 425-1 Level A, etc"); break;
        case 0x01: LOG_INFO("Found valid 'MAP' parameter: code=0x%x, means='%s'", map, "SMPTE ST 425-1 Level B-DL Mapping of ST 372 Dual-Link"); break;
        case 0x02: LOG_INFO("Found valid 'MAP' parameter: code=0x%x, means='%s'", map, "SMPTE ST 425-1 Level B-DS Mapping of two ST 292-1 Streams"); break;
        default: LOG_INFO("Found valid 'MAP' parameter: code=0x%x, means='%s'", map, "Reserved"); break;
        }

        int frame_index = -1;
        for (int i = 0; i < g_FRAME_len; i++) {
            if (hbrmp->getVideoSourceFrmFormat() == g_FRAME[i].code) {
                frame_index = i;
                LOG_INFO("Found valid 'FRAME' parameter: code=0x%x (h_active=%d, v_active=%d, v_total=%d, sampling_struct=%d)", g_FRAME[i].code, g_FRAME[i].horizontal_active, g_FRAME[i].vertical_active, g_FRAME[i].vertical_total, g_FRAME[i].sampling_struct);
                break;
            }
        }
        if (frame_index == -1) {
            LOG_ERROR("can't found valid 'FRAME' parameter: code=0x%x", hbrmp->getVideoSourceFrmFormat());
            return SMPTE_NOT_DEFINED;
        }

        int frate_index = -1;
        for (int i = 0; i < g_FRATE_len; i++) {
            if (hbrmp->getVideoSourceRateFormat() == g_FRATE[i].code) {
                frate_index = i;
                LOG_INFO("Found valid 'FRATE' parameter: code=0x%x (rate=%.2f)", g_FRATE[i].code, g_FRATE[i].frame_rate_in_hz);
                break;
            }
        }
        if (frate_index==-1) {
            LOG_ERROR("can't found valid 'FRATE' parameter: code=0x%x", hbrmp->getVideoSourceRateFormat());
            return SMPTE_NOT_DEFINED;
        }

        int sample_index = -1;
        for (int i = 0; i < g_SAMPLE_len; i++) {
            if (hbrmp->getVideoSourceSampleFormat() == g_SAMPLE[i].code) {
                sample_index = i;
                LOG_INFO("Found valid 'SAMPLE' parameter: code=0x%x (rate=%d, bit_depth=%d)", g_SAMPLE[i].code, g_SAMPLE[i].sampling_struct, g_SAMPLE[i].bit_depth);
                break;
            }
        }
        if (sample_index == -1) {
            LOG_ERROR("can't found valid 'SAMPLE' parameter: code=0x%x", hbrmp->getVideoSourceSampleFormat());
            return SMPTE_NOT_DEFINED;
        }

        // Be carefull, if stream is interlaced, framerate of profile is double than real framerate
        // i.e.: 1080i60 => real framerate is 30fps        
        //       1080i59.94 => real framerate is 29.97fps        
        //       1080i50 => real framerate is 25fps        
        if (g_FRAME[frame_index].sampling_struct == 0) {
            if (sample_index == 5)
                sample_index = 0;
            else if (sample_index == 6)
                sample_index = 1;
            else if (sample_index == 7)
                sample_index = 2;
        }

        LOG_INFO("Stream identified: %d%s%.2f (real framerate)", g_FRAME[frame_index].vertical_active, (g_FRAME[frame_index].sampling_struct == 0)?"i":"p", g_FRATE[frate_index].frame_rate_in_hz);

        // Everything is Ok... try to detect a supported SMPTE profile
        int size = sizeof(g_profile) / sizeof(SMPTEProfile);
        for (int i = 0; i < size; i++) {
            if (g_profile[i].nActivePixelsNb == g_FRAME[frame_index].horizontal_active &&
                g_profile[i].nActiveLineNb   == g_FRAME[frame_index].vertical_active &&
                g_profile[i].nScanLinesNb    == g_FRAME[frame_index].vertical_total &&
                g_profile[i].fRate           == g_FRATE[frate_index].frame_rate_in_hz &&
                g_profile[i].nBitDepth       == g_SAMPLE[sample_index].bit_depth &&
                g_profile[i].map             == hbrmp->getVideoSourceMapFormat() &&
                g_profile[i].interlaced      == (g_FRAME[frame_index].sampling_struct==0)
                ) {
                LOG_INFO("Match SMPTE profile: [%s]", g_profile[i].name.c_str());
                setProfile(g_profile[i].name.c_str());
                // [vDCM request] add FRAME parameter on vMI headers
                _smpteframeCode = hbrmp->getVideoSourceFrmFormat();
                return _smpteProfile.smpteStandard;
            }
        }
    }
    return SMPTE_NOT_DEFINED;
}

/*!
* \fn initProfileFromIP2VF
* \brief try to detect the profile from IP2vf internal format
*
* \param headers pointer to the CFrameHeaders headers
*
* \return SMPTE_STANDARD if format detected, false otherwise
*/
SMPTE_STANDARD CSMPTPProfile::initProfileFromIP2VF(CFrameHeaders* headers)
{
    LOG_INFO("init SMPTE profile from IP2vf headers for muxer");
    if (headers) {
        float frame_rate_in_hz = 0.0f;
        for (int i = 0; i < g_FRATE_len; i++) {
            if (headers->GetFramerateCode() == g_FRATE[i].code) {
                frame_rate_in_hz = g_FRATE[i].frame_rate_in_hz;
                LOG_INFO("Found valid 'FRATE' parameter: code=0x%x (rate=%f)", g_FRATE[i].code, g_FRATE[i].frame_rate_in_hz);
                break;
            }
        }

        int size = sizeof(g_profile) / sizeof(SMPTEProfile);
        for (int i = 0; i < size; i++) {
            if (g_profile[i].nActivePixelsNb == headers->GetW() &&
                g_profile[i].nActiveLineNb   == headers->GetH() &&
                g_profile[i].fRate           == frame_rate_in_hz &&
                g_profile[i].nBitDepth       == headers->GetDepth() &&
                g_profile[i].interlaced      == (headers->GetFrameType() == FRAMETYPE::FIELD)
                ) {
                LOG_INFO("Match SMPTE profile: '%s'", g_profile[i].name.c_str());
                setProfile(g_profile[i].name.c_str());
                return _smpteProfile.smpteStandard;
            }
        }
    }
    return SMPTE_NOT_DEFINED;
}

/*!
* \fn _detectFormat
* \brief detect stream format SMPTE standard of a SMPTE frame (to use when receive the first complete frame)
*
* \return true if format detected, false otherwise
*/
/*bool CSMPTPProfile::_detectFormat()
{
}*/

/*!
* \fn dumpProfile
* \brief dump some current format parameters
*
*/
void CSMPTPProfile::dumpProfile() {
    LOG_INFO("------");
    LOG_INFO("Active frame size in pixels = %dx%d", _smpteProfile.nActivePixelsNb, _smpteProfile.nActiveLineNb);
    LOG_INFO("SMPTE frame size in pixels  = %dx%d", _smpteProfile.nNbPixelsPerScanline, _smpteProfile.nScanLinesNb);
    LOG_INFO("depth = %d, rate=%.2ffps, FRAME=0x%x", _smpteProfile.nBitDepth, _smpteProfile.fRate, _smpteframeCode);
}



