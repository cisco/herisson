package com.cisco.vmi.testing.tests.test_lib_vmi_win;

import com.cisco.vmi.testing.Helpers;
import com.cisco.vmi.testing.ShellComponnent;
import com.cisco.vmi.testing.TestingConfiguration;

public class VmiDemuxerComponnent extends ShellComponnent {

    public void configure() {
        String loglevel = config.get("loglevel", "1");
        String video_file = config.get("video_file");
        String id = m_ParentTest.getNextPipelineComponnentId();
        String name = "vMI_demux"+id;
        String in = "in_type=smpte,filename=" + video_file;
        String out1 = "out_type=shmem,control=5555,fmt=2";
        String out2 = "out_type=shmem,control=5556";
        String out = out1 + "," + out2;
        String flags = "";
        m_OutputPrefix = name;
        setShellCommand("vMI_demuxer"+Helpers.getExecutableExtensions()
                +" -c id=" + id + ",name=" + name + ",loglevel=" + loglevel + "," + in + "," + out + " " + flags);
    }
}
