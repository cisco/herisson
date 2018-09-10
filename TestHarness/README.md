# Herisson Testing Harness
## Introduction
This test harness will run some automated tests, by building and tearing down some sample video pipelines.
## Prerequsites
* The tests stream video from a "PCAP" video file which is not included with this repository.
* Docker is required to launch collectd.
* These tests are written in Java.
* Maven should be installed to run these tests.
* A UI is recommended so that you can see the tests' video output in the video player


## Launching the tests
* [run_tests.sh](run_tests.sh) - to launch the tests on Linux
* [run_tests.bat](run_tests.bat) - to launch the tests on Windows
