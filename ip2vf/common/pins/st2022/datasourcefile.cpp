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


CFileDataSource::CFileDataSource()
    : CDMUXDataSource()
{
    _fps = 25.0f;
    _time = tools::getCurrentTimeInMicroS();
    _pcapFile = 0;
    _fileOffset = 0;
    _packetOffset = 0;
    _samplesize = RTP_PACKET_SIZE;  // by default, will be refresh 
    _type = DataSourceType::TYPE_FILE;
}


CFileDataSource::~CFileDataSource() {
    if (_f.is_open())
        _f.close();
}

void CFileDataSource::init(PinConfiguration *pconfig)
{
    // This allow to setup a file SMPTE stream. The file can be a pcap file (network capture), or a dump of an UDP stream

    if (_f.is_open())
        return;
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

    _f.open(_filename, ios::in | ios::binary | ios::ate);
    if (!_f.is_open())
    {
        LOG_ERROR("***ERROR*** can't open '%s'", _filename);
        exit(1);
    }
    if (_pcapFile == 1) {
        _fileOffset = PCAP_FILE_HEADER_SIZE;
        _packetOffset = PCAP_PACKET_OFFSET;
    }

    LOG_INFO("Ok to open file '%s'", _filename);
    int filesize = (int)_f.tellg();
    _f.seekg(_fileOffset, ios::beg);

    // Get first packet to analyse header and calculate real _samplesize
    char rtp_packet[RTP_PACKET_SIZE];
    if (_packetOffset > 0)
        _f.seekg(_packetOffset, ios_base::cur);
    _f.read(rtp_packet, RTP_PACKET_SIZE);
    if (_f.good()) {
        CRTPFrame frame((unsigned char*)rtp_packet, RTP_PACKET_SIZE);
        CHBRMPFrame* hbrmp = frame.getHBRMPFrame();
        if (hbrmp->getClockFrequency() == 0)
            _samplesize -= 4;
        LOG_INFO("Sample size is %d", _samplesize);
        delete hbrmp;
        _f.clear();
        _f.seekg(_fileOffset, ios::beg); 
    }
}

void CFileDataSource::setFrameRate(float fps) {
    LOG_INFO("Set fps to %.2f", fps);
    _fps = fps;
}

void CFileDataSource::waitForNextFrame()
{
    if (_f.is_open()) {
        if (_fps <= 0.0f) {
            LOG_ERROR("fps null...");
            return;
        }
        long long microsecond = (long long)((double)1000.0*1000.0 / (double)_fps);
        long long currenttime = tools::getCurrentTimeInMicroS();
        long long deltatime = (currenttime - _time);
        int timetosleep = (int)(microsecond - deltatime);
        _time = currenttime + timetosleep;
        //LOG_INFO("cur=%lld, delta=%lld, timetosleep=%d", currenttime, deltatime, timetosleep);
        if (timetosleep > 0) {
            // We are in advance... let wait a little before get next chunk
            usleep(timetosleep);
        }
        else if (timetosleep < -10000000) {
            // Too much in late... try to resync again
            LOG_INFO("Too much in late... resync. (in late of %.02f seconds)", (double)(-timetosleep/1000000.0));
            _time = currenttime;
        }
    }
}

int CFileDataSource::read(char* buffer, int size)
{
    int result = -1;

    if (size != _samplesize) {
        LOG_WARNING("Size is not equal to sample size (request=%d, samplesize=%d)", size, _samplesize);
    }

    std::unique_lock<std::mutex> lock(_cs);
    if (_f.is_open()) {

        if (_packetOffset > 0)
            _f.seekg(_packetOffset, ios_base::cur);

        _f.read(buffer, size);

        if (_f.eof()) {
            LOG_INFO("EOF");
            LOG_INFO("looping in file...");
            _f.clear();
            _f.seekg(_fileOffset, ios::beg);
            if (_packetOffset > 0)
                _f.seekg(_packetOffset, ios_base::cur);
            _f.read(buffer, size);
        }
        else if(_f.fail())
            LOG_INFO("FAIL");
        else if (_f.bad())
            LOG_INFO("DAB");

        if (_f.good()) {
            result = (int)_f.gcount();
            //LOG("read %d bytes", result);
        }
        else
            LOG_ERROR("error when reading from file: result=%d", result);
    }

    return result;
}

void CFileDataSource::close()
{
    LOG("-->");
    std::unique_lock<std::mutex> lock(_cs);
    if (_f.is_open())
    {
        _f.close();
    }
    LOG("<--");
}
