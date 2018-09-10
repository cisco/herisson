
#ifndef _VMI_INPUT_H
#define _VMI_INPUT_H

#include <string>

#include <pins/pins.h>
#include "moduleconfiguration.h"

#include "libvMI.h"

/**
* This file contains an implementation of vMI supporting multiple modules
*/
class CvMIInput {
    int                    m_id;
    int                    m_zmqport = 5010;
    libvMI_pin_handle      m_handle = LIBVMI_INVALID_HANDLE;
    libvMI_module_handle   m_moduleHandle = LIBVMI_INVALID_HANDLE;
    CModuleConfiguration*  m_config;
    CIn*                   m_input = NULL;
    bool                   m_quit_process = false;
    bool                   m_quit_msgs = false;
    std::string            m_preconfig;
    //CFrameHeaders          m_inframefactory;
    std::thread            m_th_process;
    State                  m_state = STATE_NOTINIT;
    const libvMI_input_callback m_Callback;
    mutex                  m_ProcLock;
    CFrameCounter          m_counter;
    const void*            m_userData;

public:

    CvMIInput(std::string &configuration, libvMI_input_callback callback, libvMI_pin_handle handle, libvMI_module_handle moduleHandle, const void* user_data);

    virtual ~CvMIInput();

    inline const libvMI_pin_handle getHandle() {
        return m_handle;
    }

    inline void callbackFunction(CmdType cmd, int param, libvMI_frame_handle hFrame) {
        m_Callback(m_userData, cmd, param, m_handle, hFrame);
    }

    inline CIn * getInputManager() {
        return m_input;
    }

    inline CFrameCounter* getFrameCounter() {
        return &m_counter;
    }

    const char* getConfigAsString() { return m_preconfig.c_str(); }

public:

    /**
    * TODO: write description for internal function carried over from old interface
    * TODO: refactor this old code
    */
    void _init(const std::string &name, int id);

    void signalQuit();

    /**
    * TODO: write description for internal function carried over from old interface
    * TODO: refactor this old code
    */
    void start();

    /**
    * TODO: write description for internal function carried over from old interface
    * TODO: refactor this old code
    */
     void stop();

    /**
    * TODO: write description for internal function carried over from old interface
    * TODO: refactor this old code
    */
     void *_process(void *context);

};


#endif // _VMI_INPUT_H
