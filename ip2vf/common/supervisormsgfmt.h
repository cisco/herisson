#ifndef _SUPERVISORMSGFMT_H
#define _SUPERVISORMSGFMT_H

#include "moduleconfiguration.h"    // For MAX_CONFIG_STRING_LENGTH

class CSupervisorMsgFmt
{
public:
    CSupervisorMsgFmt();
    ~CSupervisorMsgFmt();

    // setters
    void setFPS(float fps);
    void setFrameCounter(unsigned int frames);
    void setStaticInfo(int id, const char* name, int sup_port);
    void setPinInfo(int direction, int index, int type, int frame_size);

    // getters
    std::string getStaticDataMsg();
    std::string getScheduleDataMsg();

private:
    // Module static informations
    int          _id;
    char         _name[MAX_CONFIG_STRING_LENGTH];
    int          _sup_port;
    unsigned long _start_time;

    // Module dynamic informations
    float        _fps;
    unsigned int _frames;

    // Module dynamic informations
    struct t_PinInfos {
        int direction;  // 0: in, 1: out
        int index;
        int type;
        int frame_size;
    };
    std::vector<t_PinInfos>  _pins;

};

#endif //_SUPERVISORMSGFMT_H
