#include <cstdio>
#include <cstdlib>
#include <cstring>      // strerror
#include <string>

#include "log.h"
#include "tools.h"
#include "framecounter.h"


void CFrameCounter::tick(const std::string &msg) 
{
    _frame_count++;
    _total_frame_count++;
    double currentTime = tools::getCurrentTimeInS();
    if( (currentTime - _time) > 1.0)
    {
        double fps = (double) _frame_count / (currentTime-_time);
        if (_zmq_logger) {
            _zmq_logger->setFPS(fps,_pinId);
            _zmq_logger->setFrameCounter(_total_frame_count, _pinId);
            _zmq_logger->tick();
        }
        _time  = currentTime;
        _frame_count = 0;
    }

}

void CFrameCounter::reset()
{
    _time = tools::getCurrentTimeInS();
    _frame_count = 0;
}

