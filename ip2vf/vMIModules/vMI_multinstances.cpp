#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp
#include <iostream>     // cout
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

/*
 * Global variables
 */
libvMI_module_handle    g_vMIModule0 = LIBVMI_INVALID_HANDLE;
libvMI_module_handle    g_vMIModule1 = LIBVMI_INVALID_HANDLE;
std::condition_variable g_var;
std::mutex              g_mtx;
int                     g_nbFrame = 0;

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
void libvMI_callback0(const void* user_data, CmdType cmd, int param, libvMI_pin_handle in, libvMI_frame_handle hFrame)
{
    LOG("receive msg '%d'", cmd);
    switch (cmd) {
        case CMD_INIT:
            break;
        case CMD_START:
            break;
        case CMD_TICK:
            {
                int size = 0, fmt = 0, nb_output = 0, frm = 0;

                /*
                * A new frame is available
                */
                libvMI_get_frame_headers(hFrame, MEDIA_PAYLOAD_SIZE, &size);
                libvMI_get_frame_headers(hFrame, MEDIA_FORMAT, &fmt);
                libvMI_get_frame_headers(hFrame, VIDEO_SMPTEFRMCODE, &frm);
                nb_output = libvMI_get_output_count(g_vMIModule0);
                LOG_INFO("receive frame #%d on input[%d], fmt=%s, size=%d bytes, nb_output=%d, frm=0x%x", hFrame, in, (fmt == 1 ? "video" : "audio"), size, nb_output, frm);

                if (fmt == 1) {

                    /*
                    * keep all video frame and propagate it on all outputs
                    */
                    for (int i = 0; i < nb_output; i++) {

                        libvMI_pin_handle out = libvMI_get_output_handle(g_vMIModule0, i);
                        //LOG("Send buffer to output [%d]", out);
                        libvMI_send(g_vMIModule0, out, hFrame);
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
void libvMI_callback1(const void* user_data, CmdType cmd, int param, libvMI_pin_handle in, libvMI_frame_handle hFrame)
{
    LOG("receive msg '%d'", cmd);
    switch (cmd) {
    case CMD_INIT:
        break;
    case CMD_START:
        break;
    case CMD_TICK:
    {
        int size = 0, fmt = 0, nb_output = 0, frm = 0;

        /*
        * A new frame is available
        */
        libvMI_get_frame_headers(hFrame, MEDIA_PAYLOAD_SIZE, &size);
        libvMI_get_frame_headers(hFrame, MEDIA_FORMAT, &fmt);
        libvMI_get_frame_headers(hFrame, VIDEO_SMPTEFRMCODE, &frm);
        nb_output = libvMI_get_output_count(g_vMIModule1);
        LOG_INFO("receive frame #%d on input[%d], fmt=%s, size=%d bytes, nb_output=%d, frm=0x%x", hFrame, in, (fmt == 1 ? "video" : "audio"), size, nb_output, frm);

        if (fmt == 1) {

            /*
            * keep all video frame and propagate it on all outputs
            */
            for (int i = 0; i < nb_output; i++) {

                libvMI_pin_handle out = libvMI_get_output_handle(g_vMIModule1, i);
                //LOG("Send buffer to output [%d]", out);
                libvMI_send(g_vMIModule1, out, hFrame);
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
    char preconfig0[MSG_MAX_LEN];
    char preconfig1[MSG_MAX_LEN];
    bool use_preconfig = false;

    bool demoMode = false;
    bool debug = false;
    int  debug_time_in_sec = 2;

    // Check parameters
    if (argc >= 2) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-c0") == 0 && i + 1 < argc) {
                STRNCPY(preconfig0, argv[i + 1], MSG_MAX_LEN);
                preconfig0[MSG_MAX_LEN - 1] = '\0';
                use_preconfig = true;
                i++;
            } else if (strcmp(argv[i], "-c1") == 0 && i + 1 < argc) {
                STRNCPY(preconfig1, argv[i + 1], MSG_MAX_LEN);
                preconfig1[MSG_MAX_LEN - 1] = '\0';
                use_preconfig = true;
                i++;
            }
            else if (strcmp(argv[i], "-v") == 0) {
                tools::displayVersion();
                return 0;
            }
            else if (strcmp(argv[i], "-d") == 0) {
                debug = true;
                if (i + 1 < argc)
                    debug_time_in_sec = atoi(argv[i + 1]);
            }
            else if (strcmp(argv[i], "-h") == 0) {
                std::cout << "usage: " << argv[0] << " [-h] [-v] [-c <config>]\n";
                std::cout << "         -h   display this help\n";
                std::cout << "         -v   display module version\n";
                std::cout << "         -c0 <config>  module configuration string \n";
                std::cout << "         -c1 <config>  module configuration string \n";
                return 0;
            }
            else {
                std::cout << "unknown parameter '" << argv[i]  << "'. use -h to see usage.\n";
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
    g_vMIModule0 = libvMI_create_module(&libvMI_callback0, (use_preconfig ? preconfig0 : NULL));
    if (g_vMIModule0 == LIBVMI_INVALID_HANDLE) {
        LOG_ERROR("invalid Module id. Abort!");
        return 0;
    }
    g_vMIModule1 = libvMI_create_module(&libvMI_callback1, (use_preconfig ? preconfig1 : NULL));
    if (g_vMIModule1 == LIBVMI_INVALID_HANDLE) {
        LOG_ERROR("invalid Module id. Abort!");
        return 0;
    }
    LOG_INFO("init COMPLETED");

    /*
    * Start the module. The lib will notify a CMD_START via the callback when Start is completed. 
    * From this point, the module will starts to receive media frames from inputs
    */
    libvMI_start_module(g_vMIModule0);
    libvMI_start_module(g_vMIModule1);
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
    libvMI_stop_module(g_vMIModule0);
    libvMI_stop_module(g_vMIModule1);
    LOG_INFO("stop COMPLETED");

    /*
    * Close the module and free all resources.
    * Note that from this point, module handle and all inputs/outputs handles will be invalidated.
    */
    libvMI_close(g_vMIModule0);
    g_vMIModule0 = LIBVMI_INVALID_HANDLE;
    libvMI_close(g_vMIModule1);
    g_vMIModule1 = LIBVMI_INVALID_HANDLE;
    LOG_INFO("close COMPLETED");

    LOG("<--");
    return 0;
}
