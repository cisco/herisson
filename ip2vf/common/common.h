#ifndef _COMMMON_H
#define _COMMMON_H

/*
* By default, include list of errors managed by vMI
*/
#include "error.h"

/*
 * Some common declaration
 */

/*
 * \brief versionning
 */
#define VERSION_MAJOR 2
#define VERSION_MINOR 3
#define VERSION_PATCH 1
#define VERSION_TAG   "DPDK Support"

/*
 * \brief Pin enumeration
 */
enum PinType { 
    PIN_TYPE_NONE        = -1,  // No type
    PIN_TYPE_TCP         = 1,   // (in/out) Pin allowing to receive/send video frames by IP (connected socket)
    PIN_TYPE_SHMEM       = 2,   // (in/out) Pin allowing to receive/send video frames using shared memory (1 segment)
    PIN_TYPE_FILE        = 3,   // (in)     Pin allowing to read video frames from a file
    PIN_TYPE_DEVNULL     = 4,   // (out)    Pin allowing to do nothing with video data
    PIN_TYPE_RTP         = 5,   // (in/out) Pin allowing to receive/send video frames using RTP (UDP socket)
    PIN_TYPE_SMPTE       = 7,   // (in/out) Pin allowing to receive/send SMPTE stream (on top of RTP)
    PIN_TYPE_TCP_THUMB   = 8,   // (out)    Pin allowing to send thumbnail of video (monitoring). To use only for 8bits stream.
    PIN_TYPE_RAWRTP      = 9,
    PIN_TYPE_RAWX264     = 10,  // (out)    Pin allowing to broadcast x264 stream
    PIN_TYPE_TR03        = 11,  // (in/out) Pin allowing to receive/send TR03 stream (on top of RTP)
    PIN_TYPE_AES67       = 12,  // (in)     Pin allowing to receive AES67
    PIN_TYPE_STORAGE     = 13,  // (out)
    PIN_TYPE_MAX
};

/*
* \brief Macro to 
*/
#define STREAMING_TYPE(a)   ( a == PIN_TYPE_TCP        || \
                              a == PIN_TYPE_FILE       || \
                              a == PIN_TYPE_RTP        || \
                              a == PIN_TYPE_RAWRTP     || \
                              a == PIN_TYPE_RAWX264    || \
                              a == PIN_TYPE_SMPTE      || \
                              a == PIN_TYPE_TR03       || \
                              a == PIN_TYPE_AES67       || \
                              a == PIN_TYPE_STORAGE    || \
                              a == PIN_TYPE_TCP_THUMB   )

#define MEMORY_TYPE(a)      ( a == PIN_TYPE_SHMEM      )

/*
* \brief Transport type enumeration
*/
enum TransportType {
    TRANSPORT_TYPE_NONE = 0,
    TRANSPORT_TYPE_STREAMING = 1,
    TRANSPORT_TYPE_MEMORY = 2,
    TRANSPORT_TYPE_MIXED = 3,
};

enum State {
    STATE_NOTINIT   = 0,
    STATE_STOPPED   = 1,
    STATE_STARTED   = 2
};

enum INTERLACED_MODE {
    NOT_DEFINED = -1,
    NO_INTERLACED = 0,
    INTERLACED = 1
};

/*
* \brief Prefix for external commands
*/
#define MSG_PREFIX_INIT     "init:"
#define MSG_PREFIX_START    "start:"
#define MSG_PREFIX_STOP     "stop:"
#define MSG_PREFIX_QUIT     "quit:"


/*
* \brief Manage message for notification mechanism between shmem pins
*/
#define VMI_MEM_MSG_LEN     64
#define VMI_MEM_MSG_FORMAT  "GO:key=%d:size=%d:offset=%d:sid=%lld"     /* Format = "GO:key=%SHMEM_KEY%:size=%MEM_SIZE%:offset=%MEM_OFFSET%:sid=%SESSION_ID%" */


/*
 *
 */

#ifdef _WIN32
#define SNPRINTF                _snprintf
#define STRNCPY(dst, src, len)  strncpy_s(dst, len, src, len)
#define STRDUP                  _strdup
#define STRCAT(a, b)            strcat_s(a, sizeof(a), b)      
#define GETPID                  _getpid()
#define GETTID          0	//	pthread_self()
#else
typedef void       *HANDLE; 
#define SNPRINTF    snprintf
#define STRNCPY     strncpy
#define STRDUP      strdup
#define STRCAT      strcat
#define GETPID      getpid()
#define GETTID      syscall(SYS_gettid)
#endif

#ifndef MIN
#define MIN(a, b)       (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)       (((a) > (b)) ? (a) : (b))
#endif
#ifndef ISEVEN
#define ISEVEN(x)   ((x & 1) == 0)
#endif
#ifndef ISODD
#define ISODD(x)    ((x & 1) != 0)
#endif

#define MAX_UNSIGNED_INT32    4294967295

#define RTP_PACKET_SIZE     1400    // bytes

#define MSG_MAX_LEN     1024

//Still need to integrate Cisco x264 codec:
#define DO_NOT_COMPILE_X264

// Some constants used when try to use a pcap file as a source
#define PCAP_FILE_HEADER_SIZE       24
#define PCAP_PACKET_OFFSET          58


#endif //_COMMMON_H
