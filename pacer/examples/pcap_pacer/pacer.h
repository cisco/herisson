/*
 * pacer.h
 *
 *  Created on: Aug 1, 2017
 *      Author: mhawari
 */

#ifndef EXAMPLES_PCAP_PACER_PACER_H_
#define EXAMPLES_PCAP_PACER_PACER_H_
#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_ring.h>

#ifndef AVG_WIN
#define AVG_WIN 1250000000
#endif

#ifndef AVG_SLID_WIN
#define AVG_SLID_WIN 10
#endif

extern volatile uint64_t packets_received;
/*List of parameters
 * | ctype | name | size | label | type | flag or param | defaultValue*/
#define PACER_ARGS_LIST \
  _(uint8_t, macAddr, [6], "-mac", MAC_ADDR, PARAM, {0} ) \
  _(uint8_t, ipAddr, [4], "-ip", IP_ADDR, PARAM, {0} ) \
  _(uint8_t, sipAddr, [4], "-sip", IP_ADDR, PARAM, {0} ) \
  _(uint8_t, ip_inputAddr, [4], "-ip_input", IP_ADDR, PARAM, {0}) \
  _(uint8_t, ip_refAddr, [4], "-ip_ref", IP_ADDR, PARAM, {0}) \
  _(unsigned int, phy_overhead_init, ,"-phy_overhead", NUMERIC, PARAM, 24) \
  _(uint64_t, ipg_num_init, ,"-ipg_num", LNUMERIC, PARAM, 105293230) \
  _(uint64_t, ipg_denom_init, ,"-ipg_denom", LNUMERIC, PARAM, 13491) \
  _(unsigned int, vec_size_init, ,"-vec_size", NUMERIC, PARAM, 128) \
  _(uint64_t, avg_win, ,"-avg_win", LNUMERIC, PARAM, AVG_WIN) \
  _(uint64_t, avg_slid_win, ,"-avg_slid_win", LNUMERIC, PARAM, AVG_SLID_WIN) \
  _(unsigned char, freq_control, ,"-freq_control", BOOLEAN, FLAG, 0) \
  _(unsigned char, phase_control, ,"-phase_control", BOOLEAN, FLAG, 0) \
  _(const char *, output_name, , "-output", STRING, PARAM, "") \
  _(const char *, input_src_name, , "-f", STRING, PARAM, "") \
  _(uint16_t, port_num, , "-port_num", NUMERIC, PARAM, 0x2710) \
  _(const char*, ref_name, , "-ref", STRING, PARAM, "") \
  _(uint16_t, ref_port, ,"-ref_port", NUMERIC, PARAM, 0) \

typedef struct
{
  uint8_t *pcap_data;
  volatile uint8_t init_fifo_bar;
  uint64_t pcap_size;
  struct rte_mempool *mbuf_pool;
  struct rte_ring *smpteRing;
#define _(t,n,s,p,x,f,d) \
  t n s;
  PACER_ARGS_LIST
#undef _

} pacer_main_t;

typedef struct pcap_hdr_s
{
  uint32_t magic_number; /* magic number */
  uint16_t version_major; /* major version number */
  uint16_t version_minor; /* minor version number */
  int32_t thiszone; /* GMT to local correction */
  uint32_t sigfigs; /* accuracy of timestamps */
  uint32_t snaplen; /* max length of captured packets, in octets */
  uint32_t network; /* data link type */
} pcap_hdr_t;

typedef struct pcaprec_hdr_s
{
  uint32_t ts_sec; /* timestamp seconds */
  uint32_t ts_usec; /* timestamp microseconds */
  uint32_t incl_len; /* number of octets of packet saved in file */
  uint32_t orig_len; /* actual length of packet */
} pcaprec_hdr_t;

extern pacer_main_t pacer_main;

int
parse_pacer_args (int argc, char* argv[]);

int
init_pacer_packets (void);

#endif /* EXAMPLES_PCAP_PACER_PACER_H_ */
