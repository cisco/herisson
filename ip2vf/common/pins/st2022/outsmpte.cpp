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

std::chrono::microseconds g_interframePseudoPacingDelay = std::chrono::microseconds((unsigned long long)1);

#define OUTSMPTE_STANDARD_2022_6    "2022_6"
#define OUTSMPTE_STANDARD_2110_20   "2110_20"

/**********************************************************************************************
 *
 * COutSMPTE
 *
 ***********************************************************************************************/

COutSMPTE::COutSMPTE(CModuleConfiguration* pMainCfg, int nIndex) :
        COut(pMainCfg, nIndex)
{
    LOG("%s: --> <-- ", _name.c_str());
    _nType      = PIN_TYPE_SMPTE;
    _standard   = SMPTE_STANDARD_SUITE_NOT_DEFINED;

    PROPERTY_REGISTER_MANDATORY("ip",         _ip,    "");
    PROPERTY_REGISTER_MANDATORY("port",       _port,  -1);
    PROPERTY_REGISTER_OPTIONAL( "mtu",        _mtu,   1500);
    PROPERTY_REGISTER_OPTIONAL( "port2",      _port2, -1);
    PROPERTY_REGISTER_OPTIONAL( "interface",  _interface, "");
    PROPERTY_REGISTER_OPTIONAL( "mcastgroup", _mcastgroup, "");
    PROPERTY_REGISTER_OPTIONAL( "dcast",      _useDeltacast, false);
    PROPERTY_REGISTER_OPTIONAL( "fmt",        _smptefmt, OUTSMPTE_STANDARD_2022_6);

    streamer = NULL;

    LOG_INFO("%s: %suse deltacast", _name.c_str(), (_useDeltacast ? "" : "NOT "));

    // Check format
    if (!strcmp(_smptefmt, OUTSMPTE_STANDARD_2022_6) == 0 && !strcmp(_smptefmt, OUTSMPTE_STANDARD_2110_20) == 0) {
        LOG_ERROR("%s: Invalid output format ('%s')", _name.c_str(), _smptefmt);
        LOG_ERROR("%s: Available format are:", _name.c_str());
        LOG_ERROR("%s:    - fmt=%s", _name.c_str(), OUTSMPTE_STANDARD_2022_6);
        LOG_ERROR("%s:    - fmt=%s", _name.c_str(), OUTSMPTE_STANDARD_2110_20);
        _standard = SMPTE_2022_6;
    }
    else if (strcmp(_smptefmt, OUTSMPTE_STANDARD_2022_6) == 0) {
        _standard = SMPTE_2022_6;
    }
    else if(strcmp(_smptefmt, OUTSMPTE_STANDARD_2022_6) == 0) {
        _standard = SMPTE_2110_20;
    }
}

COutSMPTE::~COutSMPTE() {

    if (streamer)
        delete streamer;
}

int COutSMPTE::send(CvMIFrame* frame) {

    int result = VMI_E_OK;

    if (!streamer) {
        if ((_standard == SMPTE_2022_6) && _useDeltacast)
            throw std::runtime_error("not supported");
        else if (_standard == SMPTE_2022_6)
            streamer = new CvMIStreamerCisco2022_6(_ip, _mcastgroup, _port, _pConfig, _interface);
        else if((_standard == SMPTE_2110_20) && _useDeltacast)
            throw std::runtime_error("not supported");
    }

    if (streamer) {
        result = streamer->send(frame);
    }
    return result;
}


bool COutSMPTE::isConnected()
{
    return (streamer ? streamer->isConnected(): false);
}

PIN_REGISTER(COutSMPTE,"smpte")
