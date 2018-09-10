#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp
#include <zmq.h>

#include "log.h"
#include "common.h"
#include "tools.h"
#include "supervisorinterface.h"

#define MSG_PREFIX_INIT     "init:"
#define MSG_PREFIX_START    "start:"
#define MSG_PREFIX_STOP     "stop:"
#define MSG_PREFIX_QUIT     "quit:"

CSupervisorInterface::CSupervisorInterface() {
    _zmqlistenport = -1;
    _zmqsendport = -1;
    _quit_msgs = false;
    _context = NULL;
    _publisher = NULL;
}
CSupervisorInterface::~CSupervisorInterface() {
    stop();
    if (_publisher != NULL)
        zmq_close(_publisher);
    _publisher = NULL;
    if (_context != NULL)
        zmq_ctx_destroy(_context);
    _context = NULL;
}

void CSupervisorInterface::listenForMsgsThread(CSupervisorInterface* sup)
{
    char msg[MSG_MAX_LEN];
    char dest[64];

    // create zmq context and responder
    SNPRINTF(dest, sizeof(dest), "tcp://*:%d", sup->_zmqlistenport);
    LOG_INFO("bind to zmq='%s'", dest);
    void *context = zmq_ctx_new();
    void *responder = zmq_socket(context, ZMQ_PAIR);
    int rc = zmq_bind(responder, dest);

    LOG_INFO("Receiver: Started\n");

    int timeout = 200;
    zmq_setsockopt(responder, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

    // Receiver msgs loop
    while (sup->_quit_msgs == false)
    {
        LOG("Receiver: wait for msg...\n");
        int num = zmq_recv(responder, msg, MSG_MAX_LEN, 0);

        if (num > 0)
        {
            msg[num] = '\0';
            LOG("Receiver: Received new msg '%s'\n", msg);
            if (strncmp(msg, MSG_PREFIX_INIT, strlen(MSG_PREFIX_INIT)) == 0) {
                
                (*sup->_callback)(CMD_INIT, 0, msg); 
            }
            else if (strncmp(msg, MSG_PREFIX_START, strlen(MSG_PREFIX_START)) == 0) {
                (*sup->_callback)(CMD_START, 0, NULL);
            }
            else if (strncmp(msg, MSG_PREFIX_STOP, strlen(MSG_PREFIX_STOP)) == 0) {
                (*sup->_callback)(CMD_STOP, 0, NULL);
            }
            else if (strncmp(msg, MSG_PREFIX_QUIT, strlen(MSG_PREFIX_QUIT)) == 0) {
                sup->_quit_msgs = true;
                break;
            }
            else
                LOG("***WARNING*** Receiver: unknown msg '%s'\n", msg);
        }
    }

    if (responder != NULL)
        zmq_close(responder);
    if (context != NULL)
        zmq_ctx_destroy(context);
}

void CSupervisorInterface::init(supervisor_callback_t func, int listening_port, int sending_port) {
    _zmqlistenport = listening_port;
    _zmqsendport = sending_port;
    _callback   = func;
}

void CSupervisorInterface::start() {
    _quit_msgs = false;
    _th_msgs = std::thread(CSupervisorInterface::listenForMsgsThread, this);
}

void CSupervisorInterface::stop() {
    _quit_msgs = true;
}

void CSupervisorInterface::waitForCompletion() {
    if (_th_msgs.joinable()) {
        _th_msgs.join();
    }
}

void CSupervisorInterface::sendMsg(const char* msg) {
    if (_context == NULL) {
        char zmq_uri[64];
        SNPRINTF(zmq_uri, sizeof(zmq_uri), "tcp://127.0.0.1:%d", _zmqsendport);
        _context = zmq_ctx_new();
        if (_context == NULL) {
            LOG_ERROR("Sender: cannot create zmq context");
            return;
        }
        _publisher = zmq_socket(_context, ZMQ_PUB);
        if (_publisher < 0) {
            LOG_ERROR("Sender: cannot create zmq socket");
            zmq_ctx_destroy(_context);
            _context = NULL;
            return;
        }
        int rc = zmq_connect(_publisher, zmq_uri);
        if (rc < 0) {
            LOG_ERROR("Sender: cannot connect zmq socket on uri='%s'", zmq_uri);
            zmq_close(_publisher);
            _publisher = NULL;
            zmq_ctx_destroy(_context);
            _context = NULL;
            return;
        }
        LOG_INFO("Sender: connected to '%s'", zmq_uri);
    }

    try {
        if(_publisher != NULL)
            zmq_send(_publisher, msg, strlen(msg), ZMQ_NOBLOCK);
    }
    catch (...) {
        LOG_ERROR("Sender: failed to send mesg='%s'", (msg?msg:"<null>"));
    }
}

