#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcpy, strerror, memcpy
#include <iostream>
#include <fstream>
#include <sstream>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "out.h"

using namespace std;

/**********************************************************************************************
*
* COut
*
***********************************************************************************************/

COut::COut(CModuleConfiguration* pMainCfg, int nIndex)
{ 
    _mediaformat    = MEDIAFORMAT::VIDEO;
    _nIndex         = nIndex;
    _nModuleId      = pMainCfg->_id;
    _pConfig        = &pMainCfg->_out[0];
    _firstFrame     = true;
    _name           = std::string(pMainCfg->_name) + std::string("[") + std::to_string(_nIndex) + std::string("]");
}

TransportType COut::getTransportType() {
    if (MEMORY_TYPE(_nType))
        return TRANSPORT_TYPE_MEMORY;
    else if (STREAMING_TYPE(_nType))
        return TRANSPORT_TYPE_STREAMING;

    LOG_ERROR("This pin is not associated with a transport type");
    return TRANSPORT_TYPE_NONE;
}
