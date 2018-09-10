#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#ifdef _WIN32
#define _WINSOCKAPI_
#include <windows.h>
#endif

#include <pins/pins.h>
#include "common.h"
#include "log.h"
#include "tools.h"

using namespace std;

// shared memory :
// > ipcs - m
// > ipcrm shm <shm_id>  // remove a shared memory segment


/**********************************************************************************************
*
* CInMem
*
***********************************************************************************************/

CInMem::CInMem(CModuleConfiguration* pMainCfg, int nIndex)
 : CIn(pMainCfg, nIndex)
#ifdef _WIN32
    , _shm_id(INVALID_HANDLE_VALUE)
#else
    , _shm_id(-1)
#endif
{
    LOG("%s: -->", _name.c_str());
    
    _nType      = PIN_TYPE_SHMEM;
    _shm_size   = 0;
    _shm_data   = NULL;
    _shm_key    = 0;
    _firstFrame = true;
    _sessionId  = 0LL;

    PROPERTY_REGISTER_MANDATORY("control",   _port,      -1);
    PROPERTY_REGISTER_OPTIONAL( "fmt",       _shm_nbseg, 1);
    PROPERTY_REGISTER_OPTIONAL( "interface", _interface, "");

    if (_port <= 0) {
        LOG_ERROR("%s: ***ERROR*** invalid parameter: port=%d. aborting", _name.c_str(), _port);
        exit(1);
    }

    LOG("%s: <--", _name.c_str());
}

CInMem::~CInMem() 
{
    // Delete the udp connectivity 
    _udpSock.closeSocket();

    // delete the memory segment
    if(_shm_data != NULL)
        tools::detachSHMSegment(_shm_data);
}

void CInMem::_checkMemorySegment(int shmkey, long long sessionId) {

    if (_shm_data == NULL || shmkey != _shm_key || sessionId != _sessionId) {

        LOG_INFO("Open memory segment with key=%d", shmkey);
        // Delete any previous seg memory segment
        if (_shm_data != NULL)
            tools::detachSHMSegment(_shm_data);

        // First, open the segment just enough to read headers. Goal was to retreive the complete frame size...
        _shm_key = shmkey;
        _shm_size = CFrameHeaders::GetHeadersLength();
        _shm_data = tools::getSHMSegment(_shm_size, _shm_key, _shm_id);
        if (_shm_data == NULL) {
            LOG_ERROR("%s: ***ERROR*** failed to create shared memory segment of size %d. Aborting!!.", _name.c_str(), _shm_size);
            return;
        }
        CFrameHeaders headers;
        int result = headers.ReadHeaders((unsigned char*)_shm_data);
        if (result != VMI_E_OK) {
            LOG_ERROR("%s: ***ERROR*** failed to read headers", _name.c_str());
            return;
        }
#ifdef _WIN32
        _shm_size = (headers.GetMediaSize() + CFrameHeaders::GetHeadersLength());
        LOG_INFO("Size of shmem seg=%d, size of a frame=%d", _shm_size, (headers.GetMediaSize() + CFrameHeaders::GetHeadersLength()));
#else
        LOG_INFO("Size of shmem seg=%d, size of a frame=%d", tools::getSHMSegmentSize(_shm_id), (headers.GetMediaSize() + CFrameHeaders::GetHeadersLength()));
        if (tools::getSHMSegmentSize(_shm_id) < (headers.GetMediaSize() + CFrameHeaders::GetHeadersLength())) {
            LOG_ERROR("%s: ***ERROR*** shmem size is too short for a frame...", _name.c_str());
            return;
        }
        _shm_size = tools::getSHMSegmentSize(_shm_id);
#endif
        tools::detachSHMSegment(_shm_data);
        _shm_data = tools::getSHMSegment(_shm_size, _shm_key, _shm_id);
        if (_shm_data == NULL) {
            LOG_ERROR("%s: ***ERROR*** failed to create shared memory segment of size %d. Aborting!!.", _name.c_str(), _shm_size);
            return;
        }
        _sessionId = sessionId;
        LOG_INFO("Opening of memory segment with key=%d and size=%d completed", _shm_key, _shm_size);
    }
}


int CInMem::read(CvMIFrame* frame)
{
    int result = VMI_E_OK;

    /*if (!frame) {
        LOG_ERROR("Error: invalid frame...");
        return VMI_E_OK;
    }*/

    //
    // Manage the connection
    //

    if (!_udpSock.isValid()) {
        result = _udpSock.openSocket(NULL, NULL, _port, true ,(char *)_interface);
        if (result != E_OK) {
            LOG_ERROR("%s: can't create %s UDP socket on [%s]:%d on interface '%s'",
                _name.c_str(), "listening", "ANY", _port,
                _interface[0] == '\0' ? "<default>" : _interface);
            return VMI_E_FAILED_TO_OPEN_SOCKET;
        }
        else
            LOG_INFO("%s: Ok to create %s UDP socket on [%s]:%d on interface '%s'",
                _name.c_str(), "listening", "ANY", _port, 
                _interface[0] == '\0' ? "<default>" : _interface);
    }

    //
    // Manage the data
    //

    int ret = 0;
    char msg[VMI_MEM_MSG_LEN];
    LOG("%s: wait for msg....", _name.c_str());
    if (_udpSock.isValid()) {
        int len = sizeof(msg)/sizeof(char);
        int num = _udpSock.readSocket(msg, &len);        //int num = zmq_recv(_zmq_receiver, msg, 32, 0);
        if (num > 0)
        {
            int shmkey = 0, memoffset = 0;
            long long sessionId = 0LL;
            msg[num] = '\0';
            LOG("%s: recv '%d' bytes, msg='%s'", _name.c_str(), num, msg);
            sscanf(msg, VMI_MEM_MSG_FORMAT, &shmkey, &memoffset, &sessionId);

            _checkMemorySegment(shmkey, sessionId);
            if (_shm_data != NULL && frame == NULL) {
                CFrameHeaders headers;
                result = headers.ReadHeaders((unsigned char*)_shm_data + memoffset);
                if (result != VMI_E_OK) {
                    LOG_ERROR("%s: ***ERROR*** (2) failed to read headers", _name.c_str());
                    return E_ERROR;
                }
                LOG_ERROR("Drop the frame #%d", headers.GetFrameNumber());
            }
            else if (_shm_data != NULL) {
                int nb; 
                CFrameHeaders headers;
                result = headers.ReadHeaders((unsigned char*)_shm_data + memoffset);
                if (result != VMI_E_OK) {
                    LOG_ERROR("%s: ***ERROR*** (3) failed to read headers", _name.c_str());
                    return E_ERROR;
                }
                result = frame->createFrameFromMem((unsigned char*)_shm_data + memoffset, (headers.GetMediaSize()+CFrameHeaders::GetHeadersLength()), _nModuleId);
                if (result == VMI_E_OK) {
                    frame->get_header(MEDIA_FRAME_NB, &nb);
                    if (_firstFrame) {
                        LOG_INFO("Dump received IP2vf headers:");
                        CFrameHeaders* headers = frame->getMediaHeaders();
                        headers->DumpHeaders();
                        _firstFrame = false;
                    }
                    LOG("%s: read frame number %d (size=%d) from shmem on page %d", _name.c_str(), nb, _shm_size, _shm_data);
                }
                else
                    ret = E_ERROR;
            }
        }
        else if (num < 0) {
            LOG("%s: ***ERROR*** recv failed with errno=%s", _name.c_str(), strerror(errno));
            ret = E_ERROR;
        }
    }
    return ret;
}

void CInMem::start()
{
    LOG("%s: -->", _name.c_str());

    // start
    CIn::start();

    LOG("%s: <--", _name.c_str());
}

void CInMem::stop()
{

    if (_udpSock.isValid())
    {
        _udpSock.closeSocket();
    }
    CIn::stop();
}

PIN_REGISTER(CInMem,"shmem");
