# The Probe
The probe can be plugged in to a pipeline to stream performance statistics to collectd

## Running the probe
In this section we will integrate the probe into the sample pipeline discussed in the [Getting Started](ip2vf/README.md) guide. Before starting this tutorial you should already be familiarize yourself with the modules in that guide.

#### Configuring collectd to recieve data:
As mentioned, the probe forwards telemetry to collectd.

You can find a [sample docker configuration](../../TestHarness/external/collectd/Dockerfile) which are used the [automatic tests](../../TestHarness) .
This demo container receives the statistics and prints them to stdout.
Please build and run the container and expose a UDP port for collectd to listen on.
You can either use your own instance of collectd, or use our provided test file.

`# Build an image, and setup the collectd schema to receive the data:`

`docker build -t herisson_collectd .`

`# Run the image and expose a udp port:`

`docker run -p 5432:5432/udp --name herisson_collectd_test -i herisson_collectd`

#### Positioning the probe in the pipeline:
In the [Getting Started](ip2vf/README.md) guide, we will initiate the pipeline as usual up to and including the "Stream over the network" stage.
Now, instead of processing the video let's probe the network stream for information.

#### Open a terminal window:
Go to the previously generated `build/vMIModules` path (or add it to your $PATH )
#### Run the probe executable:
In the terminal window run:

`vMI_probe -M -c id=4,name=vMI_probe4,loglevel=1,collectdip=localhost,collectdport=5432,in_type=rtp,pktTS=1,dbg=1,port=10021,out_type=devnull`

This command will send statistics about your video stream to collectd, where you can monitor it.
* `collectdip` - This is the IP address where collectd is listening for statistics.
* `collectdport` - The probe will stream statistics to collectd over UDP

## Understanding the output
If you ran the docker testing harness, and collectd is configured to output to stdout, you will see output such as:

#### Packet Gaps:
`PUTVAL yourmachine/vMI_pktTSmetrics-vMI_probe4/videopacketgap-i1 interval=10.000 1533567716.123:21055.134153:412449.121637:2012.000000:17844195.000000`

The inter-packet gap measures the delay between the arrival as seen by the probe of (the first byte of) two subsequent packets in the video stream.
Many standards that define video transmission over IP network (e.g. SMPTE 2022-6 or SMPTE 2110) mandate very precise pacing of the packets, i.e. they require a specific inter-packet gap that ideally should be constant or with very small fluctuations during the whole video transmission.
This probe metric collects statistics of the inter-packet gaps for each frame and provides mean, standard deviation, minimum value, and maximum value of the samples measured over the observation period.

#### Video Timestamp
`PUTVAL yourmachine/vMI_pktTSmetrics-vMI_probe4/videotimestamp-i1 interval=10.000 1533567716.019:3074606060.000000`

This is applicable only when video is transmitted with SMPTE 2022-6 (or with Herisson internal video format).
SMPTE 2022-6 packet header includes a 32 bits value named timestamp (or HBRMP timestamp), which represents the clock counter at the video source device when the SMPTE frame is transmitted.
Considering a clock rate (i.e. the number of times the clock counter ticks every second) of CR, then the timestamp cycles every (2 ^ 32)/CR seconds.
This means that the timestamp uniquely identifies a frame within a time window of 2^32/CR seconds. When the same SMPTE 2022-6 signal is sent over multiple paths and then received at the same location (e.g. the probe), ideally each frame from the 2 copies of the signal should be received at the same time.  The comparison of the arrival time of frames marked with the same video timestamp can be used to measure the �skew� accumulated by the 2 signals.

#### Frames Per Second
`PUTVAL yourmachine/vMI_metrics-vMI_demux1/videofps-i1 interval=10.000 1533567716.141:60.604785`

The frames per second measures the rate at which frames are received by the probe, i.e. the inverse of the delay between two subsequent complete frame receptions.
A value lower than the rate at which frames are generated at the source is an indicator of network problems that causes packet losses or delays, hence reception of incomplete frames.
Problems in the network can also cause frame rates higher than expected due to packet bursty arrival.

## Next steps:
Eventually you would want to stream your data to a dashboard such as [Promethues](https://prometheus.io/) and [Grafana](https://grafana.com/) .
