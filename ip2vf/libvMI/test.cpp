#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp

#include "log.h"
#include "libvMI.h"

#include <signal.h>
#include <thread>
#include <mutex>
#include <condition_variable>

libvMI_module_handle g_vMIModule = LIBVMI_INVALID_HANDLE;
std::condition_variable  g_var;
std::mutex               g_mtx;
char* g_config =    "id=0,name=module_test,loglevel=1,"
                    //"in_type=smptefile,filename=../../videos/dump.yuv,w=1920,h=1080,fmt=0,fps=25,vidfrmsize=5184000"
                    "in_type=shmem,shmkey=5555,zmqport=5555,vidfrmsize=5184000,"
                    "in_type=shmem,shmkey=5557,zmqport=5557,vidfrmsize=5184000,"
                    "out_type=shmem,shmkey=5558,zmqip=localhost,zmqport=5558,vidfrmsize=5184000";

void signal_handler(int signum) {

    LOG_INFO("Got signal, exiting cleanly...");
    std::unique_lock<std::mutex> lock(g_mtx);
    g_var.notify_all();
    lock.unlock();
}

/*
* Description: Callback used by libvMI to communicate with us
* @method libip2vf_callback
* @param CmdType cmd (defined on ip2vf.h)
* @param int param (some values returned by libip2vf, not used for now)
* @return
*/
void libvMI_callback(const void* user_data, CmdType cmd, int param, libvMI_input_handle in)
{
    LOG("libvMI_callback(): receive %d msg", cmd);
    switch(cmd) {
    case CMD_INIT:
        LOG_INFO("libvMI_callback(): receive CMD_INIT msg");
        break;
    case CMD_START:
        LOG_INFO("libvMI_callback(): receive CMD_START msg");
        break;
    case CMD_TICK:
        libvMI_send(g_vMIModule);
        break;
    case CMD_STOP:
        LOG_INFO("libvMI_callback(): receive CMD_STOP msg");
        break;
    case CMD_QUIT:
    {
        LOG_INFO("libvMI_callback(): receive CMD_QUIT msg");
        std::unique_lock<std::mutex> lock(g_mtx);
        g_var.notify_all();
        lock.unlock();
        break;
    }
    default:
        LOG("libvMI_callback(): unknown cmd %d ", cmd);
    }
}

int main(int argc, char* argv[])
{
    int port = 5010;

    setLogLevel((LogLevel)1);

    LOG("-->");

    // set signal handler    
    if (signal(SIGINT, signal_handler) == SIG_ERR)
        LOG_ERROR("can't catch SIGINT");
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
        LOG_ERROR("can't catch SIGTERM");

    std::unique_lock<std::mutex> lock(g_mtx);
    g_vMIModule = libvMI_init(port, &libvMI_callback, g_config);
    g_var.wait(lock);
    LOG_INFO("close-->");
    //libvMI_close(g_vMIModule);
    LOG_INFO("close<--");
    lock.unlock();

    LOG("<--");
    return 0;
}
