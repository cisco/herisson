/*
 * pacer.c
 *
 *  Created on: Aug 1, 2017
 *      Author: mhawari
 */

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <rte_debug.h>
#include <errno.h>
#include <unistd.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_ring.h>
#include <rte_lcore.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <linux/if_ether.h>
#include <netinet/udp.h>
#include "pacer.h"

#ifndef FIFO_INIT_SIZE
#define FIFO_INIT_SIZE 5000
#endif

pacer_main_t pacer_main =
  {
#define _(t,n,s,p,x,f,d) \
  .n = d, \

      PACER_ARGS_LIST
#undef _
  };
volatile uint64_t packets_received = 0;
static int externalReferenceInterface = 0;
static int externalReferenceIPPortOnly = 0;
static void
pacer_print_usage (void)
{

}
int
parse_pacer_args (int argc, char* argv[])
{
  int nargs = 0;
  int firstArg = 1;
#define VAR_PARAM(n) int is##n = 0;
#define VAR_FLAG(n)
#define _(t,n,s,p,x,f,d) \
  VAR_##f(n)
  PACER_ARGS_LIST
#undef _

  do
    {
      if (firstArg)
        {
          firstArg = 0;
        }
      /*BEGIN: Macro magic for argument parsing*/
#define BLOCK_NUMERIC(t,n,s,p,x,f,d) \
  pacer_main.n = atoi(argv[nargs]); \

#define BLOCK_LNUMERIC(t,n,s,p,x,f,d) \
  pacer_main.n = atol(argv[nargs]); \

#define BLOCK_STRING(t,n,s,p,x,f,d) \
  pacer_main.n = argv[nargs];\

#define BLOCK_MAC_ADDR(t,n,s,p,x,f,d) \
    sscanf (argv[nargs], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", \
                        &pacer_main.n [0], &pacer_main.n [1], \
                        &pacer_main.n [2], &pacer_main.n [3], \
                        &pacer_main.n [4], &pacer_main.n [5]); \

#define BLOCK_IP_ADDR(t,n,s,p,x,f,d) \
    inet_pton (AF_INET, argv[nargs], pacer_main.n); \

#define BLOCK_1_PARAM(t,n,s,p,x,f,d) \
  is##n = 1; \

#define BLOCK_1_FLAG(t, n, s, p, x, f, d) \
  pacer_main.n = 1; \

#define BLOCK_2_PARAM(t,n,s,p,x,f,d) \
      else if (is##n) { \
          is##n = 0; \
          BLOCK_##x(t,n,s,p,x,f,d) \
      } \

#define BLOCK_2_FLAG(t,n,s,p,x,f,d)

#define _(t,n,s,p,x,f,d) \
      else if (!strcmp(argv[nargs], p)) \
      { \
          BLOCK_1_##f(t,n,s,p,x,f,d) \
      } \
      BLOCK_2_##f(t,n,s,p,x,f,d)

      PACER_ARGS_LIST
#undef _
      /*END: Macro Magic for argument parsing*/
      else
        {
          pacer_print_usage ();
          rte_exit (0, "Pcap pacer exited because of wrong usage\n");
        }
      nargs++;
    }
  while (nargs < argc && strncmp (argv[nargs], "--", 3));
  if (nargs == argc)
    {
      pacer_print_usage ();
      rte_exit (0, "Pcap pacer exited because of wrong usage\n");
    }
  pacer_main.port_num = htons (pacer_main.port_num);
  pacer_main.ref_port = htons (pacer_main.ref_port);
  return nargs;
}
static int
lcore_main_reference (__attribute__((unused)) void* args)
{
  pacer_main_t * pm = &pacer_main;
  uint16_t rx_port_id = 0;
  int filterIP = !!(pm->ip_refAddr[0]);
  int rv = 0;
  RTE_LOG(INFO, USER1,
          "Using an external reference with a dedicated interface\n");
  rv = rte_eth_dev_get_port_by_name (pm->ref_name, &rx_port_id);
  if (rv < 0)
    {
      rte_exit (0, "Could not find port for name %s\n", pm->ref_name);
    }
  RTE_LOG(INFO, USER1, "Reference interface %s\n", pm->ref_name);
  if (filterIP)
    {
      RTE_LOG(INFO, USER1,
              "Reference IP: %" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 " ",
              pm->ip_refAddr[0], pm->ip_refAddr[1], pm->ip_refAddr[2],
              pm->ip_refAddr[3]);
    }
  RTE_LOG(INFO, USER1, "Reference Port: %" PRIu16 "\n", ntohs(pm->ref_port));
  for (;;)
    {
      struct rte_mbuf* rx_vect[32];
      rv = rte_eth_rx_burst (rx_port_id, 0, rx_vect, 32);
      if (rv < 0)
        {
          rte_exit (0, "Error in rx burst!\n");
        }
      for (int i = 0; i < rv; i++)
        {
          if (rte_pktmbuf_mtod(rx_vect[i], struct ethhdr *)->h_proto != 0x0008)
            {
              rte_pktmbuf_free (rx_vect[i]);
              continue;
            }
          if (rte_pktmbuf_mtod_offset(rx_vect[i], struct ipv4_hdr *, 14)->next_proto_id
              != 17)
            {
              rte_pktmbuf_free (rx_vect[i]);
              continue;
            }
          if (filterIP
              && memcmp (
                  &rte_pktmbuf_mtod_offset(rx_vect[i], struct ipv4_hdr *, 14)->dst_addr,
                  pm->ip_refAddr, 4))
            {
              rte_pktmbuf_free (rx_vect[i]);
              continue;
            }
          if (rte_pktmbuf_mtod_offset(rx_vect[i], struct udphdr *, 34)->uh_dport
              != pacer_main.ref_port)
            {
              rte_pktmbuf_free (rx_vect[i]);
              continue;
            }
          __sync_fetch_and_add (&packets_received, 1);
          rte_pktmbuf_free (rx_vect[i]);
        }
    }
  return 0;
}

static int
lcore_main_slave (__attribute__((unused)) void* args)
{

  pacer_main_t * pm = &pacer_main;
  struct rte_ring * smpteRing = pm->smpteRing;
  uint16_t rx_port_id = 0;
  int rv = 0;
  rv = rte_eth_dev_get_port_by_name (pm->input_src_name, &rx_port_id);
  if (rv < 0)
    {
      rte_exit (0, "Could not find port for name %s\n", pm->input_src_name);
    }
  uint16_t tx_port_id = 0;
  rv = rte_eth_dev_get_port_by_name (pm->output_name, &tx_port_id);
  if (rv < 0)
    {
      rte_exit (0, "Could not find port for name %s\n", pm->output_name);
    }
  int vlan_offload = rte_eth_dev_get_vlan_offload (rx_port_id);
  vlan_offload |= ETH_VLAN_STRIP_OFFLOAD;
  rte_eth_dev_set_vlan_offload (rx_port_id, vlan_offload);
  char zeros[6] =
    { 0 };
  int rewriteIp = !!memcmp (pm->ipAddr, zeros, 4);
  int rewriteMac = !!memcmp (pm->macAddr, zeros, 6);
  int rewriteSIp = !!memcmp (pm->sipAddr, zeros, 4);
  int filterIP = !!memcmp(pm->ip_inputAddr, zeros, 4);
  int filterRefIP = externalReferenceIPPortOnly && !!memcmp(pm->ip_refAddr, zeros, 4);
  if (rewriteIp)
    {
      RTE_LOG(INFO, USER1, "Rewriting IP.\n");
    }
  if (rewriteMac)
    {
      RTE_LOG(INFO, USER1, "Rewriting MAC.\n");
    }
  if (rewriteSIp)
    {
      RTE_LOG(INFO, USER1, "Rewriting SourceIP.\n");
    }
  if (filterIP)
    {
      RTE_LOG(INFO, USER1, "Filtering input IP.\n");
    }
  if (filterRefIP)
    {
      RTE_LOG(INFO, USER1, "Filtering reference IP. \n");
    }
  struct ether_addr srcMac;
  rte_eth_macaddr_get (tx_port_id, &srcMac);
  int ctr = 0;
  unsigned int samplingCtr = 10;
  for (;;)
    {
      struct rte_mbuf * rx_vect[32];
      rv = rte_eth_rx_burst (rx_port_id, 0, rx_vect, 32);
      if (rv < 0)
        {
          rte_exit (0, "Error in rx burst!\n");
        }
      unsigned int newPackets = 0;
      for (int i = 0; i < rv; i++)
        {
          if (rte_pktmbuf_mtod(rx_vect[i], struct ethhdr *)->h_proto != 0x0008)
            {
              rte_pktmbuf_free (rx_vect[i]);
              continue;
            }
          if (rte_pktmbuf_mtod_offset(rx_vect[i], struct ipv4_hdr *, 14)->next_proto_id
              != 17)
            {
              rte_pktmbuf_free (rx_vect[i]);
              continue;
            }
          if (rte_pktmbuf_mtod_offset(rx_vect[i], struct udphdr *, 34)->uh_dport
              != pacer_main.port_num
              || (filterIP
                  && memcmp (
                      &rte_pktmbuf_mtod_offset(rx_vect[i], struct ipv4_hdr *, 14)->dst_addr, pm->ip_inputAddr, 4)))
            {
              if (externalReferenceIPPortOnly
                  && rte_pktmbuf_mtod_offset(rx_vect[i], struct udphdr *, 34)->uh_dport
                      == pacer_main.ref_port
                  && (!filterRefIP
                      || !memcmp (
                          &rte_pktmbuf_mtod_offset(rx_vect[i], struct ipv4_hdr *, 14)->dst_addr, pm->ip_refAddr, 4)))
                __sync_fetch_and_add (&packets_received, 1);

              rte_pktmbuf_free (rx_vect[i]);
              continue;
            }
          if (rewriteMac)
            {
              memcpy (rte_pktmbuf_mtod(rx_vect[i], struct ethhdr *)->h_dest,
                      pm->macAddr, 6);
            }
          if (rewriteIp)
            {
              memcpy (
                  &rte_pktmbuf_mtod_offset(rx_vect[i], struct ipv4_hdr *, 14)->dst_addr,
                  pm->ipAddr, 4);
            }
          if (rewriteSIp)
            {
              memcpy (
                  &rte_pktmbuf_mtod_offset(rx_vect[i], struct ipv4_hdr *, 14)->src_addr,
                  pm->sipAddr, 4);
            }

          /*Always rewrite src mac*/
          memcpy (rte_pktmbuf_mtod(rx_vect[i], struct ethhdr *)->h_source,
                  &srcMac, 6);

          rx_vect[i]->l2_len = 14;
          rx_vect[i]->l3_len = 20;
          rx_vect[i]->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM
              | PKT_TX_UDP_CKSUM;
          rte_pktmbuf_mtod_offset(rx_vect[i], struct ipv4_hdr *, 14)->hdr_checksum =
              0;
          rte_pktmbuf_mtod_offset(rx_vect[i], struct udphdr *, 34)->uh_sum =
              rte_ipv4_phdr_cksum (
                  rte_pktmbuf_mtod_offset(rx_vect[i], struct ipv4_hdr *, 14),
                  rx_vect[i]->ol_flags);
          rte_ring_enqueue (smpteRing, (void*) rx_vect[i]);
          if (!externalReferenceInterface && !externalReferenceIPPortOnly)
            __sync_fetch_and_add (&packets_received, 1);

          if (unlikely(samplingCtr))
            {
              samplingCtr--;
              RTE_LOG(INFO, USER1, "Packet Size for received pkt: %d\n",
                      rx_vect[i]->data_len);
            }
          newPackets++;
        }
      if (unlikely(!pm->init_fifo_bar))
        {
          ctr += newPackets;
          if (ctr > FIFO_INIT_SIZE)
            {
              __sync_add_and_fetch (
                  (volatile unsigned long long int*) &pm->init_fifo_bar, 1);
              ctr = 0;
            }
        }
    }
  return 0;
}

int
init_pacer_packets (void)
{
  /* Create a ring that will be big enough hopefully */
  struct rte_ring * smpteRing = rte_ring_create ("SMPTE-RING", 0x1 << 20,
                                                 rte_socket_id (),
                                                 RING_F_SP_ENQ | RING_F_SC_DEQ);
  pacer_main.smpteRing = smpteRing;
  /*Three cases for the reference:
   * - either use the input as reference
   * - or reference is on the same interface but different port or different ip
   * - or reference is on different interface*/
  externalReferenceInterface = pacer_main.ref_name[0]
      && strcmp (pacer_main.ref_name, pacer_main.input_src_name)
      && pacer_main.ref_port;
  externalReferenceIPPortOnly = !externalReferenceInterface
      && pacer_main.ref_port
      && ((pacer_main.port_num != pacer_main.ref_port)
          || (pacer_main.ip_inputAddr[0] && pacer_main.ip_refAddr[0]
              && !!memcmp (pacer_main.ip_inputAddr, pacer_main.ip_refAddr, 4)));
  if (externalReferenceInterface)
    RTE_LOG(INFO, USER1, "Using external ref with dedicated interface\n");
  if (externalReferenceIPPortOnly)
    RTE_LOG(INFO, USER1, "Using external ref with dedicated IP/port\n");
  int i;
  int j = externalReferenceInterface;
  pacer_main.init_fifo_bar = 0;
  RTE_LCORE_FOREACH_SLAVE(i)
    {
      if (j == 1)
        {
          rte_eal_remote_launch (lcore_main_reference, NULL, i);
        }
      else if (j == 0)
        {
          rte_eal_remote_launch (lcore_main_slave, NULL, i);
          break;
        }
      j--;
    }
  return 0;
}
