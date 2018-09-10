#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp
#include <iostream>     // cout
#include <signal.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#ifndef _WIN32
#include <unistd.h>
#define sleep_ms(x) usleep(x*1000)   // usleep is in microseconds
#else
#include <Windows.h>
#define sleep_ms(x) Sleep(x)   // Sleep is in milliseconds
#endif

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

/*
 * Global variables
 */
libvMI_module_handle     g_vMIModule = LIBVMI_INVALID_HANDLE;
std::condition_variable  g_var;
std::mutex               g_mtx;
int                      g_nbFrame = 0;
char                     g_userData[128];


/**
* Description: signal handler to exit properly
* @method signal_handler
* @param int signum trapping signal
* @return
*/
void signal_handler(int signum) {

    static int nTry = 0;
    nTry++;
    LOG_INFO("(%d) Got signal, exiting cleanly...", nTry);
    if (nTry < 3) {

        std::unique_lock<std::mutex> lock(g_mtx);
        g_var.notify_all();
        lock.unlock();
    }
    else {

        abort();
    }
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
    LOG("receive msg '%d' for in=%d, with user data=0x%x", cmd, in, user_data);
    switch (cmd) {
        case CMD_INIT:
            LOG_INFO("receive CMD_INIT");
            break;
        case CMD_START:
            LOG_INFO("receive CMD_START");
            break;
        case CMD_TICK:
            {
                /*
                * A new frame is available: firstly, get the buffer address
                */
                int size = 0, fmt = 0;
                char* pInFrameBuffer = libvMI_get_frame_buffer(hFrame);
                libvMI_get_frame_headers(hFrame, MEDIA_PAYLOAD_SIZE, &size);
                libvMI_get_frame_headers(hFrame, MEDIA_FORMAT, &fmt);

                LOG("receive frame %d on input[%d], fmt=%s(%d), size=%d bytes, at 0x%x", hFrame, in, (fmt== MEDIAFORMAT::VIDEO ?"video":"audio"), fmt, size, pInFrameBuffer);
                
                int nb = libvMI_get_input_count(g_vMIModule);
                for (int i = 0; i < nb; i++) {
                    int inputHandle = libvMI_get_input_handle(g_vMIModule, i);
                }
                nb = libvMI_get_output_count(g_vMIModule);
                for (int i = 0; i < nb; i++) {
                    int outputHandle = libvMI_get_output_handle(g_vMIModule, i);
                }
                if ((fmt == MEDIAFORMAT::VIDEO) || (fmt == MEDIAFORMAT::AUDIO)) {

                    /*
                    * Send video frame on first output, and audio on second output
                    */ 
                    int index = (fmt == MEDIAFORMAT::VIDEO ? 0 : 1);
                    if (index < libvMI_get_output_count(g_vMIModule)) {
                        int outputHandle = libvMI_get_output_handle(g_vMIModule, index);
                        if (outputHandle != LIBVMI_INVALID_HANDLE) {
                            LOG("Send buffer to %d", outputHandle);
                            libvMI_send(g_vMIModule, outputHandle, hFrame);
                        }
                    }
                }

                /*
                * Always release the current frame
                */
                libvmi_frame_release(hFrame);
            }
            break;
        case CMD_STOP:
            LOG_INFO("receive CMD_STOP");
            break;
        case CMD_QUIT:
            LOG_INFO("receive CMD_QUIT");
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
    int port = -1;
    char preconfig[MSG_MAX_LEN];
    bool use_preconfig = false;

    bool demoMode = false;
    bool debug = false;
    int  debug_time_in_sec = 2;

    // Check parameters
    if (argc >= 2) {
        for (int i = 0; i < argc; i++) {
            if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
                STRNCPY(preconfig, argv[i + 1], MSG_MAX_LEN);
                preconfig[MSG_MAX_LEN - 1] = '\0';
                use_preconfig = true;
            } else if (strcmp(argv[i], "-v") == 0) {
                tools::displayVersion();
                return 0;
            } else if (strcmp(argv[i], "-d") == 0) {
                debug = true;
                if(i + 1 < argc)
                    debug_time_in_sec = atoi(argv[i + 1]);
            }
            else if (strcmp(argv[i], "-h") == 0) {
                std::cout << "usage: " << argv[0] << " [-h] [-v] [-c <config>]\n";
                std::cout << "         -h   display this help\n";
                std::cout << "         -v   display module version\n";
                std::cout << "         -c <config>  module configuration string \n";
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

    // Create some user data, for test purpose
    STRCPY(g_userData, "module vMIDemux");

    /*
    * Init the libvMI: provide a callback, and a pre-configuraion if any. If no pre-configuration here,
    * libvMI will wait for configuration provided by supervisor
    */
    std::unique_lock<std::mutex> lock(g_mtx);
    g_vMIModule = libvMI_create_module_ext(port, &libvMI_callback, (use_preconfig ? preconfig : NULL), (const void*) g_userData);
    if (g_vMIModule == LIBVMI_INVALID_HANDLE) {
        LOG_ERROR("invalid Module id. Abort!");
        return 0;
    }
    LOG_INFO("init COMPLETED");

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
        LOG_INFO("*** ");
        LOG_INFO("*** WARN, use debug mode. will stop in %d seconds.***", debug_time_in_sec);
        LOG_INFO("***");
        sleep_ms(debug_time_in_sec * 1000);
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

    LOG("<--");    return 0;
}
