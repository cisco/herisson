#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcpy, strerror, memcpy
#include <iostream>
#include <fstream>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "out.h"
#include <pins/pinfactory.h>
using namespace std;

/**********************************************************************************************
*
* COutDevNull
*
***********************************************************************************************/

COutDevNull::COutDevNull(CModuleConfiguration* pMainCfg, int nIndex) : COut(pMainCfg, nIndex)
{
    _nType = PIN_TYPE_DEVNULL;
}
COutDevNull::~COutDevNull() 
{
}

int  COutDevNull::send(CvMIFrame* frame) {
    return VMI_E_OK;
}

PIN_REGISTER(COutDevNull,"devnull")
