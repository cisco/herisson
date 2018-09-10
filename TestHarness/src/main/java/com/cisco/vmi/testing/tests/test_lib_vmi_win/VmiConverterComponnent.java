package com.cisco.vmi.testing.tests.test_lib_vmi_win;

import com.cisco.vmi.testing.Helpers;
import com.cisco.vmi.testing.ShellComponnent;
import com.cisco.vmi.testing.TestingConfiguration;

public class VmiConverterComponnent extends ShellComponnent {

    public void configure() {
        String loglevel = config.get("loglevel", "1");
        String id = m_ParentTest.getNextPipelineComponnentId();
        String name = "vMI_converter"+id;
        String in = "in_type=rtp,port=10021";
        String out = "out_type=shmem,control=5560";
        String flags = "";
        m_OutputPrefix = name;
        setShellCommand("vMI_converter"+Helpers.getExecutableExtensions()
                +" -c id=" + id + ",name=" + name + ",loglevel=" + loglevel + "," + in + "," + out + " " + flags);
    }
}
