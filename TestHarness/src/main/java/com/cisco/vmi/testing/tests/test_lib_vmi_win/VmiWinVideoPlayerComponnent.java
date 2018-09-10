package com.cisco.vmi.testing.tests.test_lib_vmi_win;

import com.cisco.vmi.testing.ShellComponnent;
import com.cisco.vmi.testing.TestingConfiguration;

public class VmiWinVideoPlayerComponnent extends ShellComponnent {

    public void configure() {
        String loglevel = config.get("loglevel", "1");
        String ip = "localhost";
        String port = "6041";
        String id = m_ParentTest.getNextPipelineComponnentId();
        String name = "vMI_winvideoplayer"+id;
        String in = "in_type=tcp,ip=" + ip + ",port=" + port;
        String out = "out_type=devnull";
        String flags = "";
        m_OutputPrefix = name;
        setShellCommand("vMI_winvideoplayer.exe -c id=" + id + ",name=" + name + ",loglevel=" + loglevel + "," + in + "," + out + " " + flags);
    }
}