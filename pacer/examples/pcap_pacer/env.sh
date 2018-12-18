# The pacer requires dedicated CPU cores for each
# stream to generate precise timing.
export RX_CORE=6 #CPU CORE used to receive packets
export TX_CORE=7 #CPU CORE used to transmit packets
export REF_CORE=8 #CPU CORE used to receive the reference stream
export MEM_SOCKET1=1024 #0 if the host machine only has one CPU socket, 1024 otherwise

# Please refer to pacer/README.md for details regarding the description of these streams
export SOURCE_IP=192.168.40.201 #Source IP address to be used for the output paced stream
export DESTINATION_IP=229.0.0.102 #Destination address of the output paced stream
export DESTINATION_MAC=33:33:00:00:00:01 #Destination MAC address of the output paced stream, to be calculated from DESTINATION_IP according to RFC1112 Section 6.4
export PORT_NUM=10000 #Destination port of the received stream to pace

export INPUT_MULTICAST_IP=229.0.0.100 #Multicast address of the input stream to paced
export REF_PORT=10000 #Destination port of the received reference stream
export REF_MULTICAST_IP=229.0.0.100 #Multicast address of the reference stream

# ./dpdk-devbind.py --status
# Run the above command to find the PCI adresses of you network cards
export INPUT_DEVICE=0000:0a:00.1 #PCI Address of the NIC receiving the stream to be paced
export OUTPUT_DEVICE=0000:0a:00.1 #PCI Address of the NIC sending the paced stream
# The input stream and reference stream should not share NICs
# as they are competing for RX timing.
export REF=0000:0a:00.1 #PCI Address of the NIC receiving the reference stream

# The size of each packet's preamble
# This is usually 24 for all vendors
# For a Cisco VIC this should be specified to 28
# Consult your vendor if unsure.
# A wrong value will result in bad packet timing
export PHY_OVERHEAD=24

export SLEEP_TIME=1 #NOT TO BE CHANGED
export MEM_SOCKET0=1024 #NOT TO BE CHANGED
export AVG_WIN=1250000000 #NOT TO BE CHANGED
export AVG_SLID_WIN=2 #NOT TO BE CHANGED
export VEC_SIZE=4 #NOT TO BE CHANGED

#The pacer outputs a stream at constant bitrate with an interpacket time denoted by IPG. It is expressed in bytes at the linerate of the output NIC (which is a unit of time).
#For maximum precision, IPG is expressed as the rational number IPG_NUM/IPG_DENOM. The initial value here are suitable for a 10Gb/s link and a
# ~1.5Gb/s SMPTE 2022-6 stream. The IPG value here is only used as the initial value for the pacer's operations, after some time, it's adjusted according to
# the reference stream
export IPG_NUM=12512500000 #NOT TO BE CHANGED
export IPG_DENOM=1349100 #NOT TO BE CHANGED
