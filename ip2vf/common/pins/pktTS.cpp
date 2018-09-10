#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <pins/pins.h>
#include "common.h"
#include "log.h"
#include "tcp_basic.h"

using namespace std;

#include "pktTS.h"

#define DBG_INFO        if(_dbg & 1)LOG_INFO

int                     g_isProbe;
CQueue<pktTSaggr>       g_pktTSq;       /* change to ? */

#ifdef _WIN32

static int
pktTSenable(SOCKET sock, const char* ifname, int mode)
{
    return 0;
}

int
PktTS::readSocket(char* buffer, int* len)
{
    int n;
    Time timestamp;

    if(!UDP::isValid())
        return -1;

    if((n = UDP::readSocket(buffer, len)) <= 0)
        return n;

    timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>
        (std::chrono::high_resolution_clock::now().time_since_epoch()).count();
       pktTSctl(2, 0, timestamp);

    return n;
}

#else /* _WIN32 */

#include <sys/socket.h>                 /* for setsockopt(...) */
#include <linux/net_tstamp.h>           /* for SOF_*, hwtstamp_config... */

static int
pktTSenable(SOCKET sock, const char* ifname, int mode)
{
    int f, r;

    /*
     * For now, just software timestamping;
     * many 10/40Gb NICs don't do hardware timestamping anyway :-(.
     * Still thinking through the configuration interface:
     * (May subvert some bits for debugging);
     * 0 and default is CLOCK_REALTIME, 1 is SW, 2 is HW;
     * -1 can't happen (above).
     */
    f = 0;
    switch(mode & 0x0f){
    default:
        return 0;
    case 2:
        /*FALLTHROUGH*/
    case 1:
        f |= SOF_TIMESTAMPING_SOFTWARE|SOF_TIMESTAMPING_RX_SOFTWARE;
        break;
    }

    if((r = setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &f, sizeof(f))) < 0){
        LOG_ERROR("mode: %#x: '%s'", mode, strerror(errno));

        return r;
    }

    /*
     * More to be done here...
     */

    return r;
}

static Time
pktTScmsg(struct msghdr* msg)
{
    Time t;
    struct cmsghdr *cmsg;
    struct timespec tp, *ts;

    for(cmsg = CMSG_FIRSTHDR(msg); cmsg != 0; cmsg = CMSG_NXTHDR(msg, cmsg)){
        switch(cmsg->cmsg_level){
        default:
            /* unknown, ignore for now */
            break;

        case SOL_SOCKET:
            switch(cmsg->cmsg_type){
            default:
                /* ditto: unknown, ignore for now */
                break;

            case SO_TIMESTAMPING:
                ts = (struct timespec*)CMSG_DATA(cmsg);
                /*
                 * This is an array of 3 timestamps:
                 *    0 is SW, 1 is legacy (unused), 2 is HW
                 * Return the HW or SW timestamp if non zero,
                 * preferring the HW timestamp.
                 */
                if((t = 1000000000ll*ts[2].tv_sec + ts[2].tv_nsec) != 0
                || (t = 1000000000ll*ts[0].tv_sec + ts[0].tv_nsec) != 0)
                    return t;
                break;
            }
        }
    }

    /*
     * If no CMSG or no timestamp found in the CMSG,
     * use the best system clock we have.
     * This really shouldn't fail, what to do if it does?
     * Return 0 for now.
     */
    if(clock_gettime(CLOCK_REALTIME, &tp) < 0)
        return 0;

    return 1000000000ll*tp.tv_sec + tp.tv_nsec;
}

int
PktTS::readSocket(char* buffer, int* len)
{
    int n;
    struct msghdr msg;
    struct iovec iov[1];
    Time timestamp;
    unsigned char control[128];
    struct sockaddr_storage remote;

    /*
     */
    memset(&msg, 0, sizeof(msg));

    msg.msg_name = &remote;
    msg.msg_namelen = sizeof(struct sockaddr_storage);

    iov[0].iov_base = buffer;
    iov[0].iov_len = *len;

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    switch(n = recvmsg(UDP::getSock(), &msg, 0)){
    default:
        timestamp = pktTScmsg(&msg);
        pktTSctl(2, 0, timestamp);
        break;
    case -1:
        LOG_ERROR("recvmsg: '%s'", strerror(errno));
        break;
    case 0:
        LOG_INFO("connection gracefully closed");
        break;
    }
    *len = n;

    return n;
}
#endif /* _WIN32 */

void
PktTS::pktTSctl(int ctl, unsigned int identifier, Time timestamp)
{
    Time ipg;
    double delta, delta2;

    switch(_state){
    default:
        break;
    case 0:                                                     /* idling */
        switch(ctl){
        default:
            break;
        case 0:                                                 /* init */
            if(_aggr == 0)
                _aggr = new pktTSaggr();

            memset(_aggr, 0, sizeof(*_aggr));
            _aggr->handle = _mHandle;
            _aggr->pinId = _pinId;
            _aggr->min = (std::numeric_limits<int>::max)();

            return;
        case 1:                                                 /* start */
            _aggr->identifier[0] = identifier;
            _aggr->timestamp[1] = _aggr->timestamp[0];
            _aggr->count = 0;

            _state = 1;

            return;
        case 2:                                                 /* idle */
            /* save timestamp */
            _aggr->timestamp[0] = timestamp;
            _aggr->count++;                                     /* debugging? */
            return;
        }
        break;

    case 1:                                                     /* collecting */
        switch(ctl){
        default:
            break;
        case 1:                                                 /* incomplete */
            /*FALLTHROUGH*/
        case 0:                                                 /* stop */
            /* prep _aggr and enqueue */
            _aggr->identifier[1] = identifier;
            DBG_INFO("aggr[%d][%d]: %u %u: %lld %lld: "
                "count %d, %d < mean %.1f < %d, m2 %.1f",
                    _aggr->handle, _aggr->pinId,
                    _aggr->identifier[0], _aggr->identifier[1],
                    _aggr->timestamp[0], _aggr->timestamp[1],
                    _aggr->count, _aggr->min, _aggr->mean,
                    _aggr->max, _aggr->m2);

            if(_aggr->count >= 2)
                g_pktTSq.push(*_aggr);

            _state = 0;
            pktTSctl(0, 0, 0);

            return;
        case 2:                                                 /* collect */
            ipg = timestamp - _aggr->timestamp[1];
            _aggr->timestamp[1] = timestamp;

            _aggr->count++;
            delta = ipg - _aggr->mean;
            _aggr->mean = _aggr->mean + delta/_aggr->count;
            delta2 = ipg - _aggr->mean;
            _aggr->m2 = _aggr->m2 + delta*delta2;

            if(ipg < _aggr->min)
                _aggr->min = ipg;
            if(ipg > _aggr->max)
                _aggr->max = ipg;

            return;
        }
        break;
    }

    /* all defaults (error) get you here */
    LOG_INFO("Oops: _state %d, ctl %d, identifier %u, timestamp %lld",
        _state, ctl, identifier, timestamp);

    _state = 0;
    pktTSctl(0, 0, 0);
}

int
PktTS::openSocket(const char* remote, const char* local,
        int port, bool listen, const char* ifname)
{
    int r;
    SOCKET sock;

    LOG_INFO("openSocket -->");
    if((r = UDP::openSocket(remote, local, port, listen, ifname)) != E_OK)
        return r;
    sock = UDP::getSock();

    /*
     * Initialise timestamp collection counters.
     */
    if(pktTSenable(sock, ifname, _pktTS) < 0){
        UDP::closeSocket();

        return VMI_E_FAILED_TO_OPEN_SOCKET;
    }
    _state = 0;
    pktTSctl(0, 0, 0);

    LOG_INFO("openSocket <--");

    return E_OK;
}

static int
pktOption(PinConfiguration* pConfig, const char* option)
{
    struct oOpt {
        int _option;
        PinConfiguration* _pConfig;
        PinConfiguration* getConfiguration(void){
            return _pConfig;
        }
        oOpt(PinConfiguration* pConfig, const char* option): _pConfig(pConfig){
            PROPERTY_REGISTER_OPTIONAL(option, _option, -1);
        }
    } oOpt(pConfig, option);

    return oOpt._option;
}

PktTS::PktTS(PinConfiguration* pConfig)
{
    int port;

    port = pktOption(pConfig, "port");

    _pConfig = pConfig;
    PROPERTY_REGISTER_OPTIONAL("pktTS", _pktTS, -1);
    if(port == -1 || _pktTS == -1){
        LOG_INFO("Invalid configuration: _pktTS: %d: port '%d'", _pktTS, port);
        _pConfig = 0;

        return;
    }

    PROPERTY_REGISTER_OPTIONAL("mHandle", _mHandle, -1);
    PROPERTY_REGISTER_OPTIONAL("pinId", _pinId, -1);
    PROPERTY_REGISTER_OPTIONAL("dbg", _dbg, 0);

    DBG_INFO("_pktTS: %d: _mHandle %d, _pinId %d, _port %d, _dbg %#x",
         _pktTS, _mHandle, _pinId, port, _dbg);

    _aggr = 0;
}

PktTS::~PktTS()
{
    DBG_INFO("_aggr %#p", _aggr);
    if(_aggr != 0)
        delete _aggr;
}

PktTS*
pktTSconstruct(PinConfiguration* pConfig)
{
    PktTS* pktTS;

    if(g_isProbe == 0)
        return 0;

    pktTS = new PktTS(pConfig);
    if(pktTS->getConfiguration() == 0){
        delete pktTS;
        pktTS = 0;
    }

    return pktTS;
}
