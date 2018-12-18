#ifndef _LIBEGEL_H_
#define _LIBEGEL_H_

#ifdef _WIN32

typedef uint32_t in_addr_t;
#define uint   unsigned int

#pragma once

#ifdef EGELLIBRARY_EXPORTS
#define EGELLIBRARY_API __declspec(dllexport) 
#else
#define EGELLIBRARY_API __declspec(dllimport) 
#endif

#else // _WIN32

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define EGELLIBRARY_API 

#endif // _WIN32

typedef int eg_slot_handle; ///< Opaque identifier for a libegel slot

typedef int eg_error; 

typedef void* eg_pkt_desc; ///< Opaque identifier for a retrieved packet

struct eg_config {
  const char *eal_config_str; ///< Optional string to pass init parameters to DPDK (DPDK standard)
};

struct eg_slot_config {
  const char* iface_pci_addr; ///< PCI Address of the receiving interface for the slot
  in_addr_t iface_ip_addr; ///< IP address for the network interface to use
  in_addr_t mcast_group; ///< Multicast group of the received stream for the slot
  uint16_t udp_port; ///< UDP Port (network byte-order) of the received stream for the slot
};

/*
 * Initializes the libegel environment, in particular the DPDK library.
 * 
 * @param config
 *   The configuration parameters of libegel
 * @return
 *   - 0: Success
 *   - -1: Failure
 */
EGELLIBRARY_API eg_error libegel_init(struct eg_config *config);
EGELLIBRARY_API eg_error libegel_rqst(const char*, void*, uint*);

/*
 * Allocates a new slot in libegel for packet reception
 *
 * @return
 *   - an eg_slot_handle with a positive value in case of success.
 *   - -1: Failure in slot allocation
 */
EGELLIBRARY_API eg_slot_handle libegel_new_slot (void);

/*
 * Configures a previously allocated slot. 
 *
 * @param sh
 *   The handle of the slot to configure.
 * @param config
 *   The configuration parameters of the slot.
 * @return
 *   - 0: Success
 *   - -1: Failure
 */
EGELLIBRARY_API eg_slot_handle libegel_config_slot (struct eg_slot_config *config);

/*
 * deletes a slot. 
 *
 * @param sh
 *   the handle of the slot to delete
 * @return
 *   - 0: success
 *   - -1: failure
 */
EGELLIBRARY_API eg_error libegel_delete_slot (eg_slot_handle sh);

/*
 * Receives multiple packets from a slot 
 *
 * @param sh
 *   the handle of the slot from which packets need to be received
 * @param pkts
 *   a destination array for the received packet descriptors
 * @param len
 *   size of pkts and maximum number of packet descriptors the function can store in \p pkts
 * @return
 *   Number of actually received packets
 */
EGELLIBRARY_API uint32_t libegel_rx_pkts_burst (eg_slot_handle sh, eg_pkt_desc *pkts, uint32_t len);

/*
 * Frees a previously received packet descriptor
 *
 * @param sh
 *   the handle of the slot from which the packet was received
 * @param pkt
 *   the packet descriptor to free
 * @return
 *   - 0: success
 *   - -1: failure
 */
EGELLIBRARY_API eg_error libegel_free_pkt(eg_slot_handle sh, eg_pkt_desc pkt);

/*
 * Retrieves the UDP payload of a packet descriptor
 *
 * @param sh
 *   the handle of the slot from which the packet was received
 * @param pkt
 *   the packet descriptor from which UDP data is to be retrived
 * @return
 *   - NULL: in case of failure
 *   - a pointer to the beginning of the UDP payload otherwise
 */
EGELLIBRARY_API uint8_t* libegel_get_udp_payload(eg_slot_handle sh, eg_pkt_desc pkt);

/*
 * Retrieves the UDP payload size of a packet descriptor
 *
 * @param sh
 *   the handle of the slot from which the packet was received
 * @param pkt
 *   the packet descriptor from which UDP data is to be retrived
 * @return
 *   - -1: in case of failure
 *   - UDP payload size otherwise.
 */
EGELLIBRARY_API int32_t libegel_get_udp_payload_len(eg_slot_handle sh, eg_pkt_desc pkt);

#endif // _LIBEGEL_H_
