#!/bin/bash
set -e

NO_COMPILE=""

for arg in "$@"
do
    case "$arg" in
        -n|--no-compile)
            NO_COMPILE=1
            ;;
    esac
done

startpath=$PWD
jobs=$(($(cat /proc/cpuinfo | grep processor | wc -l) * 2))

cd ../..
curpath=$PWD

cd ..
mkdir -p build
cd build
mkdir -p linux
cd linux

export CC=gcc-5
export CXX=g++-5
cmake -DCMAKE_BUILD_TYPE=Release ../../ip2vf 
make -j $jobs

cd ${startpath}

echo Success!


