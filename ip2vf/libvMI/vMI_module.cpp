
#include <functional>
#include <string>
#include <signal.h>

#include "common.h"
#include "tools.h"
#include "vMI_module.h"

/**
* Controls I/O for an entire ip2vf module.
* A module is an entity which can handle multiple inputs and produces a single output.
* This version of the API does NOT support multiple output instances (but does support multiple pins ... )
*/

int CvMIModuleController::m_nextHandle = 10;
std::mutex CvMIModuleController::m_lock;

/**
* @param moduleCallback a callback that handles events regarding entire module
*/
CvMIModuleController::CvMIModuleController(const unsigned int &zmq_listen_port, const libvMI_input_callback &moduleCallback, const void* user_data) :
    m_Callback(moduleCallback),
    m_zmqlogger(NULL),
    m_userData(user_data)
{}

CvMIModuleController::~CvMIModuleController() {
    if (m_zmqlogger != NULL) {
        delete m_zmqlogger;
        m_zmqlogger = NULL;
    }
}

int CvMIModuleController::start() {

    LOG_INFO("-->");

    if (m_state != STATE_STOPPED)
    {
        LOG_INFO("module is not on STOPPED state, can't start it");
        return 0;
    }

    //start all of the input streams:
    for (auto inputStream : m_InputStreams)
        inputStream->start();
    //start all of the output streams:
        for (auto outputStream : m_OutputStreams)
            outputStream->start();

    m_state = STATE_STARTED;
    _callbackFunction(CMD_START, 0);
    LOG_INFO("<--");
    return 0;
}

int CvMIModuleController::stop() {

    LOG_INFO("-->");

    if (m_state == STATE_STARTED) {
        //stop all of the input streams:
        for (auto inputStream : m_InputStreams)
            inputStream->stop();

        for (auto outputStream : m_OutputStreams)
            outputStream->stop();

        m_state = STATE_STOPPED;

        _callbackFunction(CMD_STOP, 0);
    }
    else
    {
        LOG_INFO("module is not on STARTED state, no need to stop it");
    }

    LOG_INFO("<--");
    return 0;
}

int CvMIModuleController::close() {
    
    LOG_INFO("-->");
    LOG_INFO("state = %d", m_state);
    if (m_state != STATE_STOPPED)
        stop();

    LOG_INFO("delete all input pins...");
    for (auto inputStream : m_InputStreams)
        delete inputStream;
    m_InputStreams.erase(m_InputStreams.begin(), m_InputStreams.end());

    LOG_INFO("delete all output pins...");
    for (auto outputStream : m_OutputStreams) 
        delete outputStream;
    m_OutputStreams.erase(m_OutputStreams.begin(), m_OutputStreams.end());

    LOG_INFO("<--");
    return 0;
}

/**
* Adds an input to the module, allowing it to eventually consume data
* @param newInput pointer to libip2vf input
* @retrurns the input's ip in the module
*/
unsigned int CvMIModuleController::registerInput(CvMIInput * newInput) {
    if (m_state != STATE_NOTINIT) {
        THROW_CRITICAL_EXCEPTION("cannot add input after initialization");
    }
    m_InputStreams.push_back(newInput);
    //return the position of the input in the input array
    return (unsigned int)m_InputStreams.size() - 1;
}
unsigned int CvMIModuleController::registerOutput(CvMIOutput * newOutput) {
    if (m_state != STATE_NOTINIT) {
        THROW_CRITICAL_EXCEPTION("cannot add input after initialization");
    }
    m_OutputStreams.push_back(newOutput);
    //return the position of the output in the output array
    return (unsigned int)m_OutputStreams.size() - 1;
}


/**
* Reads and parses module configuration
*/
void CvMIModuleController::_parseAndInitModuleConfig() {

    int index = 0;

    CModuleConfiguration config(m_Preconfig.c_str());
    m_config = config;

    // Dump the current config
    m_config.dump();

    //set the global loggin level for ALL MODULES!!!!
    setLogLevel((LogLevel)m_config._logLevel);

    //check that legacy mixed configuration is not bleeding into our new input modules:
    if ((config._out.size() + config._in.size()) != 0) {
        THROW_CRITICAL_EXCEPTION("Module configuration cannot contain input and output (are you using an old configuration?)");
    }

    // configure metrics collector
    if (m_config._collectdport > -1 && m_zmqlogger == NULL) {
        m_zmqlogger = new MetricsCollector(m_config._collectdip, m_config._collectdport);
        //TODO: Fix the metrics collection port here
#ifdef HAVE_ZMQ
        int port = m_ZeromqController->getListenPort();
#else
        int port = 3000;
#endif
        m_zmqlogger->setStaticInfo(m_config._id, m_config._name, port);
    }

    int pin_id = 0;
    // init all input streams:
    for (auto inputStream : m_InputStreams) {
        inputStream->_init(getConfiguration()._name, pin_id++);
        if (m_zmqlogger != NULL) {
            auto input = inputStream->getInputManager();
            inputStream->getFrameCounter()->setZMQLogger(m_zmqlogger, pin_id);
            m_zmqlogger->setPinInfo(pin_id, (PinType)input->getType(), PinDirection::DIRECTION_INPUT, 5184128/*input->getVideoFrameSize()*/);
        }
    }

    //init all output streams:
    for (auto outputStream : m_OutputStreams) {
        outputStream->_init(getConfiguration()._name, pin_id++);
        if (m_zmqlogger != NULL) {
            auto output = outputStream->getOutputManager();
            outputStream->getFrameCounter()->setZMQLogger(m_zmqlogger, pin_id);
            m_zmqlogger->setPinInfo(pin_id, (PinType)output->getType(), PinDirection::DIRECTION_OUTPUT, 5184128/*output->getVideoFrameSize()*/);
        }
    }

    m_state = STATE_STOPPED;
}

int CvMIModuleController::init(const std::string &moduleConfiguration, const std::string m_OutputConfiguration) {
    LOG("-->");
    //LOG_INFO("moduleConfiguration=%s", moduleConfiguration.c_str());

    if (m_state != STATE_NOTINIT)
        return -1;
    m_Preconfig = moduleConfiguration;

    _parseAndInitModuleConfig();

    _callbackFunction(CMD_INIT, 0);

    LOG("<--");
    return 0;
}

