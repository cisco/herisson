package com.cisco.vmi.testing.tests.test_vmi_probe;

import com.cisco.vmi.testing.Helpers;
import com.cisco.vmi.testing.ShellComponnent;

public class VmiProbeComponnent extends ShellComponnent {

    public void configure() {
        String loglevel = config.get("loglevel", "1");
        String id = m_ParentTest.getNextPipelineComponnentId();
        String name = "vMI_probe"+id;
        String in = "in_type=rtp,pktTS=1,dbg=1,port=10021";
        String out = "out_type=devnull";
        String flags = "";
        String collectd="collectdip=localhost,collectdport=5432,";
        m_OutputPrefix = name;
        setShellCommand("vMI_probe"+Helpers.getExecutableExtensions()
                +" -M -c id=" + id + ",name=" + name + ",loglevel=" + loglevel + "," +collectd+ in + "," + out + " " + flags);
    }
}
