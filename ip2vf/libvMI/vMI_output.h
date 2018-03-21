
#ifndef _VMI_OUTPUT_H
#define _VMI_OUTPUT_H

#include <string>
#include <pins/pins.h>
#include "common.h"
#include "queue.h"
#include "moduleconfiguration.h"
#include "tools.h"

#include "libvMI.h"

class CvMIOutput {
    int                    m_id;
    libvMI_pin_handle      m_handle;
    std::string            m_preconfig;
    CModuleConfiguration*  m_config;
    COut*                  m_output = NULL;
    CFrameHeaders*         m_Outframefactory = NULL;
    CFrameCounter          m_counter;
    const void*            m_userData;
    State                  m_state;
    bool                   m_quit_process = false;
    std::thread            m_th_process;
    CQueue<std::pair <bool, libvMI_frame_handle> > m_frameQueue;
    bool                   m_inSync;
    unsigned int           m_syncTimestamp;
    unsigned int           m_syncClock;     // In MHz
    unsigned int           m_oldClockTimestamp;
    unsigned int           m_oldFrameTimestamp;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_syncRefTime;

public:

    CvMIOutput(const std::string &configurationString, libvMI_pin_handle handle, const void* user_data);
    virtual ~CvMIOutput();


    inline const libvMI_pin_handle getHandle() {
        return m_handle;
    }

    inline COut * getOutputManager() {
        return m_output;
    }

    inline CFrameCounter* getFrameCounter() {
        return &m_counter;
    }

    inline CFrameHeaders * getFrameHeaders() {
        return m_Outframefactory;
    }

    void setParameter(OUTPUTPARAMETER param, void* value);
    void getParameter(OUTPUTPARAMETER param, void* value);

    /**
    * replicates a source frame's headers in their entirity in the out frame:
    */
    void copyFrameHeaderValues(const CFrameHeaders &source);

    void WriteFrameHeaders(unsigned char *buffer);

    int send(libvMI_frame_handle hFrame);

    /**
    * TODO: write description for internal function carried over from old interface
    * TODO: refactor this old code
    */
    void _init(const std::string & name, int id);

    void start();

    void stop();

    void _process();

    void _enable_sync(bool flag);

};

#endif // _VMI_OUTPUT_H
