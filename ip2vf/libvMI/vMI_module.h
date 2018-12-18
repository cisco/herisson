
#ifndef _VMI_MODULE_H
#define _VMI_MODULE_H

#include <string>

#include "moduleconfiguration.h"
#include "vMI_input.h"
#include "vMI_output.h"
#include "libvMI.h"

#define MSG_MAX_LEN     1024

/**
* Controls I/O for an entire ip2vf module.
* A module is an entity which can handle multiple inputs and produces a single output.
* This version of the API does NOT support multiple output instances (but does support multiple pins ... )
*/

class CvMIModuleController {
    std::vector<CvMIInput *>    m_InputStreams;
    std::vector<CvMIOutput *>   m_OutputStreams;
    std::string                 m_Preconfig;
    State                       m_state = STATE_NOTINIT;
    CModuleConfiguration        m_config;
    const libvMI_input_callback m_Callback;
    MetricsCollector*           m_zmqlogger;    // This logger will be use by all pins
    const void*                 m_userData;

    // Handle management
    static int                  m_nextHandle;
    static std::mutex           m_lock;

private:
    /**
    * Reads and parses module configuration
    */
    void _parseAndInitModuleConfig();

    inline void _callbackFunction(CmdType cmd, int param) {
        m_Callback(m_userData, cmd, param, LIBVMI_INVALID_HANDLE, LIBVMI_INVALID_HANDLE);
    }

public:
    /**
    * @param moduleCallback a callback that handles events regarding entire module
    */
    CvMIModuleController(const libvMI_input_callback &moduleCallback, const void* user_data);
    virtual ~CvMIModuleController();


    /**
    * Manage handle
    */
    static int getNextHandle() {
        std::unique_lock<std::mutex> lock(CvMIModuleController::m_lock);
        return CvMIModuleController::m_nextHandle++;
    }

    /**
    * commands the module
    */
    int start();
    int stop();

    /**
    * returns a collection of input streams
    */
    inline std::vector<CvMIInput *> * getInputs() {
        return &m_InputStreams;
    }

    inline std::vector<CvMIOutput *> * getOutputs() {
        return &m_OutputStreams;
    }

    int close();

    /**
    * Adds an input to the module, allowing it to eventually consume data
    * @param newInput pointer to libip2vf input
    * @retrurns the input's ip in the module
    */
    unsigned int registerInput(CvMIInput * newInput);
    unsigned int registerOutput(CvMIOutput * newOutput);



    int init(const std::string &moduleConfiguration, const std::string m_OutputConfiguration);

    const CModuleConfiguration &getConfiguration() {
        return m_config;
    }

    const char* getName() {
        return m_config._name.c_str();
    }

    MetricsCollector* &getMetricsCollector(){
        return m_zmqlogger;
    }
};


#endif // _VMI_MODULE_H
