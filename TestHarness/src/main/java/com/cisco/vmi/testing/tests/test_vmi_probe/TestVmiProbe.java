/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing.tests.test_vmi_probe;

import com.cisco.vmi.testing.BaseVmiTest;
import com.cisco.vmi.testing.Helpers;
import com.cisco.vmi.testing.ShellComponnent;
import com.cisco.vmi.testing.tests.test_lib_vmi_win.VmiAdaperComponnent;
import com.cisco.vmi.testing.tests.test_lib_vmi_win.VmiDemuxerComponnent;
import java.io.File;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/**
 *
 * @author eshalev
 */
public class TestVmiProbe extends BaseVmiTest {

    public int m_WaitTimeInSeconds = 30;

    private CountDownLatch m_WaitForCollectdStats;

    @Override
    public void setup() {
        m_WaitForCollectdStats = new CountDownLatch(5);
        super.setup();
        String videoFileName = "../videos/test3.pcap";
        m_Config.set("video_file", videoFileName);
        Helpers.assertThatFileExists(videoFileName);
        /*addShellComponnent(new ShellComponnent() {
            @Override
            public void configure() {
                setShellCommand("ping -n 20 wwwin.cisco.com");
            }
        });*/

        addShellComponnent(new VmiDemuxerComponnent());
        addShellComponnent(new VmiAdaperComponnent());
        CollectdInDockerComponnent collectdComponnent = new CollectdInDockerComponnent();
        addShellComponnent(collectdComponnent);
        addShellComponnent(new VmiProbeComponnent());
        
        //do some verification here:
        collectdComponnent.m_OnOutput.subscribe((msg) -> {
            if (msg.contains("videotimestamp-i1")) {
                String[] tokens = msg.replace(":", " ").trim().split(" +");
                //verify that input fps is greater than 0
                String videotimestampToken=tokens[3];
                float timestamp = new Float(videotimestampToken);
                if (timestamp > 1532000000) {
                    System.out.println("parsed timestamp");
                    m_WaitForCollectdStats.countDown();
                }
            }
        });
    }

    @Override
    public void verify() throws Exception {
        boolean framesWereRecievedInVideoPlayer = m_WaitForCollectdStats.await(m_WaitTimeInSeconds, TimeUnit.SECONDS);
        System.out.println("Done waiting for FPS messages. Duration: ");
        assertTrue(framesWereRecievedInVideoPlayer, "Videoplayer is processing input frames");
        testCompleted();
    }
    
    @Override
 public boolean shouldRun(){
     System.err.println(this);
     return !Helpers.isWindows();
   }    
}
