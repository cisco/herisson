#ifndef _SUPERVISORINTERFACE_H
#define _SUPERVISORINTERFACE_H

#include <thread>

#define MSG_MAX_LEN     1024

enum CmdType {
    CMD_NONE = -1,
    CMD_INIT = 1,
    CMD_START = 2,
    CMD_TICK = 3,
    CMD_STOP = 4,
    CMD_QUIT = 5,
    CMD_REQSTATS = 6,
};

typedef void(*supervisor_callback_t)(CmdType, int, const char*);

class CSupervisorInterface
{
public:
    supervisor_callback_t _callback;
    int         _zmqlistenport;
    int         _zmqsendport;
    bool        _quit_msgs;
    std::thread _th_msgs;
    std::thread _th_process;
    void*       _context;
    void*       _publisher;

public:
    CSupervisorInterface();
    ~CSupervisorInterface();

public:
    void init(supervisor_callback_t func, int listening_port, int sending_port);
    void start();
    void stop();
    void waitForCompletion();
    void sendMsg(const char* msg);

    int  getListenPort() { return _zmqlistenport; };
    int  getRemotePort() { return _zmqsendport; };

protected:
    static void listenForMsgsThread(CSupervisorInterface* sup);

};

#endif //_SUPERVISORINTERFACE_H
