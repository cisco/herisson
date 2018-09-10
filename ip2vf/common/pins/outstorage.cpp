#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cmath>
#include <chrono>

#include <pins/pins.h>
#include "common.h"
#include "log.h"
#include "tools.h"
#include "rtpframe.h"
#include "tcp_basic.h"

using namespace std;


/**********************************************************************************************
 *
 * COutStorage
 *
 ***********************************************************************************************/

COutStorage::COutStorage(CModuleConfiguration* pMainCfg, int nIndex) :
        COut(pMainCfg, nIndex)
{
    LOG("%s: --> <-- ", _name.c_str());
    _nType      = PIN_TYPE_STORAGE;

    PROPERTY_REGISTER_MANDATORY("ip",         _ip,    "");
    PROPERTY_REGISTER_MANDATORY("port",       _port,  -1);
    PROPERTY_REGISTER_OPTIONAL( "mtu",        _mtu,   1500);
    PROPERTY_REGISTER_OPTIONAL( "interface",  _interface, "");
    PROPERTY_REGISTER_OPTIONAL( "mcastgroup", _mcastgroup, "");

}

COutStorage::~COutStorage() {

}

int COutStorage::send(CvMIFrame* frame) {

    int result = VMI_E_OK;
    LOG("%s: --> <-- ", _name.c_str());

    return result;
}


bool COutStorage::isConnected()
{
    // TODO
    return true;
}

PIN_REGISTER(COutStorage, "storage")
