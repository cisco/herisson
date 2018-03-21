#ifndef _FRAMECOUNTER_H
#define _FRAMECOUNTER_H

#include <memory>

#include "metricscollector.h"



class CFrameCounter
{
public:
    double      _time; 
    int         _frame_count;
    int         _total_frame_count;
    int         _pinId;
    MetricsCollector*  _zmq_logger;     // A reference to the zmq_logger of the module.
public:
    CFrameCounter() { 
        _time = 0.0; 
        _frame_count = 0;
        _total_frame_count = 0;
        _pinId = -1;
        _zmq_logger = NULL;
    };
    ~CFrameCounter() { 
        // don't delete _zmq_logger: it's managed by the caller
    };

public:
    void tick(const std::string &msg);
    void reset();

    inline void setZMQLogger(MetricsCollector *logger, int pinId) {
        _zmq_logger = logger;
        _pinId = pinId;
    };

    inline int getCount() { 
        return _total_frame_count; 
    };
};

#endif //_FRAMECOUNTER_H
