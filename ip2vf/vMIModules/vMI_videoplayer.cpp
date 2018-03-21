#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp
#include <iostream>
#include <fstream>
#include <mutex>
#include <signal.h>
#include <unistd.h>     // usleep

#include <X11/Xlib.h>


#include "log.h"
#include "tools.h"
#include "libvMI.h"

using namespace std;

#define MSG_MAX_LEN     1024

Display*            g_display = NULL;      // xwindow stuff
Visual*             g_visual = NULL;       // xwindow stuff
Window              g_window;       // xwindow stuff
GC                  g_gc;           // xwindow stuff

libvMI_module_handle    g_vMIModule = LIBVMI_INVALID_HANDLE;
unsigned char*      g_inBuffer = NULL;
unsigned char*      g_xbuffer = NULL;
std::mutex          g_mtx;          // mutex for critical section: protect ximage buffer
int                 g_frame_width = 0;
int                 g_frame_height = 0;
int                 g_frame_depth = 0;
unsigned int        g_frame_size = 0;
int                 g_sampling_fmt = 0;
bool                g_bExit = false;

/**
* Description: create a X window
* @method createXWindow
* @return
*/
void createXWindow() {
    XSetWindowAttributes    frame_attributes;
    XFontStruct             *fontinfo;
    XGCValues               gr_values;

    g_display = XOpenDisplay(NULL);
    g_visual = DefaultVisual(g_display, 0);
    int depth = DefaultDepth(g_display, 0);

    frame_attributes.background_pixel = XWhitePixel(g_display, 0);
    /* create the application window */
    g_window = XCreateWindow(g_display, XRootWindow(g_display, 0),
        0, 0, 100, 100, 5, depth,
        InputOutput, g_visual, CWBackPixel,
        &frame_attributes);
    XStoreName(g_display, g_window, "vMI probe window");
    XSelectInput(g_display, g_window, ExposureMask | StructureNotifyMask);

    fontinfo = XLoadQueryFont(g_display, "10x20");
    gr_values.font = fontinfo->fid;
    gr_values.foreground = XBlackPixel(g_display, 0);
    g_gc = XCreateGC(g_display, g_window, GCFont + GCForeground, &gr_values);
    XMapWindow(g_display, g_window);
}

/**
* Description: resize an X window
* @method resizeXWindow
* @param int w new width of the window
* @param int h new height of the window
* @return
*/
void resizeXWindow(int w, int h) {

    LOG_INFO("Resize to %dx%d", w, h);
    int result = XResizeWindow(g_display, g_window, w, h);
    //printf("XResizeWindow  return value: %d\n", result);
    if (result == BadValue) printf("   bad value!!!\n");
    if (result == BadWindow) printf("   bad window!!!\n");
}

/**
* Description: signal handler to exit properly
* @method signal_handler
* @param int signum trapping signal
* @return
*/
void signal_handler(int signum) {

    LOG_INFO("Got signal, exiting cleanly...");
    g_bExit = true;
}

/**
* Description: process an XEvent event
* @method processEvent
* @return
*/
void processEvent() {

    XEvent event;
    XNextEvent(g_display, &event);
    switch( event.type ) {
        case Expose:
        {
            if (g_xbuffer == NULL)
                break;
            //LOG("processEvent(): Expose event -->");
            std::unique_lock<std::mutex> lock(g_mtx);
            LOG("w=%d, h=%d, depth=%d", g_frame_width, g_frame_height, g_frame_depth);
            XImage* ximage = XCreateImage(g_display, g_visual, 24, ZPixmap, 0, (char*)g_xbuffer, g_frame_width, g_frame_height, g_frame_depth * 8, 0);
            if (ximage != 0) {
                XPutImage(g_display, g_window, g_gc, ximage, 0, 0, 0, 0, g_frame_width, g_frame_height);
                XFree(ximage);
            }
            else
                LOG_ERROR("can't create image");
            //LOG("processEvent(): Expose event <--");
            break;
        }
        case ButtonPress:
            //exit(0);
            g_bExit = true;
            break;
        default:
            break;
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

        if (g_frame_size != size || g_frame_width != w || g_frame_height != h || g_sampling_fmt != sampling_fmt || g_inBuffer==NULL || g_xbuffer==NULL) {
            g_frame_size = size;
            g_frame_width = w;
            g_frame_height = h;
            g_sampling_fmt = sampling_fmt;
            if (g_sampling_fmt == SAMPLINGFMT::RGB || g_sampling_fmt == SAMPLINGFMT::BGR)
                g_frame_depth = 3;
            else if (g_sampling_fmt == SAMPLINGFMT::RGBA || g_sampling_fmt == SAMPLINGFMT::BGRA)
                g_frame_depth = 4;
            else {
                LOG_ERROR("Sampling format not supported. Abort!");
                //exit(0);
            }
            if (g_inBuffer != NULL) {
                delete[] g_inBuffer;
                g_inBuffer = NULL;
            }
            if (g_xbuffer != NULL) {
                delete[] g_xbuffer;
                g_xbuffer = NULL;
            }
            if (g_inBuffer == NULL) {
                g_inBuffer = new unsigned char[g_frame_size];
            }
            if (g_xbuffer == NULL) {
                g_xbuffer = new unsigned char[g_frame_size];
            }
            LOG_INFO("Identify new format on input[%d], fmt=%d, size=%d bytes (%dx%d, %dbpp)", in, fmt, g_frame_size, g_frame_width, g_frame_height, g_frame_depth * 8);
            // Resize the window
            resizeXWindow(g_frame_width, g_frame_height);
        }

        if (fmt == MEDIAFORMAT::VIDEO && g_inBuffer != NULL) {

            std::unique_lock<std::mutex> lock(g_mtx);
            memcpy(g_xbuffer, pInFrameBuffer, g_frame_size);

            // If received frame is in RGBA format, need to convert to BGRA as XCreateImage expect only BGRA
            if (g_sampling_fmt == SAMPLINGFMT::RGBA || g_sampling_fmt == SAMPLINGFMT::BGRA) {
                unsigned char* p = g_xbuffer;
                for (int i = 0; i < (int)g_frame_size; i += 4) {
                    char t = *p;
                    *p = *(p + 2);
                    *(p + 2) = t;
                    p += 4;
                }
            }

            LOG("processVideoThread(): Sending event of type Expose\n");
            XEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = Expose;
            ev.xexpose.window = g_window;
            XSendEvent(g_display, g_window, False, ExposureMask, &ev);
            XFlush(g_display);
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
int main (int argc, char *argv[])
{
    int         port = -1;
    LogLevel    log_level = LOG_LEVEL_INFO;
    char        preconfig[MSG_MAX_LEN];
    bool        debug = false;

    preconfig[0]='\0';

    //////////////////////////////////////////////////////////////////////////////////
    // Check parameters
    if( argc >= 2 ) {
        for( int i=0; i<argc; i++ ) {
            if( strcmp(argv[i], "-debug") == 0 ) {
                log_level = LOG_LEVEL_VERBOSE;
            }
            else if( strcmp(argv[i], "-c")==0  && i+1<argc) {
                strncpy(preconfig, argv[i+1], MSG_MAX_LEN);
                preconfig[MSG_MAX_LEN-1]='\0';
            }
            else if( strcmp(argv[i], "-h") == 0 ) {
                std::cout << "usage: " << argv[0] << " [-h] -c <config> [-debug]\n";
                std::cout << "         -h \n";
                std::cout << "         -debug \n";
                std::cout << "         -c <config> \n";
                return 0;
            }
        }
    }

    if( strlen(preconfig)==0 ) {
        LOG_ERROR("main(): ***ERROR*** missing parameter: -c");
        exit(0);
    }
    setLogLevel(log_level);

    // set signal handler    
    if (signal(SIGINT, signal_handler) == SIG_ERR)
        LOG_ERROR("can't catch SIGINT");
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
        LOG_ERROR("can't catch SIGTERM");

    //////////////////////////////////////////////////////////////////////////////////
    // init xwindow stuff

    XInitThreads();
    createXWindow();

    //////////////////////////////////////////////////////////////////////////////////
    // init ip2vf stuff

    /*
    * Init the libvMI: provide a callback, and a pre-configuraion if any. If no pre-configuration here,
    * libvMI will wait for configuration provided by supervisor
    */
    g_vMIModule = libvMI_create_module(port, &libvMI_callback, preconfig);
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


    while ( !g_bExit ) {
        processEvent();
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

    if( g_inBuffer != NULL )
        delete[] g_inBuffer;
    if( g_xbuffer != NULL )
        delete[] g_xbuffer;

    LOG("<--");

    return(0);
}

