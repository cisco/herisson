# Herrison
## Introduction
Herrison streams high throughput low latency media streams over IP based networks. The main goals of the project are to allow virtualization of [SDI](https://en.wikipedia.org/wiki/Serial_digital_interface) connections thereby making it both easier to develop new live video processing applications, and to allow the transfer of high quality raw video  over a standard IP networking infrastructure.

To achieve this, Herrison employs many low level performance optimizations, and can produce high quality outputs for external studio hardware devices.

## Overview
Herrison is distributed into many modules. The core of which is a native C++ library which allows high throughput production and consumption of many formats:
* SMPTE
* Shared memory (fast inter-module communications)
* PNG Thumbnails
* ...
* Many other outputs are possible by extending the library.

A developer can create their own modules by linking to the Herrison library, thereby creating their own pipeline components such as: Color Filters, Video Compositing, and assorted video converters. The library exposes a simple frame buffer approach. As such both the real time networking and media format conversion aspects are handled internally by the system. This empowers the developer to concentrate purely on video processing without having to worry about communicating with the other stages of the pipeline.

Herrison modules can be run inside Docker containers to support modern data-center virtualization paradigms. Or they can be run on bare metal GPU wielding Windows machines to allow backwards compatibility with the current setup of the current paradigms of SDI based video pipelines.

## Additional Reading:
[Getting Started](ip2vf/README.md) - Will teach you how to build and run a sample pipeline.
