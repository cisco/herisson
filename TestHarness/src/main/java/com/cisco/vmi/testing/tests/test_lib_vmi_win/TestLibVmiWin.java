/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing.tests.test_lib_vmi_win;

import com.cisco.vmi.testing.BaseVmiTest;
import com.cisco.vmi.testing.Helpers;
import com.cisco.vmi.testing.ShellComponnent;
import java.io.File;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/**
 *
 * @author eshalev
 */
public class TestLibVmiWin extends BaseVmiTest {

    public int m_WaitTimeInSeconds = 30;

    private ShellComponnent m_VideoPlayer;
    private CountDownLatch m_CountFpsMessages;

    @Override
    public void setup() {
        m_CountFpsMessages = new CountDownLatch(2);
        super.setup();
        String videoFileName= "../videos/test3.pcap";
        m_Config.set("video_file", videoFileName);
        Helpers.assertThatFileExists(videoFileName);
        /*addShellComponnent(new ShellComponnent() {
            @Override
            public void configure() {
                setShellCommand("ping -n 20 wwwin.cisco.com");
            }
        });*/
        if (Helpers.isWindows()) {
            m_VideoPlayer = new VmiWinVideoPlayerComponnent();
        } else {
            m_VideoPlayer = new VmiLinuxVideoPlayerComponnent();
        }

        addShellComponnent(new VmiDemuxerComponnent());
        addShellComponnent(new VmiAdaperComponnent());
        addShellComponnent(new VmiConverterComponnent());
        addShellComponnent(new VmiAdaperComponnent2());
        addShellComponnent(m_VideoPlayer);
        //do some verification here:
        m_VideoPlayer.m_OnOutput.subscribe((msg) -> {
            if (msg.contains("FPS: i1:")) {
                String[] tokens = msg.replace(",", "").trim().split(" +");
                //verify that input fps is greater than 0
                String fpsToken=Helpers.removeColorCodesFromString(tokens[7]);
                float framesPerSecond = new Float(fpsToken);
                if (framesPerSecond > 0.5) {
                    System.out.println("Input frames detected");
                    m_CountFpsMessages.countDown();
                }
            }
        });
    }

    
    @Override
    public void verify() throws Exception {
        boolean framesWereRecievedInVideoPlayer = m_CountFpsMessages.await(m_WaitTimeInSeconds, TimeUnit.SECONDS);
        System.out.println("Done waiting for FPS messages. Duration: ");
        assertTrue(framesWereRecievedInVideoPlayer, "Videoplayer is processing input frames");
        testCompleted();
    }
    
   
}
