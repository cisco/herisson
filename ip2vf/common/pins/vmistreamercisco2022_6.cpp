#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "common.h"
#include "error.h"
#include "log.h"
#include "tools.h"
#include "vmistreamer.h"
#include "frameheaders.h"
#include "configurable.h"

using namespace std;

/**********************************************************************************************
*
* CvMIStreamerCisco2022_6
*
***********************************************************************************************/

CvMIStreamerCisco2022_6::CvMIStreamerCisco2022_6(const char* ip, const char* mcastgroup, int port, PinConfiguration *pconfig, const char* ifname) : CBasevMIStreamer(ip, mcastgroup, port, pconfig, ifname)
{
    _isMulticast = !!mcastgroup[0];
    _frameCount = 0;
    _hbrmpTimestamp = 0;
    _firstvMIFrame = true;
    _firstCompletedFrame = false;
    _frame.frame = new CSMPTPFrame();
    _curFrameNb = 0;
}

CvMIStreamerCisco2022_6::~CvMIStreamerCisco2022_6() {

    delete _frame.frame;
    if (_udpSock.isValid()) {
        _udpSock.closeSocket();
    }
}

int CvMIStreamerCisco2022_6::send(CvMIFrame* frame) {

    // Manage the connection
    if (!_udpSock.isValid())
    {
        const char* nic = _ifname;
        int result = -1;
        if (_isMulticast)
        {
            result = _udpSock.openSocket(_mcastgroup, _ip, _port, false, nic);
        }
        else
        {
            result = _udpSock.openSocket((char*)_ip, NULL, _port, false, nic);
        }
        if (result != E_OK)
            LOG_ERROR("can't create %s main UDP socket on [%s]:%d on interface '%s'",
                "connected", _ip, _port, nic[0] == '\0' ? "<default>" : nic);
        else
            LOG_INFO("Ok to create %s main UDP socket on [%s]:%d on interface '%s'",
                "connected", _ip, _port, nic[0] == '\0' ? "<default>" : nic);
    }

    // Verify data
    if (frame == NULL)
        return VMI_E_INVALID_PARAMETER;

    unsigned char* srcBuffer = frame->getMediaBuffer();
    CFrameHeaders* headers = frame->getMediaHeaders();

    if (_firstvMIFrame && headers->GetMediaFormat() == MEDIAFORMAT::VIDEO)
    {
        CSMPTPProfile profile;
        profile.initProfileFromIP2VF(headers);
        if (profile.getStandard() == SMPTE_NOT_DEFINED)
        {
            LOG_ERROR("can't find appropriate SMPTE profile. abort!");
            return VMI_E_INVALID_FRAME;
        }
        profile.dumpProfile();
        headers->DumpHeaders();
        _firstvMIFrame = false;
        _firstCompletedFrame = true;
        _frame.frame->initNewFrame();
        //TODO: fix profile parsing, for now it's hardcoded
        _frame.frame->setProfile("1080i59.94");
        _frame.frame->prepareFrame();
        _frame.frame->getProfile()->dumpProfile();
        _packetizer.setProfile(_frame.frame->getProfile());
    }
    else if (_firstvMIFrame) {
        //  Nothing to do for Audio... Wait for first Video frame
        return VMI_E_OK;
    }

    // Current implementation is video as master
    if (headers->GetMediaFormat() == MEDIAFORMAT::VIDEO)
    {
        _curFrameNb = _frameCount;
        _frame.frame->setFrameNumber(_curFrameNb);
        _curFrameNb = headers->GetFrameNumber();
        //_frame.frame->resetFrame();
        _frame.frame->insertVideoContentToSMPTEFrame((char*)srcBuffer);
        _packetizer.send(&_udpSock, (char*)_frame.frame->getBuffer(), _frame.frame->getBufferSize());
    }
    else if (headers->GetMediaFormat() == MEDIAFORMAT::AUDIO)
    {
        _frame.frame->insertAudioContentToSMPTEFrame(srcBuffer, headers->GetMediaSize());
    }
    else {
        LOG_ERROR("vMI media format not supported (%d)", headers->GetMediaFormat());
    }

    return VMI_E_OK;
}

bool CvMIStreamerCisco2022_6::isConnected() {

    return _udpSock.isValid();
}

