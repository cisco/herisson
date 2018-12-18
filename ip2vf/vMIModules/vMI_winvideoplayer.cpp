#include <opencv2/imgproc/imgproc.hpp> 
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <thread>
#include <mutex>
#include <map>
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

#define MSG_MAX_LEN             1024
#define OPENCV_WINDOW_NAME      "vMI probe window"
#define THUMBNAIL_SIZE_WIDTH    480
#define THUMBNAIL_SIZE_HEIGHT   270
#define DEFAULT_IMAGE           "noimg.jpg"

#if defined(WIN32) || defined(_WIN32) 
#define PATH_SEPARATOR '\\' 
#else 
#define PATH_SEPARATOR '/' 
#endif 

/*
* Global variables
*/
libvMI_module_handle    g_vMIModule = LIBVMI_INVALID_HANDLE;
std::mutex      g_mtx;
int             g_nbFrame = 0;
int             g_nbInputs = 0;
bool            g_bExit = false;
bool            g_bInit = false;
int             g_thumbSizeW = THUMBNAIL_SIZE_WIDTH;
int             g_thumbSizeH = THUMBNAIL_SIZE_HEIGHT;
int             g_totalSizeW = THUMBNAIL_SIZE_WIDTH;
int             g_totalSizeH = THUMBNAIL_SIZE_HEIGHT;
int             g_row = 0;
int             g_col = 0;


using tChannelItem = std::tuple<  libvMI_pin_handle /*pin handle*/, int /*w*/, int /*h*/, int /*sampling_fmt*/, cv::Mat /*Im*/, long long /* time in millisecond*/, bool /*need refresh flag*/>;
std::vector< tChannelItem > g_channels;
cv::Mat                     g_mat = cv::Mat(THUMBNAIL_SIZE_HEIGHT, THUMBNAIL_SIZE_WIDTH, CV_8UC3);
cv::Mat                     m_noImgMat;
CQueue<tChannelItem>        g_q;
std::map<libvMI_pin_handle, std::string> g_pinInfoMap;
std::map<libvMI_pin_handle, std::string> g_srcInfoMap;

int         g_cvfontface = cv::FONT_HERSHEY_SIMPLEX; // cv::FONT_HERSHEY_SCRIPT_SIMPLEX;
double      g_cvscale = 0.5;
int         g_cvthickness = 1;
int         g_cvbaseline = 0;

#if CV_VERSION_MAJOR > 2
#define LINE_AA CV_AA
#endif


/**
* \brief signal handler to exit properly
*
* \param int signum trapping signal
* \return
*/
void signal_handler(int signum) {

    LOG_INFO("Got signal, exiting cleanly...");
    g_bExit = true;
    // Inject a fake item on the queue to unblock it
    g_q.push(std::make_tuple(0, THUMBNAIL_SIZE_WIDTH, THUMBNAIL_SIZE_HEIGHT, SAMPLINGFMT::RGBA, cv::Mat(THUMBNAIL_SIZE_HEIGHT, THUMBNAIL_SIZE_WIDTH, CV_8UC4), 0, true));
}

/**
*  \brief extract a substring
*
* Search for a pattern on a string, and extract the substring between
* this pattern and a "," or the end of string
*
* \param pattern the string to search
* \param str the string where to search
* \return substring
*/
std::string getParameter(const char* pattern, const std::string& str) {

    std::string ret = "";
    std::size_t found = str.find(pattern);
    if (found) {
        found += strlen(pattern);
        std::size_t found2 = str.find(",", found + 1);
        if (found2) {
            ret += str.substr(found, found2 - found);
        }
        else
            ret += str.substr(found);
    }
    return ret;
}

/**
*  \brief format pin configuration string
*
* Extract information to display from the vMI configuration 
* string (i.e. string like: "name=vMI_adapter,in_type=tcp,ip=10.228.40.49,port=6041")
*
* \param pinConfig string which contain the vMI configuration
* \return formated string, ready to display
*/
std::string getInfoFromPinConfig(const char* pinConfig) {

    std::string ret = "";
    //LOG_INFO("pin, info='%s'", pinConfig);
    std::string str = std::string(pinConfig);
    if (str.find("in_type=tcp") != std::string::npos) {
        ret = "TCP (";
        std::string ip = getParameter("ip=", str);
        std::string port = getParameter("port=", str);
        if (ip.size() > 0 && port.size() > 0)
            ret += (ip + ":" + port);
        else if (port.size() > 0)
            ret += ("port=" + port);
        ret += ")";
    }
    else if (str.find("in_type=shmem") != std::string::npos) {
        ret = "SHMEM (";
        std::string ip = getParameter("control=", str);
        ret += ip;
        ret += ")";
    }
    return ret;
}

/**
*  \brief initializing
*
* Do some stuff to prepare everything. Called when receive first vMI 
* frame from callback
*
* \param hModule handle of the vMI module
* \return
*/
void init(libvMI_module_handle hModule) {

    int g_nbInputs = libvMI_get_input_count(hModule);
    for (int i = 0; i < g_nbInputs; i++) {

        libvMI_pin_handle in = libvMI_get_input_handle(hModule, i);
        std::string sConfig = getInfoFromPinConfig(libvMI_get_input_config_stream(g_vMIModule, in));
        g_pinInfoMap.insert(std::pair<libvMI_pin_handle, std::string>(in, sConfig));

        g_srcInfoMap.insert(std::pair<libvMI_pin_handle, std::string>(in, ""));

        auto item = std::make_tuple(in, g_thumbSizeW, g_thumbSizeH, SAMPLINGFMT::RGB, m_noImgMat, 0, true);
        g_channels.push_back(item);
    }

    g_row = 0;
    g_col = 0;
    if (g_channels.size() == 1) {
        g_row = 1; g_col = 1;
    }
    else if (g_channels.size() == 2) {
        g_row = 1; g_col = 2;
    }
    else if (g_channels.size() < 5) {
        g_row = 2; g_col = 2;
    }
    else if (g_channels.size() < 7) {
        g_row = 2; g_col = 3;
    }
    else if (g_channels.size() < 10) {
        g_row = 3; g_col = 3;
    }
    else {
        LOG_ERROR("Too much input... nb=%d", g_channels.size());
        return;
    }
    LOG_INFO("Use %d row(s) and %d column(s)", g_row, g_col);
    g_bInit = true;
}

/**
*  \brief get vMI pin configuration string
*
* return configuration string from a pin identified by its handle
*
* \param pin handle of the vMI pin
* \return configuration string
*/
std::string getPinInfo(libvMI_pin_handle pin) {

    std::map<libvMI_pin_handle, std::string>::iterator it;
    it = g_pinInfoMap.find(pin);
    if (it != g_pinInfoMap.end())
        return it->second;
    return "";
}

std::string getSrcInfo(libvMI_pin_handle pin) {

    std::map<libvMI_pin_handle, std::string>::iterator it;
    it = g_srcInfoMap.find(pin);
    if (it != g_srcInfoMap.end())
        return it->second;
    return "";
}
void setSrcInfo(libvMI_pin_handle pin, char* srcInfoStr) {

    std::map<libvMI_pin_handle, std::string>::iterator it;
    it = g_srcInfoMap.find(pin);
    if (it != g_srcInfoMap.end())
        it->second = srcInfoStr;
}

/**
*  \brief convert a long long to readeable time string
*
* convert a long long int which contain millisecond since Epoch to
* a formated time string ("yyyy/mm/dd hh:mm:ss")
*
* \param str char array on which put resulted string
* \param strlen str length
* \param t time value in millisecond since Epoch
* \return
*/
void convertMSToString(char* str, int strlen, long long t) {

    std::chrono::milliseconds duration(t);
    std::chrono::time_point<std::chrono::system_clock> dt(duration);
    std::time_t tt = std::chrono::system_clock::to_time_t(dt);
    std::strftime(str, strlen, "%F %T", std::localtime(&tt));
}

/**
*  \brief display the date on a thumbnail
*
* \param im cv::Mat of the thumbnail
* \param label string that contains the date to display
* \return
*/
void setDateLabel(cv::Mat& im, const std::string label) {

    cv::Size text = cv::getTextSize(label, g_cvfontface, g_cvscale, g_cvthickness, &g_cvbaseline);
    cv::Point orig = cv::Point(1, 18);

    cv::Point pt = orig + cv::Point(0, -text.height - 1);
    cv::Size size = cv::Size(text.width, text.height + g_cvbaseline - 1);
    cv::Mat roi = im(cv::Rect(pt.x, pt.y, size.width, size.height));
    cv::Mat color(roi.size(), CV_8UC3, cv::Scalar(0, 0, 0));
    double alpha = 0.6;
    cv::addWeighted(color, alpha, roi, 1.0 - alpha, 0.0, roi);

    cv::putText(im, label, orig, g_cvfontface, g_cvscale, CV_RGB(255, 255, 255), g_cvthickness, LINE_AA);
}

/**
*  \brief display pin informations on a thumbnail
*
* \param im cv::Mat of the thumbnail
* \param label string that contains information to display
* \return
*/
void setInfoLabel(cv::Mat& im, const std::string label) {

    cv::Size text = cv::getTextSize(label, g_cvfontface, g_cvscale, g_cvthickness, &g_cvbaseline);
    cv::Point orig = cv::Point(g_thumbSizeW - text.width, g_thumbSizeH - text.height);

    cv::Point pt = orig +cv::Point(0, -text.height - 1);
    cv::Size size = cv::Size(text.width, text.height + g_cvbaseline - 1);
    cv::Mat roi = im(cv::Rect(pt.x, pt.y, size.width, size.height));
    cv::Mat color(roi.size(), CV_8UC3, cv::Scalar(0, 0, 0));
    double alpha = 0.6;
    cv::addWeighted(color, alpha, roi, 1.0 - alpha, 0.0, roi);

    cv::putText(im, label, orig, g_cvfontface, g_cvscale, CV_RGB(255, 255, 255), g_cvthickness, LINE_AA);
}

/**
*  \brief display src name on a thumbnail
*
* \param im cv::Mat of the thumbnail
* \param label string that contains information to display
* \return
*/
void setSrcLabel(cv::Mat& im, const std::string label) {

    cv::Size text = cv::getTextSize(label, g_cvfontface, g_cvscale, g_cvthickness, &g_cvbaseline);
    cv::Point orig = cv::Point(g_thumbSizeW - text.width, g_thumbSizeH - text.height -18);

    cv::Point pt = orig + cv::Point(0, -text.height - 1);
    cv::Size size = cv::Size(text.width, text.height + g_cvbaseline - 1);
    cv::Mat roi = im(cv::Rect(pt.x, pt.y, size.width, size.height));
    cv::Mat color(roi.size(), CV_8UC3, cv::Scalar(0, 0, 0));
    double alpha = 0.6;
    cv::addWeighted(color, alpha, roi, 1.0 - alpha, 0.0, roi);

    cv::putText(im, label, orig, g_cvfontface, g_cvscale, CV_RGB(255, 255, 255), g_cvthickness, LINE_AA);
}

/**
*  \brief main window update
*
* refresh the window content, called when received new vMI frames
*
* \return
*/
void update() {

    bool bNeedRefreshAll = false;
    if (g_row*g_thumbSizeH != g_totalSizeH || g_col*g_thumbSizeW != g_totalSizeW) {

        g_totalSizeW = g_col * g_thumbSizeW;
        g_totalSizeH = g_row * g_thumbSizeH;
        g_mat = cv::Mat(g_totalSizeH, g_totalSizeW, CV_8UC3);
        LOG_INFO("Create Full map Size=(%d, %d)", g_mat.cols, g_mat.rows);
        bNeedRefreshAll = true;
    }

    int xpos = 0, ypos = 0;
    for (std::vector< tChannelItem >::iterator it = g_channels.begin(); it != g_channels.end(); ++it) {

        if (bNeedRefreshAll || std::get<6>(*it)) {
            cv::Mat matRoi = g_mat(cv::Rect(xpos * g_thumbSizeW, ypos * g_thumbSizeH, g_thumbSizeW, g_thumbSizeH));
            cv::Mat src = std::get<4>(*it);
            if (src.cols == 0 || src.rows == 0) {
                LOG_ERROR("incorrect image size detected. Size=(%d, %d)", src.cols, src.rows);
                continue;
            }
            if(std::get<1>(*it)== g_thumbSizeW && std::get<2>(*it)== g_thumbSizeH)
                std::get<4>(*it).copyTo(matRoi);
            else {
                cv::Mat dst = cv::Mat(g_thumbSizeH, g_thumbSizeW, CV_8UC3);
                cv::resize(std::get<4>(*it), dst, dst.size());
                dst.copyTo(matRoi);
            }

            long long t = std::get<5>(*it);
            if (t > 0) {
                char str[100];
                convertMSToString(str, sizeof(str), std::get<5>(*it));
                setDateLabel(matRoi, str);

                std::string ss = getSrcInfo(std::get<0>(*it));
                setSrcLabel(matRoi, ss.c_str());
            }
            std::string ss = getPinInfo(std::get<0>(*it));
            setInfoLabel(matRoi, ss.c_str());
            std::get<6>(*it) = false;   // the image has been refreshed
        }
        xpos += 1;
        if (xpos >= g_col) {
            ypos += 1;
            xpos = 0;
        }
    }
}

/**
* \brief Callback used by libvMI to communicate with us

* \param user_data Pointer to some user defined value (if any). Null if not used.
* \param cmd Command type (defined on ip2vf.h)
* \param param (some values returned by libip2vf, not used for now)
* \param in handle of the pin providing cmd, LIBVMI_INVALID_HANDLE if none
* \param hFrame handle of the vMI frame provided. LIBVMI_INVALID_HANDLE if not relevant.
* \return
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
        int size = 0, media_fmt = 0, w = 0, h = 0, sampling_fmt = 0; char* name;
        libvMI_get_frame_headers(hFrame, MEDIA_PAYLOAD_SIZE, &size);
        libvMI_get_frame_headers(hFrame, MEDIA_FORMAT, &fmt);
        libvMI_get_frame_headers(hFrame, VIDEO_WIDTH, &w);
        libvMI_get_frame_headers(hFrame, VIDEO_HEIGHT, &h);
        libvMI_get_frame_headers(hFrame, VIDEO_FORMAT, &sampling_fmt);
        libvMI_get_frame_headers(hFrame, NAME_INFORMATION, &name);

        LOG("receive buffer 0x%x on input[%d], fmt=%d, size=%d bytes (%dx%d, %dbpp)", pInFrameBuffer, in, fmt, size, w, h, sampling_fmt * 8);

        if (!g_bInit)
            init(g_vMIModule);

        if (fmt == MEDIAFORMAT::VIDEO && g_bInit) {

            setSrcInfo(in, name);

            long long t1 = tools::getCurrentTimeInMilliS();
            auto item = std::make_tuple(in, w, h, sampling_fmt, cv::Mat(h, w, CV_8UC3), tools::getUTCEpochTimeInMs(), true);
            if (sampling_fmt == SAMPLINGFMT::RGB) {
                cv::Mat src = cv::Mat(h, w, CV_8UC3, pInFrameBuffer);
                cvtColor(src, std::get<4>(item), cv::COLOR_RGB2BGR);
            }
            else if (sampling_fmt == SAMPLINGFMT::BGR) {
                std::get<4>(item) = cv::Mat(h, w, CV_8UC3, pInFrameBuffer);
            }
            else if (sampling_fmt == SAMPLINGFMT::RGBA) {
                cv::Mat src = cv::Mat(h, w, CV_8UC4, pInFrameBuffer);
                cvtColor(src, std::get<4>(item), cv::COLOR_RGBA2BGR);
            }
            else if (sampling_fmt == SAMPLINGFMT::BGRA) {
                cv::Mat src = cv::Mat(h, w, CV_8UC4, pInFrameBuffer);
                cvtColor(src, std::get<4>(item), cv::COLOR_BGRA2BGR);
            }
            else {
                LOG_ERROR("unsupported format");
            }
            g_q.push(item);
            long long t4 = tools::getCurrentTimeInMilliS();
            //LOG_INFO("Process take %d ms", (int)(t4 - t1));        
        }

        // IMPORTANT... need to resume explicitely the current frame
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
* \brief main
*
* \param argc
* \param argv[]
* \return
*/
int main( int argc, char** argv ) {

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

    LOG_INFO("Uses opencv version %d.%d.%d", CV_VERSION_MAJOR, CV_VERSION_MINOR, CV_VERSION_REVISION);
    namedWindow(OPENCV_WINDOW_NAME, WINDOW_AUTOSIZE);// Create a window for display.
    LOG_INFO("main(): openCV window created");
    m_noImgMat = cv::imread(DEFAULT_IMAGE);
    if (m_noImgMat.size().width <= 0 || m_noImgMat.size().height <= 0) {
        LOG_INFO("main(): '%s' not found in current path", DEFAULT_IMAGE);
        std::string impath = tools::getEnv("RESSOURCE_PATH");
        if (impath.length() > 0) {
            if( impath.back() != PATH_SEPARATOR)
                impath += PATH_SEPARATOR;
            impath += DEFAULT_IMAGE;
            LOG_INFO("main(): try to find '%s'", impath.c_str());
            m_noImgMat = cv::imread(impath);
        }
    }
    if (m_noImgMat.size().width <= 0 || m_noImgMat.size().height <= 0) {
        // Error, no default image, create a fake one to continue
        LOG_ERROR("Can't load default image '%s'", DEFAULT_IMAGE);
        m_noImgMat = cv::Mat(THUMBNAIL_SIZE_HEIGHT, THUMBNAIL_SIZE_WIDTH, CV_8UC3);
    }

    //////////////////////////////////////////////////////////////////////////////////
    // init ip2vf stuff

    /*
    * Init the libvMI: provide a callback, and a pre-configuraion if any. If no pre-configuration here,
    * libvMI will wait for configuration provided by supervisor
    */
    LOG_INFO("main(): Registering callbacks with libvmi");
    g_vMIModule = libvMI_create_module(&libvMI_callback, (use_preconfig ? preconfig : NULL));
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

        if (g_q.size() > 0) {
            while (g_q.size() > 0) {
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
            }
            update();// std::get<4>(item);
        }

        imshow(OPENCV_WINDOW_NAME, g_mat);
        if (cv::waitKey(1) != -1) {
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

