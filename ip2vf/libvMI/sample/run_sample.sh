#!/bin/bash
export LD_LIBRARY_PATH=.

GDB=
#GDB="gdb --args "

# Modules parameters 
LOGLVL=1
NAME=vMI_adapter_2
ID=4
FLAGS=

# Video frame size settings
WIDTH=1920
HEIGHT=1080
COMPONENTS=2
DEPTH=8
FRAME_SIZE=$((WIDTH*HEIGHT*COMPONENTS*DEPTH/8))

IN1=in_type=shmem,shmkey=5560,zmqport=5560,vidfrmsize=$FRAME_SIZE
IN=$IN1
OUT1=out_type=shmem,shmkey=5561,zmqip=localhost,zmqport=5561,vidfrmsize=$FRAME_SIZE
OUT2=out_type=rtp,ip=10.60.40.92,port=10000,vidfrmsize=$FRAME_SIZE
OUT3=out_type=thumbnails,port=6041,fmt=4,ratio=4,fps=2,vidfrmsize=$FRAME_SIZE
OUT=$OUT1,$OUT2,$OUT3

$GDB ./sample -p 5013 -c id=$ID,name=$NAME,loglevel=$LOGLVL,$IN,$OUT $FLAGS
