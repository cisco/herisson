#!/bin/bash

PACER_HOME=.
RX_CORE=10
TX_CORE=12
REF_CORE=14

SOURCE_IP=181.10.0.92
DESTINATION_IP=192.168.40.68
DESTINATION_MAC=00:50:1E:04:E3:C1
INPUT_DEVICE=0000:81:00.1
OUTPUT_DEVICE=0000:0e:00.0
PHY_OVERHEAD=28 #28 if VLAN tagging
IPG_NUM=125125000
IPG_DENOM=13491
VEC_SIZE=4
PORT_NUM=10000
AVG_WIN=1250000000
AVG_SLID_WIN=2
REF=
REF_PORT=
FLAGS=" -freq_control -phase_control "

source $PACER_HOME/../../tests/env.sh
modprobe uio
modprobe uio_pci_generic

./dpdk-devbind.py --unbind dpdk
./dpdk-devbind.py --bind=uio_pci_generic $OUTPUT_DEVICE
./dpdk-devbind.py --bind=uio_pci_generic $INPUT_DEVICE
chrt -r 1 $PACER_HOME/build/basicfwd -f $INPUT_DEVICE -ip $DESTINATION_IP -sip $SOURCE_IP -mac $DESTINATION_MAC -phy_overhead $PHY_OVERHEAD -ipg_num $IPG_NUM -ipg_denom $IPG_DENOM -vec_size $VEC_SIZE -port_num $PORT_NUM -output $OUTPUT_DEVICE -avg_win $AVG_WIN -avg_slid_win $AVG_SLID_WIN -ref $REF -ref_port $REF_PORT -- -l $RX_CORE,$TX_CORE,$REF_CORE --socket-mem 1024,1024
