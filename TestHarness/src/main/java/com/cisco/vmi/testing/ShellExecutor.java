/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;

/**
 *
 * @author eshalev
 */
public class ShellExecutor implements Runnable {

    final List<String> m_ToRun;
    final TestingConfiguration m_Config;
    private Process m_RunningProcess;
    //Subscribe for a line of stdout output
    public EventPublisher<String> m_OnStdOut=new EventPublisher<>();
    //Subscribe for a line of stderr output
    public EventPublisher<String> m_OnStdErr=new EventPublisher<>();
    //Subscribe for a line of either stdout output or stderr
    public EventPublisher<String> m_OnOutput=new EventPublisher<>();
    
    ShellExecutor(List<String> toRun, TestingConfiguration config) {
        m_ToRun = toRun;
        m_Config = config;
    }

    @Override
    public void run() {
        try {
            //Runtime rt = Runtime.getRuntime();
            System.out.println("Starting command: " + m_ToRun);
            //final Process proc = rt.exec(m_ToRun);
            ProcessBuilder pb = new ProcessBuilder(m_ToRun);
            
            Map<String, String> prependEnvironement = m_Config.m_PrependEnvironement;
            //modify environment variables for child process:
            prependEnvironmentVariables(prependEnvironement, pb);
            m_RunningProcess = pb.start();
            //publish all process output to subscribers:
            Thread stdoutReader = forEachLineInStream(m_RunningProcess.getInputStream(), (msg) -> m_OnStdOut.publish(msg));
            Thread stderrReader = forEachLineInStream(m_RunningProcess.getErrorStream(), (msg) -> m_OnStdErr.publish(msg));
            //forward both messages on shared pipeline:
            m_OnStdOut.pipeTo(m_OnOutput);
            m_OnStdErr.pipeTo(m_OnOutput);
            int result = m_RunningProcess.waitFor();
            //wait for all output to complete:
            stdoutReader.join();
            stderrReader.join();
            String message="error level= " + result + " for command: " + m_ToRun;
            if (result != 0 && !m_TerminatedProactively) {
                throw new RuntimeException(message);
            }else{
                System.out.println(message);
            }
        } catch (Exception ex) {
            throw new RuntimeException(ex);
        }

    }

    private void prependEnvironmentVariables(Map<String, String> prependEnvironement, ProcessBuilder pb) {
        for(String key : prependEnvironement.keySet()){
            String value=prependEnvironement.get(key)+pb.environment().getOrDefault(key, "");
            System.out.println("Setting environment: "+key+"="+value);
            pb.environment().put(key, value);
        }
    }

    public Thread forEachLineInStream(InputStream in, Consumer<String> callback) {
        BufferedReader reader = new BufferedReader(new InputStreamReader(in));
        Thread t = new Thread(() -> {
            String s = null;
            try {
                while ((s = reader.readLine()) != null) {
                    callback.accept(s);
                }
            } catch (IOException ex) {
                if(ex.getMessage().trim().toLowerCase().equals("stream closed")){
                    System.out.println("stdout stream closed for: "+m_ToRun); 
                }else{
                ex.printStackTrace();
                }
            }
        });
        t.start();
        return t;

    }
    
    boolean m_TerminatedProactively=false;
    
    void terminate() {
        if(m_RunningProcess!=null){
            m_TerminatedProactively=true;
            m_RunningProcess.destroy();
        }
    }

    void await() {
        if(m_RunningProcess!=null){
            try {
                m_RunningProcess.waitFor();
            } catch (InterruptedException ex) {
                ex.printStackTrace();
            }
        }
    }

}
