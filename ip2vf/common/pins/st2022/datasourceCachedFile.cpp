#include <cstdio>
#include <cstdlib>
#include <cstring>      // strerror
#include <string>
#include <fstream>      // for file saving
#include <iostream>     // for file saving
#include <sys/types.h>  // open
#include <sys/stat.h>   // open
#include <fcntl.h>      // open

#include "common.h"
#include "log.h"
#include "tools.h"
#include "datasource.h"
#include "moduleconfiguration.h"
#include "configurable.h"
#include "rtpframe.h"
using namespace std;

#define PCAP_FILE_HEADER_SIZE       24
#define PCAP_PACKET_OFFSET          58

CCachedFileDataSource::CCachedFileDataSource()
    : CDMUXDataSource() {

    _fps = 25.0f;
    _time = tools::getCurrentTimeInMicroS();
    _pcapFile = 0;
    _fileOffset = 0;
    _packetOffset = 0;
    _samplesize = RTP_PACKET_SIZE;  // by default, will be refresh 
    _type = DataSourceType::TYPE_FILE;
    _cache = NULL;
    _p = NULL;
}


CCachedFileDataSource::~CCachedFileDataSource() {

    if (_cache != NULL)
        delete[] _cache;
}

void CCachedFileDataSource::init(PinConfiguration *pconfig) {

    // This allow to setup a file SMPTE stream. The file can be a pcap file (network capture), or a dump of an UDP stream

    if (_cache != NULL) {
        return;
    }

    _pConfig = pconfig;
    PROPERTY_REGISTER_MANDATORY("filename", _filename, "");
    // Basic verification
    if (_filename == NULL || strlen(_filename) == 0) {
        LOG_ERROR("***ERROR*** bad filename format");
        exit(1);
    }
    PROPERTY_REGISTER_OPTIONAL("fps", _fps, 25.0f);
    // detect if PCAP file, otherwise standard dump file
    if (tools::endsWith(_filename, ".pcap"))
        _pcapFile = 1;
    LOG_INFO("demux from file '%s' %s",_filename, (_pcapFile==1?"(PCAP file)":""));

    if (_fps > 0)
        // This allow to setup the framerate when reading a source file (where there is no notion of time)
        setFrameRate(_fps);

    std::ifstream f;
    f.open(_filename, ios::in | ios::binary | ios::ate);
    if (!f.is_open())
    {
        LOG_ERROR("***ERROR*** can't open '%s'", _filename);
        exit(1);
    }
    if (_pcapFile == 1) {
        _fileOffset = PCAP_FILE_HEADER_SIZE;
        _packetOffset = PCAP_PACKET_OFFSET;
    }
    _size = (int)f.tellg();

    LOG_INFO("Ok to open file '%s'", _filename);

    // Get first packet to analyse header and calculate real _samplesize
    char rtp_packet[RTP_PACKET_SIZE];
    if (_packetOffset > 0)
        f.seekg(_packetOffset, ios_base::cur);
    f.read(rtp_packet, RTP_PACKET_SIZE);
    if (f.good()) {
        CRTPFrame frame((unsigned char*)rtp_packet, RTP_PACKET_SIZE);
        CHBRMPFrame* hbrmp = frame.getHBRMPFrame();
        if (hbrmp->getClockFrequency() == 0)
            _samplesize -= 4;
        LOG_INFO("Sample size is %d", _samplesize);
        delete hbrmp;
        f.clear();
    }

    // Put the file content in cache
    f.seekg(_fileOffset, ios::beg);
    _cache = new unsigned char[_size];
    f.read((char*)_cache, _size);
    f.close();
    _p = _cache;
}

void CCachedFileDataSource::setFrameRate(float fps) {

    LOG_INFO("Set fps to %.2f", fps);
    _fps = fps;
}

void CCachedFileDataSource::waitForNextFrame() {

    if (_fps <= 0.0f) {
        LOG_ERROR("fps null...");
        return;
    }
    long long microsecond = (long long)((double)1000.0*1000.0 / (double)_fps);
    long long currenttime = tools::getCurrentTimeInMicroS();
    long long deltatime = (currenttime - _time);
    int timetosleep = (int)(microsecond - deltatime - 200);
    //LOG_INFO("cur=%lld, delta=%lld, timetosleep=%d", currenttime, deltatime, timetosleep);
    if (timetosleep > 0)
        usleep(timetosleep);
    _time = tools::getCurrentTimeInMicroS();
}

int CCachedFileDataSource::read(char* buffer, int size) {

    int result = -1;

    std::unique_lock<std::mutex> lock(_cs);
    if (_cache != NULL) {
        if (_packetOffset > 0) {
            if (_packetOffset + (_p - _cache) > _size - 1) {
                _p = _cache;
                LOG_INFO("looping in file...");
            }
            _p += _packetOffset;
        }

        if (size + (_p - _cache) > _size - 1) {
            _p = _cache;
            LOG_INFO("looping in file...");
        }

        memcpy(buffer, _p, size);
        _p += size;
        result = size;
    }

    return result;
}

void CCachedFileDataSource::close() {

    LOG("-->");
    std::unique_lock<std::mutex> lock(_cs);
    if (_cache != NULL)
        delete[] _cache;
    _cache = NULL;
    _p = NULL;
    LOG("<--");
}

