#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp

#include "log.h"
#include "common.h"
#include "tools.h"
#include "supervisormsgfmt.h"

CSupervisorMsgFmt::CSupervisorMsgFmt() {
}
CSupervisorMsgFmt::~CSupervisorMsgFmt() {
}

void CSupervisorMsgFmt::setFPS(float fps) {
    _fps = fps;
}
void CSupervisorMsgFmt::setFrameCounter(unsigned int frames) {
    _frames = frames;
}
void CSupervisorMsgFmt::setStaticInfo(int id, const char* name, int sup_port) {
    _id = id;
    strncpy(_name, name, MAX_CONFIG_STRING_LENGTH);
    _start_time = tools::getUTCEpochTimeInMs();
    _sup_port = sup_port;
}
void CSupervisorMsgFmt::setPinInfo(int direction, int index, int type, int frame_size) {
    t_PinInfos pin;
    pin.direction = direction;
    pin.index = index;
    pin.type = type;
    pin.frame_size = frame_size;
    _pins.push_back(pin);
}

//std::string CSupervisorMsgFmt::getStaticDataMsg() {
//}

std::string CSupervisorMsgFmt::getScheduleDataMsg() {
    int user = 0;
    int kernel = 0;
    int mem = 0;
    tools::getCPULoad(user, kernel);
    tools::getMEMORYLoad(mem);
    char buf[4096];

    SNPRINTF(buf, 4096, "IP2VF ID_=%d;FPS=%.2f;FRM=%ld;USE=%d;KER=%d;MEM=%d", _id, _fps, (long int)_frames, user, kernel, mem);
    return std::string(buf);
}
