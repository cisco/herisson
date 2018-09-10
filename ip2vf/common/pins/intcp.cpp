#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "in.h"
#include "tcp_basic.h"

using namespace std;

/**********************************************************************************************
*
* CInTCP
*
***********************************************************************************************/

CInTCP::CInTCP(CModuleConfiguration* pMainCfg, int nIndex) : CIn(pMainCfg, nIndex)
{
    LOG_INFO("%s: --> <--", _name.c_str());
    _nType      = PIN_TYPE_TCP;
    _firstFrame = true;
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_OPTIONAL("ip", _ip, "");
    PROPERTY_REGISTER_OPTIONAL("interface", _interface, "");
    _isListen   = (_ip[0]=='\0');
}

CInTCP::~CInTCP()
{
    _tcpSock.closeSocket();
}

void CInTCP::reset()
{
    LOG_INFO("%s: --> <--", _name.c_str());
    _tcpSock.closeSocket();
}

int CInTCP::read(CvMIFrame* frame)
{
    LOG("%s: --> <--", _name.c_str());
    int result = E_OK;

    if (frame == NULL) {
        LOG_ERROR("Error: invalid frame...");
        return VMI_E_OK;
    }

    //
    // Manage the connection
    //

    if( !_tcpSock.isValid() ) {

        if( _isListen )
            result = _tcpSock.openSocket((char*)C_INADDR_ANY, _port, _interface);
        else 
            result = _tcpSock.openSocket(_ip, _port, _interface);
        if (result != E_OK) {
            // Not really an error, just that there is no more previous module in pipeline
            LOG("%s: can't create %s TCP socket on [%s]:%d on interface '%s'",
                _name.c_str(), (_isListen ? "listening" : "connected"), (_isListen ? "NULL" : _ip), _port, _interface[0] == '\0' ? "<default>" : _interface);
            // Do a small pause (100ms) before try to reconnect... this is to not stress CPU.
            usleep(100000);
            return VMI_E_FAILED_TO_OPEN_SOCKET;
        }
        else
            LOG_INFO("%s: Ok to create %s TCP socket on [%s]:%d on interface '%s'", 
                _name.c_str(), (_isListen?"listening":"connected"), (_isListen?"NULL":_ip), _port, _interface[0]=='\0'?"<default>":_interface);
    }

    //
    // Receive data
    //

    if (_tcpSock.isValid()) {

        // Create a new vMI frame from the tcp input connection
        result = frame->createFrameFromTCP(&_tcpSock, _nModuleId);
        if (result != VMI_E_OK) {
            if (result == VMI_E_FAILED_TO_RCV_SOCKET)
                // Not really an error...
                _tcpSock.closeSocket();
            else
                LOG_ERROR("errors when try to get vMI frame...");
            return VMI_E_INVALID_FRAME;
        }
        else if (_firstFrame) {
            LOG_INFO("Dump received IP2vf headers:");
            CFrameHeaders* headers = frame->getMediaHeaders();
            headers->DumpHeaders();
            _firstFrame = false;
        }

    }

    return VMI_E_OK;
}

void CInTCP::stop()
{
    if (_tcpSock.isValid())
    {
        _tcpSock.closeSocket();
    }
    CIn::stop();
}

PIN_REGISTER(CInTCP, "tcp")


