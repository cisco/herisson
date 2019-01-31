#!/bin/bash
set -e
echo
echo This file will clean make all the vMI pacer 
echo

echo
echo cleaning and building 
echo
cd ../pacer/dpdk
git submodule init
git submodule update
git reset --hard HEAD
git apply -3 < ../dpdk.patch
make install T=x86_64-native-linuxapp-gcc
cd ../examples/pcap_pacer/
source ../../tests/env.sh
make

echo $0

