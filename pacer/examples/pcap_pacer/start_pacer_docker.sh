#!/bin/bash
MODNAME=igb_uio
IIFNAME=`./dpdk-devbind.py --status | grep $INPUT_DEVICE | sed "s/^[^ ].*' if=\([^ ]*\).*\$/\1/g"`
echo "Detected input device $IIFNAME"
IIFDRIVER=`./dpdk-devbind.py --status | grep $INPUT_DEVICE | sed "s/^[^ ].*' if=[^ ]* drv=\([^ ]*\).*\$/\1/g"`
echo "Detected input driver $IIFDRIVER"
IIPADDR=`ip addr list dev $IIFNAME | grep " inet " | awk '{print $2}'`
echo "Detected input address $IIPADDR"

OIFNAME=`./dpdk-devbind.py --status | grep $OUTPUT_DEVICE | sed "s/^[^ ].*' if=\([^ ]*\).*\$/\1/g"`
echo "Detected output device $OIFNAME"
OIFDRIVER=`./dpdk-devbind.py --status | grep $OUTPUT_DEVICE | sed "s/^[^ ].*' if=[^ ]* drv=\([^ ]*\).*\$/\1/g"`
echo "Detected output driver $OIFDRIVER"
OIPADDR=`ip addr list dev $OIFNAME | grep " inet " | awk '{print $2}'`
echo "Detected output address $OIPADDR"

RIFNAME=`./dpdk-devbind.py --status | grep $REF | sed "s/^[^ ].*' if=\([^ ]*\).*\$/\1/g"`
echo "Detected ref device $RIFNAME"
RIFDRIVER=`./dpdk-devbind.py --status | grep $REF | sed "s/^[^ ].*' if=[^ ]* drv=\([^ ]*\).*\$/\1/g"`
echo "Detected ref driver $RIFDRIVER"
RIPADDR=`ip addr list dev $RIFNAME | grep " inet " | awk '{print $2}'`
echo "Detected ref address $RIPADDR"

function cleanup() {
 kill $PID
 ./dpdk-devbind.py --unbind $INPUT_DEVICE
 ./dpdk-devbind.py --unbind $REF
 ./dpdk-devbind.py --unbind $OUTPUT_DEVICE
 ./dpdk-devbind.py --bind=$IIFDRIVER $INPUT_DEVICE
 ./dpdk-devbind.py --bind=$RIFDRIVER $REF
 sleep $SLEEP_TIME
 ip link set dev $IIFNAME up
 ip link set dev $RIFNAME up
 ip addr add $IIPADDR dev $IIFNAME
 ip addr add $RIPADDR dev $RIFNAME
 smcroute -k
 sleep 1
 smcroute -d
 sleep 1
 smcroute -j $IIFNAME $INPUT_MULTICAST_IP
 smcroute -j $RIFNAME $REF_MULTICAST_IP
 sleep 1
 smcroute -l $IIFNAME $INPUT_MULTICAST_IP
 smcroute -l $RIFNAME $REF_MULTICAST_IP
 sleep 1
 ./dpdk-devbind.py --bind=$OIFDRIVER $OUTPUT_DEVICE
 ip link set dev $OIFNAME up
 ip addr add $OIPADDR dev $OIFNAME
 sleep 1
 smcroute -k
 exit 0
}
modprobe uio
insmod igb_uio.ko
#mkdir -p /dev/hugepages
#mount -t hugetlbfs none /dev/hugepages

#join multicast group for input and ref
sleep $SLEEP_TIME
smcroute -k
ip link set dev $IIFNAME up || :
ip link set dev $RIFNAME ip || :
sleep 1
smcroute -d
sleep 1
smcroute -j $IIFNAME $INPUT_MULTICAST_IP
smcroute -j $RIFNAME $REF_MULTICAST_IP
sleep 1
ip link set dev $IIFNAME down
ip link set dev $RIFNAME down
ip link set dev $OIFNAME down
sleep 1
smcroute -k

trap "cleanup" SIGINT SIGTERM EXIT
./dpdk-devbind.py --unbind $INPUT_DEVICE
./dpdk-devbind.py --unbind $REF
./dpdk-devbind.py --unbind $OUTPUT_DEVICE
./dpdk-devbind.py --bind=$MODNAME $OUTPUT_DEVICE
./dpdk-devbind.py --bind=$MODNAME $INPUT_DEVICE
./dpdk-devbind.py --bind=$MODNAME $REF
./vMI_pacer -f $INPUT_DEVICE\
 -ip $DESTINATION_IP\
 -sip $SOURCE_IP\
 -mac $DESTINATION_MAC\
 -phy_overhead $PHY_OVERHEAD\
 -ipg_num $IPG_NUM\
 -ipg_denom $IPG_DENOM\
 -vec_size $VEC_SIZE\
 -port_num $PORT_NUM\
 -output $OUTPUT_DEVICE\
 -avg_win $AVG_WIN\
 -avg_slid_win $AVG_SLID_WIN\
 -ref $REF\
 -freq_control\
 -ref_port $REF_PORT\
 -ip_input $INPUT_MULTICAST_IP\
 -ip_ref $REF_MULTICAST_IP\
 -- -l $RX_CORE,$TX_CORE,$REF_CORE\
 -w $INPUT_DEVICE\
 -w $REF\
 -w $OUTPUT_DEVICE\
 --socket-mem $MEM_SOCKET0,$MEM_SOCKET1 --file-prefix "$DESTINATION_IP" &
PID=$!
wait $PID
