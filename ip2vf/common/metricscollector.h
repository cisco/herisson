#ifndef _METRICSCOLLECTOR_H
#define _METRICSCOLLECTOR_H

#include "moduleconfiguration.h"    // For MAX_CONFIG_STRING_LENGTH
#include "collectdframe.h"
#include "tcp_basic.h"
#include <mutex>
enum PinDirection {
    DIRECTION_INPUT = 0,
    DIRECTION_OUTPUT = 1,
};

struct PinInfo {
    int          _id;
    PinType      _type;         // Type of PIN (cf enum PinType on common.h)
    PinDirection _direction;    // direction
    int          _vidfrmsize;   // Frame size for this pin
    double       _fps;
    unsigned int  _frames;

    PinInfo() {
        _id = -1;
        _direction = DIRECTION_INPUT;
        _type = PIN_TYPE_NONE;
        _vidfrmsize = 0;
        _fps = 0.0f;
        _frames = 0;
    };
};

#include "libvMI_int.h"

class MetricsCollector
{
public:
    MetricsCollector(std::string &ip, int port);
    ~MetricsCollector();

    // To set static infos (one time at begin, and not change after)
    void setStaticInfo(int id, std::string &name, int mtn_port);
    // To set pin informations. Depends of the configuration
    void setPinInfo(int id, PinType type, PinDirection direction, int framesize);
    // To set stats (change each frame)
    void setFPS(double fps, int pinId);
    void setFrameCounter(unsigned int frames, int pinId);

    // Send periodic data to supervisor
    void tick();

    // PktTS metrics
    VMILIBRARY_API_INT void pktTSmetricsinit(int);
    VMILIBRARY_API_INT void pktTSmetrics(
        const std::chrono::high_resolution_clock::time_point,
        const char*, int, double[], int);

private:
    UDP _collectdSocket;
    CollectdFrame *_frame;
    int     _tickCounter;

    int           _id;
    char          _name[MAX_CONFIG_STRING_LENGTH];
    int           _mtn_port;
    std::mutex    _tickLock;

    std::vector<PinInfo> _pinsVec;

    int           _pktTSflg;
    CollectdFrame*_pktTSframe;
};

#endif //_ZMQLOGGER_H
