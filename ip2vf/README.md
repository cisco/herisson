# Getting Started
This repository contains the sources to build both the Herrison library and some sample Herrison modules. This getting started guide will explain how to erect a simple Herrison pipeline using the provided modules.
* Modules - Each module is a single step in a media processing pipeline: For example a module can add video effects, such as: adding some captions, scaling a video's size, or converting it's format. Modules are connected to each other via libvMI

* libvMI - A library which will allow you to write your own media processing modules, and connect them with the rest of a VMI pipeline. libvMI will take care of the video transport so that modules only need to handle raw video frames.

**This guide was tested on Ubuntu 16.04 LTS**


## Building
You can build the project by running [clean_make.sh](clean_make.sh). This will generate a path called build in the root path of this repository. In the build path you will find  two important output paths:
* build/vMIModules - Will contain some sample VMI modules which we will use in the next section.

* build/libVMI - Will contain the VMI library which you can link to if you want to create a module

## Running a sample pipeline
In this section we will be running a sample herrison pipeline which will generate and process a live video stream. *This tutorial uses a "PCAP" video file which is not included in this repository.*
#### Open a terminal window:
Go to the previously generated `build/vMIModules` path (or add it to your $PATH )
#### Generate a "live" stream:
In the terminal window run:

`vMI_demuxer -c id=1,name=vMI_demux1,loglevel=1,in_type=smpte,filename=../../videos/test3.pcap,out_type=shmem,control=5555`
This command will take the pre-recorded PCAP file (not included) and generate a live looping video stream. Notice the comma separated string which is used to configure this module. Let's examine some of the parameters, as they will be used to configure later modules as well:
* `id=1` - Assigns a unique ID to this instance in the pipeline. It can be used later to control the module or gather statistics
* `name=vMI_demux1` - Assigns a human readable name for the module
* `in_type=smpte` - Tells the module what kind input it is ingesting. In this case, this is an smpte capture file
* `out_type=shmem` - This module will be outputting frames to a shared memory buffer. Pipeline components can communicate over shared memory buffers, thus saving the overhead of unnecessary network overhead when running  on the same machines
* `control=5555` - Will generate a control signal on port 5555 signaling other modules when new frames are available

#### Stream over the network:
In the terminal window run:

`vMI_adapter -c id=2,name=vMI_adapter2,loglevel=1,in_type=shmem,control=5555,out_type=rtp,ip=localhost,port=10021`
The adapter is a pass-through module which can take a local output and stream it over the network to another machine. A Herrison pipeline is completely modular, at any stage you can interconnect local modules via shared memory, or stream the data to remote modules. This allows you to easily scale your workload across multiple severs and operating systems. Lets take a look at some of the new configuration options introduced in this step:
* `in_type=shmem,control=5555` - Configures the adapter to ingest the stream from the previous step via shared memory.
* `out_type=rtp,ip=localhost,port=10021` - Streams the output over RTP (possibly to another compute node)

#### Process the video
In the terminal window run:

`vMI_converter -c id=3,name=vMI_converter3,loglevel=1,in_type=rtp,port=10021,out_type=shmem,control=5560`
In this stage we will demonstrate a simple video processing step: Our video is encoded in 10 bit color space, and we would like to convert it to 8 bits so that it is more suitable for playback on a computer. Mind you this is just a simple example of what a processing module can do. You can build your own video processing modules using libVMI and connect them to this pipeline, or choose from a variety of third party modules.
* `in_type=rtp,port=10021` - Reads the input from a network stream produced by the previous step.
* `out_type=shmem,control=5560` - Output the resulting video to the next step over shared memory

#### Generate some thumbnails to debug the current stream
In the terminal window run:

`vMI_adapter -c id=4,name=vMI_adapter4,loglevel=1,in_type=shmem,control=5560,out_type=shmem,control=5561,out_type=thumbnails,port=6041,fmt=4,ratio=4,fps=20`

In this step we will demonstrate how to generate a stream of thumbnails, which will allow us to inspect the pipeline at different points, and possibly display them on a control console somewhere in the production studio. Here we are using the adapter again to transport the output of the previous module to another network location:
* `out_type=thumbnails,port=6041` - Create a stream of thumbnails on port 6041.
* `fmt=4` - Use 4 byte pixel depth
* `ratio=4` - Scale each thumbnail to a 4th of the original frame's size
* `fps=20` - Generate 20 thumbnails per second

#### View the thumbnails in real time
In the terminal window run:

`vMI_videoplayer -c id=5,name=vMI_winvideoplayer5,loglevel=1,in_type=tcp,ip=localhost,port=6041,out_type=devnull`

In this step we will view the thumbnails generated by the previous step. If the pipeline was configured successfully, we will be able to view a thumbnail preview of our video stream in a new window. **It is best to run this step on a desktop machine so that you can see the output**
* `out_type=devnull` - Being the final module in the pipeline, this module will not output anything.

#### Congratulations:
If everything went well, at this stage you should now be viewing video in the videoplayer window.

### Next steps:
Try to build your own Herrison module, and discover how easy it is to insert it into a pipeline.
