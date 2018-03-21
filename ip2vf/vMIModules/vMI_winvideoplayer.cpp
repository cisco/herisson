#include <opencv2/imgproc/imgproc.hpp> 
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <thread>
#include <mutex>
#ifndef _WIN32
#include <unistd.h>
#else
#include <Windows.h>
#define sleep(x) Sleep(x)
#endif

#include "log.h"
#include "tools.h"
#include "queue.h"
#include "libvMI.h"

using namespace cv;
using namespace std;

#define MSG_MAX_LEN         1024
#define OPENCV_WINDOW_NAME  "vMI probe window"

/*
* Global variables
*/
libvMI_module_handle    g_vMIModule = LIBVMI_INVALID_HANDLE;
std::mutex      g_mtx;
int             g_nbFrame = 0;
bool            g_bExit =false;


using tChannelItem = std::tuple<  libvMI_pin_handle /*pin handle*/, int /*w*/, int /*h*/, int /*sampling_fmt*/, Mat /*Im*/>;
std::vector< tChannelItem > g_channels;
Mat g_mat = Mat(480, 640, CV_8UC4);
CQueue<tChannelItem>     g_q;

void signal_handler(int signum) {

    LOG_INFO("Got signal, exiting cleanly...");
    g_bExit = true;
    g_q.push(std::make_tuple(0, 480, 640, SAMPLINGFMT::RGBA, Mat(480, 640, CV_8UC4)));
}

Mat createNewImage() {
    int col = 0, row = 0;
    if (g_channels.size() == 1) {
        row = 1; col = 1; 
    }
    else if (g_channels.size() == 2) {
        row = 1; col = 2;
    }
    else if (g_channels.size() < 5) {
        row = 2; col = 2;
    }
    Mat dstImg = Mat(row*270, col*480, CV_8UC3);
    int xpos = 0, ypos = 0;
    for (std::vector< tChannelItem >::iterator it = g_channels.begin(); it != g_channels.end(); ++it) {
        Mat matRoi = dstImg(Rect(xpos * 480, ypos * 270, 480, 270));
        std::get<4>(*it).copyTo(matRoi);
        xpos += 1;
        if (xpos >= col) {
            ypos += 1;
            xpos = 0;
        }
    }
    return dstImg;
}

/*tChannelItem& getChannelItem(libvMI_pin_handle pinHandle) {

    std::unique_lock<std::mutex> lock(g_channelsMutex);
    for (std::vector< tChannelItem >::iterator it = g_channels.begin(); it != g_channels.end(); ++it) {
        if (std::get<0>(*it)== pinHandle) 
            return *it;
    }
    // Not found
    tChannelItem item = std::make_tuple(pinHandle, 480, 640, SAMPLINGFMT::RGBA, Mat(480, 640, CV_8UC4));
    g_channels.push_back(item);
    LOG_INFO("create new item with handle [%d], now frame array size =%d", std::get<0>(item), g_channels.size());
    return item;
}*/




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
void libvMI_callback(const void* user_data, CmdType cmd, int param, libvMI_pin_handle in, libvMI_pin_handle hFrame)
{
    LOG("receive msg '%d'", cmd);
    switch (cmd) {
    case CMD_INIT:
        break;
    case CMD_START:
        break;
    case CMD_TICK:
    {
        int fmt = 0, nb_output = 0;
        /*
        * A new frame is available: firstly, get the buffer address, then retreive wanted headers
        */
        unsigned char* pInFrameBuffer = (unsigned char*)libvMI_get_frame_buffer(hFrame);
        int size = 0, media_fmt = 0, w = 0, h = 0, sampling_fmt = 0;
        libvMI_get_frame_headers(hFrame, MEDIA_PAYLOAD_SIZE, &size);
        libvMI_get_frame_headers(hFrame, MEDIA_FORMAT, &fmt);
        libvMI_get_frame_headers(hFrame, VIDEO_WIDTH, &w);
        libvMI_get_frame_headers(hFrame, VIDEO_HEIGHT, &h);
        libvMI_get_frame_headers(hFrame, VIDEO_FORMAT, &sampling_fmt);

        LOG("receive buffer 0x%x on input[%d], fmt=%d, size=%d bytes (%dx%d, %dbpp)", pInFrameBuffer, in, fmt, size, w, h, sampling_fmt * 8);

        if (fmt == MEDIAFORMAT::VIDEO) {

            long long t1 = tools::getCurrentTimeInMilliS();
            auto item = std::make_tuple(in, w, h, sampling_fmt, Mat(h, w, CV_8UC3));
            if (sampling_fmt == SAMPLINGFMT::RGB)
                std::get<4>(item) = Mat(h, w, CV_8UC3, pInFrameBuffer);
            else if (sampling_fmt == SAMPLINGFMT::BGR) {
                Mat src = Mat(h, w, CV_8UC3, pInFrameBuffer);
                cvtColor(src, std::get<4>(item), COLOR_BGR2RGB);
            }
            else if (sampling_fmt == SAMPLINGFMT::RGBA) {
                Mat src = Mat(h, w, CV_8UC4, pInFrameBuffer);
                cvtColor(src, std::get<4>(item), COLOR_RGBA2RGB);
            }
            else if (sampling_fmt == SAMPLINGFMT::BGRA) {
                Mat src = Mat(h, w, CV_8UC4, pInFrameBuffer);
                cvtColor(src, std::get<4>(item), COLOR_BGRA2RGB);
            }
            else {
                LOG_ERROR("unsupported format");
            }
            g_q.push(item);
            long long t4 = tools::getCurrentTimeInMilliS();
            //LOG_INFO("Process take %d ms", (int)(t4 - t1));        
        }

        // IMPORTANT... We didn't consume the frame (no send...) then need to resume explicitely the current frame
        libvmi_frame_release(hFrame);
    }
    break;
    case CMD_STOP:
        break;
    case CMD_QUIT:
        g_bExit = true;
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
int main( int argc, char** argv )
{
    int         port = -1;
    char        preconfig[MSG_MAX_LEN];
    bool        use_preconfig = false;
    LogLevel    log_level = LOG_LEVEL_INFO; // LOG_LEVEL_VERBOSE;
    bool        debug = false;

    //////////////////////////////////////////////////////////////////////////////////
    // Check parameters
    preconfig[0] = '\0';
    if (argc >= 2) {
        for (int i = 0; i<argc; i++) {
            if (strcmp(argv[i], "-debug") == 0) {
                log_level = LOG_LEVEL_VERBOSE;
            }
            else if (strcmp(argv[i], "-c") == 0 && i + 1<argc) {
                strncpy(preconfig, argv[i + 1], MSG_MAX_LEN);
                preconfig[MSG_MAX_LEN - 1] = '\0';
                use_preconfig = true;
            }
            else if (strcmp(argv[i], "-h") == 0) {
                std::cout << "usage: " << argv[0] << " [-h] -c <config> [-debug]\n";
                std::cout << "         -h \n";
                std::cout << "         -debug \n";
                std::cout << "         -c <config> \n";
                return 0;
            }
        }
    }

    if (strlen(preconfig) == 0) {
        LOG_ERROR("main(): ***ERROR*** missing parameter: -c");
        exit(0);
    }

    // set signal handler    
    if (signal(SIGINT, signal_handler) == SIG_ERR)
        LOG_ERROR("can't catch SIGINT");
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
        LOG_ERROR("can't catch SIGTERM");

    namedWindow(OPENCV_WINDOW_NAME, WINDOW_AUTOSIZE);// Create a window for display.

    //////////////////////////////////////////////////////////////////////////////////
    // init ip2vf stuff

    /*
    * Init the libvMI: provide a callback, and a pre-configuraion if any. If no pre-configuration here,
    * libvMI will wait for configuration provided by supervisor
    */
    g_vMIModule = libvMI_create_module(port, &libvMI_callback, (use_preconfig ? preconfig : NULL));
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
    while (!g_bExit) {

        if (g_q.size() > 1) {
            // Get new item
            auto item = g_q.pop();
            bool bFound = false;
            for (std::vector< tChannelItem >::iterator it = g_channels.begin(); it != g_channels.end(); ++it) {
                if (std::get<0>(*it) == std::get<0>(item)) {
                    *it = item;
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
                g_channels.push_back(item);
            //LOG_INFO("g_q.size()=%d, g_channels.size()=%d", g_q.size(), g_channels.size());

            g_mat = createNewImage();// std::get<4>(item);
        }

        imshow(OPENCV_WINDOW_NAME, g_mat);
        if (waitKey(1) != -1) {
            LOG_INFO("Key detected. Exit.");
            break;
        }
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

    return(0);
}

