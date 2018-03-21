#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp
#include <string>

#include <signal.h>
#include <pins/pins.h>

#include "common.h"
#include "libvMI_int.h"
#include "vMI_input.h"

/**
* This file contains an implementation of lib2vf supporting multiple modules
*/

CvMIInput::CvMIInput(std::string &configuration, libvMI_input_callback callback, libvMI_pin_handle handle, libvMI_module_handle moduleHandle, const void* user_data) :
    m_Callback(callback),
    m_handle(handle),
    m_moduleHandle(moduleHandle),
    m_preconfig(configuration),
    m_userData(user_data)
{
}

CvMIInput::~CvMIInput() {
    LOG("[%d] -->", m_handle);
    if (m_config != NULL) {
        delete m_config;
    }
    if (m_input != NULL) {
        delete m_input;
    }
    LOG("[%d] <--", m_handle);
}

void CvMIInput::_init(const std::string &name, int id)
{

    m_id = id;

    m_preconfig = "name=" + name + "," + m_preconfig;
    LOG_INFO("m_preconfig=%s", m_preconfig.c_str());
    m_config = new CModuleConfiguration(m_preconfig.c_str());

    // Dump the current config
    m_config->dump();

    //check that legacy mixed configuration is not bleeding into our new input modules:
    if (m_config->_out.size() != 0)
    {
        THROW_CRITICAL_EXCEPTION(
                "Input configuration cannot contain output (are you using an old configuration?)");
    }

    // Create input pin
    if (m_config->_in.size() != 1)
    {
        THROW_CRITICAL_EXCEPTION(
                "***ERROR*** nb input=" + std::to_string(m_config->_in.size())
                        + "NOT SUPPORTED. aborting.");
    }

    m_input = CPinFactory::getInstance()->createInputPin(m_config->_in[0]._type,
            m_config, m_id);

    if (!m_input)
    {
        LOG_ERROR("%s: ***ERROR*** incompatible input pin %s (%s)",
                name.c_str(), std::to_string(m_config->_in.size()).c_str(),
                m_config->_in[0]._type);
        THROW_CRITICAL_EXCEPTION(
                "***ERROR*** incompatible input pin"
                        + std::to_string(m_config->_in.size()) + "("
                        + std::string(m_config->_in[0]._type) + ")");
    }

    m_state = STATE_STOPPED;
}

void CvMIInput::signalQuit() {
}

/**
* TODO: write description for internal function carried over from old interface
* TODO: refactor this old code
*/
void CvMIInput::start() {
    LOG_INFO("[%d] -->", m_handle);
    if (m_state != STATE_STOPPED)
    {
        LOG_INFO("[%d] module is not on STOPPED state, can't start it", m_handle);
        return;
    }
    m_input->start();
    m_quit_process = false;
    m_th_process = std::thread([this] { _process(NULL); });

    m_state = STATE_STARTED;
    LOG_INFO("[%d] <--", m_handle);
}

/**
* TODO: write description for internal function carried over from old interface
* TODO: refactor this old code
*/
void CvMIInput::stop() {
    LOG_INFO("[%d] -->", m_handle);
    if (m_state != STATE_STARTED)
    {
        LOG_INFO("[%d] module is not on STARTED state, can't stop it", m_handle);
        return;
    }
    m_state = STATE_STOPPED;
    m_quit_process = true;
    m_input->stop();
    //usleep(10000);

    m_ProcLock.lock();
    if (m_th_process.joinable()) {
        m_th_process.join();
    }

    m_ProcLock.unlock();

    LOG_INFO("[%d] <--", m_handle);
}

/**
* TODO: write description for internal function carried over from old interface
* TODO: refactor this old code
*/
void *CvMIInput::_process(void *context)
{
    int count = 0, result;
    LOG_INFO("[%d] -->", m_handle);

    //Blocking all other signals
#ifndef WIN32
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_SETMASK, &mask, NULL);
#endif
    if (m_input == NULL) {
        LOG("[%d] No input configurate. exit.", m_handle, count);
        return 0;
    }

    m_input->reset();

    while (m_quit_process == false) {
        LOG("[%d] iterate, c=%d", m_handle, count);

        // the read is blocking -- won't end when trying to quit ----
        libvMI_frame_handle hFrame = libvmi_frame_create();
        if (hFrame == LIBVMI_INVALID_HANDLE) {
            LOG_ERROR("Unable to get/create new vMIframe on the queue to transport this one... drop it");
            if (m_quit_process)
                break;
            //We create a frame not part of the pool and we drop it afterwards
            CvMIFrame *tmpFrame = new CvMIFrame();
            m_input->read(tmpFrame);
            delete tmpFrame;
            continue;
        }
        CvMIFrame* frame = libvMI_frame_get(hFrame);
        result = m_input->read(frame);

        if (m_quit_process) {
            LOG_INFO("[%d] Exit", m_handle);
            libvmi_frame_release(hFrame);
            break;
        }
        else if (result != VMI_E_OK) {
            LOG("[%d] Error reading frame", m_handle);
            // Don't forget to release the frame as nothing else will be consume it...
            libvmi_frame_release(hFrame);
            continue;
        }

        // Refresh in and out headers structure. Note that output header will be writed only just before the send
        //m_inframefactory.ReadHeaders((unsigned char*)m_input->getCurrentBuffer());

        LOG_WARNING("[%d] need to reenable: m_counter.tick(m_config._name)", m_handle);
        // notify the processing node
        m_counter.tick("");
        callbackFunction(CMD_TICK, m_moduleHandle, hFrame);
    }
    m_state = STATE_STOPPED;

    LOG_INFO("[%d] <--", m_handle);

    return 0;
}

