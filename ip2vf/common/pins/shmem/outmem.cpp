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


// shared memory :
// > ipcs - m
// > ipcrm shm <shm_id>  // remove a shared memory segment

using namespace std;

/**********************************************************************************************
*
* COutMem
*
***********************************************************************************************/

COutMem::COutMem(CModuleConfiguration* pMainCfg, int nIndex)
    : COut(pMainCfg, nIndex)
#ifdef _WIN32
    , _shm_id(INVALID_HANDLE_VALUE)
#else
    , _shm_id(-1)
#endif

{
    LOG("%s: -->", _name.c_str());
    pMainCfg->dump();

    _nType      = PIN_TYPE_SHMEM;
    _shm_size   = 0;
    _shm_data   = NULL;
    _shm_key    = 5000;
    _firstFrame = true;
    _sessionId  = 0LL;

    // Keep parameters
    PROPERTY_REGISTER_MANDATORY("control",   _port,      -1);
    PROPERTY_REGISTER_OPTIONAL( "ip",        _ip,        "localhost");
    PROPERTY_REGISTER_OPTIONAL( "interface", _interface, "");
    PROPERTY_REGISTER_OPTIONAL( "fmt",       _shm_nbseg, 1);

    LOG("%s: <--", _name.c_str());
}

COutMem::~COutMem() 
{
    LOG("%s: -->", _name.c_str());

    // Delete the udp connectivity 
    if (!_udpSock.isValid())
        _udpSock.closeSocket();

    // delete the memory segment
    if(_shm_data!=NULL)
        tools::deleteSHMSegment(_shm_data, _shm_id);

    LOG("%s: <--", _name.c_str());
}

/*!
* \fn send
* \brief Extract audio data from SMPTE frame
*
* \param pOutputBuffer pointer to the output buffer
* \return size of the copied audio data
*/
int COutMem::send(CvMIFrame* frame)
{
    LOG("%s: --> frame=0x%x, size=%d", _name.c_str(), frame, (frame?frame->getFrameSize():-1));
    int result = E_OK;
    int ret = 0;

    //
    // Manage the shared memory segment, if needed
    //

    if (_shm_data == NULL || _shm_nbseg * frame->getFrameSize() > _shm_size) {

        if (_shm_data != NULL) {
            tools::deleteSHMSegment(_shm_data, _shm_id);
            _shm_data = NULL;
        }
        _shm_size = _shm_nbseg * frame->getFrameSize();
        LOG_INFO("Get another shmem segment of size %d bytes, _shm_key=%d, nbSeg=%d. Frame size=%d", _shm_size, _shm_key, _shm_nbseg, frame->getFrameSize());
        _shm_data = tools::createSHMSegment_ext(_shm_size, _shm_key, _shm_id, false);
        if (_shm_data == NULL) {
            LOG_ERROR("%s: ***ERROR*** failed to create shared memory segment. Aborting!!.", _name.c_str());
            exit(1);
        }

        _shm_wr_pt = 0;
        _sessionId = tools::getCurrentTimeInMilliS();
        LOG_INFO("%s: Ok to open shmem (key=%d) of size=%d", _name.c_str(), _shm_key, _shm_size);
    }
 
    //
    // Manage the udp connection (to send notification)
    //

    if (!_udpSock.isValid()) {

        result = _udpSock.openSocket((char *)_ip, NULL, _port, false, _interface);
        if (result != E_OK)
            LOG_ERROR("%s: can't create %s UDP socket on [%s]:%d on interface '%s'",
                _name.c_str(), "outgoing", "ANY", _port,
                _interface[0] == '\0' ? "<default>" : _interface);
        else
            LOG_INFO("%s: Ok to create %s UDP socket on [%s]:%d on interface '%s'",
                _name.c_str(), "outgoing", "ANY", _port, 
                _interface[0] == '\0' ? "<default>" : _interface);
    }

    // Update ModuleId
    //frame->set_header(MODULE_ID, _nModuleId);
    if (_firstFrame) {
        LOG_INFO("Dump output IP2vf headers:");
        CFrameHeaders* headers = frame->getMediaHeaders();
        headers->DumpHeaders();
        _firstFrame = false;
    }

    //
    // Propagate data
    //

    if (_shm_data != NULL) {

        // Copy the buffer to the SHM
        //LOG_INFO("_shm_size=%d, size=%d to shmkey=%d", _shm_size, frame->getFrameSize(), _shm_key);
        int memoffset = _shm_wr_pt * frame->getFrameSize();
        result = frame->copyFrameToMem((unsigned char*)_shm_data + memoffset, _shm_size / _shm_nbseg);
        if (result != VMI_E_OK) {
            LOG_ERROR("Invalid frame, _shm_data=0x%x, offset=%d", _shm_data, memoffset);
            ret = E_ERROR;
        }

        // Some logging stuff
        if (getLogLevel() > LOG_LEVEL_WARNING) {
            //int n = headers->GetFrameNumber();
            //LOG("%s: send frame %d", _name.c_str(), n);
            //LOG("%s: payload= 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x", _name.c_str(), (unsigned char)_pData[24], (unsigned char)_pData[25], (unsigned char)_pData[26], (unsigned char)_pData[27], (unsigned char)_pData[28]);
        }

        // Notify next node via udp queue
        if (_udpSock.isValid() && ret!=E_ERROR) {
            // just notify the next module
            char msg[VMI_MEM_MSG_LEN];
            snprintf(msg, VMI_MEM_MSG_LEN, VMI_MEM_MSG_FORMAT, _shm_key, memoffset, _sessionId);
            int len = (int)strlen(msg);
            int rc = _udpSock.writeSocket(msg, &len);
            if (rc < 0)
                LOG("%s: ***ERROR*** zmq_send failed with errno=%s", _name.c_str(), strerror(errno));    // Not an error, it can be because the recv is not here, and we already fill the queue
            LOG("%s: write frame of size=%d on shmem", _name.c_str(), _shm_size);
            _shm_wr_pt = (_shm_wr_pt + 1) % _shm_nbseg;
        }
    }

    LOG("%s: <-- ", _name.c_str());
    return ret;
}

bool COutMem::isConnected() 
{
#ifdef _WIN32
    return (_shm_id != INVALID_HANDLE_VALUE);
#else
    return (_shm_id != -1);
#endif
}

PIN_REGISTER(COutMem, "shmem");
