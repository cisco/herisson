#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp
#include <iostream>     // cout
#include <climits>      // ULONG_MAX
#include <signal.h>
#include <thread>
#include <mutex>
#ifndef _WIN32
#include <unistd.h>
#define sleep_ms(x) usleep(x*1000)   // usleep is in microseconds
#else
#include <Windows.h>
#define sleep_ms(x) Sleep(x)   // Sleep is in milliseconds
#endif
#include <condition_variable>

#include "log.h"
#include "tools.h"
#include "libvMI.h"

using namespace std;

#ifdef _WIN32

#define VSNPRINTF(a,...)   vsnprintf(a, (sizeof(a)/sizeof(char)), __VA_ARGS__)
#define SNPRINTF(a,...)    _snprintf_s(a, (sizeof(a)/sizeof(char)), _TRUNCATE, __VA_ARGS__)
#define STRCPY(a,b)        strcpy_s(a, (sizeof(a)/sizeof(char)), b)
#define STRNCPY(a,b,c)     strncpy_s(a, (sizeof(a)/sizeof(char)), b, c)
#define STRCAT(a,b)        strcat_s(a, (sizeof(a)/sizeof(char)), b)

#else   // _WIN32

#define VSNPRINTF(a,...)    vsnprintf(a, (sizeof(a)/sizeof(char)), __VA_ARGS__)
#define SNPRINTF(a,...)     snprintf(a, (sizeof(a)/sizeof(char)), __VA_ARGS__)
#define STRCPY              strcpy
#define STRNCPY             strncpy
#define STRCAT              strcat

#endif  // _WIN32

/*
 * Some defines...
 */
#define MSG_MAX_LEN     1024
#define LOOP_DETECT     (ULONG_MAX/2)


/*
 * Global variables
 */
libvMI_module_handle    g_vMIModule = LIBVMI_INVALID_HANDLE;
std::condition_variable g_var;
std::mutex              g_mtx;
libvMI_pin_handle       g_firstInput = -1;
libvMI_pin_handle       g_secondInput = -1;
libvMI_pin_handle       g_refInput = -1;
bool                    g_init = false;
unsigned int            g_firstTimestamp = 0;
bool                    g_activesync = true;


/**
* Description: signal handler to exit properly
* @method signal_handler
* @param int signum trapping signal
* @return
*/
void signal_handler(int signum) {

    LOG_INFO("Got signal, exiting cleanly...");
    std::unique_lock<std::mutex> lock(g_mtx);
    g_var.notify_all();
    lock.unlock();
}


/*
* Description: Callback used by libvMI to communicate with us
* @method libvMI_callback
* @param const void* user_data Some user defined value (if any). Null if not used.
* @param CmdType cmd Command type (defined on ip2vf.h)
* @param int param (some values returned by libip2vf, not used for now)
* @param libvMI_pin_handle in handle of the pin providing cmd, LIBVMI_INVALID_HANDLE if none
* @param libvMI_frame_handle hFrame handle of the vMI frame provided. LIBVMI_INVALID_HANDLE if not relevant.
* @return
*/
void libvMI_callback(const void* user_data, CmdType cmd, int param, libvMI_pin_handle in, libvMI_frame_handle hFrame)
{
    LOG("receive msg '%d'", cmd);
    switch (cmd) {
        case CMD_INIT:
            break;
        case CMD_START:
            break;
        case CMD_TICK:
            {
                int size = 0, fmt = 0, framenumber = 0;
                unsigned int timestamp = 0;

                /*
                * A new frame is available
                */
                libvMI_get_frame_headers(hFrame, MEDIA_PAYLOAD_SIZE, &size);
                libvMI_get_frame_headers(hFrame, MEDIA_FORMAT, &fmt);
                libvMI_get_frame_headers(hFrame, MEDIA_FRAME_NB, &framenumber);
                libvMI_get_frame_headers(hFrame, MEDIA_TIMESTAMP, &timestamp);

                if (framenumber % 100 == 0) {
                    int curFrameInList = 0;
                    libvMI_get_parameter(CUR_FRAMES_IN_LIST, &curFrameInList);
                    int freeFrameInList = 0;
                    libvMI_get_parameter(FREE_FRAMES_IN_LIST, &freeFrameInList);
                    LOG_INFO("current free frame in list=%d/%d", freeFrameInList, curFrameInList);
                }

                if(framenumber%10==0)
                    LOG("receive frame #%d on input[%d], fmt=%s, timestamp=%lu, size=%d bytes", framenumber, in, (fmt == 1 ? "video" : "audio"), timestamp, size);

                if (g_init) {

                    // Refresh ref timestamp if needed 
                    if (in == g_refInput && framenumber % 100 == 0) {
                        unsigned int value = timestamp;
                        int hOutput1 = libvMI_get_output_handle(g_vMIModule, 0);
                        int hOutput2 = libvMI_get_output_handle(g_vMIModule, 1);
                        libvMI_set_output_parameter(g_vMIModule, hOutput1, SYNC_TIMESTAMP, &value);
                        libvMI_set_output_parameter(g_vMIModule, hOutput2, SYNC_TIMESTAMP, &value);
                        LOG("Refreshing ref timestamp=%lu", timestamp);
                    }
                    // start to send frame
                    int index = (g_firstInput == in)?0:1;
                    int outputHandle = libvMI_get_output_handle(g_vMIModule, index);
                    if (outputHandle != LIBVMI_INVALID_HANDLE) {
                        //LOG_INFO("Send frame #%d to %d, timestamp=%lu", framenumber, outputHandle, timestamp);
                        libvMI_send(g_vMIModule, outputHandle, hFrame);
                    }
                }
                else {

                    if (g_firstInput == -1 || g_firstInput == in) {

                        g_firstInput = in;
                        g_firstTimestamp = timestamp;
                        LOG_INFO("Identify first input [%d] with frame#%d, timestamp=%lu", g_firstInput, framenumber, timestamp);
                    }
                    else if (g_secondInput == -1 && g_firstInput != in) {

                        g_secondInput = in;
                        LOG_INFO("Identify second input [%d] with frame#%d, timestamp=%lu", g_secondInput, framenumber, timestamp);

                        // Determinate which is our ref input pin. It will be the stream in late.
                        if (g_firstTimestamp > (unsigned int)timestamp) {
                            // The first stream seems in advance comparing to the secondone... except if second one has already looped
                            g_refInput = g_secondInput;
                            if ((g_firstTimestamp - timestamp) > LOOP_DETECT) 
                                g_refInput = g_firstInput;
                        }
                        else {
                            g_refInput = g_firstInput;
                            if ((timestamp - g_firstTimestamp) > LOOP_DETECT)
                                g_refInput = g_secondInput;
                        }
                        unsigned int refTimestamp = (g_refInput == g_firstInput) ? g_firstTimestamp : timestamp;
                        LOG_INFO("Init completed. Reference is input [%d] with timestamp=%lu", g_refInput, refTimestamp);

                        // Now, configurate output pin...
                        if (g_activesync) {
                            int nb_output = libvMI_get_output_count(g_vMIModule);
                            int hOutput1 = libvMI_get_output_handle(g_vMIModule, 0);
                            int hOutput2 = libvMI_get_output_handle(g_vMIModule, 1);
                            unsigned int value = refTimestamp;
                            libvMI_set_output_parameter(g_vMIModule, hOutput1, SYNC_TIMESTAMP, &value);
                            libvMI_set_output_parameter(g_vMIModule, hOutput2, SYNC_TIMESTAMP, &value);
                            value = 1;
                            libvMI_set_output_parameter(g_vMIModule, hOutput1, SYNC_ENABLED, &value);
                            libvMI_set_output_parameter(g_vMIModule, hOutput2, SYNC_ENABLED, &value);
                        }
                        
                        g_init = true;
                    }
                }
                /*
                * Always release the current frame
                */
                libvmi_frame_release(hFrame);
            }
            break;
        case CMD_STOP:
            break;
        case CMD_QUIT:
            {
                std::unique_lock<std::mutex> lock(g_mtx);
                g_var.notify_all();
                lock.unlock();
            }
            break;
        default:
            LOG("unknown cmd %d ", cmd);
    }
}

/*
 * Description: main
 * @method main
 * @param int argc
 * @param char* argv[]
 * @return int
 */
int main(int argc, char* argv[]) {

    bool autostart = false;
    char preconfig[MSG_MAX_LEN];
    bool use_preconfig = false;

    bool demoMode = false;
    bool debug = false;
    int  debug_time_in_sec = 2;

    // Check parameters
    if (argc >= 2) {
        for (int i = 0; i < argc; i++) {
            if (strcmp(argv[i], "-autostart") == 0) {
                autostart = true;
            } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
                STRNCPY(preconfig, argv[i + 1], MSG_MAX_LEN);
                preconfig[MSG_MAX_LEN - 1] = '\0';
                use_preconfig = true;
            } else if (strcmp(argv[i], "-v") == 0) {
                tools::displayVersion();
                return 0;
            }
            else if (strcmp(argv[i], "-d") == 0) {
                debug = true;
            }
            else if (strcmp(argv[i], "-nosync") == 0) {
                g_activesync = false;
                LOG_INFO("Warning, SYNC MODE DISABLED");
            }
            else if (strcmp(argv[i], "-h") == 0) {
                std::cout << "usage: " << argv[0] << " [-h] [-v] [-c <config>] [-autostart] [-s <image size>]\n";
                std::cout << "         -h \n";
                std::cout << "         -v \n";
                std::cout << "         -c <config> \n";
                std::cout << "         -autostart \n";
                return 0;
            }
        }
    }

    LOG("-->");

    // set signal handler    
    if (!debug) {
        if (signal(SIGINT, signal_handler) == SIG_ERR)
            LOG_ERROR("can't catch SIGINT");
        if (signal(SIGTERM, signal_handler) == SIG_ERR)
            LOG_ERROR("can't catch SIGTERM");
    }

    /*
     * Init the libvMI: provide a callback, and a pre-configuraion if any. If no pre-configuration here, 
     * libvMI will wait for configuration provided by supervisor
     */
    std::unique_lock<std::mutex> lock(g_mtx);
    g_vMIModule = libvMI_create_module(&libvMI_callback, (use_preconfig ? preconfig : NULL));
    if (g_vMIModule == LIBVMI_INVALID_HANDLE) {
        LOG_ERROR("invalid Module id. Abort!");
        return 0;
    }
    LOG_INFO("init COMPLETED");

    int nbMaxFrameInList = 60;
    libvMI_set_parameter(MAX_FRAMES_IN_LIST, &nbMaxFrameInList);

    /*
    * Start the module. The lib will notify a CMD_START via the callback when Start is completed. 
    * From this point, the module will starts to receive media frames from inputs
    */
    libvMI_start_module(g_vMIModule);
    LOG_INFO("start COMPLETED");

    /*
     * wait for exit cmd
     */
    if (debug) {
        LOG_INFO("*** WARN, use debug mode. will stop in %d seconds.***", debug_time_in_sec);
        sleep_ms(debug_time_in_sec*1000);
    }
    else {
        g_var.wait(lock);
        lock.unlock();
    }

    /*
    * Stop the module. The lib will notify a CMD_STOP via the callback when Stop is completed.
    * From this point, the module will no longer received media frames from inputs.
    * Note that module resources are not free... We can use start again to restart data flow processing.
    */
    libvMI_stop_module(g_vMIModule);
    LOG_INFO("stop COMPLETED");

    /*
    * Close the module and free all resources.
    * Note that from this point, module handle and all inputs/outputs handles will be invalidated.
    */
    libvMI_close(g_vMIModule);
    g_vMIModule = LIBVMI_INVALID_HANDLE;
    LOG_INFO("close COMPLETED");

    LOG("<--");
    return 0;
}
