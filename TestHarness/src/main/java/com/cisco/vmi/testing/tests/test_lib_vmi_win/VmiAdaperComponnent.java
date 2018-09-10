package com.cisco.vmi.testing.tests.test_lib_vmi_win;

import com.cisco.vmi.testing.Helpers;
import com.cisco.vmi.testing.ShellComponnent;

public class VmiAdaperComponnent extends ShellComponnent {

    @Override
    public void configure() {
        String loglevel = config.get("loglevel", "1");
        String id = m_ParentTest.getNextPipelineComponnentId();
        String name = "vMI_adapter"+id;
        String in1 = "in_type=shmem,control=5555";
        String in2 = "in_type=shmem,control=5556";
        String in = in1;
        String out1 = "out_type=shmem,control=5558";
        String out2 = "out_type=shmem,,control=5559";
        String out3 = "out_type=rtp,ip=localhost,port=10021";
        String out = out3;
        String flags = "";
        m_OutputPrefix=name;
        setShellCommand("vMI_adapter"+Helpers.getExecutableExtensions()
                +" -c id=" + id + ",name=" + name + "," + "loglevel=" + loglevel + "," + in + "," + out + " " + flags);
    }
}
