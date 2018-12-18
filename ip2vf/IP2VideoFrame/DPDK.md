# DPDK Integration
## Overview
Some raw video streams have such high throughputs that the traditional network stack is challenged to keep up. To overcome this, Herisson optionally integrates [DPDK](https://dpdk.org) support to communicate with the network in a more efficient manner.

As DPDK requires the use of the latest cards and drivers, it is important to note that our test setup uses an XL710 network card


## Building with DPDK support on Windows
Note: DPDK __windows__ integration is still in early stages of development. As such it uses a separate build and testing pipeline from the rest of the project.

1. Download [POSIX Threads for Windows](https://sourceforge.net/projects/pthreads4w/files/pthreads-w32-2-9-1-release.zip/download)
   * Unzip the archive's contents into [external/pthreads-w32-2-9-1-release](../../external)
1. Edit the [external/libegel/src/vs/DpdkRteLib.props](../../external/libegel/src/vs/DpdkRteLib.props) to point to the modified version DPDK mentioned in the prerequisites section above.
   * __Prerequisite__: You will need to obtain a build of the DPDK library for __windows__ . This version is not included with Herisson, and should be obtained from the DPDK project. This project was tested with windpdk-v17.11-rc2 
   * Replace the line ```<RTE_SDK>C:\PATH_TO_DPDK</RTE_SDK>```  with the path to the modified DPDK library mentioned above.
1. Open [IP2VideoFrame.sln](IP2VideoFrame.sln) in Visual Studio.
   * Select the `Release_libegel` build configuration
   * Select the `x64` platform.
   * Build the solution.

### Testing the Windows build
Assuming successful completion of the previous build step. Open a command prompt in the  `6DC-MDC\build\windows\x64\Release_libegel` path and prepare to run the following command:

```vMI_demuxer.exe -c id=1,name=vMI_demux,loglevel=1,in_type=smpte,dpdk=1,port=10000,pci="0000:01:00.1",mcastgroup="229.0.0.123",ip="192.168.12.34",eal="eal -l 0-4 -n 4",nbpkts=1000,out_type=shmem,control=5555```

Please replace the different parameters to reflect your hardware and network setup:
* `pci` - should be PCI id of your network input device
* `mcastgroup` - The multicast address of the stream that is being ingested
* `ip` - an IP address used for IGMP/DPDK filtering of source packets
* `port` - The port to listen on
* `eal` - DPDK EAL [command line options](https://doc.dpdk.org/guides-16.04/testpmd_app_ug/run_app.html)
* [This document](../README.md) - describes the configuration of the non DPDK related parameters in the "Getting Started"


#### Monitor these messages on stdout:
To make sure that the test is running properly please monitor the output for the following messages:

Make sure that DPDK is configured with the correct network card:
* `libegel_slot_config: flow 0000:01:00.1 229.0.0.100 10000: returns 0x2, rlen 16
libegel_slot_config: 000002244F58D280 000002244F58C040`

Make sure that that the FPS statistics are greater than 0, so that the system is processing incomming video frames.
* `0: -i- MetricsCollector::tick(): vMI_demux: FPS: i1: 60, o2: 30,`
