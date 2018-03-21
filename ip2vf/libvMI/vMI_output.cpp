#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp
#include <string>

#include "libvMI_int.h"
#include "vMI_output.h"

CvMIOutput::CvMIOutput(const std::string &configurationString,
        libvMI_pin_handle handle, const void* user_data) :
        m_handle(handle), m_preconfig(configurationString), m_userData(
                user_data), m_state(STATE_NOTINIT), m_inSync(false), m_syncTimestamp(0), m_syncClock(148500000)
{
    m_Outframefactory = new CFrameHeaders();
}

CvMIOutput::~CvMIOutput()
{
    LOG("[%d] -->", m_handle);
    if (m_config != NULL)
        delete m_config;
    if (m_output != NULL)
        delete m_output;
    if (m_Outframefactory != NULL)
        delete m_Outframefactory;
    LOG("[%d] <--", m_handle);
}

/**
 * replicates a source frame's headers in their entirity in the out frame:
 */
void CvMIOutput::copyFrameHeaderValues(const CFrameHeaders &source)
{
    m_Outframefactory = new CFrameHeaders();
    m_Outframefactory->CopyHeaders(&source);
}

void CvMIOutput::WriteFrameHeaders(unsigned char *buffer)
{
    m_Outframefactory->WriteHeaders(buffer);
}

int CvMIOutput::send(libvMI_frame_handle hFrame)
{
    libvmi_frame_addref(hFrame);
    auto newVal = std::make_pair(false, hFrame);
    m_frameQueue.push(newVal);
    return 0;
}

void CvMIOutput::setParameter(OUTPUTPARAMETER param, void* value) {

    switch (param) {
    case SYNC_ENABLED:
        {
            bool flag = (*static_cast<int*>(value)) == 1;
            _enable_sync(flag);
        }
        break;
    case SYNC_TIMESTAMP:
        m_syncTimestamp = *static_cast<unsigned int*>(value);
        m_syncRefTime = std::chrono::high_resolution_clock::now();
        break;
    case SYNC_CLOCK:
        m_syncClock = *static_cast<unsigned int*>(value);
        break;
    default:
        break;
    }
}

void CvMIOutput::getParameter(OUTPUTPARAMETER param, void* value) {

    switch (param) {
    case SYNC_ENABLED:
        break;
    case SYNC_TIMESTAMP:
        break;
    case SYNC_CLOCK:
        break;
    default:
        break;
    }
}


/**
 * TODO: write description for internal function carried over from old interface
 * TODO: refactor this old code
 */
void CvMIOutput::_init(const std::string & name, int id)
{

    m_id = id;

    m_preconfig = "name=" + name + "," + m_preconfig;
    LOG_INFO("m_preconfig=%s", m_preconfig.c_str());
    m_config = new CModuleConfiguration(m_preconfig.c_str());

    // Dump the current config
    m_config->dump();

    // check for uncaught old configuration parameters slipping through guard mechanisms...
    if (m_config->_in.size() != 0)
    {
        THROW_CRITICAL_EXCEPTION(
                "Input configuration detected on output pin. Are you using a deprecated configuration setting?");
    }

    // Create output pin
    if (m_config->_out.size() != 1)
    {
        LOG_ERROR("***ERROR*** nb output=%d NOT SUPPORTED. aborting.",
                m_config->_out.size());
        THROW_CRITICAL_EXCEPTION("Only one output is supported");
    }

    m_output = CPinFactory::getInstance()->createOutputPin(
            m_config->_out[0]._type, m_config, m_id);
    if (!m_output)
    {
        LOG_ERROR("%s: ***ERROR*** incompatible output pin %s (%s)",
                name.c_str(), std::to_string(m_config->_out.size()).c_str(),
                m_config->_out[0]._type);
        THROW_CRITICAL_EXCEPTION(
                "***ERROR*** incompatible output pin"
                        + std::to_string(m_config->_out.size()) + "("
                        + std::string(m_config->_out[0]._type) + ")");
    }
    m_state = STATE_STOPPED;

}

void CvMIOutput::start()
{
    LOG_INFO("[%d] -->", m_handle);
    if (m_state != STATE_STOPPED)
    {
        LOG_INFO("[%d] module is not on STOPPED state, can't start it",
                m_handle);
        return;
    }
    m_quit_process = false;
    m_th_process = std::thread( [this] { _process(); } );

    m_state = STATE_STARTED;
    LOG_INFO("[%d] <--", m_handle);
}

void CvMIOutput::_process()
{
    int count = 0;
    LOG_INFO("[%d] -->", m_handle);

    //Blocking all other signals
#ifndef WIN32
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_SETMASK, &mask, NULL);
#endif
    if (m_output == NULL)
    {
        LOG("[%d] No input configurate. exit.", m_handle, count);
        return;
    }

    while (!m_quit_process)
    {
        LOG("[%d] iterate, c=%d", m_handle, count);

        auto res = m_frameQueue.pop();
        if (res.first)
        {
            break;
        }
        else
        {
            if (res.second == LIBVMI_INVALID_HANDLE) {
                LOG_ERROR("Invalid handle...");
                break;
            }
            CvMIFrame* frame = libvMI_frame_get(res.second);
            if (frame) {
                if (m_inSync) {

                    // keep frame number
                    int framenumber = 0;
                    frame->get_header(MEDIA_FRAME_NB, &framenumber);

                    // Keep frame timestamp
                    unsigned int frameTimestamp = 0;
                    frame->get_header(MEDIA_TIMESTAMP, &frameTimestamp);

                    // calculate real timestamp (warning, will loop)
                    auto t = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double, std::milli> elapsed = t - m_syncRefTime;
                    unsigned int curTimestamp = m_syncTimestamp + (elapsed.count()*(m_syncClock / 1000.0));
                    int timeToWait = (frameTimestamp - curTimestamp);
                    if (curTimestamp < m_oldClockTimestamp && frameTimestamp > m_oldFrameTimestamp) {
                        // Clock timestamp loop, but not yet frame timestamp... be careful
                        int oldTimeToWait = timeToWait;
                        timeToWait = ULONG_MAX - frameTimestamp + curTimestamp;
                        LOG_INFO("[%d] frame#%d, old timeToWait=%d, new=%d", m_handle, framenumber, oldTimeToWait, timeToWait);
                    }
                    //LOG_INFO("[%d] frame#%d, ref=%lu, cur=%lu, frame=%lu", m_handle, framenumber, m_syncTimestamp, curTimestamp, frameTimestamp);

                    if (timeToWait > 0) {
                        std::chrono::duration<double, std::milli> toWaitInMs (1000.0 * timeToWait / m_syncClock);
                        //LOG_INFO("[%d] must wait %lu (%lums)", m_handle, toWait, (unsigned int)toWaitInMs.count());
                        if (toWaitInMs.count() > 2000.0) {
                            LOG_INFO("[%d] frame#%d, ref=%lu, cur=%lu, frame=%lu", m_handle, framenumber, m_syncTimestamp, curTimestamp, frameTimestamp);
                            LOG_ERROR("[%d] frame#%d, must wait %d (%lums)", m_handle, framenumber, timeToWait, (unsigned int)toWaitInMs.count());
                        }
                        else 
                            // Note: in case of compile error: C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\include\chrono(769): error C2679: binary '+=': no operator found which takes a right-hand operand of type 'const std::chrono::duration<double,std::milli>' (or there is no acceptable conversion) (compiling source file vMI_output.cpp)
                            // Then ensure you running VS2015 Update 3. Upgrade if not.
                            std::this_thread::sleep_for(toWaitInMs);
                    }
                    m_oldClockTimestamp = curTimestamp;
                    m_oldFrameTimestamp = frameTimestamp;
                }
                LOG("[%d] send frame [%d] frame ptr=0x%x, queue size=%d", m_handle, res.second, frame, m_frameQueue.size());
                m_output->send(frame);
                libvmi_frame_release(res.second);
            }
        }

        m_counter.tick("");
    }
    m_state = STATE_STOPPED;

    LOG_INFO("[%d] <--", m_handle);
}

void CvMIOutput::stop()
{
    if (m_state != STATE_STARTED)
    {
        LOG_INFO("[%d] module is not on STARTED state, can't stop it",
                m_handle);
        return;
    }
    m_quit_process = true;
    auto newVal = std::make_pair(true, LIBVMI_INVALID_HANDLE);
    m_frameQueue.push(newVal);

    if (m_th_process.joinable())
    {
        m_th_process.join();
    }
    m_state=STATE_STOPPED;

}

void CvMIOutput::_enable_sync(bool flag) {

    if (flag == m_inSync)
        return;
    LOG_INFO("[%d] Sync enabled: %d, clock=%lu, actual ref timestamp: %lu", m_handle, flag, m_syncClock, m_syncTimestamp);
    m_inSync = flag;
}

