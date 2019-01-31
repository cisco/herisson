/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <linux/if_ether.h>
#include "pacer.h"
#include <sys/mman.h>
#include <fcntl.h>
#define RX_RING_SIZE 4096
#define TX_RING_SIZE 4096

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250


#ifndef MIN_PKT_SIZE
#define MIN_PKT_SIZE 700
#endif

static const struct rte_eth_conf port_conf_default =
  { .rxmode =
    { .max_rx_pkt_len = ETHER_MAX_LEN } };

static const struct rte_eth_txconf port_txconf_default =
  { .txq_flags = 0, .tx_rs_thresh = 1, .tx_free_thresh = 1 };
static const struct rte_eth_rxconf port_rxconf_default =
  { .rx_free_thresh = 1 };

static volatile int64_t* shm_phase_shift = NULL;
/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init (uint8_t port, struct rte_mempool *mbuf_pool)
{
  struct rte_eth_conf port_conf = port_conf_default;
  const uint16_t rx_rings = 1, tx_rings = 1;
  uint16_t nb_rxd = RX_RING_SIZE;
  uint16_t nb_txd = TX_RING_SIZE;
  int retval;
  uint16_t q;

  if (port >= rte_eth_dev_count ())
    return -1;

  /* Configure the Ethernet device. */
  retval = rte_eth_dev_configure (port, rx_rings, tx_rings, &port_conf);
  if (retval != 0)
    return retval;

  retval = rte_eth_dev_adjust_nb_rx_tx_desc (port, &nb_rxd, &nb_txd);
  if (retval != 0)
    return retval;

  /* Allocate and set up 1 RX queue per Ethernet port. */
  for (q = 0; q < rx_rings; q++)
    {
      retval = rte_eth_rx_queue_setup (port, q, nb_rxd,
                                       rte_eth_dev_socket_id (port), &port_rxconf_default,
                                       mbuf_pool);
      if (retval < 0)
        return retval;
    }

  /* Allocate and set up 1 TX queue per Ethernet port. */
  for (q = 0; q < tx_rings; q++)
    {
      retval = rte_eth_tx_queue_setup (port, q, nb_txd,
                                       rte_eth_dev_socket_id (port), &port_txconf_default);
      if (retval < 0)
        return retval;
    }

  /* Start the Ethernet port. */
  retval = rte_eth_dev_start (port);
  if (retval < 0)
    return retval;

  /* Display the port MAC address. */
  struct ether_addr addr;
  rte_eth_macaddr_get (port, &addr);
  printf ("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
  " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
          (unsigned) port, addr.addr_bytes[0], addr.addr_bytes[1],
          addr.addr_bytes[2], addr.addr_bytes[3], addr.addr_bytes[4],
          addr.addr_bytes[5]);

  /* Enable RX in promiscuous mode for the Ethernet device. */
  rte_eth_promiscuous_enable (port);
  /* For i40e only: disable 802.3 PAUSE Frames filtering  */
  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(port, &dev_info);
  if (!strcmp (dev_info.driver_name, "net_i40e"))
    {
      struct rte_eth_ethertype_filter filter;
      filter.flags = RTE_ETHTYPE_FLAGS_DROP;
      filter.queue = 0;
      filter.ether_type = 0x8808;
      rte_eth_dev_filter_ctrl(port, RTE_ETH_FILTER_ETHERTYPE, RTE_ETH_FILTER_DELETE, &filter);
    }

  return 0;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static __attribute__((noreturn)) void
lcore_main_master (void)
{
  pacer_main_t * pm = &pacer_main;
  uint16_t tx_port_id = 0;
  int rv = 0;
  rv = rte_eth_dev_get_port_by_name (pm->output_name, &tx_port_id);
  if (rv < 0)
    {
      rte_exit (0, "TX Port : %s not found\n", pm->output_name);
    }
  struct rte_mempool * pool = pm->mbuf_pool;
  struct rte_ring * ring = pm->smpteRing;

  /*Create the 802.3 PAUSE frames*/
  struct rte_mbuf * pauseFrameFull = rte_pktmbuf_alloc (pool);
  char *pkt = rte_pktmbuf_append (pauseFrameFull, 1514);
  struct ethhdr *eth_hdr = (typeof(eth_hdr)) pkt;
  memset (pkt, 0, 1514);
  eth_hdr->h_proto = 0x0888;
  eth_hdr->h_dest[0] = 0x01;
  eth_hdr->h_dest[1] = 0x80;
  eth_hdr->h_dest[2] = 0xC2;
  eth_hdr->h_dest[5] = 0x01;

  /*Create the partial PAUSE frame*/
  struct rte_mbuf * pauseFramePartial = rte_pktmbuf_alloc (pool);
  pkt = rte_pktmbuf_append (pauseFramePartial, 1514);
  eth_hdr = (typeof(eth_hdr)) pkt;
  memset (pkt, 0, 1514);
  eth_hdr->h_proto = 0x0888;
  eth_hdr->h_dest[0] = 0x01;
  eth_hdr->h_dest[1] = 0x80;
  eth_hdr->h_dest[2] = 0xC2;
  eth_hdr->h_dest[5] = 0x01;

  /*Create the partial PAUSE frame 2*/
  struct rte_mbuf * pauseFramePartial2 = rte_pktmbuf_alloc (pool);
  pkt = rte_pktmbuf_append (pauseFramePartial2, 1514);
  eth_hdr = (typeof(eth_hdr)) pkt;
  memset (pkt, 0, 1514);
  eth_hdr->h_proto = 0x0888;
  eth_hdr->h_dest[0] = 0x01;
  eth_hdr->h_dest[1] = 0x80;
  eth_hdr->h_dest[2] = 0xC2;
  eth_hdr->h_dest[5] = 0x01;

  uint64_t current_time = 0; /* Current time in Ethernet bytes */
  unsigned int iterationBeforeStart = pm->avg_slid_win;
  unsigned int currentSlot = 0;
  uint64_t slidingWin[pm->avg_slid_win];
  uint64_t slidingWinTime[pm->avg_slid_win];
  uint64_t slidingWinTimeCycles[pm->avg_slid_win];
  memset (slidingWin, 0, 8 * pm->avg_slid_win);
  memset (slidingWinTime, 0, 8 * pm->avg_slid_win);
  memset (slidingWinTimeCycles, 0, 8 * pm->avg_slid_win);

  for (;;)
    {
      RTE_LOG(INFO, USER1, "Waiting to receive enough packets...\n");
      while (!pm->init_fifo_bar)
        ;
      RTE_LOG(INFO, USER1, "Resetting state after starvation...\n");
      iterationBeforeStart = pm->avg_slid_win;
      currentSlot = 0;
      memset (slidingWin, 0, 8 * pm->avg_slid_win);
      memset (slidingWinTime, 0, 8 * pm->avg_slid_win);
      memset (slidingWinTimeCycles, 0, 8 * pm->avg_slid_win);
      uint64_t state = 0;
      uint64_t stateFracNum = 0;
      unsigned int eos = 0;
      int twoPartialPauses = 0;
      const unsigned int vec_size=pm->vec_size_init;
      uint64_t quot = pm->ipg_num_init/pm->ipg_denom_init;
      uint64_t remaind = pm->ipg_num_init - quot*pm->ipg_denom_init;
      const uint64_t ipg_denom = pm->ipg_denom_init;
      const uint64_t phy_overhead = pm->phy_overhead_init;
      int64_t currentPhaseShift = 0;
      RTE_LOG(INFO, USER1, "Done.\n");
      while (!eos)
        {
          /*Build next vector to send*/
          struct rte_mbuf* vec[vec_size];
          for (unsigned int i = 0; i < vec_size; i++)
            {
              if (state == 0 )
                {
                  int rv = rte_ring_dequeue (ring, (void**) &vec[i]);
                  if (unlikely(rv < 0))
                    {
                      RTE_LOG(INFO, USER1, "End of Stream\n");
                      eos = 1;
                      break;
                    }
//#define GET_PHY(x) (  (!!(((x) + phy_overhead + 14) & 15))*16 + (((x) +phy_overhead + 14)/16)*16 - 14)
//#define GET_PHY(x) ((((x) + phy_overhead - 3)/4) * 4 + 3 )
//#define GET_UPPER_PHY(x) ((x) - phy_overhead - 4)
#define GET_PHY(x) ((x) + phy_overhead)
#define GET_UPPER_PHY(x) ((x) - phy_overhead)
                  state += quot - GET_PHY(vec[i]->data_len);
#define MIN(x,y) (x<y?x:y)
#define MAX(x,y) (x>y?x:y)
                  int64_t phaseShiftConsumable = MAX(-100ll,MIN(100ll, currentPhaseShift));
                  state -= phaseShiftConsumable;
                  currentPhaseShift -= phaseShiftConsumable;
                  stateFracNum += remaind;
                  if (stateFracNum >= ipg_denom)
                    {
                      stateFracNum -= ipg_denom;
                      state++;
                    }
                  current_time += GET_PHY(vec[i]->data_len);
                }
              else if (state >= MIN_PKT_SIZE + GET_PHY(1514))
                {
                  vec[i] = pauseFrameFull;
                  state -= GET_PHY(1514);
                  current_time += GET_PHY(1514);
                  rte_mbuf_refcnt_update (vec[i], 1);
                }
              else if (state <= GET_PHY(1514) && state >= MIN_PKT_SIZE
                  && !twoPartialPauses)
                {
                  vec[i] = pauseFramePartial;
                  pauseFramePartial->data_len = GET_UPPER_PHY((int) state);
                  pauseFramePartial->pkt_len = GET_UPPER_PHY((int) state);
                  state -= GET_PHY(pauseFramePartial->data_len);
                  rte_mbuf_refcnt_update (vec[i], 1);
                  current_time += GET_PHY(pauseFramePartial->data_len);
                }
              else if (state <= GET_PHY(1514) && state >= MIN_PKT_SIZE
                  && twoPartialPauses)
                {
                  vec[i] = pauseFramePartial2;
                  pauseFramePartial2->data_len = GET_UPPER_PHY((int) state);
                  pauseFramePartial2->pkt_len = GET_UPPER_PHY((int) state);
                  state -= GET_PHY(pauseFramePartial2->data_len);
                  twoPartialPauses = 0;
                  rte_mbuf_refcnt_update (vec[i], 1);
                  current_time += GET_PHY(pauseFramePartial2->data_len);
                }
              else if (state > GET_PHY(1514) && !twoPartialPauses)
                {
                  vec[i] = pauseFramePartial;
                  pauseFramePartial->data_len = GET_UPPER_PHY(MIN_PKT_SIZE);
                  pauseFramePartial->pkt_len = GET_UPPER_PHY(MIN_PKT_SIZE);
                  state -= GET_PHY(GET_UPPER_PHY(MIN_PKT_SIZE));
                  twoPartialPauses = 1;
                  rte_mbuf_refcnt_update (vec[i], 1);
                  current_time += GET_PHY(pauseFramePartial->data_len);
                }
              else
                {
                  rte_exit (0, "Invalid state: %lu!\n", state);
                }
            }
          if (eos)
            {
              break;
            }
          unsigned int sent = 0;
          /* Now send this vector and keep trying */
          while (sent < vec_size)
            {
              int rv = rte_eth_tx_burst (tx_port_id, 0, &vec[sent], vec_size - sent);
              sent += rv;
            }
          uint64_t oldTimeCycles = 0;
          if (current_time > pm->avg_win)
            {
              __sync_synchronize ();
              uint64_t pkt_received = __sync_fetch_and_and (&packets_received,
                                                            0);
              slidingWin[currentSlot] = pkt_received;
              slidingWinTime[currentSlot] = current_time;
              {
                uint64_t now = rte_get_tsc_cycles();
                oldTimeCycles = slidingWinTimeCycles[currentSlot];
                slidingWinTimeCycles[currentSlot] = now;

              }
              currentSlot = (currentSlot + 1) % pm->avg_slid_win;


              if (!iterationBeforeStart)
                {
                  uint64_t num_packets = 0;
                  uint64_t total_time_bytes = 0;
                  uint64_t total_time_cycles = slidingWinTimeCycles[(currentSlot + pm->avg_slid_win - 1) % pm->avg_slid_win] - oldTimeCycles;
                  for (unsigned int i = 0; i < pm->avg_slid_win; i++)
                    {
                      num_packets += slidingWin[i];
                      total_time_bytes += slidingWinTime[i];
                    }
                  double pps_bytes = ((double)(num_packets * 5ull * 1000000000ull))/((double)total_time_bytes * 4.0);
                  double pps_cycles = ( ((double)(num_packets)) * ((double)rte_get_tsc_hz()))/((double)total_time_cycles);
                  RTE_LOG(INFO, USER1, "Frequency in packets per second (bytes): %f (%lu/%lu)\n", pps_bytes, num_packets * (uint64_t)1250000000, total_time_bytes);
                  RTE_LOG(INFO, USER1, "Frequency in packets per second (cycles): %f\n", pps_cycles);
                  /*We adjust ipg_num to fit measurements*/
                  //uint64_t newNum = (uint64_t)floor(((double)ipg_denom)*(1250000000.*1.)/(double)pps_bytes);
                  uint64_t newNum = ( ((uint64_t)ipg_denom) * total_time_bytes ) /num_packets;
                  if (pm->freq_control)
                    {
                      quot = newNum/ipg_denom;
                      remaind = newNum - quot*ipg_denom;
                    }
                  /* Phase adjustement as well */
                  if (pm->phase_control)
                    {
                      currentPhaseShift = __sync_fetch_and_and(shm_phase_shift, 0LL);
                      RTE_LOG(INFO, USER1, "Chosen phase shift %li\n", currentPhaseShift);
                    }
                }
              else if (iterationBeforeStart)
                {
                  iterationBeforeStart--;
                }

              current_time = 0;
            }
        }
      RTE_LOG(INFO, USER1, "Resetting after starvation!\n");
      __sync_lock_test_and_set (&pm->init_fifo_bar, 0);
    }
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main (int argc, char *argv[])
{
  /* Initialize the pacer */
  int pacer_args = parse_pacer_args (argc, argv);
  argc -= pacer_args;

  /* Initialize the Environment Abstraction Layer (EAL). */
  int ret = rte_eal_init (argc, &argv[pacer_args]);
  if (ret < 0)
    rte_exit (EXIT_FAILURE, "Error with EAL initialization\n");

  if (pacer_main.phase_control)
    {
      printf("Enabling phase control, opening a shared memory \n");
      int fd = shm_open("pacer_phase", O_CREAT | O_RDWR | O_TRUNC, 0);
      if (ftruncate(fd, 8))
        {
          rte_exit (EXIT_FAILURE, "ftruncate call failed.\n");
        }
      shm_phase_shift = mmap(NULL, 8,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      *shm_phase_shift = 0LL;
    }
  argc -= ret;
  argv += ret;

  struct rte_mempool *mbuf_pool;
  /* Creates a new mempool in memory to hold the mbufs. */
  mbuf_pool = rte_pktmbuf_pool_create ("MBUF_POOL", 1 << 18,
  MBUF_CACHE_SIZE,
                                       0, RTE_MBUF_DEFAULT_BUF_SIZE,
                                       rte_socket_id ());

  if (mbuf_pool == NULL)
    rte_exit (EXIT_FAILURE, "Cannot create mbuf pool\n");

  pacer_main.mbuf_pool = mbuf_pool;
  int input_init = 0;
  int output_init = 0;
  int ref_init = 0;
  /* Initialize input interface */
  if (!input_init && pacer_main.input_src_name && pacer_main.input_src_name[0])
    {
      uint16_t portid = 0;
      int res = rte_eth_dev_get_port_by_name (pacer_main.input_src_name,
                                              &portid);
      if (res < 0)
        rte_exit (EXIT_FAILURE, "Cannot find port %s\n",
                  pacer_main.input_src_name);
      if (port_init (portid, mbuf_pool) != 0)
        rte_exit (EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);
      input_init = 1;
      if (pacer_main.output_name
          && !strcmp (pacer_main.input_src_name, pacer_main.output_name))
        output_init = 1;
      if (pacer_main.ref_name
          && !strcmp (pacer_main.input_src_name, pacer_main.ref_name))
        ref_init = 1;
    }

  /* Initialize output interface */
  if (!output_init && pacer_main.output_name && pacer_main.output_name[0])
    {
      uint16_t portid = 0;
      int res = rte_eth_dev_get_port_by_name (pacer_main.output_name, &portid);
      if (res < 0)
        rte_exit (EXIT_FAILURE, "Cannot find port %s\n",
                  pacer_main.output_name);
      if (port_init (portid, mbuf_pool) != 0)
        rte_exit (EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);
      output_init = 1;
      if (pacer_main.ref_name
          && !strcmp (pacer_main.output_name, pacer_main.ref_name))
        ref_init = 1;
    }

  /* Initialize ref interface */
  if (!ref_init && pacer_main.ref_name && pacer_main.ref_name[0])
    {
      uint16_t portid = 0;
      int res = rte_eth_dev_get_port_by_name (pacer_main.ref_name,
                                              &portid);
      if (res < 0)
        rte_exit(EXIT_FAILURE, "Cannot find port %s\n", pacer_main.ref_name);
      if (port_init (portid, mbuf_pool) != 0)
        rte_exit (EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);
      ref_init = 1;
    }

  if (rte_lcore_count () > 1)
    printf ("\nWARNING: Too many lcores enabled. Only 1 used.\n");

  init_pacer_packets ();
  /* Call lcore_main on the master core only. */
  lcore_main_master ();

  return 0;
}
