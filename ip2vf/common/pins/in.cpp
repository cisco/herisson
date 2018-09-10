#include <cstdio>
#include <cstdlib>
#include <cstring>	// strcopy
#include <iostream>
#include <fstream>

#include "common.h"
#include "log.h"
#include "frameheaders.h"
#include "tools.h"
#include "in.h"

using namespace std;

/**********************************************************************************************
*
* CIN
*
***********************************************************************************************/


CIn::CIn(CModuleConfiguration* pMainCfg, int nIndex)
{ 
    _mediaformat    = MEDIAFORMAT::VIDEO;
    _nIndex         = nIndex;
    _nModuleId      = pMainCfg->_id;
    _pConfig        = &pMainCfg->_in[0];
    _firstFrame     = true;
    _name           = std::string(pMainCfg->_name)+ std::string("[")+ std::to_string(_nIndex) + std::string("]");
};

TransportType CIn::getTransportType() {
    if (MEMORY_TYPE(_nType))
        return TRANSPORT_TYPE_MEMORY;
    else if(STREAMING_TYPE(_nType))
        return TRANSPORT_TYPE_STREAMING;

    LOG_ERROR("This pin is not associated with a transport type");
    return TRANSPORT_TYPE_NONE;
}


