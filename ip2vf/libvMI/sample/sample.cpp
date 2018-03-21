/**
* \file sample.cpp
* \brief Sample code to use libvMI
* \author A.Taldir M.Hawari
* \version 0.0
* \date 06 october 2017
*
* Create a small application, using libvMI, that propagate any VIDEO frames received by inputs, on all outputs.
*
* 2017/10/06 - version 0.0 - creation
*
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp
#include <iostream>     // cout
#include <signal.h>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "libvMI.h"

using namespace std;

/**
 * \brief Some defines...
 */
#define MSG_MAX_LEN     1024

/**
 * \brief Global variables
 */
libvMI_module_handle    g_vMIModule = LIBVMI_INVALID_HANDLE;
std::condition_variable g_var;
std::mutex              g_mtx;
int                     g_nbFrame = 0;

/**
 * \brief signal_handler
 */
void signal_handler(int signum) {

    std::cout << "signal_handler(): Got signal, exiting cleanly..." << std::endl;
    std::unique_lock<std::mutex> lock(g_mtx);
    g_var.notify_all();
    lock.unlock();
}

/**
 * \brief process_data
 */
void process_data(unsigned char* in, unsigned char* out, int size) {
    g_nbFrame++;
    memcpy(out, in, size);
}

/*
 * \brief Callback used by libvMI to communicate with us
 * 
 * \param user_data (some values returned by libvMI, not used for now)
 * \param cmd event id of type 'CmdType' (defined on libvMI.h) that is provided by the library
 * \param param (some values returned by libvMI, not used for now)
 * \param in handle of the input that provide the event
 */
void libvMI_callback(const void* user_data, CmdType cmd, int param, libvMI_pin_handle in, libvMI_frame_handle hFrame)
{
    switch (cmd) {
        case CMD_INIT:
            break;
        case CMD_START:
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

                std::cout << "libvMI_callback(): receive frame [" << hFrame << "] on input[" << in << "], fmt=" << (fmt == 1 ? "video" : "audio") << ", size=" << size << " bytes" << std::endl;

                if (fmt == MEDIAFORMAT::VIDEO ) {

                    int nb_output = libvMI_get_output_count(g_vMIModule);
                    /*
                    * Iterate on each output to copy and send what received from input
                    */
                    for (int i = 0; i < nb_output; i++) {
                        int outputHandle = libvMI_get_output_handle(g_vMIModule, i);
                        if (outputHandle != LIBVMI_INVALID_HANDLE) {
                            std::cout << "Send buffer to output [" << outputHandle << "]" << std::endl;
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
            break;
        case CMD_QUIT:
            {
                std::unique_lock<std::mutex> lock(g_mtx);
                g_var.notify_all();
                lock.unlock();
            }
            break;
        default:
            std::cout << "libvMI_callback(): unknown cmd '" << cmd << "'" << std::endl;
    }
}

/*
 * \brief main
 * 
 * \param argc number of argument on the command line
 * \param argv pointer to a char array that contain command string tokens
 * \return int
 */
int main(int argc, char* argv[]) {
    int port = 5010;
    bool autostart = false;
    char preconfig[MSG_MAX_LEN];
    bool use_preconfig = false;

    bool demoMode = false;

    // Check parameters
    if (argc >= 2) {
        for (int i = 0; i < argc; i++) {
            if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
                port = atoi(argv[i + 1]);
            } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
                strncpy(preconfig, argv[i + 1], MSG_MAX_LEN);
                preconfig[MSG_MAX_LEN - 1] = '\0';
                use_preconfig = true;
            } else if (strcmp(argv[i], "-h") == 0) {
                std::cout << "usage: " << argv[0] << " [-h] [-v] [-p <port>] [-c <config>] [-autostart] [-s <image size>]" << std::endl;
                std::cout << "         -h " << std::endl;
                std::cout << "         -v " << std::endl;
                std::cout << "         -p <port> " << std::endl;
                std::cout << "         -c <config> " << std::endl;
                std::cout << "         -autostart " << std::endl;
                return 0;
            }
        }
    }

    std::cout << "main(): -->" << std::endl;

    // set signal handler    
    if (signal(SIGINT, signal_handler) == SIG_ERR)
        std::cout << "main(): *E* can't catch SIGINT" << std::endl;
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
        std::cout << "main(): *E* can't catch SIGTERM" << std::endl;

    /*
     * Init the libvMI: provide a callback, and a pre-configuraion if any. If no pre-configuration here, 
     * libvMI will wait for configuration provided by supervisor
     */
    std::unique_lock<std::mutex> lock(g_mtx);
    g_vMIModule = libvMI_create_module(port, &libvMI_callback, (use_preconfig ? preconfig : NULL));
    if (g_vMIModule == LIBVMI_INVALID_HANDLE) {
        std::cout << "main(): *E* invalid Module id. Abort!" << std::endl;
        return 0;
    }

    /*
    * Start the module. The lib will notify a CMD_START via the callback when Start is completed. 
    * From this point, the module will starts to receive media frames from inputs
    */
    libvMI_start_module(g_vMIModule);

    /*
     * wait for exit cmd
     */
    g_var.wait(lock);
    lock.unlock();

    /*
    * Stop the module. The lib will notify a CMD_STOP via the callback when Stop is completed.
    * From this point, the module will no longer received media frames from inputs.
    * Note that module resources are not free... We can use start again to restart data flow processing.
    */
    libvMI_stop_module(g_vMIModule);

    /*
    * Close the module and free all resources.
    * Note that from this point, module handle and all inputs/outputs handles will be invalidated.
    */
    libvMI_close(g_vMIModule);
    g_vMIModule = LIBVMI_INVALID_HANDLE;

    std::cout << "main(): <--" << std::endl;
    return 0;
}
