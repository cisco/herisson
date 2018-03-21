#ifndef _ERROR_H
#define _ERROR_H

#include <limits.h>

enum _ERRORCODE {

    VMI_E_OK = 0,

    VMI_E_ERROR = INT_MIN,        // DO NOT CHANGE THAT, and do not insert error before that, as it allow to garantize
                                // negative values for followings errors

    // Generic errors
    VMI_E_INVALID_PARAMETER,        // In case of bad/unexpected parameter
    VMI_E_BAD_INIT,                 // In case of inialisation error
    
    // Errors relatives to memory management
    VMI_E_MEM_FAILED_TO_ALLOC,

    // Errors relatives to frame headers
    VMI_E_INVALID_HEADERS,
    VMI_E_INVALID_HEADER_VERSION,
    VMI_E_INVALID_FRAME,

    // Errors relative to network
    VMI_E_FAILED_TO_OPEN_SOCKET,
    VMI_E_FAILED_TO_RCV_SOCKET,
    VMI_E_FAILED_TO_SND_SOCKET,
    VMI_E_CONNECTION_CLOSED,

    // Errors relative to datasource
    VMI_E_NOT_PRIMARY_SRC,
    VMI_E_PACKET_LOST,

};

#endif  // _ERROR_H