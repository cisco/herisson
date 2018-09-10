package com.cisco.vmi.testing.tests.test_lib_vmi_win;

import com.cisco.vmi.testing.Helpers;
import com.cisco.vmi.testing.ShellComponnent;
import com.cisco.vmi.testing.TestingConfiguration;

public class VmiAdaperComponnent2 extends ShellComponnent {

    public void configure() {
        String loglevel = config.get("loglevel", "1");
        String id = m_ParentTest.getNextPipelineComponnentId();
        String name = "vMI_adapter"+id;
        String in = "in_type=shmem,control=5560";
        String out1 = "out_type=shmem,control=5561";
        String out3 = "out_type=thumbnails,port=6041,fmt=4,ratio=4,fps=20";
        String out = out1 + "," + out3;
        String flags = "";
        m_OutputPrefix = name;
        setShellCommand("vMI_adapter"+Helpers.getExecutableExtensions()
                +" -c id=" + id + ",name=" + name + "," + "loglevel=" + loglevel + "," + in + "," + out + " " + flags);
    }
}
