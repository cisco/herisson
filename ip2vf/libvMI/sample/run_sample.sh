#!/bin/bash
export LD_LIBRARY_PATH=.

GDB=
#GDB="gdb --args "

# Modules parameters 
LOGLVL=1
NAME=vMI_adapter_2
ID=4
FLAGS=

VIDEO_FILE=../../../videos/test3.pcap

IN1=in_type=smpte,filename=$VIDEO_FILE
IN=$IN1
OUT1=out_type=shmem,control=5555
OUT2=out_type=rtp,ip=10.60.40.92,port=10000
OUT3=out_type=thumbnails,port=6041,fmt=4,ratio=4,fps=2
OUT=$OUT1,$OUT2,$OUT3

$GDB ./sample -c id=$ID,name=$NAME,loglevel=$LOGLVL,$IN,$OUT $FLAGS
