#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "tcp_basic.h"
#include "out.h"
#include "yuv.h"
#include <pins/pinfactory.h>
#include "configurable.h"
using namespace std;

/**********************************************************************************************
*
* COutThumbSocket
*
***********************************************************************************************/

COutThumbSocket::COutThumbSocket(CModuleConfiguration* pMainCfg, int nIndex) : COut(pMainCfg, nIndex)
{
    _nType      = PIN_TYPE_TCP_THUMB;
    PROPERTY_REGISTER_OPTIONAL("ip", _ip, "\0");
    PROPERTY_REGISTER_MANDATORY("fmt", _depth, 0);
    PROPERTY_REGISTER_OPTIONAL("w", _w, 0);
    PROPERTY_REGISTER_OPTIONAL("h", _h, 0);
    PROPERTY_REGISTER_MANDATORY("fps", _fps, 0.0);
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_OPTIONAL("ratio", _ratio, 1);
    PROPERTY_REGISTER_OPTIONAL("interface", _interface, "");
    _isListen   = (_ip[0]=='\0');
    _output_fps = 25;
    _last_time  = 0.0f;
    _frame_rate = 1.0f/(float)_fps;
    _rgb_buffer = NULL;
    _frame_w    = (int)((float)_w/ (float)_ratio);
    _frame_h    = (int)((float)_h/ (float)_ratio);

    if (_depth < 3 || _depth>4) {
        LOG_ERROR("%s: ***ERROR*** invalid output pixel depth=%d. Must be 3 (rgb) or 4 (argb)", _name.c_str(), _depth);
    }

    LOG_INFO("%s: ratio=%d, format=%dx%d, depth=%d", _name.c_str(), _ratio, _frame_w, _frame_h, _depth);
}

COutThumbSocket::~COutThumbSocket()
{
    _tcpSock.closeSocket();
    if (_rgb_buffer != NULL)
        delete[] _rgb_buffer;
}

void convertYUV8ToRGB(unsigned char* src, int src_w, int src_h, unsigned char* dest, int factor, int depth) {
    LOG("-->");
    int dst_w = src_w / factor;
    int dst_h = src_h / factor;
    int nDstFullLine = dst_w * depth;
    int nSrcFullLine = src_w * 2;
    //LOG_INFO("factor=%d, dst_w=%d, dst_h=%d, nDstFullLine=%d, depth=%d", factor, dst_w, dst_h, nDstFullLine, depth);
    for (int y = 0; y < dst_h; y += 1)
        for (int x = 0; x < dst_w; x += 1) {
            unsigned char* out = dest + y * nDstFullLine + x * depth;
            unsigned char* in = src + y * factor * nSrcFullLine + x * factor * 2;
            YCbCr2RGBA((char*)out, (char*)in, 1);
        }
    LOG("<--");
}

void convertRGBAToRGB(unsigned char* src, int src_w, int src_h, unsigned char* dest, int factor, int depth) {
    LOG_INFO("-->");
    int dst_w = src_w / factor;
    int dst_h = src_h / factor;
    int nDstFullLine = dst_w * depth;
    int nSrcFullLine = src_w * 4;
    //LOG_INFO("nFactorW=%d, nFactorH=%d, nRGBFullLine=%d, nYUV8FullLine=%d", nFactorW, nFactorH, nRGBFullLine, nYUV8FullLine);
    for (int y = 0; y < dst_h; y += 1)
        for (int x = 0; x < dst_w; x += 1) {
            unsigned char* out = dest + y * nDstFullLine + x * depth;
            unsigned char* in = src + y * factor * nSrcFullLine + x * factor * 4;
            memcpy(out, in, depth);
        }
    LOG_INFO("<--");
}

int COutThumbSocket::send(CvMIFrame* frame)
{
    if (frame == NULL) {
        LOG_ERROR("Error: invalid frame...");
        return VMI_E_OK;
    }

    unsigned char* buffer = frame->getFrameBuffer();

    LOG("%s: --> <--", _name.c_str());
    int result = E_OK;
    int ret = 0;

    if (buffer == NULL) {
        LOG_WARNING("buffer <null> for thumbnail output pin");
        return ret;
    }

    //
    // Manage the connection
    //
    if( !_tcpSock.isValid() ) {
        if( _isListen )
            result = _tcpSock.openSocket((char*)C_INADDR_ANY, _port, _interface);
        else 
            result = _tcpSock.openSocket(_ip, _port, _interface);
        if( result != E_OK )
            LOG("%s: can't create %s TCP socket on [%s]:%d on interface '%s'", 
                _name.c_str(), (_isListen?"listening":"connected"), (_isListen?"NULL":_ip), _port, _interface[0]=='\0'?"<default>":_interface);
        else
            LOG_INFO("%s: Ok to create %s TCP socket on [%s]:%d on interface '%s'", 
                _name.c_str(), (_isListen?"listening":"connected"), (_isListen?"NULL":_ip), _port, _interface[0]=='\0'?"<default>":_interface);
    }

    //
    // Manage data
    //
    if( _tcpSock.isValid() ) {
        double currentTime = tools::getCurrentTimeInS();
        if( (currentTime - _last_time) > _frame_rate)
        {
            // Get Headers to identify the internal sampling format
            CFrameHeaders* headers = frame->getMediaHeaders();
            if (_firstFrame) {
                LOG_INFO("dump headers:");
                headers->DumpHeaders();
                _firstFrame = false;
            }
            SAMPLINGFMT fmt = headers->GetSamplingFmt();
            int depth = headers->GetDepth();
            _frame_w = headers->GetW();
            _frame_h = headers->GetH();
            if (_frame_w > 4096 || _frame_h > 4096) {
                LOG_ERROR("unsupported format readed from internal headers: w=%d, h=%d", _frame_w, _frame_h);
                ret = -1;
            }
            else {
                int offset = CFrameHeaders::GetHeadersLength();
                if (_frame_w == 0 || _frame_h == 0) {
                    LOG_ERROR("improper parameters read from ip2vf headers... frame_w=%d, frame_h=%d", _frame_w, _frame_h);
                    LOG_ERROR("abort!");
                    exit(0);
                }
                LOG("Conversion from %dx%d to %dx%d, fmt=%d to RGB(A) %d bytes", _frame_w, _frame_h, (_frame_w / _ratio), (_frame_h / _ratio), fmt, _depth);
                if (_rgb_buffer == NULL) {
                    int mediasize = (_frame_w / _ratio) * (_frame_h / _ratio) * _depth;
                    _rgb_buffer_len = mediasize + offset;
                    LOG_INFO("%s: allocate buffer of %d bytes for thumbnails+headers", _name.c_str(), _rgb_buffer_len);
                    _rgb_buffer = new unsigned char[_rgb_buffer_len];  // rgb buffer size + headers length
                    CFrameHeaders fh;
                    fh.CopyHeaders(headers);
                    fh.SetW(_frame_w / _ratio);
                    fh.SetH(_frame_h / _ratio);
                    fh.SetMediaSize(mediasize);
                    fh.SetMediaFormat(MEDIAFORMAT::VIDEO);
                    fh.SetSamplingFmt(_depth==3 ? SAMPLINGFMT::BGR : SAMPLINGFMT::BGRA);
                    fh.WriteHeaders(_rgb_buffer, 0);
                }
                // Extract RGB thumbnail
                if (fmt == SAMPLINGFMT::YCbCr_4_2_2 && depth == 8)
                    convertYUV8ToRGB(frame->getMediaBuffer(), _frame_w, _frame_h, _rgb_buffer + offset, _ratio, _depth);
                else if (fmt == SAMPLINGFMT::RGBA)
                    convertRGBAToRGB(frame->getMediaBuffer(), _frame_w, _frame_h, _rgb_buffer + offset, _ratio, _depth);
                else
                    LOG_ERROR("Conversion not supported: fmt=%d %dbits to RGB(A) bytes (from %dx%d to %dx%d)", fmt, _depth, _frame_w, _frame_h, (_frame_w / _ratio), (_frame_h / _ratio));
                // Send it
                int len = _rgb_buffer_len;
                result = _tcpSock.writeSocket((char*)_rgb_buffer, &len);
                LOG("%s: write %d/%d bytes to '%s:%d', result=%d", _name.c_str(), len, frame->getFrameSize(), _ip, _port, result);
                if (result != E_OK || len == 0) {
                    _tcpSock.closeSocket();
                    ret = -1;
                }
            }
            _last_time = currentTime;
        }
    }
    else {
        LOG("%s: ***ERROR*** can't send frame, socket not connected", _name.c_str());
        ret = -1;
    }

    return ret;
}

bool COutThumbSocket::isConnected()
{
    return _tcpSock.isValid();
}

PIN_REGISTER(COutThumbSocket,"thumbnails")
