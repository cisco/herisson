#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#ifdef _WIN32
#else
#include <unistd.h>     // usleep
#endif

#include "common.h"
#include "log.h"
#include "tools.h"
#include "in.h"
#include "configurable.h"
using namespace std;

/**********************************************************************************************
*
* CInFile
*
***********************************************************************************************/

CInFile::CInFile(CModuleConfiguration* pMainCfg, int nIndex) : CIn(pMainCfg, nIndex)
{
    LOG_INFO("%s: --> <--", _name.c_str());

    _nType = PIN_TYPE_FILE;
    PROPERTY_REGISTER_MANDATORY("filename", _filename, "");
    PROPERTY_REGISTER_MANDATORY("fps", _fps, 0.0f);
    _frame_time_in_microsec = (int)(1000*1000/(float)_fps);
    _time = 0.0f;

    _f.open (_filename, ios::in | ios::binary |ios::ate);
    if( _f.is_open() ) {
        _file_size = (int)_f.tellg();
        LOG("%s: file=%s, size=%d, frametime=%d microsec", _name.c_str(), _filename, _file_size, _frame_time_in_microsec);
        _f.seekg (0, ios::beg);
    }
    else {
        THROW_CRITICAL_EXCEPTION(_name + " can't open file: " + _filename + ", error: " + strerror(errno));
    }
}

CInFile::~CInFile() 
{
    if( _f.is_open() )  _f.close();
}

int CInFile::read(char* buffer) 
{
    int ret = 0;
    double currenttime = tools::getCurrentTimeInS();
    double deltatime_in_microsec = (currenttime - _time) * 1000000;
    int timetosleep_in_microsec = (int)(_frame_time_in_microsec - deltatime_in_microsec);
    //LOG("%s: cur=%.4f, delta=%.4f, timetosleep=%d", _name, currenttime, deltatime, timetosleep);
    if( timetosleep_in_microsec > 0 ) 
        usleep( timetosleep_in_microsec );
    _time = tools::getCurrentTimeInS();

    _headers.WriteHeaders((unsigned char*)buffer);
    int offset = _headers.GetHeadersLength();
    _f.read (buffer + offset, _mediaFrameSize);
    int read_size = (int)_f.gcount();

    if( read_size == 0 ) {
        _f.clear();
        _f.seekg (0, ios::beg);
        _f.read (buffer + offset, _mediaFrameSize);
        read_size = (int)_f.gcount();
    }
    //DumpHeader((unsigned char*)buffer);

    if( read_size < _mediaFrameSize) {
        LOG("%s: ***ERROR*** read incomplete frame, size=%d<%d", _name.c_str(), read_size, _mediaFrameSize);
    }
    LOG("%s: read=%d", _name.c_str(), read_size);
    return ret;
}

PIN_REGISTER(CInFile,"file");
