#!/bin/bash
set -e
echo
echo This file will clean make all vMI binaries/libraries 
echo

echo
echo cleaning and building 
echo
cd scripts/setup
./build_demo.sh $*

echo $0

