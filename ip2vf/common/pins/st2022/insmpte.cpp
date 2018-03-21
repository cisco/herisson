#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#ifdef _WIN32
#else
#include <unistd.h>     // usleep
#endif

#include <pins/pins.h>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "rtpframe.h"
#include "datasource.h"

using namespace std;

#define MAX_NB_SMPTE_FRAME  2


/**********************************************************************************************
*
* CInSMPTEFile
*
***********************************************************************************************/

CInSMPTE::CInSMPTE(CModuleConfiguration* pMainCfg, int nIndex) : CIn(pMainCfg, nIndex)
{
    LOG_INFO("%s: --> <--", _name.c_str());

    _nType = PIN_TYPE_SMPTE;
    _source = NULL;
    _firstFrame = true;
    _bStarted = false;
    _currentFrame = NULL;
    _nbSMPTEFrameToQueue = MAX_NB_SMPTE_FRAME;
    _streamType = SMPTE_STANDARD_SUITE_NOT_DEFINED;

    PROPERTY_REGISTER_OPTIONAL("fmt", _fmt, 10);
    PROPERTY_REGISTER_OPTIONAL("queuesize", _nbSMPTEFrameToQueue, MAX_NB_SMPTE_FRAME);
    
    LOG_INFO("Nb SMPTE Frame to queue=%d", _nbSMPTEFrameToQueue);

    // Detect and init the source from the PIN configuration
    _source = CDMUXDataSource::create(_pConfig);

    // Create a fix number of SMPTE frame
    for (int i = 0; i < _nbSMPTEFrameToQueue; i++) {
        _smpteFrameArray.push_back(new SmpteFrameBuffer());
    }
}

CInSMPTE::~CInSMPTE()
{
    LOG_INFO("%s: -->", _name.c_str());
    
    if (_source != NULL)
        delete _source;

    for (int i = 0; i < _smpteFrameArray.size(); i++)
        delete _smpteFrameArray[i];
    _smpteFrameArray.clear();

    LOG_INFO("%s: <--", _name.c_str());
}

int CInSMPTE::_rcv_process() {

    unsigned char   rtp_packet[RTP_PACKET_SIZE];
    int             lastSeq = -1, result;
    int             frameCounter = 0;
    int             sampleSize = RTP_PACKET_SIZE;

    //Blocking all other signals
#ifndef WIN32
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_SETMASK, &mask, NULL);
#endif

    LOG_INFO("%s: -->", _name.c_str());
    int queueSize = (int)_smpteFrameArray.size();
    int framePointer = 0;
    LOG_INFO("%s: Use %d frame buffer on input", _name.c_str(), queueSize);

    if (_source == NULL)
        return 0;

    // Get First packet to identify kind of stream
    sampleSize = _source->getSampleSize();
    result = _source->read((char*)rtp_packet, sampleSize);
    if (result <= 0) {
        LOG_ERROR("%s: Can't read from source, result=%d", _name.c_str(), result);
        return 0;
    }
    CRTPFrame rtp(rtp_packet, result);
    if (rtp._pt == 98) {
        _streamType = SMPTE_2022_6;
        LOG_INFO("%s: RECEIVE SMPTE_2022_6 standard suite", _name.c_str());
    }
    else if (rtp._pt == 96) {
        _streamType = SMPTE_2110_20;
        LOG_INFO("%s: RECEIVE SMPTE_2110_20 standard suite", _name.c_str());
    }
    else {

    }

    // The samplesize can be updated by the source following the receive of the first packet...
    sampleSize = _source->getSampleSize();
    LOG_INFO("%s: Detect samplesize=%d", _name.c_str(), sampleSize);

    while (_bStarted) {

        _source->waitForNextFrame();

        SmpteFrameBuffer* pFrame = _smpteFrameArray[framePointer];
        std::lock_guard<std::mutex> lock(pFrame->_lock);
        //LOG_INFO("%s: start to receive a SMPTE frame on buffer %d", pin->_name.c_str(), framePointer);
        pFrame->_frame.initNewFrame();
        pFrame->_frame.setFrameNumber(frameCounter++);

        while (!pFrame->_frame.isComplete()) {

            // First, keep the full RTP frame from the current UDP packet
            result = _source->read((char*)rtp_packet, sampleSize);

            // Detect stop
            if (!_bStarted)
                break;

            if (result <= 0) {
                if (result == VMI_E_NOT_PRIMARY_SRC || result == VMI_E_PACKET_LOST)
                    // Not really an error, continue to accumulate packets for this frame
                    continue;
                // Otherwise, break to lost this frame
                break;
            }
            else if (result != sampleSize)
                LOG_ERROR("%s: incorrect packet size, size=%d, wanted=%d", _name.c_str(), result, sampleSize);

            CRTPFrame frame(rtp_packet, result);

            //LOG_INFO("read=%d, frame._seq=%d", result, frame._seq);

            // As soon as possible, prevent duplicated packet
            if (lastSeq == frame._seq)
                continue;
            lastSeq = frame._seq;

            pFrame->_frame.addRTPPacket(&frame);
        }

        // Detect stop
        if (!_bStarted)
            break;

        if (result <= 0) {
            // An error occured. This frame is lost. Retry to get a valid frame
            pFrame->_frame.abortCurrentFrame();
            continue;
        }

        //LOG_INFO("%s: SMPTE frame on buffer %d COMPLETED", pin->_name.c_str(), framePointer);
        _q.push(framePointer);
        framePointer = (framePointer + 1) % queueSize;
    }

    LOG_INFO("%s: <--", _name.c_str());
    return 0;
}

SmpteFrameBuffer* CInSMPTE::_get_next_frame() {
    LOG("%s: --> <--", _name.c_str());
    int ret = 0, mediasize = 0;

    // Wait for an available frame
    //LOG_INFO("%s: Wait for an available SMPTE frame...", _name.c_str());
    int framePointer = _q.pop();
    if (framePointer == -1)
        return NULL;
    //LOG_INFO("%s: A new frame is available (#%d)", _name.c_str(), framePointer);
    SmpteFrameBuffer* pFrame = _smpteFrameArray[framePointer];
    std::lock_guard<std::mutex> lock(pFrame->_lock);

    int frameNumber = pFrame->_frame.getFrameNumber();
    LOG("frame #%d readed, videoFrameLen=%d", frameNumber, pFrame->_frame.getFrameSize());
    if (_firstFrame) {
        LOG_INFO("%s: First frame initialisation", _name.c_str());
        mediasize = pFrame->_frame.getFrameSize();
        if (mediasize <= 0) {
            LOG_ERROR("%s: Unexpected and incorrect media size (%d bytes)... Abort.", _name.c_str(), mediasize);
            exit(0);
        }
        LOG_INFO("%s: Use a mediasize of %d bytes", _name.c_str(), mediasize);
        _firstFrame = false;
        float fps = pFrame->_frame.getProfile()->getFramerate();
        LOG_INFO("%s: Use framerate defined in profile: %.2f", _name.c_str(), fps);
        if (_source->getType() == DataSourceType::TYPE_FILE)
            (static_cast<CFileDataSource*>(_source))->setFrameRate(fps);
    }

    return pFrame;
}

int CInSMPTE::read(CvMIFrame* frame)
{
    // If current SMPTE frame entirely consumed, get a new SMPTE frame
    if (_currentFrame == NULL || _currentFrame->_frame.getNbOfMediaBuffer()==0 ) {
        _currentFrame = _get_next_frame();
    }

    // If available media buffer on the current frame, take next media buffer and
    // create a new vMIFrame from it.
    if (_currentFrame && _currentFrame->_frame.getNbOfMediaBuffer() > 0) {
        SMPTEFRAME_BUFFERS buf = _currentFrame->_frame.getNextAvailableMediaBuffer();
        MEDIAFORMAT kindOfBuffer = _currentFrame->_frame.getMediaBufferType(buf);
        switch (kindOfBuffer) {
        case MEDIAFORMAT::VIDEO:
            frame->createVideoFromSmpteFrame(&_currentFrame->_frame, buf, _nModuleId); 
            if (_fmt == 8) {
                CFrameHeaders* headers = frame->getMediaHeaders();
                int offset = CFrameHeaders::GetHeadersLength();
                int mediasize = headers->GetMediaSize();
                tools::convert10bitsto8bits(frame->getMediaBuffer(), mediasize, frame->getMediaBuffer());
                mediasize = mediasize * 8 / 10;
                headers->SetMediaSize(mediasize);
                headers->SetDepth(8);
                headers->WriteHeaders(frame->getFrameBuffer());
            }
            break;
        case MEDIAFORMAT::AUDIO:
            frame->createAudioFromSmpteFrame(&_currentFrame->_frame, buf, _nModuleId); break;
        case MEDIAFORMAT::ANC:
            // TODO
            return -1;
        default: 
            // ERROR
            return -1;
        }
    }
    else {
        // Error
        return -1;
    }

    return 0;
}

void CInSMPTE::start()
{
    LOG("%s: -->", _name.c_str());
    _bStarted = true;

    // Detect and init the source from the PIN configuration
    if (_source)
        _source->init(_pConfig);

    // start
    CIn::start();

    // Start the receive thread
    _t = std::thread([this] { _rcv_process(); });

    LOG("%s: <--", _name.c_str());
}

void CInSMPTE::stop()
{
    LOG("%s: -->", _name.c_str());

    LOG_INFO("%s: stop thread -->", _name.c_str());
    _bStarted = false;
    if (_source)
        _source->close();
    _q.push(-1);    // In case of, to unblock blocking pop
    _t.join();
    LOG_INFO("%s: stop thread <-- ", _name.c_str());

    if (_source)
        _source->close();
    CIn::stop();
    LOG("%s: <--", _name.c_str());
}
PIN_REGISTER(CInSMPTE,"smpte");
