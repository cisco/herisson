#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <pins/pinfactory.h>
#include "common.h"
#include "log.h"
#include "tools.h"
#include "tcp_basic.h"
#include "out.h"
#include "configurable.h"
using namespace std;

/**********************************************************************************************
*
* COutSocket
*
***********************************************************************************************/

COutTCP::COutTCP(CModuleConfiguration* pMainCfg, int nIndex) : COut(pMainCfg, nIndex) {

    _nType      = PIN_TYPE_TCP;
    PROPERTY_REGISTER_OPTIONAL("ip", _ip, "");
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_OPTIONAL("interface", _interface, "");
    _isListen   = (_ip[0]=='\0');
}

COutTCP::~COutTCP() {

    _tcpSock.closeSocket();
}

int COutTCP::send(CvMIFrame* frame) {

    LOG("%s: --> <--", _name.c_str());
    int result = E_OK;
    int ret = 0;

    //
    // Manage the connection
    //
    if( !_tcpSock.isValid() ) {
         if( _isListen )
            result = _tcpSock.openSocket((char*)C_INADDR_ANY, _port, _interface);
        else 
            result = _tcpSock.openSocket(_ip, _port, _interface);
        if( result != E_OK ) 
            LOG("%s: can't create %s TCP socket on [%s]:%d on interface '%s'", 
                _name.c_str(), (_isListen?"listening":"connected"), (_isListen?"NULL":_ip), _port, _interface[0]=='\0'?"<default>":_interface);
        else
            LOG_INFO("%s: Ok to create %s TCP socket on [%s]:%d on interface '%s'", 
                _name.c_str(), (_isListen?"listening":"connected"), (_isListen?"NULL":_ip), _port, _interface[0]=='\0'?"<default>":_interface);
    }

    //
    // Manage data
    //
    if (_tcpSock.isValid()) {

        int result = frame->sendToTCP(&_tcpSock);
        if (result != VMI_E_OK) {
            LOG_ERROR("%s: error when send frame", _name.c_str());
            _tcpSock.closeSocket();
            ret = -1;
        }
    }

    return ret;
}

bool COutTCP::isConnected() {

    return _tcpSock.isValid();
}


PIN_REGISTER(COutTCP,"tcp")
