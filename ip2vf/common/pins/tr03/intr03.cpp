/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include "intr03.h"

CInTR03::CInTR03(CModuleConfiguration* pMainCfg, int nIndex) :
        CIn(pMainCfg, nIndex)
{
    LOG_INFO("%s: --> <--", _name.c_str());
    _nType = PIN_TYPE_TR03;
    _isListen = true;
    _lastSeq = -1;
    _frameNb = 0;
    _firstFrame = true;
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_OPTIONAL("w", _w, 1920);
    PROPERTY_REGISTER_OPTIONAL("h", _h, 1080);
    PROPERTY_REGISTER_OPTIONAL("fmt", _fmt, 8);
    PROPERTY_REGISTER_OPTIONAL("mcastgroup", _zmqip, "");
    PROPERTY_REGISTER_OPTIONAL("ip", _ip, "");
    PROPERTY_REGISTER_OPTIONAL("interface", _interface, "");
#ifdef USE_NETMAP
    _udpSock =
            (strncmp(_interface, "netmap-", 7) == 0) ?
                    new Netmap() : new UDP();
#else
    _udpSock = new UDP();
#endif

    /**********************************************************
     if (_pConfig->_fmt == 1)
     _smpteFrame.setInterlacedMode(false);
     */

    //create a new frame factory:
    _tr03FrameParser.reset(new CTR03FrameParser(
    //width of frame in pixels:
            _w
            //height of frame in pixels:
            ,_h
            //depth in bits per pixel
            , (2*_fmt)
            //the size of the frame in bytes:
            , (_w*_h*2*_fmt/8)));

    // Init Internal format header
    SetModuleId(_nModuleId);
    InitVideoHeadersFromTR03(_w, _h,
            SAMPLINGFMT::YCbCr_4_2_2, _fmt, false);
}

CInTR03::~CInTR03()
{
    _udpSock->closeSocket();
    delete _udpSock;
}

void CInTR03::reset()
{
    LOG_WARNING("no reset mechanism defined for tr03 input pin");
}

int CInTR03::read(CvMIFrame* vmiFrame)
{
    LOG("%s: --> <--", _name.c_str());
    int offset = GetHeadersLength();

    //
    // Manage the connection
    //
    if (!_udpSock->isValid())
    {
        int result = E_OK;
        if (_isListen)
            result = _udpSock->openSocket(_zmqip, _ip, _port, true, _interface);
        else
            result = _udpSock->openSocket(NULL, NULL, _port, false, _interface);
        if (result != E_OK)
            LOG_ERROR(
                    "%s: can't create %s UDP socket on [%s]:%d on interface '%s'",
                    _name.c_str(), (_isListen ? "listening" : "connected"), (_isListen ? "NULL" : _ip), _port,
                    _interface[0] == '\0' ? "<default>" : _interface);
        else
            LOG_INFO(
                    "%s: ok to create %s UDP socket on [%s]:%d on interface '%s'",
                    _name.c_str(), (_isListen ? "listening" : "connected"), (_isListen ? "NULL" : _ip), _port,
                    _interface[0] == '\0' ? "<default>" : _interface);
    }

    //
    // Manage recv of data
    //

    if (_udpSock->isValid()) {

        bool doneParsingFrame = false;
        _tr03FrameParser->resetFrame();

        while (!doneParsingFrame)
        {
            int len, result;

            // First, keep the full RTP frame from the current UDP packet
            len = RTP_MAX_FRAME_LENGTH;
            result = _udpSock->readSocket((char*) _RTPframe, &len);
            if (result == -1)
            {
                LOG_ERROR(
                        "%s: error when read RTP frame: size readed=%d, result=%d",
                        _name.c_str(), len, result);
            }
            CRTPFrame frame(_RTPframe, len);

            LOG("%s: read=%d, frame._seq=%d", _name.c_str(), result, frame._seq);
            // As soon as possible, prevent duplicate packet
            if (_lastSeq == frame._seq)
                continue;
            // Prevent no video frame (the seq number of a such packet can be different than the video one)
            if (frame._pt != 96) {
                LOG_ERROR("%s: read=%d, frame._pt=%d", _name.c_str(), result, frame._pt);
                continue;
            }

            _lastSeq = frame._seq;

            // called all rtp packets have been collected into a tr03 frame
            auto onCompleteFrameParsed =
                    [&]()
                    {
                        //if (_firstFrame)
                        //{
                            _firstFrame = false;
                            //int framesize = _w*_h * 2 * 10 / 8;
                            int framesize = _w*_h * 2 * _fmt / 8;
                            vmiFrame->createFrameUninitialized(framesize+ CFrameHeaders::GetHeadersLength());
                            //_videoFrameBuffer = new unsigned char [this->getVideoFrameSize()];
                            this->SetMediaSize(framesize);
                            //LOG_ERROR("First frame !");
                        //}
                        if (_frameNb == 4)
                        {
                            //We can detect framerate by comparing timestamps
                            unsigned int oldTimestamp = this->GetMediaTimestamp();
                            unsigned int interFramedelay = frame._timestamp - oldTimestamp;
                            switch(interFramedelay){
                            case 3000:
                                SetFramerateCode(0x16);
                                break;
                            case 3003:
                                SetFramerateCode(0x17);
                                break;
                            case 3600:
                                SetFramerateCode(0x18);
                                break;
                            case 3750:
                                SetFramerateCode(0x1a);
                                break;
                            default:
                                LOG_ERROR("Unsupported fps w/ delay: %d!", interFramedelay);
                                break;
                            }
                        }
                        //add propiatary header to buffer:
                        SetMediaTimestamp(frame._timestamp);
                        WriteHeaders((unsigned char*)vmiFrame->getFrameBuffer(), _frameNb++);
                        vmiFrame->refreshHeaders();
                        //read loop should be terminated after frame is parsed:
                        _tr03FrameParser->copyFrameContents(vmiFrame->getMediaBuffer());
                        doneParsingFrame = true;

                    };
            //parse an incomming rtp packet:
            _tr03FrameParser->addRtpFrame(&frame, onCompleteFrameParsed);

        }
    }

    return VMI_E_OK;
}
PIN_REGISTER(CInTR03,"tr03")
