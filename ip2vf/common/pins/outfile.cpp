#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcpy, strerror, memcpy
#include <iostream>
#include <fstream>

#include "pins/pinfactory.h"
#include "common.h"
#include "log.h"
#include "tools.h"
#include "out.h"

using namespace std;

/**********************************************************************************************
*
* COutFile
*
***********************************************************************************************/

COutFile::COutFile(CModuleConfiguration* pMainCfg, int nIndex) : COut(pMainCfg, nIndex)
{
    LOG("%s: --> <--", _name.c_str());
    _nType       = PIN_TYPE_FILE;
    _output_fps  = 25;
    _last_time   = 0.0f;
    _frame_rate  = 1.0f / (float)_pConfig->_fps;
    _frame_in_w  = _pConfig->_w;
    _frame_in_h  = _pConfig->_h;
    _frame_out_w = (int)((float)_pConfig->_w / (float)_pConfig->_ratio);
    _frame_out_h = (int)((float)_pConfig->_h / (float)_pConfig->_ratio);
    _filename    = std::string(_pConfig->_filename);
    _pTempBuffer = new unsigned char[_frame_out_w*_frame_out_h*3];

    _tempFilename = _filename + std::string(".tmp");
    char buf[512]; 
    sprintf(buf, "mv %s %s", _tempFilename.c_str(), _filename.c_str());
    _cmd = std::string(buf);

    // Get context for conversion (_frame_in_w, _frame_in_h) in yuv8bits to (_frame_out_w, _frame_out_h) in rgb24
    _swsContext = (void*) sws_getContext(_frame_in_w, _frame_in_h, AV_PIX_FMT_UYVY422, _frame_out_w, _frame_out_h, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (_swsContext == NULL) {
        LOG_ERROR("%s: failed to get sws context to convert (%dx%d)yuv8 to (%dx%d)rgb24", _name.c_str(), _frame_in_w, _frame_in_h, _frame_out_w, _frame_out_h);
    }
    _pData = new char [_ip2vfFrameSize];
}

COutFile::~COutFile() 
{
    if( _swsContext )
        sws_freeContext((struct SwsContext*)_swsContext);
    delete _pData;
}

int COutFile::send(char* buffer) 
{
    buffer = _pData;
    int ret = 0;
    LOG("%s: --> <--", _name.c_str());

    if (buffer == NULL)
        return ret;

    double currentTime = tools::getCurrentTimeInS();
    if ((currentTime - _last_time) > _frame_rate)
    {
        _last_time = currentTime;

        // Scale & convert to rgb24
        int offset = CFrameHeaders::GetHeadersLength();
        int in_linesize = _frame_in_w * 2;
        uint8_t* in = (uint8_t*)buffer + offset;

        int out_linesize = _frame_out_w * 3;
        uint8_t* out = (uint8_t*)_pTempBuffer;

        try {
            sws_scale((struct SwsContext*)_swsContext, (const uint8_t* const*)&in, &in_linesize, 0, _frame_in_h, (uint8_t* const*)&out, &out_linesize);  // see x264_encoder_delayed_frames( mEncoder )
        }
        catch (...) {
            LOG_ERROR("catch exception on  sws_scale()...");
        }

        // Save it to png format
        tools::savePNGImage(_tempFilename.c_str(), _frame_out_w, _frame_out_h, _pTempBuffer);

        // Rename it
        system(_cmd.c_str());
    }

    return ret;
}

bool COutFile::isConnected() 
{
    return true;
}

PIN_REGISTER(COutFile,"file")
