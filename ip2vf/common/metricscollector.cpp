#include "metricscollector.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sstream>
#include <streambuf>
#include <chrono>
#include <mutex>
#include "log.h"
#include "common.h"
#include "tools.h"
#include "tcp_basic.h"
#include "collectdframe.h"
MetricsCollector::MetricsCollector(std::string &ip, int port): _name{}
{
    _tickCounter = 0;
    _id = -1;
    _mtn_port = -1;
    _collectdSocket.openSocket(ip.c_str(), NULL, port, false);
    _frame = nullptr;
    _pktTSframe = nullptr;
    if (!_collectdSocket.isValid())
    {
        LOG_ERROR("Could not open socket to collectd on %s:%d\n", ip.c_str(), port);
    }

}

MetricsCollector::~MetricsCollector()
{
    _collectdSocket.closeSocket();
    if (_frame)
        delete _frame;
    if (_pktTSframe)
        delete _pktTSframe;
}

void MetricsCollector::setFPS(double fps, int pinId)
{
    for (auto && pinInfo : this->_pinsVec)
    {
        if (pinInfo._id == pinId)
        {
            pinInfo._fps = fps;
            return;
        }
    }
}
void MetricsCollector::setFrameCounter(unsigned int frames, int pinId)
{
    for (auto && pinInfo : this->_pinsVec)
    {
        if (pinInfo._id == pinId)
        {
            pinInfo._frames = frames;
            return;
        }
    }
}
void MetricsCollector::setStaticInfo(int id, std::string &name, int mtn_port)
{
    _id = id;
    strncpy(_name, name.c_str(), MAX_CONFIG_STRING_LENGTH);
    _mtn_port = mtn_port;
    LOG_INFO("_mtn_port=%d", _mtn_port);
    _frame = new CollectdFrame("vMI_metrics", name.c_str());
}

void MetricsCollector::setPinInfo(int id, PinType type, PinDirection direction, int framesize)
{
    PinInfo pin;
    pin._id = id;
    pin._type = type;
    pin._direction = direction;
    pin._vidfrmsize = framesize;
    _pinsVec.push_back(pin);
    LOG("_mtn_port=%d", _mtn_port);
    LOG("pin._id=%d", pin._id);
    LOG("pin._direction=%d", pin._direction);
    LOG("pin._vidfrmsize=%d", pin._vidfrmsize);
}

#ifndef _WIN32
#define DISPFORMAT_BEGIN    "\033[1;32m"
#define DISPFORMAT_CLOSE    "\033[0m"
#else
#define DISPFORMAT_BEGIN    ""
#define DISPFORMAT_CLOSE    ""
#endif // _WIN32

void MetricsCollector::tick()
{
    std::lock_guard<std::mutex> lock(this->_tickLock);

    _frame->resetFrame();
    auto ts = std::chrono::high_resolution_clock::now();
    _frame->setTimestamp(ts);

    _tickCounter++;
    //Only report data each time every pin has sent its own report
    if (_tickCounter % (_pinsVec.size()) != 0)
        return;

    std::ostringstream res;
    _frame->setType("videofps");
    res << this->_name << ": FPS: ";
    for (auto && pi : _pinsVec)
    {
        _frame->setTypeInstance(
                ((pi._direction == DIRECTION_INPUT ? "i" : "o")
                        + std::to_string(pi._id)).c_str());
        double fps = (double) pi._fps;
        _frame->addRecord(COLLECTD_DATACODE_GAUGE, (void *)&fps);
        res << (pi._direction == DIRECTION_INPUT ? "i" : "o") << pi._id << ": " << DISPFORMAT_BEGIN << tools::to_string_with_precision(pi._fps, 2) << DISPFORMAT_CLOSE << ", ";
    }
    LOG_INFO(res.str().c_str());
    if (_collectdSocket.isValid())
    {
        int len = _frame->getLen();
        _collectdSocket.writeSocket((char *)_frame->getBuffer(), &len);
    }


}

#include <cmath>

void
MetricsCollector::pktTSmetricsinit(int flg)
{
    if(_frame == nullptr)
        return;

    _pktTSflg = flg;
    _pktTSframe = new CollectdFrame("vMI_pktTSmetrics", _name);
}

void
MetricsCollector::pktTSmetrics(
    const std::chrono::high_resolution_clock::time_point ts,
    const char* type, int pin, double stats[], int n)
{
    int i, len;
    PinInfo pi;
    std::string ti;
    std::ostringstream res;

    if(_pktTSframe == nullptr || n <= 0)
        return;

    _pktTSframe->resetFrame();
    _pktTSframe->setTimestamp(ts);
    _pktTSframe->setType(type);

    pi = _pinsVec[pin];
    if(pi._direction == DIRECTION_INPUT)
        ti = "i" + std::to_string(pi._id);
    else
        ti = "o" + std::to_string(pi._id);
    _pktTSframe->setTypeInstance(ti.c_str());

    _pktTSframe->addRecordn(COLLECTD_DATACODE_GAUGE, (void*)stats, n);

    if(_pktTSflg){
        res << this->_name << ": " << type << ": ";
        res << ti << ": " << DISPFORMAT_BEGIN;
        for(i = 0; i < n-1; i++)
            res << (long long)round(stats[i]) << ", ";
        res << (long long)round(stats[i]) << DISPFORMAT_CLOSE;
        LOG_INFO(res.str().c_str());
    }

    if(_collectdSocket.isValid()){
        len = _pktTSframe->getLen();
        _collectdSocket.writeSocket((char*)_pktTSframe->getBuffer(), &len);
    }
}
