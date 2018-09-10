package com.cisco.vmi.testing.tests.test_vmi_probe;

import com.cisco.vmi.testing.Helpers;
import com.cisco.vmi.testing.ShellComponnent;
import java.io.IOException;
import java.util.logging.Level;
import java.util.logging.Logger;

public class CollectdInDockerComponnent extends ShellComponnent {
    String container_name="herisson_collectd_test";
    public void configure() {
        String loglevel = config.get("loglevel", "1");
        String id = m_ParentTest.getNextPipelineComponnentId();
        String name = "docker_collectd"+id;
        m_OutputPrefix = name;
        setShellCommand("docker run -p 5432:5432/udp --name "+container_name+" -i herisson_collectd");
    }
    
     @Override
    public void terminate(){
        try {
            int e1=Helpers.waitForShellCommand("docker kill "+container_name);
            if(e1!=0){
                System.err.println("docker kill returned with exit code: "+e1);
            }
            int e2=Helpers.waitForShellCommand("docker rm "+container_name);
            if(e2!=0){
                System.err.println("docker rm returned with exit code: "+e2);
            }
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        super.terminate();
    }
}
