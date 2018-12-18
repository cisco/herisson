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
#define _WINSOCKAPI_
#include <Windows.h>
#define sleep_ms(x) Sleep(x)   // Sleep is in milliseconds
#endif

#include "log.h"

using namespace std;

#include "vMI_module.h"
#include "libvMI_int.h"
#include <cmath>

#include "queue.h"
#include "pins/pktTS.h"


/*
 * Global variables
 */
libvMI_module_handle     g_vMIModule = LIBVMI_INVALID_HANDLE;
std::condition_variable  g_var;
std::mutex               g_mtx;
int                      g_nbFrame = 0;
char                     g_userData[] = "module vMIProbe";

int                      g_isProbe;
CQueue<pktTSaggr>        g_pktTSq;

/*
 */
static MetricsCollector* metrics;

static bool Dflg;
static bool Mflg;
static bool Vflg;

#define DBG_INFO		if(Dflg)LOG_INFO

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
    int size, fmt;
    double stats[4];
    unsigned int mediatimestamp;
    static unsigned int lastmediatimestamp;
	std::chrono::high_resolution_clock::time_point ts;

    DBG_INFO("receive msg '%d' for in=%d, with user data=0x%x", cmd, in, user_data);
    switch (cmd) {
    default:
        LOG("unknown cmd %d", cmd);
    case CMD_INIT:
        LOG_INFO("receive CMD_INIT");
        break;
    case CMD_START:
        LOG_INFO("receive CMD_START");
        if((metrics = static_cast<MetricsCollector*>(libvMI_get_metrics_handle(g_vMIModule))) != 0)
            metrics->pktTSmetricsinit(Mflg);
        break;
    case CMD_TICK:
        /*
         * A new frame is available.
         */
        libvMI_get_frame_headers(hFrame, MEDIA_PAYLOAD_SIZE, &size);
        libvMI_get_frame_headers(hFrame, MEDIA_FORMAT, &fmt);
        libvMI_get_frame_headers(hFrame, MEDIA_TIMESTAMP, &mediatimestamp);

        if(fmt == VIDEO){
            DBG_INFO("V frame# %5d TS %10u on input[%2d], size=%d bytes",
                hFrame, mediatimestamp, in, size);
        }
        else if(fmt == AUDIO){
            DBG_INFO("A frame# %5d TS %10u on input[%2d], size=%d bytes",
                hFrame, mediatimestamp, in, size);
        }
		else{
            LOG_INFO("receive frame %d on input[%d], fmt=%s(%d), size=%d bytes",
                hFrame, in, (fmt == MEDIAFORMAT::VIDEO? "video": "audio"),
                fmt, size);
        }
        /*
        Insert code here...
         */
        if(mediatimestamp != lastmediatimestamp){
            lastmediatimestamp = mediatimestamp;
            g_nbFrame++;
        }

        /*
         * Probably the least efficient datastructure for the job.
         *
         * Variance is aggr.m2/(aggr.count-1),
         * standard deviation is sqrt(variance).
         */
        ts = std::chrono::high_resolution_clock::now();
        while(g_pktTSq.size() > 0){
            auto aggr = g_pktTSq.pop();

            stats[0] = aggr.mean;
            if(Vflg)
                stats[1] = aggr.m2/(aggr.count-1);
            else
                stats[1] = sqrt(aggr.m2/(aggr.count-1));
            stats[2] = aggr.min;
            stats[3] = aggr.max;

            DBG_INFO("aggr[%d][%d]: %u %u: %lld %lld: "
                "count %d, %d < mean %.1f < %d, stddev %.1f, m2 %.1f",
                     aggr.handle,  aggr.pinId,
                     aggr.identifier[0],  aggr.identifier[1],
                     aggr.timestamp[0],  aggr.timestamp[1],
                     aggr.count,  aggr.min,  aggr.mean,
                     aggr.max, stats[1], aggr.m2);

            if(metrics == 0)
                continue;

            metrics->pktTSmetrics(ts, "videopacketgap", aggr.pinId, stats, 4);

            using time_point = std::chrono::high_resolution_clock::time_point;
            time_point tp{
                std::chrono::duration_cast<time_point::duration>
                (std::chrono::nanoseconds(aggr.timestamp[0]))
            };
            stats[0] = aggr.identifier[1];
            metrics->pktTSmetrics(tp, "videotimestamp", aggr.pinId, stats, 1);
        }

        /*
         * Always release the current frame
         */
        libvmi_frame_release(hFrame);
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
    char *preconfig = NULL;

	g_isProbe = 1;

    bool debug = false;
    int  debug_time_in_sec = 2;

    // Check parameters
    if (argc >= 2) {
        for (int i = 0; i < argc; i++) {
            if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
                port = atoi(argv[i + 1]);
            } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
                preconfig = argv[i + 1];
            } else if (strcmp(argv[i], "-v") == 0) {
                tools::displayVersion();
                return 0;
            } else if (strcmp(argv[i], "-d") == 0) {
                debug = true;
                if(i + 1 < argc)
                    debug_time_in_sec = atoi(argv[i + 1]);
            } else if (strcmp(argv[i], "-D") == 0) {
                Dflg = true;
            } else if (strcmp(argv[i], "-M") == 0) {
                Mflg = true;
            } else if (strcmp(argv[i], "-V") == 0) {
                Vflg = true;
            }
            else if (strcmp(argv[i], "-h") == 0) {
                std::cout << "usage: " << argv[0] << " [-h] [-v] [-p <port>] [-c <config>] [-D] [-V]\n";
                std::cout << "         -h \n";
                std::cout << "         -v \n";
                std::cout << "         -p <controler_listen_port> \n";
                std::cout << "         -c <config> \n";
                std::cout << "         -D\n";
                std::cout << "         -V\n";
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
    g_vMIModule = libvMI_create_module_ext(&libvMI_callback, preconfig, (const void*) g_userData);
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
