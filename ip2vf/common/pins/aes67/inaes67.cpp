/*
 * inaes67.cpp
 *
 *  Created on: Nov 3, 2017
 *      Author: mhawari
 */

#include <cstring>
#include "inaes67.h"

#ifdef _WIN32
unsigned int CInAES67::SamplesPerMs[4] = { 0 , 0, 48, 96 };
#else
constexpr unsigned int CInAES67::SamplesPerMs[];
#endif //_WIN32

CInAES67::CInAES67(CModuleConfiguration *pMainCfg, int nIndex) :
        CIn(pMainCfg, nIndex)
{
    _lastSeq = -1;
    _lastTimestamp = -1;
    _udpSock = new UDP();
    _headers.SetFrameNumber(0);
    _headers.SetModuleId(_nModuleId);
    _headers.SetMediaFormat(AUDIO);
    _headers.SetAudioFmt(L24_PCM);
    _headers.SetPacketTime(1000); //Packet Time in usec, we assume the default 1ms packet time
    _nType = PIN_TYPE_AES67;
    _audioParametersDetected = false;
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_OPTIONAL("mcastgroup", _zmqip, "");
    PROPERTY_REGISTER_OPTIONAL("ip", _ip, "");
}

CInAES67::~CInAES67()
{
    _udpSock->closeSocket();
    delete _udpSock;
}

int  CInAES67::read(CvMIFrame* frame)
{
    int ret = VMI_E_OK;
    int currentDataOffsetForFrame = 0;

    //
    // Manage the connection
    //

    if (!_udpSock->isValid())
    {
        int result = E_OK;
        result = _udpSock->openSocket(_zmqip,
                _ip, _port, true);
        if (result != E_OK)
            LOG_ERROR("%s: can't create UDP socket on [%s]:%d'", _name.c_str(),
                    _zmqip, _port);
        else
            LOG_INFO("%s: ok to create UDP socket on [%s]:%d'", _name.c_str(),
                    _zmqip, _port);
    }


    if (_udpSock->isValid()) {

        char rtpData[RTP_MAX_FRAME_LENGTH] = { 0 };

        frame->createFrameUninitialized(MaxAudioFrameSize + CFrameHeaders::GetHeadersLength());
        frame->memset(0);
        
        _headers.SetMediaSize(MaxAudioFrameSize);
        /*if (!_pData)
        {
            _pData =
                new char[MaxAudioFrameSize + CFrameHeaders::GetHeadersLength()];
            _headers.SetMediaSize(MaxAudioFrameSize);
        }
        memset(getCurrentVideoFrame(), 0, MaxAudioFrameSize);*/
        while (!_audioParametersDetected
            || currentDataOffsetForFrame < _headers.GetMediaSize())
        {
            int len = RTP_MAX_FRAME_LENGTH;
            int result = _udpSock->readSocket(rtpData, &len);
            if (result < 0)
            {
                LOG_ERROR("%s: error when read RTP frame: size read=%d, result=%d",
                    _name.c_str(), len, result);
                return -1;
            }
            CRTPFrame rtpFrame((unsigned char *)rtpData, len);
            currentDataOffsetForFrame += _audioParametersDetected
                * (((0x10000 + rtpFrame._seq - _lastSeq - 1) % 0x10000)) * AudioPCMDepth
                * _headers.GetChannelNb() * _headers.GetPacketTime()
                * (SamplesPerMs[_headers.GetSampleRate()]) / 1000;
            int toCopy = MIN(_headers.GetMediaSize() - currentDataOffsetForFrame,
                (int)rtpFrame.getSize() - RTP_HEADERS_LENGTH);
            std::memcpy(frame->getMediaBuffer() + currentDataOffsetForFrame,
                rtpFrame.getData() + RTP_HEADERS_LENGTH, toCopy);
            if (!_audioParametersDetected)
            {
                if (_lastSeq != -1 && ((_lastSeq + 1) & 0xffff) == rtpFrame._seq)
                {
                    /*How many samples per packet*/
                    int samplesPerPacket = rtpFrame._timestamp - _lastTimestamp;
                    if (samplesPerPacket * 1000 == 96 * _headers.GetPacketTime())
                    {
                        _headers.SetSampleRate(S_96KHz);
                        LOG("Detected 96 Khz audio");
                    }
                    else if (samplesPerPacket * 1000
                        == 48 * _headers.GetPacketTime())
                    {
                        _headers.SetSampleRate(S_48KHz);
                        LOG("Detected 48Khz audio");
                    }
                    else
                    {
                        LOG_ERROR("Audio Sampling rate not supported");
                    }
                    /*How many channels?*/
                    _headers.SetChannelNb(
                        (rtpFrame.getSize() - RTP_HEADERS_LENGTH)
                        / (samplesPerPacket * AudioPCMDepth));
                    LOG("Detected %d audio channels", _headers.GetChannelNb());
                    _headers.SetMediaSize(
                        AudioFrameDuration * AudioPCMDepth
                        * _headers.GetChannelNb() * samplesPerPacket
                        * 1000 / _headers.GetPacketTime());
                    LOG("Detected Media size: %d", _headers.GetMediaSize());
                    _audioParametersDetected = true;
                }
            }
            if (_audioParametersDetected)
                currentDataOffsetForFrame += toCopy;
            _lastSeq = rtpFrame._seq;
            _lastTimestamp = rtpFrame._timestamp;
        }
        _headers.SetFrameNumber(_headers.GetFrameNumber() + 1);
        _headers.SetMediaTimestamp(_lastTimestamp);
        _headers.SetSrcTimestamp(_lastTimestamp);
        CFrameHeaders* h = frame->getMediaHeaders();
        *h = _headers;
        h->WriteHeaders((unsigned char *)frame->getFrameBuffer(),
            _headers.GetFrameNumber() + 1);
    }
    return 0;
}

void CInAES67::stop()
{
    LOG("%s: -->", _name.c_str());
    if (_udpSock && _udpSock->isValid())
    {
        _udpSock->closeSocket();
    }
    CIn::stop();
    LOG("%s: <--", _name.c_str());
}

void CInAES67::reset()
{
}

PIN_REGISTER(CInAES67,"aes67")
